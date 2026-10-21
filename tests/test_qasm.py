import struct

import pytest

from emu import Emu, ErrorShown, compare, TRAP_BASE

LOG_SIZE = 0x40000
FILE = 0x5A5A0001


def trap(e, name, fn):
    addr = TRAP_BASE + len(e.imports) * 4
    e.imports.append(("fake", name))
    e.write(addr, b"\xc3")
    e.stub(name, fn)
    return addr


def patch(e, addr, name, fn):
    t = trap(e, name, fn)
    e.write(addr, b"\xe9" + struct.pack("<i", t - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def cstr(e, s):
    if isinstance(s, str):
        s = s.encode("latin-1")
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def init_list(e, p):
    e.write(p, u32(p + 4) + u32(0) + u32(p))


RULES = [
    (rb"[ \t\n\r]+", 1, 1),
    (rb'"([~\\"]*(\.)*)*"', 2, 1),
    (rb"[a-zA-Z][a-zA-Z0-9_]*:", 10, 1),
    (rb"-?[0-9]+/.[0-9]+", 5, 1),
    (rb",", 6, 1),
    (rb"/(", 7, 1),
    (rb"/)", 8, 1),
    (rb"0x[0-9a-fA-F]+", 3, 1),
    (rb"[0-9a-zA-Z]+:", 11, 1),
    (rb"@[0-9a-zA-Z]+", 12, 1),
    (rb"[a-zA-Z][a-zA-Z0-9_.]*", 9, 0),
    (rb"-?[0-9]+", 4, 0),
]


def make_emu():
    """Memory set up, File_Write logging into memory, the assembler's token rules built."""
    e = Emu()
    e.stub("strcat", lambda e: (e.write(e.arg(0) + len(e.read_cstr(e.arg(0), 1 << 20)), e.read_cstr(e.arg(1), 1 << 20) + b"\0"), e.ret(e.arg(0))))
    e.call(0x4A537B)  # the C runtime's float formatting hooks, or %f fails
    e.call(0x4B0BA0)
    e.write(0xA969C4, u32(0x1000))
    e.write(0xA96598, u32(0))
    e.log = e.alloc(LOG_SIZE)

    def file_write(e):
        f, buf, n = e.arg(0), e.arg(1), e.arg(2)
        used = e.u32(e.log)
        rec = u32(f) + u32(n) + e.read(buf, n)
        assert used + len(rec) < LOG_SIZE - 4
        e.write(e.log + 4 + used, rec)
        e.write(e.log, u32(used + len(rec)))
        e.ret(0)

    patch(e, 0x4B1700, "File_Write", file_write)
    e.rules = e.alloc(12)
    e.call(0x4C0660, e.rules)
    for pat, id_, acc in RULES:
        e.call(0x4C0680, e.rules, cstr(e, pat), id_, acc)
    e.empty_rules = e.alloc(12)
    init_list(e, e.empty_rules)
    e.baseline = e.snapshot()
    return e


emu = make_emu()


def written(e):
    data = e.read(e.log + 4, e.u32(e.log))
    out = b""
    while data:
        n = struct.unpack("<I", data[4:8])[0]
        out += data[8:8 + n]
        data = data[8 + n:]
    return out


def compare_error(e, orig, ours, setup=None, args=()):
    results = []
    for target in (orig, ours):
        e.reset()
        a = (setup(e) or args) if setup else args
        before = e.snapshot()
        with pytest.raises(ErrorShown) as info:
            e.call(target, *a)
        results.append((str(info.value), e.changes(before)))
    assert results[0] == results[1]
    return results[0][0]


# --- lexer ----------------------------------------------------------------

def lexer(e, src, rules=None):
    r = e.alloc(0x20)
    e.call(0x4BF1C0, r, cstr(e, src), len(src))
    lex = e.alloc(0x14)
    e.call(0x4C0720, lex, rules or e.rules, r)
    return lex


def push_token(e, lex, text, type_):
    n = e.call(0x4B0C60, 0x2C8, 4)[0]
    e.write(n + 8, text + b"\0")
    e.write(n + 0x2C4, u32(type_))
    e.call(0x4AF8F0, lex + 8, n)


def test_lex_init():
    compare(emu, 0x4C0720, "QvmLex_Init", setup=lambda e: (e.alloc(0x14), 0x1234, 0x5678), check_eax=False)


@pytest.mark.parametrize("n", [0, 1, 3])
def test_lex_free(n):
    def setup(e):
        lex = lexer(e, b"x")
        for i in range(n):
            push_token(e, lex, b"t%d" % i, i)
        return (lex,)
    compare(emu, 0x4C0750, "QvmLex_Free", setup=setup, check_eax=False)


LONG = b'"' + b"a" * 800 + b'"'


@pytest.mark.parametrize("src", [
    b"  abc", b"abc def", b'"str" x', b"lbl: x", b"1.5", b"-12", b"0x1F", b"@ab", b",", b"(", b")",
    b"$", b"", b"\n\n", b"\nfoo", LONG, b'"' + b"b" * 698 + b'"', b'"' + b"c" * 697 + b'"', b"12ab:", b"a.b.c",
])
def test_lex_scan(src):
    def setup(e):
        return e.alloc(0x2C8), lexer(e, src)
    compare(emu, 0x4C0780, "QvmLex_Scan", setup=setup)


def test_lex_scan_pending():
    def setup(e):
        lex = lexer(e, b"abc")
        push_token(e, lex, b"first", 9)
        push_token(e, lex, b"second", 4)
        return e.alloc(0x2C8), lex
    compare(emu, 0x4C0780, "QvmLex_Scan", setup=setup)


def test_lex_scan_no_rules():
    def setup(e):
        return e.alloc(0x2C8), lexer(e, b"abc", e.empty_rules)
    compare(emu, 0x4C0780, "QvmLex_Scan", setup=setup)


# --- assembler ------------------------------------------------------------

@pytest.mark.parametrize("s", [b"", b"0", b"1234abcd", b"FFFFFFFF", b"deadBEEF", b"g", b"123456789", b"!", b"Zz"])
def test_parse_hex(s):
    compare(emu, 0x4C08C0, "QvmAsm_ParseHex", setup=lambda e: (cstr(e, s),))


def test_asm_register_ops():
    compare(emu, 0x4C0900, "QvmAsm_RegisterOps", check_eax=False, normalized=True)


def text_list(e, p, count, prefix):
    """count small nodes holding distinct names, built directly in memory."""
    if not count:
        init_list(e, p)
        return
    base = e.alloc(16 * count)
    data = bytearray()
    for i in range(count):
        node = base + 16 * i
        nxt = node + 16 if i + 1 < count else p + 4
        prev = node - 16 if i else p
        data += u32(nxt) + u32(prev) + (prefix + b"%05x" % i).ljust(8, b"\0")
    e.write(base, bytes(data))
    e.write(p, u32(base) + u32(0) + u32(base + 16 * (count - 1)))


def asm(e, src, pc=0x40, strings=0, symbols=0):
    a = e.alloc(0x50)
    e.write(a, u32(pc))
    e.write(a + 0x10, u32(FILE))
    e.write(a + 0x18, u32(lexer(e, src)))
    e.write(a + 0x1C, u32(e.call(0x4C0360, 31)[0]))
    init_list(e, a + 0x20)
    text_list(e, a + 0x2C, symbols, b"y")
    text_list(e, a + 0x38, strings, b"s")
    init_list(e, a + 0x44)
    return a


def asm_setup(src, op=0, **kw):
    def setup(e):
        return e.alloc(4), asm(e, src, **kw), op, 2
    return setup


TINY = b"0." + b"0" * 60 + b"1"


@pytest.mark.parametrize("src", [
    b"0", b"1", b"-1", b"5", b"-128", b"127", b"128", b"-129", b"32767", b"-32768", b"32768", b"-32769",
    b"100000", b"-2147483648", b"0.0", b"1.0", b"-1.0", b"2.5", b"-0.0", b"1.00000001", TINY, b"3.4e", b"",
    b'"hello"', b"foo", b"lbl:", b"@12", b",",
])
def test_asm_op_push(src):
    compare(emu, 0x4C0A10, "QvmAsm_OpPush", setup=asm_setup(src), check_eax=False)


@pytest.mark.parametrize("kind,count", [("strings", 0xFF), ("strings", 0x100), ("strings", 0xFFFF), ("strings", 0x10000),
                                        ("symbols", 0xFF), ("symbols", 0x100), ("symbols", 0x10000)])
def test_asm_op_push_index(kind, count):
    src = b'"new"' if kind == "strings" else b"newname"
    compare(emu, 0x4C0A10, "QvmAsm_OpPush", setup=asm_setup(src, **{kind: count}), check_eax=False)


def test_asm_op_push_written():
    emu.reset()
    args = asm_setup(b"-129")(emu)
    emu.call("QvmAsm_OpPush", *args)
    assert written(emu) == b"\x04\x7f\xff"


@pytest.mark.parametrize("src", [b"target", b"@1234abcd", b"@0", b"5", b"", b"lbl:"])
@pytest.mark.parametrize("op", [0x14, 0x16])
def test_asm_op_branch(src, op):
    compare(emu, 0x4C0E60, "QvmAsm_OpBranch", setup=asm_setup(src, op), check_eax=False)


@pytest.mark.parametrize("op", [0, 1, 0x13, 0x30, 0x1FF])
def test_asm_op_plain(op):
    compare(emu, 0x4C0F50, "QvmAsm_OpPlain", setup=asm_setup(b"", op), check_eax=False)


@pytest.mark.parametrize("src", [
    b"(a, @10, b)", b"()", b"(@1)", b"(a,)", b"(@ff, a)", b"(a, b, c, d, e, f)", b"x", b"(a b)", b"(5)", b"(",
    b"(a", b"(a,", b"", b"(@1,,@2)",
])
def test_asm_op_call(src):
    compare(emu, 0x4C0F80, "QvmAsm_OpCall", setup=asm_setup(src, 0x18), check_eax=False)


# --- disassembler -----------------------------------------------------------

def table(e, p, names, size=None, text_size=None):
    text = b"".join(n + b"\0" for n in names)
    offs, o = [], 0
    for n in names:
        offs.append(o)
        o += len(n) + 1
    po = e.alloc(4 * len(offs) + 4)
    e.write(po, b"".join(u32(x) for x in offs))
    pt = cstr(e, text)
    e.write(p + 8, u32(len(names) if size is None else size) + u32(len(text) if text_size is None else text_size) + u32(po) + u32(pt))


def dis_setup(code, pc=0, indent=8, strings=(), symbols=(), op=0, **kw):
    def setup(e):
        vm = e.alloc(0x70)
        c = e.alloc(len(code) + 16)
        e.write(c, code)
        e.write(vm, u32(c))
        table(e, vm + 0x30, strings, kw.get("ssize"), kw.get("stext"))
        table(e, vm + 0x48, symbols)
        d = e.alloc(12)
        e.write(d, u32(FILE) + u32(indent) + u32(pc))
        return d, vm, op, 3
    return setup


def test_dis_register_ops():
    compare(emu, 0x4C10E0, "QvmDis_RegisterOps", check_eax=False, normalized=True)


def test_dis_op_plain():
    compare(emu, 0x4C11F0, "QvmDis_OpPlain", setup=dis_setup(b"\x01"), check_eax=False)


@pytest.mark.parametrize("v", [0, 1, -1, 0x7FFFFFFF, -0x80000000, 123456])
def test_dis_op_push(v):
    compare(emu, 0x4C1210, "QvmDis_OpPush", setup=dis_setup(b"\x00\x00\x02" + struct.pack("<i", v), pc=2), check_eax=False)


@pytest.mark.parametrize("f", [0.0, 1.5, -2.25, 1e20, -1e19, 1e-30])
def test_dis_op_pushf(f):
    compare(emu, 0x4C1270, "QvmDis_OpPushF", setup=dis_setup(b"\x05" + struct.pack("<f", f)), check_eax=False)


def test_dis_op_pushf_nan():
    compare(emu, 0x4C1270, "QvmDis_OpPushF", setup=dis_setup(b"\x05" + u32(0x7FC00000)), check_eax=False)


@pytest.mark.parametrize("v", [0, -5, 0x100, -0x1000])
def test_dis_op_branch(v):
    compare(emu, 0x4C12E0, "QvmDis_OpBranch", setup=dis_setup(b"\x00" * 7 + b"\x14" + struct.pack("<i", v), pc=7), check_eax=False)


@pytest.mark.parametrize("v", [0, 0x7F, 0x80, 0xFF])
def test_dis_op_pushb(v):
    compare(emu, 0x4C1340, "QvmDis_OpPushB", setup=dis_setup(bytes([3, v])), check_eax=False)


@pytest.mark.parametrize("v", [0, 0x7FFF, 0x8000, 0xFFFF])
def test_dis_op_pushw(v):
    compare(emu, 0x4C13A0, "QvmDis_OpPushW", setup=dis_setup(b"\x04" + struct.pack("<H", v)), check_eax=False)


STRS = [b"plain", b'q"uote', b"tab\there", b"nl\nx\ry", b"\x0bvt"]
SYMS = [b"alpha", b"beta", b"gamma"]


@pytest.mark.parametrize("op,operand", [
    (8, u32(1)), (9, b"\x02"), (10, b"\x03\x00"), (8, u32(4)), (9, b"\x09"), (10, b"\x00\x01"), (8, u32(0xFFFFFFFF)),
    (12, u32(0)), (13, b"\x02"), (14, b"\x01\x00"), (13, b"\x05"),
])
def test_dis_op_push_indexed(op, operand):
    setup = dis_setup(bytes([op]) + operand, strings=STRS, symbols=SYMS, op=op)
    compare(emu, 0x4C1400, "QvmDis_OpPushIndexed", setup=setup, check_eax=False)


def test_dis_op_push_indexed_text_range():
    setup = dis_setup(b"\x09\x02", strings=STRS, op=9, stext=6)
    compare(emu, 0x4C1400, "QvmDis_OpPushIndexed", setup=setup, check_eax=False)


@pytest.mark.parametrize("op", [11, 7, 15, 0])
def test_dis_op_push_indexed_bad(op):
    setup = dis_setup(bytes([op]) + u32(0), strings=STRS, op=op)
    assert compare_error(emu, 0x4C1400, "QvmDis_OpPushIndexed", setup=setup) == "Opcode not implemented for PUSHSTRING"


@pytest.mark.parametrize("i", [0, 2, 3, 5, -1])
def test_dis_get_text(i):
    def setup(e):
        t = e.alloc(0x18)
        table(e, t, STRS, size=4)
        return t, i
    compare(emu, 0x4C1530, "QvmDis_GetText", setup=setup, normalized=True)


@pytest.mark.parametrize("s", STRS + [b"", b'"', b"\x08\x0c\x0e\x1f !#~\xff"])
def test_dis_write_string(s):
    compare(emu, 0x4C1560, "QvmDis_WriteString", setup=lambda e: (FILE, cstr(e, s)), check_eax=False)


@pytest.mark.parametrize("count", [0, 1, 4, 5, 6, 11, 12])
@pytest.mark.parametrize("indent", [8, 0, -2, -3, 0x1FE, 0x1FF, 0x400])
def test_dis_op_call(count, indent):
    code = b"\x00\x18" + u32(count) + b"".join(u32(0x1000 * i + 7) for i in range(count))
    compare(emu, 0x4C1600, "QvmDis_OpCall", setup=dis_setup(code, pc=1, indent=indent), check_eax=False)


# --- task lists -------------------------------------------------------------

def tasklist(e, items, max_=4):
    l = e.call(0x4C1800, max_)[0]
    for t in items:
        e.call(0x4C1790, l, t)
    return l


@pytest.mark.parametrize("n", [0, 1, 7])
def test_tasklist_new(n):
    compare(emu, 0x4C1800, "QTaskList_New", args=(n,))


def test_tasklist_add():
    compare(emu, 0x4C1790, "QTaskList_Add", setup=lambda e: (tasklist(e, [0x11, 0x22]), 0x33), check_eax=False)


def test_tasklist_add_full():
    setup = lambda e: (tasklist(e, [1, 2, 3, 4]), 5)
    assert compare_error(emu, 0x4C1790, "QTaskList_Add", setup=setup) == "QTaskList is full"


@pytest.mark.parametrize("t", [0x11, 0x22, 0x44])
def test_tasklist_remove(t):
    compare(emu, 0x4C17C0, "QTaskList_Remove", setup=lambda e: (tasklist(e, [0x11, 0x22, 0x33, 0x44]), t), check_eax=False)


@pytest.mark.parametrize("items", [[], [1, 2]])
def test_tasklist_remove_missing(items):
    setup = lambda e: (tasklist(e, items), 9)
    assert compare_error(emu, 0x4C17C0, "QTaskList_Remove", setup=setup) == "Trying to remove unrigistered QTask from QTaskList"


def test_tasklist_free():
    compare(emu, 0x4C1830, "QTaskList_Free", setup=lambda e: (tasklist(e, [1]),), check_eax=False)


# --- script directory scan ----------------------------------------------------

scan_emu = Emu()
scan_emu.fake(0x4C48E0, nargs=1)


@pytest.mark.parametrize("name", [b"a.qsc", b"a.QVM", b"x.qvm", b"qsc", b"qvm", b"x.txt", b"", b"aqsc"])
@pytest.mark.parametrize("flag", [0, 1])
@pytest.mark.parametrize("is_dir", [0, 1])
def test_scan_file(name, flag, is_dir):
    def setup(e):
        e.write(0x5C8BF0, u32(flag))
        ent = e.alloc(12)
        e.write(ent, u32(cstr(e, name)) + u32(0) + bytes([is_dir, 0, 0, 0]))
        return (ent,)
    compare(scan_emu, 0x4C2F90, "sub_4C2F90", setup=setup, check_eax=False, normalized=True)
