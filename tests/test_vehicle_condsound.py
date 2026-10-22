import pytest

from emu import compare
from test_vehicle_util import (LOG, RET, add_child, cstr, floatfake, handler, init_emu, logfake, pyfake, run,
                               set_events, set_handler, setup_init, task, u32)

CS, CHILD = 0x50, 0x51

emu = init_emu()
set_events(emu)
for slot in (1, 0x25):
    for t in (CS, CHILD):
        set_handler(emu, slot, t, handler(emu, "%x_%x" % (slot, t)))
logfake(emu, 0x4F1320, 1)  # VarString_Init
logfake(emu, 0x4F1340, 1)  # VarString_Free
logfake(emu, 0x4F1400, 2)  # VarString_Bind
logfake(emu, 0x4F16C0, 1)  # VarString_HasExpr
floatfake(emu, 0x4F16E0, 2)  # VarString_EvalReal
logfake(emu, 0x4015F0, 1)  # Task_Delete
logfake(emu, 0x401B20, 1)  # TaskRef_Remove
logfake(emu, 0x401AE0, 1, ret=0x7000)  # TaskRef_Add
logfake(emu, 0x497450, 1, strings=(0,), ret=0x11)
logfake(emu, 0x497410, 1, strings=(0,), ret=0x22)
logfake(emu, 0x4974A0, 1)
logfake(emu, 0x4E6D60, 1, derefs={0: 16})
logfake(emu, 0x4E6B00, 2, strings=(1,))  # Sound_New
logfake(emu, 0x4E6C30, 2, derefs={1: 24})
logfake(emu, 0x4E6CE0, 2)
logfake(emu, 0x4E6FA0, 1)
logfake(emu, 0x4F1050, 1)  # Task_GetId
logfake(emu, 0x4B7EE0, 2, strings=(0,))  # Script_RegisterBool8
logfake(emu, 0x4B7FA0, 2, strings=(0,))  # Script_RegisterInt2
logfake(emu, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4C1830, 1)  # QTaskList_Free


def get_name(e):
    e.write(e.arg(0), b"Sound_12\0")
    if LOG:
        LOG[-1].append(("name", e.normalize(e.arg(1))))
    e.ret(0)


pyfake(emu, 0x4F1270, get_name)

init = init_emu()


def cs(e, sound=None, **f):
    t = task(e, 0x128, CS)
    e.write(t + 0x70, b"engine.wav\0")
    u32(e, t + 0x90, 1, 2, 3, 4, 5, 6)
    for off, name in ((0xA8, "stream"), (0xA9, "once"), (0xB0, "run"), (0xB1, "last"), (0xB2, "playing"),
                      (0xB3, "loop"), (0xB4, "registered"), (0x124, "played")):
        e.write(t + off, bytes([f.get(name, 0)]))
    if sound is not None:
        ref = e.alloc(4)
        u32(e, ref, sound)
        u32(e, t + 0x120, ref)
    return t


def test_init():
    compare(init, 0x424E80, "ConditionalSound_Init", setup=setup_init, check_eax=False, normalized=True)


def test_create():
    run(emu, 0x425050, "ConditionalSound_Create", lambda e: (cs(e, sound=5, run=1, played=1),))


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("value", [0.0, 1.0, 0.4, 2.6, 257.0])
@pytest.mark.parametrize("f", [dict(), dict(run=1), dict(run=1, last=1), dict(once=1, played=1),
                               dict(once=1, run=1), dict(stream=1, run=1)])
@pytest.mark.parametrize("children", [0, 2])
def test_run(has, value, f, children):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        RET[0x4E6B00] = 0x6000
        t = cs(e, sound=0x6000, **f)
        for i in range(children):
            add_child(e, t, task(e, 0x40, CHILD if i else CS))
        return (t,)
    run(emu, 0x4250D0, "ConditionalSound_Run", setup)


@pytest.mark.parametrize("sound", [None, 0, 0x6000])
def test_stop(sound):
    run(emu, 0x4251C0, "ConditionalSound_Stop", lambda e: (cs(e, sound=sound, playing=1),))


@pytest.mark.parametrize("f", [dict(), dict(loop=1), dict(stream=1)])
@pytest.mark.parametrize("new", [0, 0x6100])
def test_play(f, new):
    def setup(e):
        RET[0x4E6B00] = new
        return (cs(e, sound=0x6000, **f),)
    run(emu, 0x425200, "ConditionalSound_Play", setup)


@pytest.mark.parametrize("stream", [0, 1])
@pytest.mark.parametrize("sound", [None, 0, 0x6000])
@pytest.mark.parametrize("playing", [0, 1])
def test_update_playing(stream, sound, playing):
    def setup(e):
        RET[0x4E6FA0] = playing
        return (cs(e, sound=sound, stream=stream, playing=1 - playing),)
    run(emu, 0x4252E0, "ConditionalSound_UpdatePlaying", setup)


@pytest.mark.parametrize("id_", [-1, 0, 5])
@pytest.mark.parametrize("handled", [0, 1])
def test_delete(id_, handled):
    def setup(e):
        RET[0x4F1050] = id_
        if not handled:
            set_handler(e, 0x25, CS, 0)
        return (cs(e, sound=0x6000),)
    run(emu, 0x425340, "ConditionalSound_Delete", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57BC6C, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x425390, "ConditionalSound_GetParams", setup)


def test_changed():
    run(emu, 0x4253A0, "ConditionalSound_Changed", lambda e: (cs(e),))


@pytest.mark.parametrize("registered", [0, 1])
def test_activate(registered):
    run(emu, 0x4253C0, "ConditionalSound_Activate", lambda e: (cs(e, registered=registered),))


@pytest.mark.parametrize("registered", [0, 1])
def test_deactivate(registered):
    run(emu, 0x4254A0, "ConditionalSound_Deactivate", lambda e: (cs(e, registered=registered),))


def test_reset():
    run(emu, 0x425560, "ConditionalSound_Reset", lambda e: (cs(e, sound=0x6000, run=1, last=1),))


def test_close():
    def setup(e):
        u32(e, 0x57BC68, 0x44)
        u32(e, 0x57BC6C, 0x5151)
    run(emu, 0x425580, "ConditionalSound_Close", setup)
