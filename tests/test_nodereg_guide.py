"""SplinePathGuideQTask (0x51AE50 - 0x51B3A0)."""

import pytest

from emu import Emu
from test_nodereg_util import (CWS, FISTP_CWS, NODE, PATH, RET, d64, f32, handler, logfake, path, rnd, run, set_handler,
                               task, u8, u16, u32)

TARGET, GUIDE = 0x58, 0x59
EV_MOVE, EV_START = 6, 8

emu = Emu()
logfake(emu, 0x4C1790, 2)  # QTaskList_Add
logfake(emu, 0x4C17C0, 2)  # QTaskList_Remove
logfake(emu, 0x4F1030, 1, effect=lambda e, a: RET.get(("id", a[0]), 0))  # Task_FindById
logfake(emu, 0x4C6940, 4, derefs={2: 24})  # DynCube_Move
u16(emu, 0x54ED4C, NODE)
u16(emu, 0x54ED4E, PATH)
u8(emu, 0x54ED52, EV_MOVE)
u8(emu, 0x54ED53, EV_START)
u32(emu, 0xA83760, 0x3760)
set_handler(emu, EV_MOVE, TARGET, handler(emu, "move", 2, derefs={1: 12}))
set_handler(emu, EV_START, TARGET, handler(emu, "start", 2))
emu.baseline = emu.snapshot()


def guide(e, path_id=1, target_id=2, seg=0, flags=1, loop=0, active=1, u=0.5, speed=1.0, start=0.0):
    g = task(e, 0x44, GUIDE)
    u32(e, g + 0x20, path_id, target_id, seg, flags, loop, active)
    f32(e, g + 0x38, u, speed, start)
    return g


def target(e, typ=TARGET):
    t = task(e, 0x100, typ)
    d64(e, t + 0x20, 100.0, 200.0, 300.0)
    u32(e, t + 0x44, 0x40800000)
    f32(e, t + 0x70, 1, 0, 0, 0, 1, 0, 0, 0, 1)
    return t


def world(e, p, t):
    RET[("id", 1)] = p
    RET[("id", 2)] = t
    u32(e, 0xA44344, 0x4344)
    f32(e, 0xA83758, 30.0)


def test_create():
    run(emu, 0x51AE50, "SplinePathGuide_Create", lambda e: (task(e, 0x44, GUIDE),))


def test_delete():
    run(emu, 0x51AE90, "SplinePathGuide_Delete", lambda e: (task(e, 0x44, GUIDE),))


RUNS = [
    dict(flags=0), dict(path_id=-1), dict(path_id=3), dict(target_id=-1), dict(target_id=3),
    dict(), dict(u=0.0), dict(u=0.99), dict(seg=2, u=0.95), dict(seg=2, u=0.95, loop=1), dict(seg=3),
    dict(seg=1, u=0.98, speed=40.0), dict(u=0.3, speed=0.0), dict(speed=float("nan")), dict(u=1.5),
]


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("n", [2, 4])
@pytest.mark.parametrize("kw", RUNS)
def test_run(kw, n, cw):
    def setup(e):
        p, nodes = path(e, rnd(40 + n), n, speeds=[10.0, 20.0, 5.0, 0.0][:n])
        for i, k in enumerate(nodes):
            f32(e, k + 0x98, 50.0 + 10 * i)
        world(e, p, target(e, TARGET if kw.get("u") != 0.0 else 0x5A))
        return (guide(e, **kw),)
    run(emu, 0x51AEB0, "SplinePathGuide_Run", setup, cw=cw)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("n,start,found", [(4, 0.0, 1), (4, 0.3, 1), (4, 0.999, 1), (4, 1.0, 1), (4, 1.5, 1),
                                           (2, 0.5, 1), (4, 0.5, 0), (5, float("nan"), 1)])
def test_set_start(n, start, found, cw):
    def setup(e):
        p, nodes = path(e, rnd(50), n)
        if found:
            RET[("id", 1)] = p
        return (guide(e, start=start, seg=7, u=0.125),)
    run(emu, 0x51B0B0, "SplinePathGuide_SetStart", setup, cw=cw)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("start", [-1.0, 0.0, 0.5, 1.0, 2.0, float("nan")])
@pytest.mark.parametrize("active,speed,target_id", [(0, -1.0, 0), (1, 0.0, 2), (1, 2.0, 3), (0, float("nan"), 2),
                                                    (1, 3.0, 4)])
def test_params_changed(start, active, speed, target_id, cw):
    def setup(e):
        p, nodes = path(e, rnd(60), 4)
        world(e, p, target(e))
        RET[("id", 4)] = target(e, 0x5A)
        u32(e, 0xA8375C, 0x375C)
        return (guide(e, start=start, active=active, speed=speed, target_id=target_id, flags=6 | (1 - active)),)
    run(emu, 0x51B2C0, "SplinePathGuide_ParamsChanged", setup, cw=cw)
