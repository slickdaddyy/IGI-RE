import pytest

from emu import compare
from test_props_util import (RET, handler, init_emu, logfake, make_emu, run, set_handler, setup_init, task, u32, vfake)

ALARM, TERM = 0x5B, 0x5A

emu = make_emu()
logfake(emu, 0x443E80, 0, ret=TERM)  # Terminal_GetType
logfake(emu, 0x4F1080, 0, ret=1234)  # Level_GetTick
for slot, n in ((0, 2), (1, 1), (2, 1), (0x31, 2), (0x24, 1), (0x25, 1)):
    set_handler(emu, slot, TERM, handler(emu, "%x_term" % slot, n))
set_handler(emu, 0x25, ALARM, handler(emu, "25_alarm", 1))
vfake(emu, 0x4F16C0, 1, effect=lambda e, a: RET["has"][a[0] - RET["obj"] == 0x2FC])  # VarString_HasExpr
emu.baseline = emu.snapshot()

init = init_emu()


def alarm(e, off=0, powered=1, trigger=0, is_alarm=0, registered=0):
    t = task(e, 0x350, ALARM)
    e.write(t + 0x190, bytes([off]))
    e.write(t + 0x240, bytes([powered]))
    e.write(t + 0x2A0, bytes([trigger, is_alarm, 7]))
    u32(e, t + 0x2A4, 77)
    e.write(t + 0x2A8, bytes([registered]))
    return t


def test_open():
    def setup(e):
        setup_init(e)
        u32(e, 0x57C1A0, e.call(0x4F1950, 3)[0])  # the Terminal parameter table
    compare(init, 0x440710, "AlarmControl_Open", setup=setup, check_eax=False, normalized=True)


def test_create():
    run(emu, 0x440840, "AlarmControl_Create", lambda e: (alarm(e, trigger=1, registered=1), 0x4242))


@pytest.mark.parametrize("off,powered", [(0, 1), (1, 1), (0, 0)])
@pytest.mark.parametrize("has", [(0, 0), (1, 0), (0, 1), (1, 1)])
@pytest.mark.parametrize("value", [0.0, 1.0, 0.5, 1.5, -1.0])
@pytest.mark.parametrize("trigger", [0, 1])
def test_run(off, powered, has, value, trigger):
    def setup(e):
        t = alarm(e, off=off, powered=powered, trigger=trigger, is_alarm=1)
        RET["obj"], RET["has"] = t, has
        RET[0x4F16E0] = value
        return (t,)
    run(emu, 0x4408A0, "AlarmControl_Run", setup)


@pytest.mark.parametrize("id_", [-1, 0, 2])
@pytest.mark.parametrize("handled", [0, 1])
def test_delete(id_, handled):
    def setup(e):
        RET[0x4F1050] = id_
        if not handled:
            set_handler(e, 0x25, ALARM, 0)
        return (alarm(e),)
    run(emu, 0x440970, "AlarmControl_Delete", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57C168, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x4409E0, "AlarmControl_GetParams", setup)


def test_params_changed():
    run(emu, 0x4409F0, "AlarmControl_ParamsChanged", lambda e: (alarm(e), 0x4343))


@pytest.mark.parametrize("registered", [0, 1])
def test_activate(registered):
    run(emu, 0x440A40, "AlarmControl_Activate", lambda e: (alarm(e, registered=registered),))


@pytest.mark.parametrize("registered", [0, 1])
def test_deactivate(registered):
    run(emu, 0x440B20, "AlarmControl_Deactivate", lambda e: (alarm(e, registered=registered),))


def test_close():
    def setup(e):
        u32(e, 0x57C168, 0x5151)
        u32(e, 0x57C164, 0x12340044)
    run(emu, 0x440BE0, "AlarmControl_Close", setup)


@pytest.mark.parametrize("value", [0, 1, 0x80])
def test_is_alarm(value):
    run(emu, 0x440C00, "AlarmControl_IsAlarm", lambda e: (alarm(e, is_alarm=value),), check_eax=True, eax_mask=0xFF)
