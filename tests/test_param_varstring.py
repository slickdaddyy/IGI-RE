import struct

import pytest

from emu import Emu, compare
from test_menu_util import cstr, task, trap, u32

SET_ID = 0x4F1090
TASKREF_ADD = 0x401AE0
MEM_ALLOC = 0x4B0C60
BIND = 0x4F1400
MEM_MODE = 0x9422FD

e = Emu()
e.call(0x4B0BA0)  # Mem_Init
e.call(0x4B8B20)  # Script_Init
u32(e, 0xA43E70, trap(e, "script_error", lambda e: e.ret(0)))
e.call(0x4010D0)  # Task_InitSystem
e.call(0x4F1840)  # Param_Init
e.call(0x4B8810, cstr(e, b"FIVE"), 5)  # Script_RegisterConstant


def var(name, data, register):
    p = e.alloc(8)
    e.write(p, data)
    e.call(register, cstr(e, name), p)


var(b"Door_1.isOpen", struct.pack("<i", 1), 0x4B7E80)
var(b"Door_2.isOpen", struct.pack("<i", 0), 0x4B7E80)
var(b"Door_2.v", struct.pack("<d", 0.7), 0x4B7FC0)
var(b"QTask_-1.isOn", struct.pack("<i", 3), 0x4B7E80)
e.baseline = e.snapshot()


def with_cw(e, target, cw):
    """Code that loads x87 control word cw and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    cell = e.fake_top
    code = cell + 4
    e.fake_top += 32
    e.write(cell, struct.pack("<H", cw))
    e.write(code, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (code + 11)))
    return code


def heap_str(e, s):
    p = e.call(MEM_ALLOC, len(s) + 1, 4)[0]
    e.write(p, s + b"\0")
    return p


def new_vs(e, text):
    v = e.alloc(0x50)
    if text is not None:
        u32(e, v, heap_str(e, text))
    return v


def ids(e, n):
    ts = [task(e, 0x40) for _ in range(n)]
    for i, t in enumerate(ts):
        e.call(SET_ID, i + 1, t)
    return ts


TEXTS = [
    None,
    b"",
    b"FIVE",
    b"Door_1.isOpen + 2",
    b"Door_1.isOpen && Door_2.isOpen || Door_1.x",
    b"Door_9.isOpen",
    b"Door_3.isExists",
    b"Door_9.isExists",
    b"this.isOn == 1",
    b"  12.5 * 3 + 0x1F - $1 ",
    b"\"a.b_3\" + 7.isOpen",
    b"x.y.z_2 >= (FIVE*2);",
]


@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("text", TEXTS)
def test_bind(text, mode):
    def setup(e):
        ts = ids(e, 3)
        e.write(MEM_MODE, bytes([mode]))
        return new_vs(e, text), ts[1]
    compare(e, BIND, "VarString_Bind", setup=setup, check_eax=False)


def test_bind_again():
    """Bind a second time: the old references and expression are released."""
    def setup(e):
        ts = ids(e, 3)
        v = new_vs(e, b"Door_1.isOpen + Door_2.isOpen")
        e.call(BIND, v, ts[0])
        return v, ts[0]
    compare(e, BIND, "VarString_Bind", setup=setup, check_eax=False)


def test_init():
    def setup(e):
        v = e.alloc(0x50)
        e.write(v, b"\xAA" * 0x50)
        return (v,)
    compare(e, 0x4F1320, "VarString_Init", setup=setup, check_eax=False)


@pytest.mark.parametrize("state", ["empty", "text", "bound"])
def test_free(state):
    def setup(e):
        ts = ids(e, 3)
        v = new_vs(e, None if state == "empty" else b"Door_1.isOpen + Door_2.isOpen")
        if state == "bound":
            e.call(BIND, v, ts[0])
        return (v,)
    compare(e, 0x4F1340, "VarString_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("which", [0, 1, 2])
def test_add_ref(which):
    def setup(e):
        ts = ids(e, 3)
        v = new_vs(e, b"Door_1.a + Door_2.a")
        e.call(BIND, v, ts[0])
        return v, ts[which]
    compare(e, 0x4F13C0, "VarString_AddRef", setup=setup, check_eax=False)


@pytest.mark.parametrize("state", ["notext", "emptyexp", "noexpr", "expr"])
def test_has_expr(state):
    def setup(e):
        v = e.alloc(0x50)
        if state != "notext":
            u32(e, v, cstr(e, b"x"), cstr(e, b"" if state == "emptyexp" else b"x;"), 0 if state == "noexpr" else 0x1234)
        return (v,)
    compare(e, 0x4F16C0, "VarString_HasExpr", setup=setup, eax_mask=0xFF)


def dead(e, task_ref):
    u32(e, task_ref, 0)


@pytest.mark.parametrize("state", ["norefs", "alive", "dead", "deadnoexpr"])
def test_check_refs(state):
    def setup(e):
        ts = ids(e, 3)
        v = new_vs(e, b"FIVE" if state == "norefs" else b"Door_1.a + Door_2.a")
        e.call(BIND, v, ts[0])
        if state.startswith("dead"):
            u32(e, e.u32(v + 0x10), 0)  # second reference lost its task
        if state == "deadnoexpr":
            u32(e, v + 8, 0)
        return (v,)
    compare(e, 0x4F1740, "VarString_CheckRefs", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F, 0xC7F, 0x77F])
@pytest.mark.parametrize("state", ["new", "bound", "dead", "empty", "notext"])
def test_eval_real(state, cw):
    orig, ours = with_cw(e, 0x4F16E0, cw), with_cw(e, "VarString_EvalReal", cw)

    def setup(e):
        ts = ids(e, 3)
        text = {"empty": b"", "notext": None}.get(state, b"FIVE / 3.0 + 0.1 + Door_2.v")
        v = new_vs(e, text)
        if state in ("bound", "dead"):
            e.call(BIND, v, ts[0])
        if state == "dead":
            u32(e, e.u32(v + 0xC), 0)
        return v, ts[0]
    compare(e, orig, ours, setup=setup, check_eax=False, check_st0=True)


PARSE = [b"abc_12.x", b"Door_7.isOpen", b"abc", b"abc_3", b"a1b2", b".x", b"", b"x_.y", b"12.x", b"7", b"a_007.b.c_4"]


@pytest.mark.parametrize("before", [0, 0x35])
@pytest.mark.parametrize("s", PARSE)
def test_parse_id(s, before):
    def setup(e):
        p = e.alloc(len(s) + 2)
        e.write(p, bytes([before]) + s + b"\0")
        return (p + 1,)
    compare(e, 0x4F17D0, "VarString_ParseId", setup=setup)
