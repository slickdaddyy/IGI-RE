import math
import struct

import pytest

from emu import Emu, ErrorShown, TRAP_BASE, compare, diff_text

NAN = struct.unpack("<d", struct.pack("<Q", 0x7FF8000000000001))[0]


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


LOG = []


def trap_stub(e, name, fn):
    """An address that runs the Python function fn(e) when called."""
    trap = TRAP_BASE + 4 * len(e.imports)
    e.imports.append(("fake", name))
    e.write(trap, b"\xc3")
    e.stub(name, fn)
    return trap


def compare_logged(e, orig, ours, setup=None, args=(), **kw):
    """compare() that also checks the Python fakes saw the same calls. When the original stops in
    Error_Show ours must stop there too; when it crashes on a NULL pointer the test is skipped."""
    try:
        return _compare_logged(e, orig, ours, setup, args, **kw)
    except ErrorShown:
        same_error(e, orig, ours, setup, args)
    except RuntimeError as x:
        if "unmapped access at 0x0 " not in str(x):
            raise
        e.reset()
        a = setup(e) or args if setup else args
        with pytest.raises(RuntimeError, match="unmapped access at 0x0 "):
            e.call(orig, *a)
        pytest.skip("the original dereferences NULL here")


def _compare_logged(e, orig, ours, setup, args, **kw):
    runs = []

    def s(e):
        runs.append(list(LOG))
        r = setup(e) if setup else None
        LOG.clear()
        return r
    r = compare(e, orig, ours, args=args, setup=s, **kw)
    assert runs[1] == LOG, "calls differ:\n orig %s\n ours %s" % (runs[1], LOG)
    return r


def same_error(e, orig, ours, setup=None, args=()):
    """Both versions stop in Error_Show with the same message and the same memory writes."""
    out = []
    for target in (orig, ours):
        e.reset()
        a = setup(e) or args if setup else args
        before = e.snapshot()
        with pytest.raises(ErrorShown) as info:
            e.call(target, *a)
        out.append((str(info.value), e.changes(before)))
    assert out[0][0] == out[1][0]
    assert out[0][1] == out[1][1], diff_text(out[0][1], out[1][1])


# --- a script system with test symbols ---------------------------------------

def error_handler(e):
    LOG.append(("error", e.read_cstr(e.arg(0))))
    e.ret(0)


def make_script_emu():
    e = Emu()
    e.call(0x4B0BA0)
    e.call(0x4B8B20)
    e.write(0xA43E70, u32(trap_stub(e, "ScriptErrorHandler", error_handler)))
    v = {}

    def var(name, size, data, register, *extra):
        p = e.alloc(max(size, 8))
        e.write(p, data)
        e.call(register, cstr(e, name), p, *extra)
        v[name] = p
    var(b"IntVar", 4, u32(42), 0x4B7E80)
    var(b"ByteVar", 1, b"\xfb", 0x4B7EE0)
    var(b"ShortVar", 2, struct.pack("<h", -300), 0x4B7F40)
    var(b"RealVar", 8, struct.pack("<d", 2.75), 0x4B7FC0)
    var(b"NanVar", 8, struct.pack("<d", NAN), 0x4B7FC0)
    var(b"BigVar", 8, struct.pack("<d", 3e10), 0x4B7FC0)
    var(b"StrVar", 16, b"hello\0", 0x4B8030, 16)
    var(b"RealConst", 8, struct.pack("<d", 3.5), 0x4B8870)
    e.call(0x4B8810, cstr(e, b"CONST7"), 7)
    # value handlers double as natives returning the value at their data pointer
    e.call(0x4B88C0, cstr(e, b"GetInt"), 0x4B7EA0, v[b"IntVar"], -1)
    e.call(0x4B88F0, cstr(e, b"GetReal"), 0x4B7FE0, v[b"RealVar"], -1)
    e.call(0x4B8890, cstr(e, b"GetStr"), 0x4B8060, v[b"StrVar"], -1)
    e.call(0x4B8890, cstr(e, b"Fixed2"), 0x4B7EA0, v[b"IntVar"], 2)
    e.baseline = e.snapshot()
    return e, v


