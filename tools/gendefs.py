# Writes the data definitions for igidecomp.exe: each global named in src/addr.c is defined in
# one .c file that declares it (or includes its header), in a block at the end of that file.
# A file's globals are grouped into runs of nearby addresses; each run is one section the
# linker places at the run's IGI.exe address (tools/genfill.py fills the gaps), so the layout
# is IGI.exe's. Initial values come from IGI.exe: numbers as numbers, pointers as the global
# or function they point at. Names at the same address or inside another global become
# aliases. Arrays of unknown length and values that can't be written yet (structs, pointers
# into unnamed data) are kept as IGI.exe's bytes; globals that can't be placed exactly
# (shared words, odd alignment) are left to fill.c.
# usage: gendefs.py IGI.exe   (run from the repo root)
import bisect
import collections
import glob
import os
import re
import struct
import subprocess
import sys
import tempfile

import pefile

pe = pefile.PE(sys.argv[1])
img = pe.get_memory_mapped_image()
B = 0x400000
assert pe.FILE_HEADER.TimeDateStamp == 0x3A1D74C6, 'needs the EU IGI.exe'

SECTIONS = (('igd', 0x533000, 0x550000), ('igb', 0x550000, 0xC35000))
# IGI.exe's import address table: its slots hold the functions they name (mixerOpen_ -> mixerOpen)
IAT = (0x533000, 0x533228)
# dinput.lib's object formats in .text, which src/exe/fill.c carries (tools/genfill.py)
TEXT_DATA = ((0x4AE680, 'igi_dimouse', 0xB0), (0x4AE740, 'igi_dikeyboard', 0x1000), (0x516FA0, 'igi_dijoystick', 0x2C0))
MARK = '/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */'
CC = ['i686-w64-mingw32-gcc', '-std=gnu11', '-O2', '-fno-strict-aliasing', '-march=i686', '-malign-data=abi',
      '-DIGI_STANDALONE', '-Isrc', '-w']
NOT_IN_EXE = {'src/addr.c', 'src/dllmain.c', 'src/terrdraw/crt.c'}
SCALARS = {
    'int': 'i', 'signed': 'i', 'signed int': 'i', 'int32_t': 'i', 'long': 'i', 'BOOL': 'i',
    'unsigned': 'I', 'unsigned int': 'I', 'uint32_t': 'I', 'DWORD': 'I', 'unsigned long': 'I', 'UINT': 'I',
    'short': 'h', 'int16_t': 'h', 'uint16_t': 'H', 'unsigned short': 'H', 'WORD': 'H',
    'char': 'b', 'signed char': 'b', 'int8_t': 'b', 'uint8_t': 'B', 'unsigned char': 'B', 'BYTE': 'B',
    'float': 'f', 'double': 'd',
}

at = {n: int(a, 16) for n, a in re.findall(r'^AT\((\w+), (0x[0-9A-F]+)\)', open('src/addr.c').read(), re.M)}
ident = re.compile(r'[A-Za-z_]\w*')
files = sorted(f for f in glob.glob('src/**/*.[ch]', recursive=True) if not f.startswith('src/exe/'))
text = {f: open(f).read().split('\n' + MARK)[0] for f in files}

# code addresses that have a rewrite
hooks = {}
for f in files:
    for a, n in re.findall(r'^HOOK\((0x[0-9A-Fa-f]+), (\w+)\);', text[f], re.M):
        if f not in NOT_IN_EXE:
            hooks[int(a, 16)] = n

# file-scope declarations: name -> [(file, declaration without extern)]
decls = collections.defaultdict(list)
for f in files:
    for line in text[f].split('\n'):
        m = re.match(r'extern\s+([^;]*);', line)
        if not m:
            continue
        names = [i for i in ident.findall(m[1]) if i in at]
        if len(names) == 1 and ',' not in m[1].replace('(', '').split(')')[0]:
            decls[names[0]].append((f, m[1].strip()))

# who includes which header
includes = collections.defaultdict(set)
for f in files:
    if f.endswith('.c'):
        for h in re.findall(r'^#include "([^"]+)"', text[f], re.M):
            for cand in (os.path.normpath(os.path.join(os.path.dirname(f), h)), os.path.normpath(os.path.join('src', h))):
                if cand in text:
                    includes[cand].add(f)


