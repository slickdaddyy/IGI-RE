import struct

import pytest

from emu import Emu, ErrorShown, compare, diff_text
from test_menu_util import LOG, check, cstr, handler, logfake, trap, u32

PARAM_INIT = 0x4F1840
TYPE_NEW = 0x4F19C0
TYPES = 0xA758E0
MEM_ALLOC = 0x4B0C60
ADD_CONSTANTS = 0x4F1DB0


def base_emu():
    e = Emu()
    e.call(0x4B0BA0)  # Mem_Init
    e.call(0x4B8B20)  # Script_Init
    e.call(0x4010D0)  # Task_InitSystem
    e.baseline = e.snapshot()
    return e


e = base_emu()
OPFN = handler(e, "op", 2)


def add(e):
    t, n, p = struct.unpack("<3I", e.read(e.arg(1), 12))
    LOG[-1].append(("add", e.normalize(e.arg(0)), t, e.read_cstr(n), e.u32(p)))
    e.ret(0)


ADD = trap(e, "enum_add", add)
e.baseline = e.snapshot()


def same_error(e, orig, ours, setup):
    out = []
    for target in (orig, ours):
        e.reset()
        a = setup(e) or ()
        before = e.snapshot()
        with pytest.raises(ErrorShown) as info:
            e.call(target, *a)
        out.append((str(info.value), e.changes(before)))
    assert out[0][0] == out[1][0]
    assert out[0][1] == out[1][1], diff_text(out[0][1], out[1][1])


def init(e):
    e.call(PARAM_INIT)


def test_param_init():
    compare(e, PARAM_INIT, "Param_Init", check_eax=False)


def test_param_close():
    compare(e, 0x4F18C0, "Param_Close", setup=init, check_eax=False)


@pytest.mark.parametrize("n", [0, 3, 40])
def test_table_new(n):
    compare(e, 0x4F1950, "ParamTable_New", setup=lambda e: (n,))


@pytest.mark.parametrize("count,extra", [(0, 2), (3, 0), (5, 4)])
def test_table_extend(count, extra):
    def setup(e):
        t = e.call(0x4F1950, count + 2)[0]
        u32(e, t, count)
        e.write(t + 12, bytes(range(16 * (count + 2))))
        return t, extra
    compare(e, 0x4F1980, "ParamTable_Extend", setup=setup)


def make_types(e):
    """Types 0..5 with parents -1, 0, 1, 0, -1, 0; type 3 freed again."""
    init(e)
    ids = []
    for parent in (-1, 0, 1, 0, -1, 0):
        ids.append(e.call(TYPE_NEW, cstr(e, b"T%d" % len(ids)), parent)[0])
    for i in range(6):
        e.write(TYPES + 0x4C * i + 8, struct.pack("<16I", *range(0x1000 * i, 0x1000 * i + 16)))
    u32(e, TYPES + 0x4C * 3 + 0x48, 0)
    return ids


@pytest.mark.parametrize("parent", [-1, 1, 3])
def test_type_new(parent):
    def setup(e):
        make_types(e)
        return cstr(e, b"Fresh"), parent
    compare(e, TYPE_NEW, "ParamType_New", setup=setup)


def test_type_new_full():
    def setup(e):
        init(e)
        for i in range(64):
            u32(e, TYPES + 0x4C * i + 0x48, 1)
        return cstr(e, b"Fresh"), -1
    same_error(e, TYPE_NEW, "ParamType_New", setup)


def test_type_free():
    def setup(e):
        make_types(e)
        return (2,)
    compare(e, 0x4F1A40, "ParamType_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("addr,name", [
    (0x4F1A60, "Param_GetTableEvent"), (0x4F1A70, "Param_GetChangedEvent"), (0x4F1B60, "ParamOp_Parse"),
    (0x4F1B70, "ParamOp_ArgCount"), (0x4F1B80, "ParamOp_Format"), (0x4F1B90, "ParamOp_Size"),
    (0x4F1BA0, "ParamOp_Copy"), (0x4F1BB0, "ParamOp_Paste")])