se, VARS = make_script_emu()
emu = Emu()


def symbol(e, name):
    out = e.alloc(4)
    e.call(0x4BAB80, cstr(e, name), out)
    return e.u32(out)


# --- value handlers ----------------------------------------------------------

def value_test(addr, ours, data):
    def setup(e):
        out = e.alloc(24)
        p = e.alloc(16)
        e.write(p, data)
        return out, p
    p = emu.baseline[1]
    compare(emu, addr, ours, setup=setup, ignore=[(p + 4, p + 8), (p + 20, p + 24)])


@pytest.mark.parametrize("v", [0, 1, -1, 0x7FFFFFFF, -0x80000000])
def test_int_value(v):
    value_test(0x4B7EA0, "Script_IntValue", u32(v))


@pytest.mark.parametrize("v", [0, 5, 0x7F, 0x80, 0xFF])
def test_bool8_value(v):
    value_test(0x4B7F00, "Script_Bool8Value", bytes([v]))


@pytest.mark.parametrize("v", [0, 300, -300, 0x7FFF, -0x8000])
def test_int16_value(v):
    value_test(0x4B7F60, "Script_Int16Value", struct.pack("<h", v))


@pytest.mark.parametrize("v", [0.0, 2.5, -2.5, 3.5, 1e10, -1e300, NAN, math.inf])
def test_real_value(v):
    value_test(0x4B7FE0, "Script_RealValue", struct.pack("<d", v))


def test_string_value():
    value_test(0x4B8060, "Script_StringValue", b"text\0")


@pytest.mark.parametrize("v", [0, 7, -3, 0x7FFFFFFF])
def test_constant_value(v):
    def setup(e):
        out = e.alloc(24)
        return out, v
    p = emu.baseline[1]
    compare(emu, 0x4B8830, "Script_ConstantValue", setup=setup, ignore=[(p + 4, p + 8), (p + 20, p + 24)])


# --- registration ------------------------------------------------------------

def mem_init(e):
    e.call(0x4B0BA0)


def script_table(e):
    e.call(0x4B0BA0)
    e.call(0x4BACD0)


@pytest.mark.parametrize("orig,ours,extra", [
    (0x4B7F40, "Script_RegisterInt16", (0x567C90,)),
    (0x4B8030, "Script_RegisterString", (0x567C90, 40)),
    (0x4B8870, "Script_RegisterRealConstant", (0x567C90,)),
    (0x4B88C0, "Script_RegisterIntFunction", (0x403660, 5, 2)),
])
def test_register(orig, ours, extra):
    def setup(e):
        script_table(e)
        return (cstr(e, b"TestSymbol"),) + extra
    compare(emu, orig, ours, setup=setup, check_eax=False)


def table_with_refs(e):
    script_table(e)
    e.call(0x4B8890, cstr(e, b"Sym"), 0x403660, 0, 1)
    e.call(0x4B8890, cstr(e, b"Other"), 0x403660, 0, 1)
    refs = [e.call(0x4BABB0, cstr(e, n))[0] for n in (b"Sym", b"Other", b"Sym")]
    return refs


@pytest.mark.parametrize("orig,ours", [
    (0x4B80A0, "Script_RemoveSymbol"),
    (0x4B8920, "Script_UnregisterSymbol"),
    (0x4BAB30, "Script_DeleteSymbol"),
])
def test_delete_symbol(orig, ours):
    def setup(e):
        table_with_refs(e)
        return (cstr(e, b"Sym"),)
    compare(emu, orig, ours, setup=setup, check_eax=False)


def test_delete_symbol_missing():
    def setup(e):
        table_with_refs(e)
        return (cstr(e, b"Nope"),)
    same_error(emu, 0x4BAB30, "Script_DeleteSymbol", setup=setup)


def test_set_symbol_context_missing():
    def setup(e):
        table_with_refs(e)
        return cstr(e, b"Nope"), 5
    same_error(emu, 0x4B8930, "Script_SetSymbolContext", setup=setup)