def uses(f, n):
    return len(re.findall(r'\b%s\b' % n, text[f]))


owner = {}
for n in at:
    cands = []
    for f, d in decls[n]:
        if f.endswith('.c') and f not in NOT_IN_EXE:
            cands.append((uses(f, n), f, d))
        elif f.endswith('.h'):
            cands += [(uses(g, n), g, d) for g in includes[f] if g not in NOT_IN_EXE and uses(g, n)]
    if cands:
        c = max(cands, key=lambda c: (c[0], c[1]))
        owner[n] = (c[1], c[2])


def unsized(n):
    return bool(re.search(r'\b%s\s*\[\s*\]' % n, owner[n][1]))


# sizeof / alignof as each owner file sees the name
info = {}
tmp = tempfile.mkdtemp()


def probe(f, names):
    """{name: (sizeof, alignof)} as f sees them; None if f can't size them all"""
    p = os.path.join(tmp, 'probe.c')
    with open(p, 'w') as o:
        o.write('#include "%s"\n' % os.path.abspath(f))
        for n in names:
            e = n + '[0]' if unsized(n) else n
            o.write('char __sz_%s[sizeof(%s)]; char __al_%s[__alignof__(%s)];\n' % (n, e, n, e))
    if subprocess.run(CC + ['-fcommon', '-c', p, '-o', p + '.o'], capture_output=True).returncode:
        return None
    vals = {}
    for line in subprocess.run(['i686-w64-mingw32-nm', p + '.o'], capture_output=True, text=True).stdout.splitlines():
        q = line.split()
        if len(q) == 3 and q[2].startswith(('___sz_', '___al_')):
            vals[q[2][1:]] = int(q[0], 16)
    return {n: (vals['__sz_' + n], vals['__al_' + n]) for n in names}


byfile = collections.defaultdict(list)
for n, (f, d) in owner.items():
    byfile[f].append(n)
for f, names in byfile.items():
    got = probe(f, names)
    if got is None:  # some type isn't complete there: size the names one by one
        got = {}
        for n in names:
            got.update(probe(f, [n]) or {})
    for n, (size, align) in got.items():
        info[n] = (size, align)


def is_const(d, n):
    """the object itself is const (not just what it points at)"""
    before = d.split(n)[0]
    return bool(re.search(r'\bconst\s*$', before)) or ('*' not in before and re.match(r'\s*const\b', before))


def scalar_kind(d, n):
    """(struct code, C type) of a declaration that's a scalar or an array of scalars"""
    m = re.match(r'\s*(?:const\s+)?(.*?)\s*\b%s\s*((?:\[[^\]]*\])*)\s*$' % n, d)
    if not m:
        return None
    t = m[1].strip()
    if t.endswith('*'):
        return 'P', t
    return (SCALARS[t], t) if t in SCALARS else None


def number(code, raw):
    v = struct.unpack('<' + code, raw)[0]
    if code == 'f':
        if v != v or v in (float('inf'), float('-inf')):
            return None
        for k in range(6, 12):
            s = '%.*g' % (k, v)
            if struct.pack('<f', float(s)) == raw:
                return (s if any(c in s for c in '.en') else s + '.0') + 'f'
    if code == 'd':
        if v != v or v in (float('inf'), float('-inf')):
            return None
        s = repr(v)
        return s
    return str(v)




def c_string(raw):
    """a C string literal for raw (which ends at its first NUL), or None"""
    txt = raw[:raw.index(b'\0')]
    if any(c < 32 and c not in b'\t\n\r' or c > 126 for c in txt):
        return None
    out = ''
    for c in txt:
        ch = chr(c)
        out += {'"': '\\"', '\\': '\\\\', '\t': '\\t', '\n': '\\n', '\r': '\\r'}.get(ch, ch)
    return '"%s"' % out


