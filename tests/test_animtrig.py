import struct

import pytest

from emu import Emu, ErrorShown
from test_animtrig_util import add_stubs
from test_menu_util import LOG, RET, check as _check, cstr, handler, logfake, set_handler, trap, u16, u32

COUNT, TABLE = 0xA5EF84, 0xA5EC80
FIRE, SIZE, PARSE = 0x54DB80, 0x54DB81, 0x54DB82


def check(em, orig, ours, setup=None, **kw):
    kw.setdefault("check_eax", False)
    return _check(em, orig, ours, setup, **kw)


e = Emu()
add_stubs(e)
logfake(e, 0x4017C0, 1, masks={0: 0xFFFF}, ret=0x33)
logfake(e, 0x4018C0, 1, masks={0: 0xFF})
logfake(e, 0x4B8890, 4)
logfake(e, 0x4B80B0, 1, strings=(0,))
logfake(e, 0x4B85B0, 1)
logfake(e, 0x4B83D0, 1)
logfake(e, 0x4B1420, 3, strings=(0,))
logfake(e, 0x48F340, 0)
logfake(e, 0x4B0C60, 2, effect=lambda em, a: em.alloc(a[0]))
logfake(e, 0x4B0D10, 1)
ARGS = {}
logfake(e, 0x4B8A20, 2, effect=lambda em, a: ARGS[a[1]])


def file_info(em, path, is_dir=0):
    f = em.alloc(12)
    u32(em, f, cstr(em, path), 0x1234, is_dir)
    return (f,)


@pytest.mark.parametrize("path,is_dir,qvm", [
    (b"LOCAL:animtrigger/a.qsc", 0, 0), (b"LOCAL:animtrigger/a.QSC", 0, 0), (b"LOCAL:animtrigger/a.qvm", 0, 0),
    (b"LOCAL:animtrigger/a.qvm", 0, 1), (b"LOCAL:animtrigger/b.qsc", 1, 0), (b"qsc", 0, 0), (b"xqsc", 0, 0),
    (b"x.txt", 0, 1)])
def test_load_file(path, is_dir, qvm):
    def setup(em):
        RET[0x48F340] = qvm
        RET[0x4B80B0] = 0x5555
        return file_info(em, path, is_dir)
    check(e, 0x4EBDF0, "AnimTrigger_LoadFile", setup)


def test_init():
    check(e, 0x4EBE70, "AnimTrigger_Init", normalized=True)


@pytest.mark.parametrize("path,script", [(b"LOCAL:a.qsc", 0x5555), (b"LOCAL:vssver.scc", 0x5555)])
def test_parse_config_file(path, script):
    def setup(em):
        RET[0x4B80B0] = script
        return (cstr(em, path),)
    check(e, 0x4EC070, "AnimTrigger_ParseConfigFile", setup)


def test_parse_config_file_error():
    msgs = []
    for target in (0x4EC070, "AnimTrigger_ParseConfigFile"):
        e.reset()
        RET[0x4B80B0] = 0
        LOG.append([])
        with pytest.raises(ErrorShown) as err:
            e.call(target, cstr(e, b"LOCAL:bad.qsc"))
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]


def test_load():
    check(e, 0x4EC0C0, "AnimTriggerLoad", lambda em: (cstr(em, b"LOCAL:animtrigger"),), normalized=True)


SIZE_FN = trap(e, "size", lambda em: (em.write(em.arg(1), struct.pack("<I", 0x30)), em.ret(0)))
PARSE_FN = handler(e, "parse", 2, derefs={1: 12})
FIRE_FN = handler(e, "fire", 2, derefs={1: 12})


@pytest.mark.parametrize("count,has", [(0, 1), (3, 1), (2, 0)])
def test_define(count, has):
    def setup(em):
        u32(em, COUNT, count)
        em.write(FIRE, b"\x21\x22\x23")
        ARGS[0] = 77
        ARGS[1] = 0x10055
        if has:
            set_handler(em, 0x22, 0x55, SIZE_FN)
            set_handler(em, 0x23, 0x55, PARSE_FN)
        return 0x23F00000, 0x1000, 0x3000
    o = 0x23F00000
    check(e, 0x4EBEB0, "DefineAnimTrigger", setup, ignore=[(o + 4, o + 8), (o + 0x14, o + 0x18)], check_eax=True)