def test_clear_refs():
    def setup(e):
        table_with_refs(e)
        return (symbol(e, b"Sym"),)
    compare(emu, 0x4BAA90, "Script_ClearRefs", setup=setup, check_eax=False)


def test_get_symbol_table():
    compare(emu, 0x4BABA0, "Script_GetSymbolTable", setup=script_table)


@pytest.mark.parametrize("name", [b"Sym", b"Nope"])
@pytest.mark.parametrize("prealloc", [False, True])
def test_ref_symbol(name, prealloc):
    def setup(e):
        table_with_refs(e) if prealloc else (script_table(e), e.call(0x4B8890, cstr(e, b"Sym"), 0x403660, 0, 1))
        return (cstr(e, name),)
    compare(emu, 0x4BABB0, "Script_RefSymbol", setup=setup)


def test_ref_symbol_peak():
    def setup(e):
        refs = table_with_refs(e)
        e.call(0x4BACA0, refs[0])
        e.call(0x4BACA0, refs[1])
        return (cstr(e, b"Sym"),)
    compare(emu, 0x4BABB0, "Script_RefSymbol", setup=setup)


@pytest.mark.parametrize("n", [0, 1, 5])
def test_alloc_refs(n):
    def setup(e):
        table_with_refs(e)
        return (n,)
    compare(emu, 0x4BAC30, "Script_AllocRefs", setup=setup, check_eax=False)


def test_unref_symbol():
    def setup(e):
        return (table_with_refs(e)[1],)
    compare(emu, 0x4BACA0, "Script_UnrefSymbol", setup=setup, check_eax=False)


def test_init_symbols():
    compare(emu, 0x4BACD0, "Script_InitSymbols", setup=mem_init, check_eax=False)


@pytest.mark.parametrize("leak", [False, True])
def test_free_symbols(leak):
    def setup(e):
        refs = table_with_refs(e)
        if not leak:
            for r in refs:
                e.call(0x4BACA0, r)
    compare(emu, 0x4BAD40, "Script_FreeSymbols", setup=setup, check_eax=False)


def test_free_symbols_twice():
    def setup(e):
        table_with_refs(e)
        e.call(0x4BAD40)
    compare(emu, 0x4BAD40, "Script_FreeSymbols", setup=setup, check_eax=False)


def test_free_ref_block():
    def setup(e):
        table_with_refs(e)
        return (e.u32(0xA43EA0),)
    compare(emu, 0x4BADA0, "Script_FreeRefBlock", setup=setup, check_eax=False)


# --- small setters -----------------------------------------------------------

@pytest.mark.parametrize("orig,ours", [
    (0x4B8970, "Script_SetWarningLevel"),
    (0x4B89F0, "Script_SetMaxArgs"),
    (0x4B8A00, "Script_SetLineSize"),
    (0x4B8A10, "Script_SetBreakEnabled"),
])
def test_setters(orig, ours):
    compare(emu, orig, ours, args=(0x1234,), check_eax=False)


@pytest.mark.parametrize("sp", [0x100, 1])
def test_push_warning_level(sp):
    def setup(e):
        e.write(0xA9659C, u32(sp))
        e.write(0xA96598, u32(2))
        return (7,)
    compare(emu, 0x4B8980, "Script_PushWarningLevel", setup=setup, check_eax=False)


def test_push_warning_level_underflow():
    same_error(emu, 0x4B8980, "Script_PushWarningLevel", setup=lambda e: (e.write(0xA9659C, u32(0)), (1,))[1])


@pytest.mark.parametrize("sp", [0, 0xFF])
def test_pop_warning_level(sp):
    def setup(e):
        e.write(0xA9659C, u32(sp))
        e.write(0xA965C0 + 4 * sp, u32(9))
    compare(emu, 0x4B89C0, "Script_PopWarningLevel", setup=setup, check_eax=False)


def test_pop_warning_level_overflow():
    same_error(emu, 0x4B89C0, "Script_PopWarningLevel", setup=lambda e: e.write(0xA9659C, u32(0x100)))