# the declared type of each name, from the debug info of a probe object
def dwarf(obj):
    """{offset: (tag, {attr: value}, [child offsets])} from objdump --dwarf=info"""
    out = subprocess.run(['i686-w64-mingw32-objdump', '--dwarf=info', obj], capture_output=True, text=True).stdout
    dies, stack, cur = {}, [], None
    for line in out.splitlines():
        m = re.match(r'\s*<(\d+)><([0-9a-f]+)>: Abbrev Number: (\d+)(?: \((\w+)\))?', line)
        if m:
            depth, off = int(m[1]), int(m[2], 16)
            if m[3] == '0':
                continue
            cur = [m[4], {}, []]
            dies[off] = cur
            del stack[depth:]
            if stack:
                stack[-1][2].append(off)
            stack.append(cur)
            continue
        m = re.match(r'\s*<[0-9a-f]+>\s+(DW_AT_\w+)\s*:\s*(.*)$', line)
        if m and cur is not None:
            cur[1][m[1]] = m[2].strip()
    return dies


def ref(v):
    m = re.match(r'<0x([0-9a-f]+)>', v)
    return int(m[1], 16) if m else None


def attr_int(v):
    m = re.match(r'(-?\d+)', v)
    return int(m[1]) if m else None


def name_of(v):
    return v.rsplit(': ', 1)[-1].strip() if v else None


class Fallback(Exception):
    pass


def fallback(why):
    if os.environ.get('GENDEFS_WHY'):
        print('bytes:', why, file=sys.stderr)
    raise Fallback(why)


def strip_t(dies, t):
    while t is not None and dies[t][0] in ('DW_TAG_typedef', 'DW_TAG_const_type', 'DW_TAG_volatile_type'):
        t = ref(dies[t][1].get('DW_AT_type', ''))
    return t


def size_of(dies, t):
    t = strip_t(dies, t)
    tag, a, kids = dies[t]
    if 'DW_AT_byte_size' in a:
        return attr_int(a['DW_AT_byte_size'])
    if tag == 'DW_TAG_array_type':
        n = 1
        for k in kids:
            n *= dim(dies, k)
        return n * size_of(dies, ref(a['DW_AT_type']))
    fallback('no size')


def dim(dies, k):
    a = dies[k][1]
    if 'DW_AT_count' in a:
        return attr_int(a['DW_AT_count'])
    if 'DW_AT_upper_bound' in a:
        return attr_int(a['DW_AT_upper_bound']) + 1
    fallback('array bound')


def value(f, dies, t, raw, addr, expr):
    """C initialiser for raw bytes of type t at addr; expr is an lvalue of that type for casts"""
    t = strip_t(dies, t)
    tag, a, kids = dies[t]
    if tag == 'DW_TAG_base_type':
        enc = attr_int(a['DW_AT_encoding'])
        size = attr_int(a['DW_AT_byte_size'])
        if enc == 4:
            s = number({4: 'f', 8: 'd'}[size], raw)
            if s is None:
                fallback('%s: nan or inf' % expr)
            return s
        signed = enc in (5, 6)
        v = int.from_bytes(raw, 'little', signed=signed)
        return str(v) + ('u' if not signed and v > 0x7FFFFFFF else '')
    if tag == 'DW_TAG_enumeration_type':
        return str(int.from_bytes(raw, 'little', signed=True))
    if tag == 'DW_TAG_pointer_type':
        v = int.from_bytes(raw, 'little')
        if v == 0:
            return '0'
        if v in hooks:
            refs[f].add(hooks[v])
            return '(__typeof__(%s))ref_%s' % (expr, hooks[v])
        for lo, n, size in TEXT_DATA:
            if lo <= v < lo + size:
                refs[f].add(n)
                return '(__typeof__(%s))(ref_%s + %d)' % (expr, n, v - lo) if v > lo else '(__typeof__(%s))ref_%s' % (expr, n)
        hit = target(v) if 0x533000 <= v < 0xC35000 else None
        if not hit:
            fallback('%s: pointer 0x%X into nothing named' % (expr, v))
        n, off = hit
        refs[f].add(n)
        return '(__typeof__(%s))ref_%s' % (expr, n) if not off else '(__typeof__(%s))(ref_%s + %d)' % (expr, n, off)
    if tag == 'DW_TAG_array_type':
        et = ref(a['DW_AT_type'])
        dims = [dim(dies, k) for k in kids]
        if len(dims) != 1:  # an array of arrays: one brace level per dimension
            inner = size_of(dies, et)
            for d in dims[1:]:
                inner *= d
            vals = []
            for i in range(dims[0]):
                chunk = raw[i * inner:(i + 1) * inner]
                vals.append(sub_array(f, dies, et, dims[1:], chunk, addr + i * inner, '%s[%d]' % (expr, i)))
            while vals and vals[-1] in ('0', '{0}'):
                vals.pop()
            return '{' + ', '.join(vals) + '}' if vals else '{0}'
        es = size_of(dies, et)
        n = dims[0]
        e = strip_t(dies, et)
        if dies[e][0] == 'DW_TAG_base_type' and es == 1 and attr_int(dies[e][1]['DW_AT_encoding']) in (6, 8) \
                and b'\0' in raw and not raw[raw.index(b'\0'):].strip(b'\0'):
            lit = c_string(raw)
            if lit:
                return lit
        vals = [value(f, dies, et, raw[i * es:(i + 1) * es], addr + i * es, '%s[%d]' % (expr, i)) for i in range(n)]
        while vals and vals[-1] in ('0', '{0}'):
            vals.pop()
        return '{' + ', '.join(vals) + '}' if vals else '{0}'
    if tag == 'DW_TAG_structure_type':
        covered = bytearray(len(raw))
        out = []
        for k in kids:
            ktag, ka, _ = dies[k]
            if ktag != 'DW_TAG_member':
                continue
            if 'DW_AT_bit_size' in ka or 'DW_AT_name' not in ka:
                fallback('%s: bitfield or unnamed member' % expr)
            off = attr_int(ka.get('DW_AT_data_member_location', '0'))
            mt = ref(ka['DW_AT_type'])
            ms = size_of(dies, mt)
            name = name_of(ka['DW_AT_name'])
            covered[off:off + ms] = b'\1' * ms
            v = value(f, dies, mt, raw[off:off + ms], addr + off, '%s.%s' % (expr, name))
            if v not in ('0', '{0}'):
                out.append('.%s = %s' % (name, v))
        if any(b and not c for b, c in zip(raw, covered)):
            fallback('%s: bytes outside the named members' % expr)
        return '{' + ', '.join(out) + '}' if out else '{0}'
    fallback('%s: %s' % (expr, tag))


