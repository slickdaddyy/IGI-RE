"""SoundManager task handlers (0x51C090 - 0x51C280)."""
import struct

import pytest

from test_flatwater_util import children, cstr, fake_emu, handler, run, set_handler, task, u32

emu = fake_emu({
    0x401B20: 1,  # TaskRef_Remove
    0x401AE0: 1,  # TaskRef_Add
    0x4E6C00: 1,  # Sound_Stop
    0x4E6B00: (2, {"strings": (1,)}),  # Sound_Play
})
INFO, NAME, CHANGED = 0x3C, 0x3D, 0x39
PARENT, SOUND, CHILD = 0x50, 0x51, 0x52
ANSWER = []


def _answer(e, args):
    if ANSWER and ANSWER[0] is not None:
        u32(e, args[1], cstr(e, ANSWER[0]))
    return 0


H_INFO = handler(emu, "info", 2)
H_CHG = handler(emu, "chg", 2)
H_NAME = handler(emu, "name", 2, derefs={1: 4}, effect=_answer)


def manager(e, name=b"", sound=None):
    m = task(e, 0x44, 0x60)
    e.write(m + 0x20, name + b"\0")
    u32(e, m + 0x40, sound or 0)
    return m


def test_create():
    def setup(e):
        m = task(e, 0x44)
        e.write(m + 0x20, b"abc\0")
        u32(e, m + 0x40, 0x1234)
        return (m,)
    run(emu, 0x51C090, "SoundManager_Create", setup)


@pytest.mark.parametrize("ref", [0, 0x4444])
def test_delete(ref):
    def setup(e):
        return (manager(e, sound=ref),)
    run(emu, 0x51C0A0, "SoundManager_Delete", setup)


@pytest.mark.parametrize("has", [False, True])
def test_get_info(has):
    def setup(e):
        p = task(e, 0x40, PARENT)
        m = manager(e)
        u32(e, m + 0x14, p)
        set_handler(e, INFO, PARENT, H_INFO if has else 0)
        return m, 0x777
    run(emu, 0x51C0C0, "SoundManager_GetInfo", setup)


CASES = [
    # name, arg, ref state, playing name answer, play result, kids
    (b"", None, "none", None, 0, 0),
    (b"", None, "empty", None, 0, 0),
    (b"", None, "task", None, 0, 0),
    (b"rain", b"x", "none", None, 0x9000, 0),
    (b"rain", b"x", "none", None, 0, 0),
    (b"rain", None, "none", None, 0x9000, 2),
    (b"rain", b"", "none", None, 0x9000, 3),
    (b"rain", None, "none", None, 0x9000, 0),
    (b"rain", b"x", "empty", None, 0x9000, 0),
    (b"rain", b"x", "task", b"rain", 0x9000, 0),
    (b"rain", b"x", "task", b"wind", 0x9000, 0),
    (b"rain", b"x", "task", b"rai", 0x9000, 0),
    (b"rain", b"x", "task", None, 0x9000, 0),
    (b"rain", b"x", "nohandler", None, 0x9000, 0),
]


@pytest.mark.parametrize("name,arg,state,answer,play,kids", CASES)
def test_params_changed(name, arg, state, answer, play, kids):
    def setup(e):
        ref = None
        if state != "none":
            ref = e.alloc(4)
            if state in ("task", "nohandler"):
                s = task(e, 0x40, SOUND)
                u32(e, ref, s)
        m = manager(e, name, ref)
        if kids:
            children(e, m, kids, type_=CHILD)
            set_handler(e, CHANGED, CHILD, H_CHG)
        ANSWER[:] = [answer]
        set_handler(e, NAME, SOUND, 0 if state == "nohandler" else H_NAME)
        a = cstr(e, arg) if arg is not None else 0
        return m, a
    run(emu, 0x51C100, "SoundManager_ParamsChanged", setup, rets={0x4E6B00: play, 0x401AE0: 0x5550})
