import struct

import pytest

from emu import Emu, compare, compare_hooked

emu = Emu()
emu.stub("strcat", lambda e: (e.write(e.arg(0) + len(e.read_cstr(e.arg(0), 1 << 20)), e.read_cstr(e.arg(1), 1 << 20) + b"\0"), e.ret(e.arg(0))))

NAN = 0x7FC00000


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def fbits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def mem_init(e):
    e.call(0x4B0BA0)


# --- QVM opcodes -----------------------------------------------------------

STACK = 4096


def new_vm(e, code, pc=0, error=0):
    vm = e.alloc(0x200)
    c = e.alloc(len(code) + 16)
    e.write(c, bytes(code))
    stack, types = e.alloc(4 * STACK), e.alloc(4 * STACK)
    e.write(vm, u32(c))
    e.write(vm + 0x14, u32(pc) + u32(STACK) + u32(STACK) + u32(error) + u32(stack) + u32(types))
    return vm


def push(e, vm, v):
    kind, x = v
    if kind == "i":
        e.call(0x4BDA00, vm, x)
    elif kind == "f":
        e.call(0x4BDA30, vm, x if isinstance(x, int) else fbits(x))
    else:
        e.call(0x4BCE40, vm, cstr(e, x), 3)


PAIRS = [
    (("i", 7), ("i", 3)),
    (("i", -9), ("i", 2)),
    (("i", 0), ("i", 5)),
    (("f", 1.5), ("i", 2)),
    (("i", 3), ("f", 0.0)),
    (("f", 2.0), ("f", 2.0)),
    (("f", -1.0), ("f", 4.0)),
    (("f", NAN), ("f", 1.0)),
    (("f", 0.0), ("f", NAN)),
    (("s", b"abc"), ("s", b"abd")),
    (("s", b"same"), ("s", b"same")),
    (("s", b"zz"), ("s", b"a")),
    (("s", b"a"), ("i", 1)),
    (("i", 1), ("s", b"a")),
]


@pytest.mark.parametrize("op", range(0x18, 0x2B))
@pytest.mark.parametrize("a,b", PAIRS)
def test_qvm_op_binary(op, a, b):
    def setup(e):
        vm = new_vm(e, [op], error=0)
        push(e, vm, a)
        push(e, vm, b)
        return (vm,)
    compare(emu, 0x4BDC40, "Qvm_OpBinary", setup=setup, check_eax=False)


@pytest.mark.parametrize("op", [0x19, 0x24])
def test_qvm_op_binary_long_strings(op):
    def setup(e):
        mem_init(e)
        vm = new_vm(e, [op])
        push(e, vm, ("s", b"x" * 2100))
        push(e, vm, ("s", b"y" * 2100))
        return (vm,)
    compare(emu, 0x4BDC40, "Qvm_OpBinary", setup=setup, check_eax=False)


def test_qvm_op_binary_error_set():
    def setup(e):
        vm = new_vm(e, [0x19], error=9)
        push(e, vm, ("s", b"a"))
        push(e, vm, ("i", 1))
        return (vm,)
    compare(emu, 0x4BDC40, "Qvm_OpBinary", setup=setup, check_eax=False)


@pytest.mark.parametrize("v", [("i", 1), ("f", 2.5), ("f", NAN)])
def test_qvm_pop_numbers_one(v):
    def setup(e):
        vm = new_vm(e, [0])
        push(e, vm, ("s", b"str"))
        push(e, vm, v)
        return vm, e.alloc(4), e.alloc(4), e.alloc(4), e.alloc(4)
    compare(emu, 0x4BE620, "Qvm_PopNumbers", setup=setup)


@pytest.mark.parametrize("op", range(0x2A, 0x30))
@pytest.mark.parametrize("v", [("i", 5), ("i", 0), ("f", -2.5), ("f", 0.0), ("s", b"s")])
@pytest.mark.parametrize("error", [0, 4])
def test_qvm_op_unary(op, v, error):
    def setup(e):
        vm = new_vm(e, [op], error=error)
        push(e, vm, v)
        return (vm,)
    compare(emu, 0x4BE6A0, "Qvm_OpUnary", setup=setup, check_eax=False)