def sub_array(f, dies, et, dims, raw, addr, expr):
    es = size_of(dies, et)
    for d in dims[1:]:
        es *= d
    if len(dims) == 1:
        vals = [value(f, dies, et, raw[i * es:(i + 1) * es], addr + i * es, '%s[%d]' % (expr, i)) for i in range(dims[0])]
    else:
        vals = [sub_array(f, dies, et, dims[1:], raw[i * es:(i + 1) * es], addr + i * es, '%s[%d]' % (expr, i))
                for i in range(dims[0])]
    while vals and vals[-1] in ('0', '{0}'):
        vals.pop()
    return '{' + ', '.join(vals) + '}' if vals else '{0}'


types = {}  # name -> (dies, type offset) as its owner file sees it


def probe_types(f, names):
    p = os.path.join(tmp, 'tprobe.c')
    with open(p, 'w') as o:
        o.write('#include "%s"\n' % os.path.abspath(f))
        for n in names:
            o.write('__typeof__(%s) *__probe_%s;\n' % (n + '[0]' if unsized(n) else n, n))
    if subprocess.run(CC + ['-O0', '-g', '-c', p, '-o', p + '.o'], capture_output=True).returncode:
        return
    dies = dwarf(p + '.o')
    for off, (tag, a, kids) in dies.items():
        if tag == 'DW_TAG_variable' and (name_of(a.get('DW_AT_name')) or '').startswith('__probe_'):
            n = name_of(a['DW_AT_name'])[8:]
            ptr = ref(a['DW_AT_type'])
            types[n] = (dies, ref(dies[ptr][1]['DW_AT_type']))


for f, names in byfile.items():
    want = [n for n in names if n in info and 0x533000 <= at[n] < 0x550000]
    if want:
        probe_types(f, want)


