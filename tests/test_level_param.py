"""Parameter type operations and getters (src/level/paramtype.c)."""
import struct

import pytest

from emu import Emu
from test_menu_util import LOG, check, cstr, logfake, pyfake, u32

ARGS = []


def get_arg_string(e):
    args, i, buf, size = (e.arg(n) for n in range(4))
    LOG[-1].append(("str", e.normalize(args), i, size))
    v = ARGS[i]
    e.write(buf, v[:max(size - 1, 0)] + b"\0")
    e.ret(buf)


def get_arg_int(e):
    args, i = e.arg(0), e.arg(1)
    LOG[-1].append(("int", e.normalize(args), i))
    e.ret(ARGS[i])


def mem_alloc(e):
    size, align = e.arg(0), e.arg(1)
    LOG[-1].append(("alloc", size, align))
    e.ret(e.alloc(size))


def make_emu():
    e = Emu()
    logfake(e, 0x4F1C20, 2, derefs={0: 16})  # ParamCursor_Need
    pyfake(e, 0x4B8A80, get_arg_string)
    pyfake(e, 0x4B8A20, get_arg_int)
    pyfake(e, 0x4B0C60, mem_alloc)
    logfake(e, 0x4B0D10, 1)  # Mem_Free
    e.baseline = e.snapshot()
    return e


emu = make_emu()


def cursor(e, index=2):
    c = e.alloc(16)
    u32(e, c, index, 0x7700, 0x7800, 42)
    return c


def run(orig, ours, setup, args=(), **kw):
    ARGS[:] = args
    kw.setdefault("check_eax", False)
    check(emu, orig, ours, setup=setup, normalized=True, **kw)


def ptr_to(e, data, size=None):
    """A pointer to a buffer holding data, and the address of that pointer."""
    buf = e.alloc(size or len(data) + 1)
    e.write(buf, data)
    p = e.alloc(4)
    u32(e, p, buf)
    return p


@pytest.mark.parametrize("v", [0, 0x1234, 0xFFFF])
def test_int16(v):
    def copy(e):
        f = e.alloc(4)
        e.write(f, struct.pack("<HH", v, 0x5555))
        return f, ptr_to(e, b"\xAA" * 4)

    def paste(e):
        f = e.alloc(4)
        e.write(f, b"\xAA" * 4)
        return f, ptr_to(e, struct.pack("<HH", v, 0x5555))

    run(0x4F0170, "ParamInt16_Copy", copy)
    run(0x4F0190, "ParamInt16_Paste", paste)
    run(0x4F01B0, "ParamInt16_Size", lambda e: (0, e.alloc(4)))


def var_string(e, text):
    v = e.alloc(0x50)
    if text is not None:
        u32(e, v, cstr(e, text))
    return v


@pytest.mark.parametrize("old", [None, b"old"])
@pytest.mark.parametrize("arg", [b"", b"abc", b"x" * 0x500], ids=["empty", "short", "long"])
def test_var_string_parse(old, arg):
    run(0x4F01C0, "ParamVarString_Parse", lambda e: (var_string(e, old), cursor(e)), args=(0, 0, arg))


@pytest.mark.parametrize("text", [None, b"", b"hello"])
def test_var_string_copy_size(text):
    run(0x4F0270, "ParamVarString_Copy", lambda e: (var_string(e, text), ptr_to(e, b"\xAA" * 8)))
    run(0x4F0320, "ParamVarString_Size", lambda e: (var_string(e, text), e.alloc(4)))


@pytest.mark.parametrize("old", [None, b"old"])
@pytest.mark.parametrize("src", [b"", b"xyz"])
def test_var_string_paste(old, src):
    run(0x4F02B0, "ParamVarString_Paste", lambda e: (var_string(e, old), ptr_to(e, src)))


@pytest.mark.parametrize("addr,name,size", [(0x4F0350, "ParamString16_Parse", 16), (0x4F03A0, "ParamString32_Parse", 32),
                                            (0x4F0400, "ParamString256_Parse", 256)])
