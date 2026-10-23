"""Origo (0x520730 - 0x520BB0)."""
import random

import pytest

from emu import compare
from test_litmap_util import (CWS, FISTP_CWS, RET, d64, f32, floatfake, init_emu, logfake, new_emu, rnd_task, run,
                              setup_init, u16, u32)
from test_patrol_util import fake

TYPE = 0x52

e = new_emu()
logfake(e, 0x4CEC10, 1, strings=(0,), ret=0x7700)  # VirModel_Find
logfake(e, 0x4C5710, 3)  # RigidDynCubeObj_Init
fake(e, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
fake(e, 0x4C6940, 4, derefs={2: 24})  # DynCube_Move
logfake(e, 0x4C7560, 2)  # DynCube_Remove
floatfake(e, 0x4D0950, 1)  # VirModel_GetRadius
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
e.baseline = e.snapshot()


def origo(e, rng):
    o = rnd_task(e, 0x130, TYPE, rng)
    w = e.alloc(0xE00)
    u32(e, o + 0xF0, w)
    return o


def test_create():
    def setup(e):
        u32(e, 0xA44344, e.alloc(0xE00))
        return (origo(e, random.Random(1)),)
    run(e, 0x520730, "Origo_Create", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("scale", [(1, 2, 3), (3, 2, 1), (2, 3, 1), (1, 1, 1), (-1, -5, -2), (float("nan"), 1, 2),
                                   (2, float("nan"), 1), (2, 1, float("nan")), (0.1, 0.7, 0.3)])
def test_get_bounds(scale, cw):
    def setup(e):
        RET[0x4D0950] = 12.345678901
        o = origo(e, random.Random(2))
        f32(e, o + 0x98, *scale)
        out = e.alloc(0x60)
        e.write(out, bytes(range(0x60)))
        return o, out
    run(e, 0x520910, "Origo_GetBounds", setup, cw=cw)


def test_delete():
    run(e, 0x5209E0, "Origo_Delete", lambda e: (origo(e, random.Random(3)),))


def test_get_params():
    def setup(e):
        u32(e, 0xA83850, 0x5151)
        return origo(e, random.Random(4)), e.alloc(4)
    run(e, 0x520A00, "Origo_GetParams", setup)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("placed", [0, 1])
@pytest.mark.parametrize("pos", [(0.0, 0.0, 0.0), (1.5, -2.5, 3.49), (12345.75, -98765.25, 0.5),
                                 (-1e6 - 0.5, 2.0 ** 30 + 0.7, -3.5), (7.0, 8.0, -9.0)])
def test_changed(pos, placed, cw):
    def setup(e):
        o = origo(e, random.Random(5))
        d64(e, o + 0xF8, *pos)
        u32(e, o + 0x128, placed)
        u32(e, o + 0x11C, 100, -200, 301)
        return (o,)
    run(e, 0x520A10, "Origo_Changed", setup, cw=cw)


def test_shutdown():
    def setup(e):
        u32(e, 0xA83850, 0x1234)
        u16(e, 0xA83854, 0x77)
    run(e, 0x520B90, "Origo_Shutdown", setup)


def test_init():
    ie = init_emu()
    compare(ie, 0x5207A0, "Origo_Init", setup=setup_init, check_eax=False, normalized=True)