def typed_init(f, n, a, size, expr):
    """initialiser for n's bytes at a, or None"""
    if n not in types:
        return None
    dies, t = types[n]
    try:
        if unsized(n):
            es = size_of(dies, t)
            if size % es:
                return None
            vals = [value(f, dies, t, img[a + i - B:a + i + es - B], a + i, '%s[%d]' % (expr, i // es))
                    for i in range(0, size, es)]
            while vals and vals[-1] in ('0', '{0}'):
                vals.pop()
            return '{' + ', '.join(vals) + '}' if vals else '{0}'
        return value(f, dies, t, img[a - B:a - B + size], a, expr)
    except (Fallback, KeyError):
        return None


# data the data points at that no global names (mostly string literals): each target gets a
# definition of its own (igidecomp.exe only), owned by the file that defines the global
# pointing at it, and runs up to the next named address
synthetic = set()
named = sorted(set(at.values()))


def holder_of(a):
    i = bisect.bisect_right(named, a) - 1
    if i < 0:
        return None
    names = [n for n, x in at.items() if x == named[i] and n in owner]
    return names[0] if names else None


def inside_named(v):
    i = bisect.bisect_right(named, v) - 1
    if i < 0:
        return False
    if named[i] == v:
        return True
    sizes = [info[n][0] for n, x in at.items() if x == named[i] and n in info and not unsized(n)]
    return bool(sizes) and v < named[i] + max(sizes)


wanted = {}
for a in range(0x533000, 0x550000, 4):
    v = int.from_bytes(img[a - B:a - B + 4], 'little')
    if 0x533000 <= v < 0x550000 and not inside_named(v) and holder_of(a):
        wanted.setdefault(v, owner[holder_of(a)][0])
stops = sorted(set(named) | set(wanted) | {0x550000})
for v, f in sorted(wanted.items()):
    nxt = stops[bisect.bisect_right(stops, v)]
    raw = img[v - B:v - B + (nxt - v)]
    if b'\0' in raw and c_string(raw):
        n, size, decl = 'str_%06X' % v, raw.index(b'\0') + 1, 'char str_%06X[%d]'
    else:
        n, size, decl = 'dat_%06X' % v, nxt - v, 'uint8_t dat_%06X[%d]'
    at[n] = v
    owner[n] = (f, decl % (v, size))
    info[n] = (size, 1)
    synthetic.add(n)

refs = collections.defaultdict(set)  # file -> names it points at by assembler name
ats = sorted((a, n) for n, a in at.items())


def target(v):
    """(name, offset) of the global holding address v; the gap up to the next name counts as
    part of a name whose size isn't known"""
    i = max((k for k, (a, n) in enumerate(ats) if a <= v), default=None)
    if i is None:
        return None
    a = ats[i][0]
    names = [n for b, n in ats if b == a]
    if v == a:
        return (sorted(names)[0], 0)
    nxt = next((b for b, n in ats if b > a), 0xC35000)
    size = max((info[n][0] for n in names if n in info and not unsized(n)), default=nxt - a)
    return (sorted(names)[0], v - a) if v < a + size else None


def pointer(f, t, v):
    """initialiser for a pointer of type t holding v, or None"""
    if v == 0:
        return '0'
    if v in hooks:
        refs[f].add(hooks[v])
        return '(%s)ref_%s' % (t, hooks[v])
    if 0x533000 <= v < 0xC35000:
        hit = target(v)
        if hit:
            n, off = hit
            refs[f].add(n)
            return '(%s)ref_%s' % (t, n) if not off else '(%s)(ref_%s + %d)' % (t, n, off)
    return None


def initialiser(f, n, a, size):
    """value or '{...}' for name n at a, from IGI.exe's bytes; None if it can't be written"""
    k = scalar_kind(owner[n][1], n)
    if not k:
        return None
    code, t = k
    width = 4 if code == 'P' else struct.calcsize(code)
    if size % width:
        return None
    vals = []
    for o in range(0, size, width):
        raw = img[a + o - B:a + o - B + width]
        s = pointer(f, t, struct.unpack('<I', raw)[0]) if code == 'P' else number(code, raw)
        if s is None:
            return None
        vals.append(s)
    if '[' not in owner[n][1].split(n, 1)[1]:
        return vals[0]
    raw = img[a - B:a - B + size]
    if t == 'char' and b'\0' in raw and not raw[raw.index(b'\0'):].strip(b'\0'):
        lit = c_string(raw)
        if lit:
            return lit
    while len(vals) > 1 and vals[-1] in ('0', '0.0f', '0.0'):
        vals.pop()
    return '{' + ', '.join(vals) + '}'


def bytes_init(a, n):
    b = img[a - B:a - B + n]
    if not b.strip(b'\0'):
        return ''
    return ' INIT({%s})' % ', '.join('0x%02X' % x for x in b.rstrip(b'\0'))



def typed_prefix(f, n, a, size, expr):
    """(count, initialiser) for the longest run of n's elements from a that can be written"""
    if n not in types:
        return 0, None
    dies, t = types[n]
    try:
        es = size_of(dies, t)
    except (Fallback, KeyError):
        return 0, None
    vals = []
    for i in range(0, size - size % es, es):
        try:
            vals.append(value(f, dies, t, img[a + i - B:a + i + es - B], a + i, '%s[%d]' % (expr, i // es)))
        except (Fallback, KeyError):
            break
    count = len(vals)
    while vals and vals[-1] in ('0', '{0}'):
        vals.pop()
    return count, ('{' + ', '.join(vals) + '}' if vals else '{0}')


def portable(n):
    """the declaration needs no types of its own file (a number, or a pointer to one or void)"""
    if n in synthetic:
        return True
    m = re.match(r'\s*(?:const\s+)?(.*?)\s*\b%s\s*(\[[^\]]*\])*\s*$' % n, owner[n][1])
    if not m:
        return False
    t = re.sub(r'\bconst\b|\*', ' ', m[1]).split()
    return ' '.join(t) in SCALARS or ' '.join(t) == 'void'


movedonce = set()
# phase 1: per section, the primaries that tile it and the runs they form
plans = []
ndef = nalias = ndrop = 0
for SEC, LO, HI in SECTIONS:
    addrs = sorted({a for n, a in at.items() if LO <= a < HI})
    names_at = collections.defaultdict(list)
    for n, a in at.items():
        if LO <= a < HI:
            names_at[a].append(n)
    prim = {}       # name -> (start, size, typed)
    alias = {}      # name -> (primary, offset)
    dropped = set()
    cover = (0, 0, None)
    for i, a in enumerate(addrs):
        nxt = addrs[i + 1] if i + 1 < len(addrs) else HI
        if a < cover[1] and cover[2]:
            for n in names_at[a]:
                alias[n] = (cover[2], a - cover[0])
            continue
        known = [n for n in names_at[a] if n in info]
        if not known:
            cover = (a, a, None)
            continue
        full = [n for n in known if not unsized(n)]
        if full:
            p = max(full, key=lambda n: (info[n][0], n))
            prim[p] = (a, info[p][0], True)
        else:  # an array of unknown length: storage up to the next global
            p = sorted(known)[0]
            prim[p] = (a, nxt - a, False)
        for n in names_at[a]:
            if n != p:
                alias[n] = (p, 0)
        cover = (a, a + prim[p][1], p)

    def key(p):
        return owner[p][0], SEC == 'igd' and bool(is_const(owner[p][1], p))

    while True:
        groups = collections.defaultdict(list)
        for p in prim:
            groups[key(p)].append(p)
        runs = []
        for (f, c), ps in groups.items():
            ps.sort(key=lambda p: prim[p][0])
            run = None
            for p in ps:
                a, size, _ = prim[p]
                if a % info[p][1]:
                    if os.environ.get('GENDEFS_WHY'):
                        print('fill: %s at 0x%X is not %d-aligned' % (p, a, info[p][1]), file=sys.stderr)
                    dropped.add(p)
                    continue
                if run and a - run[1] <= 64 and not any(run[1] <= prim[q][0] < a and key(q) != (f, c) for q in prim):
                    run[1], run[2] = a + size, max(run[2], info[p][1])
                    run[3].append(p)
                else:
                    run = [a, a + size, max(4, info[p][1]), [p], f, c]
                    runs.append(run)
        for p in dropped:
            prim.pop(p, None)
        for r in runs:
            r[0] -= r[0] % r[2]
            r[1] += -r[1] % r[2]
        runs.sort()
        clash, moved = set(), False
        for x, y in zip(runs, runs[1:]):
            if y[0] < x[1]:
                last = x[3][-1]
                p, other = (last, y) if prim[last][0] + prim[last][1] > y[0] else (y[3][0], x)
                q, mine = (y[3][0], x) if p == last else (x[3][-1], y)
                # a global of a plain type can be defined by the file it shares a word with
                for g, run in ((p, other), (q, mine)):
                    if portable(g) and (g, run[4]) not in movedonce and (SEC == 'igb' or bool(is_const(owner[g][1], g)) == run[5]):
                        owner[g] = (run[4], owner[g][1])
                        movedonce.add((g, run[4]))
                        moved = True
                        break
                else:
                    clash.add(p)
        if moved:
            continue
        if not clash:
            break
        for p in clash:
            if os.environ.get('GENDEFS_WHY'):
                print('fill: %s at 0x%X shares bytes with a run of another file' % (p, prim[p][0]), file=sys.stderr)
            prim.pop(p)
            dropped.add(p)
    alias = {n: v for n, v in alias.items() if v[0] in prim and n not in prim}
    plans.append((SEC, prim, alias, runs))
    ndef += len(prim)
    nalias += len(alias)
    ndrop += len(dropped)

# phase 2: the blocks
blocks = collections.defaultdict(list)
for SEC, prim, alias, runs in plans:
    macro = 'BSS' if SEC == 'igb' else 'DATA'
    home = {}   # primary -> what its aliases point at
    for start, end, align, ps, f, c in runs:
        run = '%06X' % start
        pad = 'PAD_CONST' if c else 'PAD'
        here = start
        out = []
        for p in ps:
            a, size, typed = prim[p]
            if a > here:
                out.append('%s(%s, %s, %06X, 0x%X)%s;' % (pad, SEC, run, here, a - here,
                                                        bytes_init(here, a - here) if SEC == 'igd' else ''))
            if IAT[0] <= a < IAT[1] and p.endswith('_'):
                init = p[:-1]
            else:
                init = (initialiser(f, p, a, size) if p in synthetic else typed_init(f, p, a, size, p)) \
                    if typed and SEC == 'igd' else None
            if typed and (SEC == 'igb' or init is not None):
                out.append('%s(%s) %s%s;' % (macro, run, owner[p][1], ' INIT(%s)' % init if init not in (None, '{0}', '0') else ''))
                home[p] = p
                here = a + size
                continue
            store = 'store_%06X' % a
            if not typed and SEC == 'igb' and p in info and info[p][0] and size % info[p][0] == 0:
                count, init = size // info[p][0], '{0}'
            else:
                count, init = typed_prefix(f, p, a, size, store) if not typed and SEC == 'igd' else (0, None)
            if count:  # an array of unknown length, as typed storage under the name
                es = info[p][0]
                if portable(p):  # the declaration itself: the file may not declare p
                    decl = re.sub(r'\b%s\s*\[\s*\]' % p, '%s[%d]' % (store, count), owner[p][1], count=1)
                else:
                    decl = '__typeof__(%s[0]) %s[%d]' % (p, store, count)
                out.append('STORE(%s, %s, %d) %s%s;' % (
                    SEC, run, info[p][1], decl, ' INIT(%s)' % init if init != '{0}' else ''))
                home[p] = store
                out.append('ALIAS(%s, %s, 0);' % (p, store))
                if count * es < size:
                    out.append('%s(%s, %s, %06X, 0x%X)%s;' % (pad, SEC, run, a + count * es, size - count * es,
                                                            bytes_init(a + count * es, size - count * es)))
            else:  # IGI.exe's bytes, under the name
                out.append('%s(%s, %s, %06X, 0x%X)%s;' % (pad, SEC, run, a, size,
                                                        bytes_init(a, size) if SEC == 'igd' else ''))
                home[p] = 'pad_%06X' % a
                out.append('ALIAS(%s, %s, 0);' % (p, home[p]))
            here = a + size
        if end > here:
            out.append('%s(%s, %s, %06X, 0x%X)%s;' % (pad, SEC, run, here, end - here,
                                                    bytes_init(here, end - here) if SEC == 'igd' else ''))
        for n, (p, off) in sorted(alias.items()):
            if p in ps:
                out.append('ALIAS(%s, %s, %d);' % (n, home[p], off))
        blocks[f] += out

for f in files:
    old = open(f).read()
    body = old.split('\n' + MARK)[0].rstrip('\n') + '\n'
    block = ['REF_EXTERN(%s);' % n for n in sorted(refs[f])] + blocks[f]
    new = body + ('\n' + MARK + '\n' + '\n'.join(block) + '\n' if blocks[f] else '')
    if new != old:
        open(f, 'w').write(new)
print('%d defined, %d aliases, %d left to fill.c' % (ndef, nalias, ndrop), file=sys.stderr)