# --- init, shutdown and the two built in functions ------------------------

def test_script_init():
    compare(emu, 0x4B8B20, "Script_Init", setup=mem_init, check_eax=False)


def test_script_shutdown():
    compare(se, 0x4B8CC0, "Script_Shutdown", check_eax=False)


def test_break_script():
    p = emu.baseline[1]
    compare(emu, 0x4B8C10, "BreakScript", setup=lambda e: (e.alloc(24),), ignore=[(p + 4, p + 8), (p + 20, p + 24)])


def parse(e, text):
    return e.call(0x4B86E0, cstr(e, text))[0]


def args_of(e, text):
    """ScriptArgs of the call expression in text."""
    return e.u32(parse(e, text) + 0xC)


@pytest.mark.parametrize("text", [b"F(2)", b"F(SCRIPT_WARNINGLEVEL_HIGH)", b"F(0)"])
def test_set_warning_level_cmd(text):
    def setup(e):
        out = e.alloc(24)
        return out, 0, args_of(e, text)
    p = se.baseline[1]
    compare(se, 0x4B8C60, "Script_SetWarningLevelCmd", setup=setup, ignore=[(p + 4, p + 8), (p + 20, p + 24)])


# --- argument accessors -----------------------------------------------------

ARGS = b"F(IntVar + 1, RealVar * 3, \"str\" + StrVar, 4.5, GetInt())"


@pytest.mark.parametrize("i", range(5))
def test_get_arg(i):
    compare(se, 0x4B8A20, "Script_GetArgInt", setup=lambda e: (args_of(e, ARGS), i))
    compare(se, 0x4B8A50, "Script_GetArgReal", setup=lambda e: (args_of(e, ARGS), i), check_st0=True, check_eax=False)
    compare(se, 0x4B8AD0, "Script_GetArgLine", setup=lambda e: (args_of(e, ARGS), i))

    def setup(e):
        return args_of(e, ARGS), i, e.alloc(32), 8
    if i != 0:  # a number inside a string sum hands NULL to strncat and crashes the original
        compare(se, 0x4B8A80, "Script_GetArgString", setup=setup)


@pytest.mark.parametrize("i", [0, 4, 5, -1])
def test_has_arg(i):
    compare(se, 0x4B8B00, "Script_HasArg", setup=lambda e: (args_of(e, ARGS), i))


fe = Emu()
for a, n in ((0x4BCAB0, 2), (0x4BD010, 2), (0x4BD040, 4), (0x4BD070, 2)):
    fe.fake(a, ret=a & 0xFFFF, nargs=n)


@pytest.mark.parametrize("orig,ours,n", [
    (0x4B8A20, "Script_GetArgInt", 2),
    (0x4B8A50, "Script_GetArgReal", 2),
    (0x4B8A80, "Script_GetArgString", 4),
    (0x4B8AD0, "Script_GetArgLine", 2),
])
def test_get_arg_qvm(orig, ours, n):
    def setup(e):
        args = e.alloc(20)
        e.write(args, u32(3) + u32(1))
        return (args, 1, 0x1000, 16)[:n]
    compare(fe, orig, ours, setup=setup, normalized=True, check_eax=orig != 0x4B8A50)


# --- expressions ------------------------------------------------------------

