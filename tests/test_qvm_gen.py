import struct

import pytest

import test_qvm as q
from test_qvm import compare, compare_error, cstr, emu

LOG_FN, ERR_FN, ERR_DATA, LOG_DATA = 0xA442C0, 0xA442C4, 0xA442B4, 0xA442CC


def gen_setup(e, callbacks=True):
    q.fakes.reset()
    q.handlers_setup(e)
    e.call(0x4B8A00, 0x800)
    e.call(0x4B89F0, 0x2000)
    if callbacks:
        e.write(LOG_FN, struct.pack("<I", q.fakes.callback))
        e.write(ERR_FN, struct.pack("<I", q.fakes.callback))
        e.write(ERR_DATA, struct.pack("<I", 0x1234))
        e.write(LOG_DATA, struct.pack("<I", 0x5678))


def script(e, src):
    s = e.alloc(0x94)
    e.write(s + 0x80, struct.pack("<II", cstr(e, src), len(src)))
    return s


SOURCES = [
    b'Task_New(1, "Game", "", Task_New(2, "Sub", 1.5, -3));\n',
    b"x = 5 + 3 * (2 - 1);\n",
    b"if (x > 3) { a = 1; } else if (x < 2) { a = 2; } else if (x == 0) { a = 3; } else { a = 4; }\n",
    b"if (x) { a = 1; }\nif (x) a = 1; else a = 2;\n",
    b"while (x != 0) { x = x - 1; }\n",
    b's = "esc\\"aped\\\\ tab\\t nl\\n cr\\r";\n',
    b"f(1,2,3,4,5,6,7,8,9,10,11);\ng();\nh(1,2,3,4,5);\n",
    b"y = x ? 1 : 2;\n",
    b"z = !a && b || ~c ^ d | e & f << 1 >> 2 / 3 * 4 - -5 + +6;\n",
    b"z = a < b; z = a <= b; z = a > b; z = a >= b; z = a == b; z = a != b;\n",
    b"z = 1.25; z = -0.5; z = 1e3;\n",
    b"{ a = 1; { b = 2; } }\n",
    b"a = (1, 2);\n",
    b"do { a = 1; } while (b);\n",
    b"for (i = 0; i < 3; i = i + 1) { a = i; }\n",
    b"a = ;\n",
    b"if (x { }\n",
    b"",
    b"// only a comment\n",
]


@pytest.mark.parametrize("src", SOURCES)
def test_qscgen_compile(src):
    def setup(e):
        gen_setup(e)
        return cstr(e, "LOCAL:test.qas"), script(e, src)
    compare(emu, 0x4BBCB0, "QscGen_Compile", setup=setup)


def test_qscgen_compile_no_log():
    def setup(e):
        gen_setup(e, callbacks=False)
        return cstr(e, "LOCAL:test.qas"), script(e, SOURCES[0])
    compare(emu, 0x4BBCB0, "QscGen_Compile", setup=setup)


def test_qscgen_compile_open_fails():
    def setup(e):
        gen_setup(e)
        q.fakes.fail_open = ("test.qas",)
        return cstr(e, "LOCAL:test.qas"), script(e, SOURCES[0])
    compare(emu, 0x4BBCB0, "QscGen_Compile", setup=setup)

    def setup_nocb(e):
        setup(e)
        e.write(ERR_FN, b"\0\0\0\0")
        return cstr(e, "LOCAL:test.qas"), script(e, SOURCES[0])
    msg = compare_error(emu, 0x4BBCB0, "QscGen_Compile", setup=setup_nocb)
    assert msg == "Failed to open file 'LOCAL:test.qas' for writing."


# --- small writers ----------------------------------------------------------

def file_setup(e):
    gen_setup(e)
    e.call(0x4BBCB0, cstr(e, "LOCAL:init.qas"), script(e, b""))
    return e.call(0x4B1510, cstr(e, "LOCAL:out.qas"), cstr(e, "w"))[0]


@pytest.mark.parametrize("opc", [0, 7, 24, 48])
def test_qscgen_write_opcode(opc):
    compare(emu, 0x4BBF40, "QscGen_WriteOpcode", setup=lambda e: (opc, file_setup(e)), check_eax=False)


def test_qscgen_write_newline_indent():
    compare(emu, 0x4BBF60, "QscGen_WriteNewline", setup=lambda e: (file_setup(e),), check_eax=False)
    compare(emu, 0x4BC770, "QscGen_WriteIndent", setup=lambda e: (file_setup(e),), check_eax=False)


@pytest.mark.parametrize("s", [b"", b"x", b"Arg0012ABCD"])
def test_qscgen_write_text(s):
    compare(emu, 0x4BC2C0, "QscGen_WriteText", setup=lambda e: (file_setup(e), cstr(e, s)), check_eax=False)


@pytest.mark.parametrize("s", [b"", b"plain", b'q"uote', b"back\\slash", b"t\tn\nr\r", b"\x80\xff"])
def test_qscgen_write_string(s):
    compare(emu, 0x4BC790, "QscGen_WriteString", setup=lambda e: (file_setup(e), cstr(e, s)), check_eax=False)