@pytest.mark.parametrize("addr,name,code", [
    (0x4BE810, "Qvm_OpPush1", [0x10]),
    (0x4BE830, "Qvm_OpPush0", [0x0F]),
    (0x4BE850, "Qvm_OpPushM", [0x11]),
    (0x4BE870, "Qvm_OpPushB", [3, 0x85]),
    (0x4BE870, "Qvm_OpPushB", [3, 0x7F]),
    (0x4BE8B0, "Qvm_OpPushW", [4, 0x34, 0x92]),
    (0x4BE8F0, "Qvm_OpPush", [2, 0x78, 0x56, 0x34, 0x12]),
    (0x4BE8F0, "Qvm_OpPush", [6, 0x10, 0, 0, 0]),
    (0x4BE980, "Qvm_OpBlk", [0x2F, 0x20, 0, 0, 0]),
    (0x4BE9A0, "Qvm_OpPushF", [5] + list(struct.pack("<f", -3.25))),
    (0x4BE9D0, "Qvm_OpPushS", [7] + list(b"hello\0")),
    (0x4BE9D0, "Qvm_OpPushS", [0xB] + list(b"ident_x\0")),
    (0x4BEC70, "Qvm_OpNop", [1]),
    (0x4BEC80, "Qvm_OpBrk", [0]),
    (0x4BEC90, "Qvm_OpIllegal", [0x13]),
])
@pytest.mark.parametrize("pc", [0, 3])
def test_qvm_op_simple(addr, name, code, pc):
    def setup(e):
        vm = new_vm(e, [0x55] * pc + code, pc=pc)
        push(e, vm, ("i", 0x77))
        return (vm,)
    compare(emu, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("fn,addr", [("Qvm_PushAddress", 0x4BE950), ("Qvm_PushSymbol", 0x4BEC10)])
def test_qvm_push_typed(fn, addr):
    def setup(e):
        vm = new_vm(e, [0])
        push(e, vm, ("i", 1))
        return vm, 0x12345678
    compare(emu, addr, fn, setup=setup, check_eax=False)


def new_strings(e, p, strs, size_cut=0):
    data = b"".join(s + b"\0" for s in strs)
    offs, o = [], 0
    for s in strs:
        offs.append(o)
        o += len(s) + 1
    d = e.alloc(len(data) + 4)
    e.write(d, data)
    t = e.alloc(4 * len(offs) + 4)
    e.write(t, b"".join(u32(x) for x in offs))
    e.write(p + 8, u32(len(strs)) + u32(len(data) - size_cut) + u32(t) + u32(d))


def script_setup(e):
    # what the script init at 0x4BACD0 does, with a smaller table
    mem_init(e)
    e.write(0xA43E88, u32(e.call(0x4C0360, 31)[0]))
    for lst in (0xA43E90, 0xA43EA0, 0xA43EB0):
        e.write(lst, u32(lst + 4) + u32(0) + u32(lst))
    e.write(0xA43E8C, u32(0))
    e.write(0xA43EAC, u32(0))


def pushidx_vm(e, op, idx, cached=None, ref=None):
    script_setup(e)
    e.call(0x4B8890, cstr(e, b"FuncSym"), 0x403660, 0, 1)
    e.call(0x4B7E80, cstr(e, b"IntSym"), e.alloc(4))
    if op in (8, 0xC):
        arg = list(u32(idx))
    elif op in (9, 0xD):
        arg = [idx & 0xFF]
    else:
        arg = list(struct.pack("<H", idx & 0xFFFF))
    vm = new_vm(e, [op] + arg)
    new_strings(e, vm + 0x30, [b"first", b"second", b"third", b"cut"], size_cut=3)
    new_strings(e, vm + 0x48, [b"FuncSym", b"IntSym", b"Missing", b"cut"], size_cut=3)
    syms, refs = e.alloc(16), e.alloc(16)
    e.write(vm + 0x60, u32(syms) + u32(refs))
    if cached is not None:
        e.write(syms + 4 * idx, u32(cached))
    if ref is not None:
        r = e.alloc(12)
        e.write(r + 8, u32(ref))
        e.write(refs + 4 * idx, u32(r))
    return vm


@pytest.mark.parametrize("op", [8, 9, 10])
@pytest.mark.parametrize("idx", [0, 2, 3, 4, 70000])
def test_qvm_op_push_indexed_string(op, idx):
    compare(emu, 0x4BEA30, "Qvm_OpPushIndexed", setup=lambda e: (pushidx_vm(e, op, idx),), check_eax=False)


@pytest.mark.parametrize("op", [0xC, 0xD, 0xE])
@pytest.mark.parametrize("idx,cached,ref", [
    (0, None, None), (1, None, None), (2, None, None), (3, None, None),
    (2, 0x5000, None), (1, None, 0x6000), (2, None, 0), (3, None, 0),
])
def test_qvm_op_push_indexed_symbol(op, idx, cached, ref):
    def setup(e):
        return (pushidx_vm(e, op, idx, cached, ref),)
    compare(emu, 0x4BEA30, "Qvm_OpPushIndexed", setup=setup, check_eax=False)


@pytest.mark.parametrize("idx", [0, 2, 3, 4, -1])
def test_qvm_get_string(idx):
    def setup(e):
        t = e.alloc(0x18)
        new_strings(e, t, [b"a", b"bb", b"ccc", b"dd"], size_cut=2)
        return t, idx
    compare(emu, 0x4BEC40, "Qvm_GetString", setup=setup)


@pytest.mark.parametrize("op", [0, 5, 18, 0x2A, 0x2F, 0x30, 0xFFFFFFFF])
def test_qvm_op_name(op):
    compare(emu, 0x4BECB0, "Qvm_OpName", setup=lambda e: (e.alloc(16), 0, op), check_eax=False)


def test_qvm_handler_thunk():
    compare(emu, 0x4C0250, "sub_4C0250", check_eax=False)


# --- reader ------------------------------------------------------------------

def reader_setup(e, src, linesize=64, reads=0):
    mem_init(e)
    e.write(0xA969C4, u32(linesize))
    e.write(0xA96598, u32(0))
    r = e.alloc(0x20)
    e.call(0x4BF1C0, r, cstr(e, src), len(src))
    for _ in range(reads):
        e.call(0x4BEFE0, r)
    return r


def test_reader_init():
    def setup(e):
        mem_init(e)
        e.write(0xA969C4, u32(37))
        return e.alloc(0x20), cstr(e, b"abc"), 3, 0x4BEFE0
    compare(emu, 0x4BEF60, "Reader_Init", setup=setup)


def test_reader_init_mem():
    def setup(e):
        mem_init(e)
        e.write(0xA969C4, u32(16))
        return e.alloc(0x20), cstr(e, b"abc"), 3
    compare(emu, 0x4BF1C0, "Reader_InitMem", setup=setup)


def test_reader_free():
    compare(emu, 0x4BEFC0, "Reader_Free", setup=lambda e: (reader_setup(e, b"x"),), check_eax=False)


SOURCES = [
    b"abc\r\ndef // comment\n\n\n\"str // x\" y\nlast",
    b"a\\\"b // c\r\n\"q\\",
    b"\"unterminated\n// all comment\n/",
    b"no newline at end",
    b"0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789\nnext",
    b"aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj kkkk llll mmmm nnnn oooo pppp\nx",
    b" leading blank only aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n",
    b"\0\0abc\0",
    b"",
]


@pytest.mark.parametrize("src", SOURCES)
@pytest.mark.parametrize("reads", range(5))
@pytest.mark.parametrize("linesize", [64, 6, 2])
def test_reader_read_line(src, reads, linesize):
    compare(emu, 0x4BEFE0, "Reader_ReadLine", setup=lambda e: (reader_setup(e, src, linesize, reads),))


def test_reader_read_line_not_at_end():
    def setup(e):
        r = reader_setup(e, b"abc\ndef", reads=1)
        e.write(r, u32(e.u32(r + 4) + 1))
        return (r,)
    compare(emu, 0x4BEFE0, "Reader_ReadLine", setup=setup)


def test_reader_refill():
    compare(emu, 0x4BF1E0, "Reader_Refill", setup=lambda e: (reader_setup(e, b"one\ntwo"),))


# --- lexer -----------------------------------------------------------------

def lexer_setup(e, src, nexts=0, bufsize=256, linesize=256):
    r = reader_setup(e, src, linesize)
    lex = e.alloc(0x30)
    e.call(0x4C0200, lex, r, e.alloc(bufsize), bufsize)
    for _ in range(nexts):
        e.call(0x4BFC70, lex, lex + 8)
    return lex


def test_lexer_init():
    compare(emu, 0x4C0200, "Lexer_Init", setup=lambda e: (e.alloc(0x30), 0x1234, 0x5678, 99), check_eax=False)


ALL_TOKENS = (b"! != !x & && ( ) * + , - / : ; < << <= = == > >> >= ? ^ { | || } ~\n"
              b"\"a string\" word 123 0x1F $ff $g 1.5 2e3 AX 0X1 . # @ \x80 'q' [x]\n"
              b"  \t\r\n\n  tail")


@pytest.mark.parametrize("bufsize", [256, 2, 1])
def test_lexer_next_all(bufsize):
    for n in range(64):
        compare(emu, 0x4BFC70, "Lexer_Next", setup=lambda e: (lambda l: (l, l + 8))(lexer_setup(e, ALL_TOKENS, n, bufsize)))


def test_lexer_next_pending():
    def setup(e):
        lex = lexer_setup(e, b"first 2.5 third", 2)
        e.call(0x4BFBE0, lex + 8, lex + 8)
        return lex, lex + 8
    compare(emu, 0x4BFC70, "Lexer_Next", setup=setup)


def test_lexer_unget():
    def setup(e):
        lex = lexer_setup(e, b"word 7 8", 2)
        e.call(0x4BFBE0, lex + 8, lex + 8)
        return lex + 8, lex + 8
    compare(emu, 0x4BFBE0, "Lexer_Unget", setup=setup, check_eax=False)


@pytest.mark.parametrize("text", [b"0", b"9x", b"a", b"$f", b"$G", b"$", b"0x", b"1X", b"MX", b"ab", b"", b"1.5", b"3e2", b"2E", b"12"])
@pytest.mark.parametrize("addr,name", [(0x4C0170, "Lexer_IsNumber"), (0x4C01D0, "Lexer_IsInteger")])
def test_lexer_classify(text, addr, name):
    def setup(e):
        tok = e.alloc(0x28)
        e.write(tok + 0x14, u32(cstr(e, text)))
        return (tok,)
    compare(emu, addr, name, setup=setup)


@pytest.mark.parametrize("addr,name", [(0x4BF390, "Lexer_Line"), (0x4C0230, "Lexer_TextLine"), (0x4C0240, "Lexer_LineText")])
def test_lexer_line(addr, name):
    compare(emu, addr, name, setup=lambda e: (lexer_setup(e, b"a\n\nb c", 2),))


# --- parser and blocks -----------------------------------------------------

SCRIPTS = [
    b"{ 1; 2 + 3; }",
    b"5;",
    b"if (1) { 2; } else if (0) 3; else if (1) 4; else 5;",
    b"if (0) 1; else { 2; 3; }",
    b"if (1) 2; next;",
    b"if (1) 2; else if (0) 3; next;",
    b"if (1) 2;",
    b"if (1) 2; else",
    b"if (1) 2; else if (0) 3; else",
    b"if (1) 2; else if (0) 3;",
    b"if 1 2;",
    b"if",
    b"if (1)",
    b"if (1) 2; else if 3;",
    b"if (1) 2; else if",
    b"if (1) 2; else if (1)",
    b"if (1) }",
    b"while (0) 1;",
    b"while 0;",
    b"while (0)",
    b"while",
    b"while (0) }",
    b"else 1;",
    b"do 1;",
    b"for 1;",
    b"}",
    b"{ 1; ",
    b"{",
    b"{ 1; } }",
    b"{ { 1; } while (0) { 2; } }",
    b"{ ) }",
    b"{ 1 + ; }",
]


def parse_setup(e, src):
    script_setup(e)
    lex = lexer_setup(e, src)
    e.call(0x4BFC70, lex, lex + 8)
    return lex


@pytest.mark.parametrize("src", SCRIPTS)
@pytest.mark.parametrize("addr,name", [
    (0x4BF1F0, "Parse_Block"), (0x4BF580, "Parse_Statement"), (0x4BF620, "Parse_If"), (0x4BF850, "Parse_While"),
])
def test_parse(src, addr, name):
    def setup(e):
        return parse_setup(e, src), e.alloc(4)
    compare(emu, addr, name, setup=setup)


@pytest.mark.parametrize("src", SCRIPTS[:12] + [b"do", b"for", b"x", b"elsewhere"])
def test_parse_keyword(src):
    compare(emu, 0x4BF3A0, "Parse_Keyword", setup=lambda e: (parse_setup(e, src),))


EXEC_SCRIPTS = [
    b"{ a = 1; b = 2; }",
    b"if (a) b = 1; else if (b) b = 2; else if (1) b = 3; else b = 4;",
    b"if (0) a = 1; else if (0) a = 2;",
    b"if (0) a = 1; else a = 5;",
    b"if (1) a = 7;",
    b"while (a < 5) { a = a + 1; b = b + 2; }",
    b"{ if (a == 0) { a = 3; } while (b < a) b = b + 1; }",
]


def exec_setup(e, src, a=0, b=0):
    script_setup(e)
    va, vb = e.alloc(4), e.alloc(4)
    e.write(va, u32(a))
    e.write(vb, u32(b))
    e.call(0x4B7E80, cstr(e, b"a"), va)
    e.call(0x4B7E80, cstr(e, b"b"), vb)
    lex = lexer_setup(e, src)
    e.call(0x4BFC70, lex, lex + 8)
    out = e.alloc(4)
    assert e.call(0x4BF1F0, lex, out)[0] == 1
    return e.u32(out)


def stmt(e, block, i):
    n = e.u32(block + 0xC)
    for _ in range(i):
        n = e.u32(n)
    return n


def for_block(e, with_init=True, with_step=True, src=b"{ a = 0; a < 3; a = a + 1; b = b + a; }"):
    lst = exec_setup(e, src, 9, 1)
    f = e.call(0x4B0C60, 0x1C, 4)[0]
    e.write(f, bytes(8))
    exprs = [e.u32(stmt(e, lst, i) + 0xC) for i in range(3)]
    e.write(f + 8, u32(6) + u32(exprs[0] if with_init else 0) + u32(exprs[1]) + u32(exprs[2] if with_step else 0) + u32(stmt(e, lst, 3)))
    return f


def do_block(e):
    w = stmt(e, exec_setup(e, b"{ while (a < 4) a = a + 1; }", 7), 0)
    e.write(w + 8, u32(5))
    return w


@pytest.mark.parametrize("src", EXEC_SCRIPTS)
@pytest.mark.parametrize("a,b", [(0, 0), (1, 0), (0, 1), (2, 9)])
def test_block_exec(src, a, b):
    compare(emu, 0x4BFA80, "Block_Exec", setup=lambda e: (exec_setup(e, src, a, b),), check_eax=False)


@pytest.mark.parametrize("make", [
    lambda e: for_block(e), lambda e: for_block(e, False, False), do_block,
    lambda e: (lambda b: (e.write(b + 0xC, u32(0)), b)[1])(stmt(e, exec_setup(e, b"{ a; }"), 0)),
])
def test_block_exec_built(make):
    compare(emu, 0x4BFA80, "Block_Exec", setup=lambda e: (make(e),), check_eax=False)


@pytest.mark.parametrize("src", EXEC_SCRIPTS + [b"{ }", b"{ { } { 1; } }"])
def test_block_free(src):
    compare(emu, 0x4BF8F0, "Block_Free", setup=lambda e: (exec_setup(e, src),), check_eax=False)


@pytest.mark.parametrize("make", [
    lambda e: for_block(e, src=b"{ 0; 1; 2; 3; }"), lambda e: for_block(e, False, False, b"{ 0; 1; 2; 3; }"), do_block,
    lambda e: (lambda b: (e.write(b + 0xC, u32(0)), b)[1])(stmt(e, exec_setup(e, b"{ a; }"), 0)),
    lambda e: 0,
])
def test_block_free_built(make):
    compare(emu, 0x4BF8F0, "Block_Free", setup=lambda e: (make(e),), check_eax=False)


# --- symbol tables ---------------------------------------------------------

# fn(name, value, data): data[0] = data[0] * 31 + value, data[1] += 1
CALLBACK = bytes.fromhex("8b44240c8b4c24088b106bd21f01ca8910ff4004c3")
# filter(name, value, data): value & data
FILTER = bytes.fromhex("8b4424082344240cc3")


def table_setup(e, count=7, n=12, alloc_values=False):
    mem_init(e)
    t = e.call(0x4C0360, count)[0]
    for i in range(n):
        v = e.call(0x4B0C60, 8, 4)[0] if alloc_values and i % 3 else (0x100 + i if not alloc_values else 0)
        e.call(0x4C0560, t, cstr(e, b"sym%d" % i), v)
    return t


CODE_AT = {}


def code(e, b):
    # each stub keeps its own address, the emulator may run stale translations otherwise
    p = CODE_AT.setdefault(b, 0x23F00000 + 0x40 * len(CODE_AT))
    e.write(p, b)
    return p


@pytest.mark.parametrize("count", [0, 1, 5, 64])
def test_symtab_create(count):
    compare(emu, 0x4C0360, "SymbolTable_Create", setup=lambda e: (mem_init(e), (count,))[1])


@pytest.mark.parametrize("n", [0, 1, 12])
def test_symtab_for_each(n):
    def setup(e):
        t = table_setup(e, 7, n)
        return t, code(e, CALLBACK), e.alloc(8)
    compare(emu, 0x4C0260, "SymbolTable_ForEach", setup=setup, check_eax=False)


@pytest.mark.parametrize("mask", [None, 0, 1, 0xFFFF])
def test_symtab_for_each_filtered(mask):
    def setup(e):
        t = table_setup(e, 5, 12)
        if mask is not None:
            e.write(0xA44314, u32(code(e, FILTER)) + u32(mask))
        return t, code(e, CALLBACK), e.alloc(8)
    compare(emu, 0x4C02C0, "SymbolTable_ForEachFiltered", setup=setup, check_eax=False)


@pytest.mark.parametrize("n", [0, 12])
@pytest.mark.parametrize("addr,name", [(0x4C03F0, "SymbolTable_Clear"), (0x4C03C0, "SymbolTable_Destroy")])
def test_symtab_clear(n, addr, name):
    compare(emu, addr, name, setup=lambda e: (table_setup(e, 5, n, True),), check_eax=False)


@pytest.mark.parametrize("name", [b"sym0", b"sym7", b"sym11"])
def test_symtab_remove(name):
    compare(emu, 0x4C0460, "SymbolTable_Remove", setup=lambda e: (table_setup(e, 5, 12), cstr(e, name)), check_eax=False)


# --- QVM assembler token rules ---------------------------------------------

def test_qvmlex_init_rules():
    compare(emu, 0x4C0660, "QvmLex_InitRules", setup=lambda e: (e.alloc(12),), check_eax=False)


@pytest.mark.parametrize("pattern", [b"[ \\t\\n\\r]+", b"[a-zA-Z_][a-zA-Z0-9_]*", b"0x[0-9a-fA-F]+", b"[abc", b"(", b"*"])
def test_qvmlex_add_rule(pattern):
    def setup(e):
        mem_init(e)
        rules = e.alloc(12)
        e.call(0x4C0660, rules)
        e.call(0x4C0680, rules, cstr(e, b"[0-9]+"), 5, 1)
        return rules, cstr(e, pattern), 10, 1
    compare(emu, 0x4C0680, "QvmLex_AddRule", setup=setup, check_eax=False)


# --- whole script run with our code hooked in ------------------------------

hooked = Emu()
hooked.stub("strcat", emu.stubs["strcat"])
hooked.install_hooks()


@pytest.mark.parametrize("src", [
    b"a = 1;\r\nif (a == 1) { b = 2; } else b = 3; // note\nwhile (b < 10) b = b + 3;\n",
    b"if (a) b = 1; else if (b == 0) { a = 4; b = a * 2; }\n\n{ a = a + b; }",
    b"a = 1;\n}\n",
])
def test_hooked_run_script(src):
    def setup(e):
        exec_setup(e, b"0;")
        e.write(0xA969C4, u32(128))
        obj = e.alloc(0x90)
        e.write(obj + 0x80, u32(cstr(e, src)) + u32(len(src)))
        return (obj,)
    # Script_Run returns nothing; eax is whatever its last call left there
    compare_hooked(emu, hooked, 0x4B85B0, setup=setup, check_eax=False)