EXPRS = [
    b"1+2*3", b"(1+2)*3", b"10/3-1", b"7/0", b"7.5/0", b"1<<4|3", b"-8>>1", b"1<<33", b"5^3&6", b"~5", b"!0", b"!7",
    b"3<4", b"4<3", b"3>4", b"3<=3", b"4>=5", b"3==3", b"3!=3", b"1&&0", b"0||2", b"1&&2", b"0||0",
    b"TRUE ? 5 : 7", b"0 ? 5 : 7", b"0.4 ? 5 : 7", b"5 ? 1 ? 2 : 3 : 4", b"1 + (0 ? 1 : 2) * 3", b"1 ? 2 : 3 + 4",
    b"-3*-2", b"+4", b"- -4", b"2.6*2", b"2.5", b"-2.5", b"1.5+2.25", b"1,2,3", b"2 - 1 - 1", b"8/2/2", b"1/3.0",
    b"0.1+0.2==0.3", b"0.1+0.2", b"1.0/3.0*3.0", b"2.5 < 2.5", b"2.5 <= 2.5", b"2.5 > 1", b"2.5 >= 3", b"2.5 != 2.5",
    b"IntVar", b"IntVar*2+ByteVar", b"ShortVar", b"RealVar*2", b"RealConst", b"CONST7", b"BigVar", b"BigVar*BigVar",
    b"NanVar == NanVar", b"NanVar != 1", b"NanVar < 1", b"NanVar > 1", b"NanVar <= 1", b"NanVar >= 1", b"!NanVar",
    b"NanVar && 1", b"NanVar || 0", b"NanVar ? 1 : 2", b"1 / NanVar", b"NanVar - NanVar",
    b"IntVar = 9", b"RealVar = 1.25", b"ByteVar = 300", b"ShortVar = 70000", b"IntVar = RealVar = 2.5",
    b"StrVar = \"abc\"", b"StrVar = \"a very long string that does not fit\"", b"CONST7 = 3", b"RealConst = 1",
    b"GetInt = 1", b"Unknown = 1", b"Unknown + 1", b"Unknown", b"GetInt()", b"GetReal()+1", b"GetStr()",
    b"_Script_SetWarningLevel(2)", b"BreakScript()", b"\"a\" \"b\"", b"\"str\"", b"\"a\" + \"b\"",
    b"\"a\" + StrVar + GetStr()", b"StrVar", b"(\"x\" \"y\")", b"\"a\", \"b\"", b"\"a\" * 2",
    b"1 +", b"* 2", b"(1+2", b"1 ? 2", b"1 : 2", b"?", b"1 2", b"\"a\" 1", b"IntVar IntVar", b"2.5 1", b"(1)(2)",
    b"F(1,2,3)", b"GetInt(1+2, \"s\", 3.5)", b"GetInt(", b"GetInt(1,", b"GetInt(()", b"Fixed2(1, 2)",
    b"1 + * 2", b"(1 ? 2 : 3)", b"1 ? (2 ? 3 : 4) : 5", b"IntVar == 42 && RealVar > 2", b"((((1))))",
    b"1 = 2", b"IntVar += 2", b"\"unterminated", b"", b";", b"a,b",
]


def tree(e, text):
    return (parse(e, text),)


@pytest.mark.parametrize("text", EXPRS)
def test_script_eval_expr(text):
    compare_logged(se, 0x4B8770, "Script_EvalExpr", setup=lambda e: (cstr(e, text),))


@pytest.mark.parametrize("text", EXPRS)
def test_script_parse_expr(text):
    compare_logged(se, 0x4B86E0, "Script_ParseExpr", setup=lambda e: (cstr(e, text),))


@pytest.mark.parametrize("text", EXPRS)
def test_expr_eval_int(text):
    compare_logged(se, 0x4B9120, "Expr_EvalInt", setup=lambda e: tree(e, text))


@pytest.mark.parametrize("text", EXPRS)
def test_expr_eval_real(text):
    compare_logged(se, 0x4B9660, "Expr_EvalReal", setup=lambda e: tree(e, text), check_eax=False, check_st0=True)


@pytest.mark.parametrize("text", EXPRS)
@pytest.mark.parametrize("size", [64, 4])
def test_expr_eval_string(text, size):
    def setup(e):
        buf = e.alloc(64)
        e.write(buf, b"\x77" * 64)
        return parse(e, text), buf, size
    compare_logged(se, 0x4B8CE0, "Expr_EvalString", setup=setup)


@pytest.mark.parametrize("text", EXPRS)
def test_expr_free(text):
    compare_logged(se, 0x4B9BE0, "Expr_Free", setup=lambda e: tree(e, text), check_eax=False)


