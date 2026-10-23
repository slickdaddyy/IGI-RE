"""AnimRigidDynCubeObj, AnimScaleTask and EditHazinglayer_Create (0x52AAF0 - 0x52AFE0)."""
import random

import pytest

from emu import Emu
from test_animcam_util import (CWS, FISTP_CWS, NAN, RET, compare_open, d64, f32, floatfake, logfake, open_emu, run,
                               task, u16, u32)

AOBJ, SCALE = 0x81, 0x82

e = Emu()
floatfake(e, 0x4D0950, 1)  # VirModel_GetRadius
logfake(e, 0x4C6940, 4, derefs={2: 24})  # DynCube_Move
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(e, 0x4F16C0, 1, ret=0)  # VarString_HasExpr
floatfake(e, 0x4F16E0, 2)  # VarString_EvalReal
logfake(e, 0x4F3EF0, 2)  # AnimTask_Seek
e.baseline = e.snapshot()


@pytest.mark.parametrize("index", [-5, 0, 13, 21, 22])
def test_obj_get_input_info(index):
    def setup(e):
        msg = e.alloc(8)
        u32(e, msg, index, 0x5555)
        return 0x1234, msg
    run(e, 0x52ABD0, "AnimObj_GetInputInfo", setup)


def test_obj_get_state_size():
    run(e, 0x52AC00, "AnimObj_GetStateSize", lambda e: (0x1234, e.alloc(4)))


def anim_obj(e, rng, scale=(1.0, 2.0, 1.5)):
    o = task(e, 0x158, AOBJ)
    e.write(o + 0x20, bytes(rng.randrange(256) for _ in range(0x138)))
    u32(e, o + 0x68, 0x6868, 0x6C6C)
    f32(e, o + 0x98, *scale)
    return o


def test_obj_save_state():
    def setup(e):
        rng = random.Random(1)
        o = anim_obj(e, rng)
        buf, pos = e.alloc(0x40), e.alloc(24)
        msg = e.alloc(12)
        u32(e, msg, buf, 0x77, pos)
        return o, msg
    run(e, 0x52AC10, "AnimObj_SaveState", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("scale", [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 1.5, 2.0), (2.0, 2.0, 2.0),
                                   (NAN, 1.0, 2.0), (1.0, NAN, 2.0), (1.0, 2.0, NAN), (0.3, 0.1, 0.2)])
def test_obj_load_state(scale, cw):
    def setup(e):
        rng = random.Random(2)
        RET[0x4D0950] = 12.345678901
        o = anim_obj(e, rng, scale)
        buf = e.alloc(0x40)
        d64(e, buf, 1.5, -2.5, 3.5)
        f32(e, buf + 0x18, *[rng.uniform(-1, 1) for _ in range(9)])
        u32(e, buf + 0x3C, 9)
        msg = e.alloc(8)
        u32(e, msg, buf, 0x99)
        return o, msg
    run(e, 0x52AC50, "AnimObj_LoadState", setup, cw=cw)


def test_obj_close():
    run(e, 0x52AD00, "AnimObj_Close", lambda e: u32(e, 0x54F498, 0x12340081))


def scale_task(e, rng):
    t = task(e, 0x20168, SCALE)
    e.write(t + 0x20, bytes(rng.randrange(256) for _ in range(0x100)))
    e.write(t + 0x200F0, bytes(rng.randrange(256) for _ in range(0x78)))
    u32(e, t + 0x20154, 41)
    return t


def test_scale_create():
    def setup(e):
        lst = 0xA76C28
        u32(e, lst, lst + 4, 0, lst)
        return (scale_task(e, random.Random(3)),)
    run(e, 0x52AE50, "AnimScaleTask_Create", setup)


RUN = [
    (0, None, 1, 1.0, 0.0),
    (0, None, 0, 1.0, 0.0),
    (1, 0.4, 0, 1.0, 0.0),
    (1, 0.6, 0, 1.0, 0.0),
    (1, 256.0, 0, 1.0, 0.0),
    (1, -1.5, 0, 1.0, 0.0),
    (0, None, 1, 0.05, 0.04),
    (0, None, 1, 0.05, 0.05),
    (0, None, 1, 0.3, 0.07),
    (0, None, 1, 0.1, 0.0),
    (0, None, 1, 2.5, 0.0999),
    (0, None, 1, -0.5, 0.0),
    (0, None, 1, NAN, 0.0),
    (0, None, 1, 0.1, 0.2),
]


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("case", range(len(RUN)))
def test_scale_run(case, cw):
    expr, value, is_run, speed, frac = RUN[case]

    def setup(e):
        RET[0x4F16C0] = expr
        RET[0x4F16E0] = value or 0.0
        t = scale_task(e, random.Random(4))
        e.write(t + 0x200F4, bytes([is_run]))
        f32(e, t + 0x20160, speed, frac)
        return (t,)
    run(e, 0x52AF10, "AnimScaleTask_Run", setup, cw=cw)


def test_scale_close():
    def setup(e):
        u32(e, 0xA84394, 0x9494)
        u16(e, 0x54F68C, SCALE)
    run(e, 0x52AFB0, "AnimScaleTask_Close", setup)


def test_hazinglayer_create():
    def setup(e):
        h = task(e, 0x88, 0x83)
        e.write(h + 0x20, bytes(range(0x68)))
        return (h,)
    run(e, 0x52AFE0, "EditHazinglayer_Create", setup)


@pytest.mark.parametrize("orig,ours", [(0x52AAF0, "AnimObj_Open"), (0x52AD10, "AnimScaleTask_Open")])
def test_open(orig, ours):
    compare_open(open_emu(), orig, ours)