@pytest.mark.parametrize("v", [0, -1, 123456789, -2147483648])
def test_qscgen_write_int(v):
    compare(emu, 0x4BC880, "QscGen_WriteInt", setup=lambda e: (file_setup(e), v), check_eax=False)


@pytest.mark.parametrize("v", [0.0, -1.5, 1e20, 3.14159265, float("inf")])
def test_qscgen_write_float(v):
    lo, hi = struct.unpack("<II", struct.pack("<d", v))
    compare(emu, 0x4BC8D0, "QscGen_WriteFloat", setup=lambda e: (file_setup(e), lo, hi), check_eax=False)


def test_qscgen_make_label():
    compare(emu, 0x4BC2A0, "QscGen_MakeLabel", setup=lambda e: (e.alloc(64), cstr(e, "EL"), 0x200ABCD0),
            check_eax=False)


@pytest.mark.parametrize("cb", [True, False])
def test_qscgen_log(cb):
    def setup(e):
        gen_setup(e, callbacks=cb)
        return cstr(e, "%s %d\n"), cstr(e, "hi"), 42
    compare(emu, 0x4BBEF0, "QscGen_Log", setup=setup, check_eax=False)


def test_qscgen_error():
    def setup(e):
        gen_setup(e)
        return cstr(e, "bad %s %d"), cstr(e, "thing"), 7
    compare(emu, 0x4BBE90, "QscGen_Error", setup=setup, check_eax=False)

    def setup_nocb(e):
        gen_setup(e, callbacks=False)
        return cstr(e, "bad %s %d"), cstr(e, "thing"), 7
    assert compare_error(emu, 0x4BBE90, "QscGen_Error", setup=setup_nocb) == "bad thing 7"


def test_qscgen_init():
    def setup(e):
        for a in range(0xA442B4, 0xA442D0, 4):
            e.write(a, struct.pack("<I", a))
    compare(emu, 0x4BC920, "QscGen_Init", setup=setup, check_eax=False)


# --- hand made trees --------------------------------------------------------

def block(e, t, words=()):
    b = e.alloc(0x30)
    e.write(b + 8, struct.pack("<i", t) + struct.pack("<%dI" % len(words), *words))
    return b


def expr(e, t, o=0, value=0, left=0, right=0):
    x = e.alloc(0x24)
    e.write(x, bytes([t & 0xFF, o & 0xFF]))
    e.write(x + 8, struct.pack("<I", value))
    e.write(x + 0x18, struct.pack("<II", left, right))
    return x


@pytest.mark.parametrize("t", [0, 3, 5, 6, 8, -1])
def test_qscgen_statement_bad(t):
    msg = compare_error(emu, 0x4BBF80, "QscGen_Statement", setup=lambda e: (file_setup(e), block(e, t)))
    assert msg == "Internal error: Unknown statement block type."


def test_qscgen_statement_empty_expr():
    compare(emu, 0x4BBF80, "QscGen_Statement", setup=lambda e: (file_setup(e), block(e, 7, [0])), check_eax=False)


@pytest.mark.parametrize("t", [0, 9, 0x80])
def test_qscgen_expression_bad_type(t):
    msg = compare_error(emu, 0x4BC2F0, "QscGen_Expression", setup=lambda e: (file_setup(e), expr(e, t)))
    assert msg == "Unknown expression type"


@pytest.mark.parametrize("o", [0, 7, 21, 22, 23, 26, 29, 0x80])
def test_qscgen_expression_bad_op(o):
    def setup(e):
        f = file_setup(e)
        return f, expr(e, 8, o, left=expr(e, 1, value=1), right=expr(e, 1, value=2))
    assert compare_error(emu, 0x4BC2F0, "QscGen_Expression", setup=setup) == "Unknown operator in expression."


@pytest.mark.parametrize("o", list(range(1, 29)))
def test_qscgen_expression_ops(o):
    if o in (7, 21, 22, 23, 26):
        pytest.skip("not an operator")

    def setup(e):
        f = file_setup(e)
        return f, expr(e, 8, o, left=expr(e, 1, value=1), right=expr(e, 1, value=2))
    compare(emu, 0x4BC2F0, "QscGen_Expression", setup=setup, check_eax=False)


def test_qscgen_expression_shapes():
    def setup(e):
        f = file_setup(e)
        one = expr(e, 1, value=1)
        paren_empty = expr(e, 6, value=0)
        unary = expr(e, 8, 13, right=one)
        left_only = expr(e, 8, 20, left=paren_empty)
        args = e.alloc(12)
        items = e.alloc(8)
        e.write(items, struct.pack("<II", one, unary))
        e.write(args, struct.pack("<III", 2, 0, items))
        call = expr(e, 5, value=0)
        e.write(call + 0xC, struct.pack("<II", args, cstr(e, "Fn")))
        return f, expr(e, 8, 24, left=call, right=expr(e, 6, value=left_only))
    compare(emu, 0x4BC2F0, "QscGen_Expression", setup=setup, check_eax=False)
    compare(emu, 0x4BC2F0, "QscGen_Expression", setup=lambda e: (file_setup(e), 0), check_eax=False)
