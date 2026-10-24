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
        txt = raw[:raw.index(b'\0')]
        if all(32 <= c < 127 and c not in b'"\\?' for c in txt):
            return '"%s"' % txt.decode()
    while len(vals) > 1 and vals[-1] in ('0', '0.0f', '0.0'):
        vals.pop()
    return '{' + ', '.join(vals) + '}'


def bytes_init(a, n):
    b = img[a - B:a - B + n]
    if not b.strip(b'\0'):
        return ''
    return ' INIT({%s})' % ', '.join('0x%02X' % x for x in b.rstrip(b'\0'))


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
        clash = set()
        for x, y in zip(runs, runs[1:]):
            if y[0] < x[1]:
                last = x[3][-1]
                clash.add(last if prim[last][0] + prim[last][1] > y[0] else y[3][0])
        if not clash:
            break
        for p in clash:
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
            init = initialiser(f, p, a, size) if typed and SEC == 'igd' else None
            if typed and (SEC == 'igb' or init is not None):
                out.append('%s(%s) %s%s;' % (macro, run, owner[p][1], ' INIT(%s)' % init if init else ''))
                home[p] = p
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
