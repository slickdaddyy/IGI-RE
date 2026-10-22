import pytest

from emu import Emu
from test_fxobj_util import (RET, f32, floatfake, handler, logfake, run, set_events, set_handler, task, u32)

RAIN = 0x60

emu = Emu()
set_events(emu)
logfake(emu, 0x4B8A20, 2, ret=0x12345)  # Script_GetArgInt
floatfake(emu, 0x4B8A50, 2)  # Script_GetArgReal
for a, n in ((0x4F1320, 1), (0x4F1340, 1), (0x4F1400, 2), (0x4F16C0, 1), (0x498A70, 2), (0x499CF0, 1),
             (0x499F90, 2), (0x499FA0, 2), (0x4F1050, 1), (0x498960, 5)):
    logfake(emu, a, n)
floatfake(emu, 0x4F16E0, 2)  # VarString_EvalReal
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
set_handler(emu, 0x25, RAIN, handler(emu, "deactivate"))
u32(emu, 0x57BD80, 0x6200)
u32(emu, 0x57BD88, 0x6300)
emu.baseline = emu.snapshot()


def rain(e, r=0, level=0.25, last=0.25, unk24=1):
    t = task(e, 0x88, RAIN)
    u32(e, t + 0x20, r)
    e.write(t + 0x24, bytes([unk24]))
    f32(e, t + 0x28, 50.0, 20.5, level, last)
    return t


@pytest.mark.parametrize("real", [1.0, -2.5, 3.75, 1e10])
def test_rudder_parse_args(real):
    def setup(e):
        RET[0x4B8A50] = real
        out = e.alloc(8)
        a = e.alloc(16)
        u32(e, a, out, 0x4444, 3, 0)
        return (0, a)
    run(emu, 0x436740, "Rudder_ParseDef", setup)


def test_create():
    run(emu, 0x436950, "RainEffect_Create", lambda e: (task(e, 0x88, RAIN),))


@pytest.mark.parametrize("r", [0, 0x7000])
@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("value", [2.5, 3.5, -7.4, 100.0])
def test_run(r, has, value):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        return (rain(e, r),)
    run(emu, 0x436990, "RainEffect_Run", setup)


@pytest.mark.parametrize("id_", [-1, 0, 3])
@pytest.mark.parametrize("handled", [0, 1])
def test_delete(id_, handled):
    def setup(e):
        RET[0x4F1050] = id_
        if not handled:
            set_handler(e, 0x25, RAIN, 0)
        return (rain(e, 0x7000),)
    run(emu, 0x4369E0, "RainEffect_Delete", setup)


def test_get_params():
    def setup(e):
        out = e.alloc(4)
        return (0, out)
    run(emu, 0x436A40, "RainEffect_GetParams", setup)


@pytest.mark.parametrize("r", [0, 0x7000])
@pytest.mark.parametrize("levels", [(0.25, 0.25), (0.5, 0.25), (float("nan"), 0.25)])
@pytest.mark.parametrize("unk24", [0, 1, 0x80])
def test_changed(r, levels, unk24):
    def setup(e):
        RET[0x498960] = 0x7100
        return (rain(e, r, *levels, unk24=unk24),)
    run(emu, 0x436A50, "RainEffect_Changed", setup)