def test_event_getters(addr, name):
    def setup(e):
        e.write(0x54E0E4, bytes(range(0x81, 0x89)))
    compare(e, addr, name, setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("addr,name", [(0x4F1E00, "Param_EnumInt32"), (0x4F1E10, "Param_EnumReal32"),
                                       (0x4F1E20, "Param_EnumString32")])
def test_enum_getters(addr, name):
    def setup(e):
        u32(e, 0x54E150, 0x11, 0x22, 0x33, 0x44, 0x55)
    compare(e, addr, name, setup=setup)


@pytest.mark.parametrize("count", [0, 3])
def test_table_add(count):
    def setup(e):
        t = e.call(0x4F1950, 8)[0]
        u32(e, t, count)
        base = e.alloc(0x100)
        return t, base, base + 0x2C, 7, 0x12345601, 0xA76BE0
    compare(e, 0x4F1A80, "ParamTable_Add", setup=setup, check_eax=False)


def test_table_add_undefined():
    def setup(e):
        t = e.call(0x4F1950, 8)[0]
        return t, 0x1000, 0x1010, -1, 1, 0
    same_error(e, 0x4F1A80, "ParamTable_Add", setup)


@pytest.mark.parametrize("op", [0, 5, 0x10F])
def test_type_call(op):
    def setup(e):
        make_types(e)
        u32(e, TYPES + 0x4C * 2 + 8 + 4 * (op & 0xFF), OPFN)
        return e.alloc(16), 2, op, e.alloc(16)
    check(e, 0x4F1AF0, "ParamType_Call", setup=setup, check_eax=False)


@pytest.mark.parametrize("used", [0, 5, 15])
def test_event_alloc(used):
    def setup(e):
        e.write(0xA758C8, bytes([1] * used + [0] * (16 - used)))
    compare(e, 0x4F1B20, "ParamEvent_Alloc", setup=setup, eax_mask=0xFF)


def test_event_alloc_full():
    same_error(e, 0x4F1B20, "ParamEvent_Alloc", lambda e: e.write(0xA758C8, b"\1" * 16))


def test_event_free():
    def setup(e):
        e.write(0xA758C8, b"\1" * 16)
        return (0xFFFFFF07,)
    compare(e, 0x4F1B50, "ParamEvent_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("type_", [0, 1, 4])
def test_type_set_op(type_):
    def setup(e):
        make_types(e)
        return type_, 0xFFFFFF06, 0xCAFE0000
    compare(e, 0x4F1BC0, "ParamType_SetOp", setup=setup, check_eax=False)


fe = base_emu()
logfake(fe, 0x4B8B00, 2, effect=lambda e, a: int(a[1] < 7))  # Script_HasArg


@pytest.mark.parametrize("index,count", [(0, 0), (2, 5), (0, 7)])
def test_cursor_need(index, count):
    def setup(e):
        c = e.alloc(16)
        u32(e, c, index, 0x5000, cstr(e, b"objects.qsc"), 12)
        return c, count
    check(fe, 0x4F1C20, "ParamCursor_Need", setup=setup, check_eax=False)


@pytest.mark.parametrize("index,count", [(3, 5), (7, 1)])
def test_cursor_need_error(index, count):
    def setup(e):
        LOG.append([])
        c = e.alloc(16)
        u32(e, c, index, 0x5000, cstr(e, b"objects.qsc"), 12)
        return c, count
    same_error(fe, 0x4F1C20, "ParamCursor_Need", setup)


@pytest.mark.parametrize("addr,name", [(0x4F1C70, "Param_Range01"), (0x4F1C90, "Param_Range02"),
                                       (0x4F1CB0, "Param_Range0100")])
def test_ranges(addr, name):
    compare(e, addr, name)


def test_enum_init():
    def setup(e):
        p = e.alloc(16)
        e.write(p, b"\xAA" * 16)
        return (p,)
    compare(e, 0x4F1CD0, "ParamEnum_Init", setup=setup, check_eax=False)


def test_enum_setup():
    def setup(e):
        p = e.alloc(16)
        e.write(p, b"\xAA" * 16)
        return p, 0x1234
    compare(e, 0x4F1D60, "ParamEnum_Setup", setup=setup, check_eax=False)


def visitor(e):
    v = e.alloc(8)
    u32(e, v, 0x77, ADD)
    return v


def test_enum_for_each_constant():
    def setup(e):
        init(e)
        u32(e, 0xA70C7C, 9)  # Int32 type id
        for i, name in enumerate([b"ALPHA", b"BETA", b"GAMMA"]):
            e.call(0x4B8810, cstr(e, name), 100 + i)  # Script_RegisterConstant
        p = e.alloc(8)
        e.call(0x4B7E80, cstr(e, b"IntVar"), p)
        e.call(0x4B8870, cstr(e, b"RealConst"), p)
        return (visitor(e),)
    check(e, 0x4F1CF0, "ParamEnum_ForEachConstant", setup=setup, check_eax=False)


@pytest.mark.parametrize("kind,vtype", [(2, 1), (2, 2), (0, 1), (1, 1)])
def test_enum_visit_constant(kind, vtype):
    def setup(e):
        u32(e, 0xA70C7C, 9)
        sym = e.alloc(0x1C)
        u32(e, sym, kind, vtype, 0, 4, 0, 0, 0x55AA)
        return cstr(e, b"NAME"), sym, visitor(e)
    check(e, 0x4F1D10, "ParamEnum_VisitConstant", setup=setup, check_eax=False)


def enum_list(e, n):
    p = e.alloc(16)
    e.call(0x4F1CD0, p)
    for i in range(n):
        e.call(ADD_CONSTANTS, p, cstr(e, b"^HUMAN_%d" % i))
    return p


@pytest.mark.parametrize("n", [0, 1, 3])
def test_enum_free(n):
    compare(e, 0x4F1D80, "ParamEnum_Free", setup=lambda e: (enum_list(e, n),), check_eax=False)


@pytest.mark.parametrize("n", [0, 2])
def test_enum_add_constants(n):
    def setup(e):
        return enum_list(e, n), cstr(e, b"^WEAPON_ID_[A-Z0-9]*$")
    compare(e, ADD_CONSTANTS, "ParamEnum_AddConstants", setup=setup, check_eax=False)


def enum_base_types(e):
    init(e)
    for addr in (0xA71870, 0xA70C7C, 0xA71884, 0xA71888, 0xA7186C):
        u32(e, addr, e.call(TYPE_NEW, cstr(e, b"base"), -1)[0])
        u32(e, TYPES + 0x4C * e.u32(addr) + 8, 0xB000 + addr & 0xFFFF)


def test_enum_init_types():
    compare(e, 0x4F1E30, "ParamEnum_InitTypes", setup=enum_base_types, check_eax=False, normalized=True)


def test_enum_free_types():
    def setup(e):
        enum_base_types(e)
        e.call(0x4F1E30)
    compare(e, 0x4F1EA0, "ParamEnum_FreeTypes", setup=setup, check_eax=False)