@pytest.mark.parametrize("count", [0, 3])
def test_shutdown(count):
    def setup(em):
        u32(em, COUNT, count)
        em.write(FIRE, b"\x21\x22\x23")
        for i in range(count):
            u32(em, TABLE + 12 * i, i, 0x55, 0x7000 + i if i != 1 else 0)
    check(e, 0x4EBFC0, "AnimTrigger_Shutdown", setup)


@pytest.mark.parametrize("n,name", [(0x4EC040, "AnimTrigger_GetFireEvent"), (0x4EC050, "AnimTrigger_GetSizeEvent"),
                                    (0x4EC060, "AnimTrigger_GetParseEvent")])
def test_getters(n, name):
    check(e, n, name, lambda em: em.write(FIRE, b"\x21\x22\x23"), check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("id_", [5, 9, 1])
def test_fire(id_):
    def setup(em):
        u32(em, COUNT, 3)
        em.write(FIRE, b"\x21\x22\x23")
        for i, v in enumerate((1, 5, 9)):
            u32(em, TABLE + 12 * i, v, 0x50 + i, 0x7000 + i)
            set_handler(em, 0x21, 0x50 + i, FIRE_FN)
        return id_, 0x4444, 0x5555
    check(e, 0x4EC0E0, "AnimTrigger_Fire", setup)


def test_fire_empty():
    check(e, 0x4EC0E0, "AnimTrigger_Fire", lambda em: (u32(em, COUNT, 0), (1, 2, 3))[1])


def test_unk_event():
    check(e, 0x4EC160, "AnimTriggerEvent_Init", lambda em: u32(em, 0xA5EF90, 9))
    check(e, 0x4EC180, "AnimTriggerEvent_Shutdown", lambda em: em.write(0xA5EF88, b"\x31"))


# --- state machine ---

INPUT_FN = trap(e, "input", lambda em: (LOG[-1].append(("input", em.arg(1))),
                                        u32(em, em.arg(0) + 8, 0xFFFFFFFF), em.ret(0)))
RUN_FN = handler(e, "state", 2)


def test_sm_init():
    def setup(em):
        sm = em.alloc(0x4C)
        em.write(sm, b"\xAA" * 0x4C)
        states = em.alloc(16)
        u32(em, states, 0x111, 0x222, 0x333, 0x444)
        return sm, 0x999, states, 2, 3, 0x888
    check(e, 0x4EC190, "StateMachine_Init", setup)


def combos(em, defs):
    """defs: list of steps (mode, timeout, mask)."""
    c = em.alloc(8 * len(defs))
    for i, steps in enumerate(defs):
        s = em.alloc(12 * len(steps))
        for j, (mode, timeout, mask) in enumerate(steps):
            u32(em, s + 12 * j, mode, timeout | 0xAB00, mask)
        u32(em, c + 8 * i, len(steps) | 0xCD00, s)
    return c


DEFS = [[(1, 3, 0x10), (0, 2, 0x10), (1, 0, 0x30)], [(0, 1, 0x4)], [(1, 2, 0x3), (1, 5, 0x1)]]


@pytest.mark.parametrize("bits", [0, 0x10, 0x30, 0x4, 0x3, 0x1])
@pytest.mark.parametrize("progress", [(0, 0, 0, 0, 0, 0), (1, 1, 0, 0, 1, 1), (2, 3, 0, 0, 1, 4), (1, 2, 0, 0, 0, 0)])
def test_sm_update(bits, progress):
    def setup(em):
        sm = em.alloc(0x4C)
        u32(em, sm, INPUT_FN, 0, bits, 1, 2, RUN_FN, 3, 7, combos(em, DEFS))
        em.write(sm + 0x24, bytes(progress) + b"\x00" * (40 - len(progress)))
        return sm, 0x6666
    check(e, 0x4EC1E0, "StateMachine_Update", setup)