@pytest.mark.parametrize("arg", [b"", b"short", b"y" * 300], ids=["empty", "short", "long"])
def test_string_parse(addr, name, size, arg):
    def setup(e):
        f = e.alloc(size + 16)
        e.write(f, b"\xAA" * (size + 16))
        return f, cursor(e, 1)
    run(addr, name, setup, args=(0, arg))


@pytest.mark.parametrize("text", [b"", b"abc", b"z" * 200], ids=["empty", "short", "long"])
def test_string_ops(text):
    run(0x4F0380, "ParamString_Size", lambda e: (cstr(e, text), e.alloc(4)))
    run(0x4F03D0, "ParamString_Paste", lambda e: (e.alloc(256), ptr_to(e, text)))
    run(0x4F04D0, "ParamString_Copy", lambda e: (cstr(e, text), ptr_to(e, b"\xAA" * 256)))


def text_out(e, prefix):
    buf = e.alloc(512)
    e.write(buf, prefix + b"\0" + b"\xAA" * 32)
    t = e.alloc(8)
    u32(e, t, 0x1234, buf)
    return t


@pytest.mark.parametrize("prefix", [b"", b"name = "])
@pytest.mark.parametrize("text", [b"", b"value"])
def test_string_format(prefix, text):
    run(0x4F0430, "ParamString_Format", lambda e: (cstr(e, text), text_out(e, prefix)))
    run(0x4F04A0, "ParamExpression_Format", lambda e: (ptr_to(e, text), text_out(e, prefix)))


@pytest.mark.parametrize("old", [b"", b"abcdefgh"])
@pytest.mark.parametrize("arg", [b"", b"x > 1", b"0123456789abcdef"])
def test_expression_parse(old, arg):
    run(0x4F0460, "ParamExpression_Parse", lambda e: (ptr_to(e, old, 32), cursor(e, 0)), args=(arg,))


@pytest.mark.parametrize("v", [0, 1, 0x1FF, -5])
def test_bool8(v):
    def field(e):
        f = e.alloc(4)
        e.write(f, struct.pack("<i", v))
        return f

    run(0x4F0500, "ParamBool8_Parse", lambda e: (field(e), cursor(e, 1)), args=(0, v & 0xFFFFFFFF))
    run(0x4F0530, "ParamBool8_Format", lambda e: (field(e), text_out(e, b"on: ")))
    run(0x4F0560, "ParamBool8_Copy", lambda e: (field(e), ptr_to(e, b"\xAA" * 4)))
    run(0x4F0570, "ParamBool8_Paste", lambda e: (e.alloc(4), ptr_to(e, struct.pack("<i", v))))


GETTERS = [
    (0x4F0EF0, "Param_PushButton", 0xA75898), (0x4F0F00, "Param_DropDownCombo", 0xA70C70),
    (0x4F0F10, "Param_ScriptFile", 0xA70C8C), (0x4F0F20, "Param_Angle", 0xA70C84),
    (0x4F0F30, "Param_Degrees", 0xA70CA4), (0x4F0F40, "Param_RGB", 0xA71880),
    (0x4F0F50, "Param_Real32", 0xA71870), (0x4F0F60, "Param_Real32x3", 0xA70C68),
    (0x4F0F70, "Param_Real32x9", 0xA70C6C), (0x4F0F80, "Param_Real64", 0xA71874),
    (0x4F0F90, "Param_Real64x3", 0xA75890), (0x4F0FA0, "Param_Int32", 0xA70C7C),
    (0x4F0FB0, "Param_Int16", 0xA70C78), (0x4F0FC0, "Param_VarString", 0xA70C9C),
    (0x4F0FD0, "Param_String16", 0xA71884), (0x4F0FE0, "Param_String32", 0xA71888),
    (0x4F0FF0, "Param_String256", 0xA7186C), (0x4F1000, "Param_Bool8", 0xA70C90),
    (0x4F1010, "Param_ObjectPos", 0xA70C88), (0x4F1020, "Param_Graph", 0xA70CA0),
]


@pytest.mark.parametrize("addr,name,var", GETTERS)
def test_getters(addr, name, var):
    run(addr, name, lambda e: u32(e, var, 0x12345678 ^ addr), check_eax=True)