@pytest.mark.parametrize("text", [b"7", b"-2.5", b"RealVar*2", b"\"text\"", b"\"a\" + StrVar", b"300", b"GetReal()"])
@pytest.mark.parametrize("name", [b"IntVar", b"ByteVar", b"ShortVar", b"RealVar", b"StrVar", b"GetInt", b"RealConst"])
def test_expr_assign(text, name):
    def setup(e):
        return parse(e, text), symbol(e, name)
    compare_logged(se, 0x4B9040, "Expr_Assign", setup=setup, check_eax=False)


# --- running source scripts ------------------------------------------------

RUNS = [
    b"IntVar = 5;",
    b"IntVar = 1;\nRealVar = IntVar * 2.5;\nStrVar = \"xy\";\nShortVar = -1;",
    b"IntVar = 1; BreakScript(); IntVar = 2;",
    b"IntVar = ;\nIntVar = 3;",
    b"IntVar = 1/0; IntVar = 3;",
    b"",
    b"Unknown(1);",
    b"_Script_SetWarningLevel(0); IntVar = 1/0; IntVar = 7;",
    b"IntVar = (1 +; IntVar = 4;",
]


@pytest.mark.parametrize("text", RUNS)
@pytest.mark.parametrize("brk", [0, 1])
def test_script_run(text, brk):
    def setup(e):
        p = e.call(0x4B0C60, 0x94, 4)[0]
        e.write(p, b"LOCAL:test.qsc\0")
        src = cstr(e, text)
        e.write(p + 0x80, u32(src) + u32(len(text)))
        e.write(0xA965A0, u32(brk))
        return (p,)
    compare_logged(se, 0x4B85B0, "Script_Run", setup=setup, check_eax=False)


# --- parser pieces ------------------------------------------------------------

def lexer(e, text):
    lex, reader, buf = e.alloc(0x30), e.alloc(0x20), e.alloc(0x800)
    e.call(0x4C0200, lex, reader, buf, 0x800)
    e.call(0x4BF1C0, reader, cstr(e, text), len(text))
    return lex


@pytest.mark.parametrize("text", [b"1, 2", b"IntVar + 1, 3)", b"(1, 2), 3", b"1 ? 2 : 3, 4", b"\"a\" \"b\", 1", b", 1"])
@pytest.mark.parametrize("depth,in_args", [(0, 0), (0, 1), (3, 1)])
def test_expr_parse_tokens(text, depth, in_args):
    def setup(e):
        lex = lexer(e, text)
        e.call(0x4BFC70, lex, lex + 8)
        return lex, e.alloc(4), depth, in_args
    compare_logged(se, 0x4B9CD0, "Expr_ParseTokens", setup=setup)


@pytest.mark.parametrize("text", [b"1+2;", b"", b";", b"(1"])
def test_expr_parse(text):
    compare_logged(se, 0x4B9CA0, "Expr_Parse", setup=lambda e: (lexer(e, text), e.alloc(4)))


ARG_TEXTS = [b"()", b"(1)", b"(1, 2)", b"(1, 2, 3)", b"(1,", b"(\"a\" \"b\", IntVar = 3)", b"(1 2)", b"(1, )", b"(", b"(1;"]


@pytest.mark.parametrize("text", ARG_TEXTS)
@pytest.mark.parametrize("size", [None, -1, 0, 1, 2, 3])
def test_expr_parse_args(text, size):
    def setup(e):
        lex = lexer(e, text)
        e.call(0x4BFC70, lex, lex + 8)
        sym = 0
        if size is not None:
            sym = e.alloc(0x1C)
            e.write(sym + 0xC, u32(size))
        return lex, sym
    compare_logged(se, 0x4BA4C0, "Expr_ParseArgs", setup=setup)


def test_expr_parse_args_limit():
    def setup(e):
        e.write(0xA969C0, u32(2))
        lex = lexer(e, b"(1, 2, 3)")
        e.call(0x4BFC70, lex, lex + 8)
        return lex, 0
    compare_logged(se, 0x4BA4C0, "Expr_ParseArgs", setup=setup)


@pytest.mark.parametrize("op", range(-1, 0x21))
def test_expr_op_tables(op):
    compare(emu, 0x4BA340, "Expr_IsUnary", args=(op,))
    compare(emu, 0x4BA380, "Expr_IsLeftAssoc", args=(op,))
    if op in (0, 7, 0x15, 0x16, 0x17) or not 0 <= op <= 0x1D:
        same_error(emu, 0x4BA3D0, "Expr_Precedence", args=(op,))
    else:
        compare(emu, 0x4BA3D0, "Expr_Precedence", args=(op,))


def new_args(e, case):
    name, line = case[0], 9
    if name == "int":
        return 1, cstr(e, b"\xf9\xff\xff"), line, 0
    if name == "string":
        return 2, cstr(e, b"text"), line, 0
    if name == "real":
        p = e.alloc(8)
        e.write(p, struct.pack("<d", -1.25))
        return 3, p, line, 0
    if name in ("symbol", "group", "ternary"):
        return {"symbol": 4, "group": 6, "ternary": 7}[name], 0x12345678, line, 0
    if name == "name":
        return 5, cstr(e, b"IntVar"), line, 0
    if name == "op":
        return 8, cstr(e, bytes([case[1]])), line, 0
    if name == "bad":
        return case[1], 0, line, 0
    # a string joined onto prev, which hangs off a parent in different ways
    prev = e.call(0x4BA880, 2, cstr(e, b"left"), 1, 0)[0]
    where = case[1]
    if where != "none":
        parent = e.call(0x4BA880, 6 if where == "group" else 8, 0 if where == "group" else cstr(e, b"\x01"), 1, 0)[0]
        off = {"left": 0x18, "right": 0x1C, "group": 0x8, "other": 0x10}[where]
        e.write(parent + off, u32(prev))
        e.write(prev + 0x20, u32(parent))
        if where == "other":
            e.write(parent, b"\x08")
    return 2, cstr(e, b"right"), line, prev


NEW_CASES = [("int",), ("string",), ("real",), ("symbol",), ("group",), ("ternary",), ("name",), ("op", 1),
             ("op", 0x1D), ("op", 0x19), ("join", "none"), ("join", "left"), ("join", "right"), ("join", "group")]


@pytest.mark.parametrize("case", NEW_CASES)
def test_expr_new(case):
    compare(se, 0x4BA880, "Expr_New", setup=lambda e: new_args(e, case))


@pytest.mark.parametrize("case", [("bad", 0), ("bad", 9), ("join", "other"), ("op", 0)])
def test_expr_new_error(case):
    same_error(se, 0x4BA880, "Expr_New", setup=lambda e: new_args(e, case))


# --- original callers with every hook installed ------------------------------

plain = Emu()
hooked = Emu()
hooked.install_hooks()


def hooked_setup(text):
    def setup(e):
        e.call(0x4B0BA0)
        e.call(0x4B8B20)
        e.write(0xA96598, u32(0))
        var = e.alloc(8)
        e.call(0x4B7E80, cstr(e, b"IntVar"), var)
        e.call(0x4B7FC0, cstr(e, b"RealVar"), e.alloc(8))
        e.call(0x4B8030, cstr(e, b"StrVar"), e.alloc(16), 16)
        p = e.call(0x4B0C60, 0x94, 4)[0]
        e.write(p, b"LOCAL:test.qsc\0")
        e.write(p + 0x80, u32(cstr(e, text)) + u32(len(text)))
        return (p,)
    return setup


@pytest.mark.parametrize("text", RUNS + [
    b"IntVar = 3 * (2 + 1); RealVar = IntVar / 2.0; StrVar = \"ab\" + \"cd\"; IntVar = RealVar > 4 ? 1 : 2;",
    b"_Script_SetWarningLevel(SCRIPT_WARNINGLEVEL_HIGH); IntVar = TRUE && !OFF;",
])
def test_script_run_hooked(text):
    mem = []
    for e in (plain, hooked):
        e.reset()
        args = hooked_setup(text)(e)
        before = e.snapshot()
        e.call(0x4B85B0, *args)
        mem.append(e.changes(before))
    assert mem[0] == mem[1], diff_text(mem[0], mem[1])
