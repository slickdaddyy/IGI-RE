import random

import pytest

from emu import Emu
from test_fxobj_util import RET, d64, f32, floatfake, logfake, run, set_events, task, u32, vfake

ROT = 0x69

emu = Emu()
set_events(emu)
for a, n in ((0x4F1320, 1), (0x4F1340, 1), (0x4F1400, 2), (0x4F16C0, 1), (0x4C5710, 3), (0x4C7560, 2),
             (0x401B20, 1), (0x4C1830, 1), (0x4015F0, 1), (0x4C48D0, 2)):
    logfake(emu, a, n)
floatfake(emu, 0x4F16E0, 2)  # VarString_EvalReal
floatfake(emu, 0x4D0950, 1)  # VirModel_GetRadius
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
logfake(emu, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
vfake(emu, 0x4C6940, 4, show=lambda e, a: (a[0], e.normalize(a[1]), e.read(a[2], 24), a[3]))  # DynCube_Move
logfake(emu, 0x4E6AB0, 1, ret=0x7500)
logfake(emu, 0x4CEC10, 1, strings=(0,), ret=0x7600)  # VirModel_Find
logfake(emu, 0x4E6B00, 2, strings=(1,))  # Sound_Play
logfake(emu, 0x401AE0, 1, ret=0x7700)  # TaskRef_Add
emu.baseline = emu.snapshot()


def rotating(e, rng, **f):
    t = task(e, 0x200, ROT)
    d64(e, t + 0x20, *[rng.uniform(-1e4, 1e4) for _ in range(3)])
    u32(e, t + 0x68, 0x9998, 0x7800)
    f32(e, t + 0x70, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, t + 0x94, 5)
    f32(e, t + 0x98, *f.get("scale", (1.0, 2.0, 1.5)))
    e.write(t + 0xF0, b"motor.wav\0")
    e.write(t + 0x110, b"fan_01\0")
    f32(e, t + 0x120, *[rng.uniform(-100, 100) for _ in range(6)])
    e.write(t + 0x138, bytes([f.get("stoppable", 1)]))
    f32(e, t + 0x13C, *f.get("target", [rng.uniform(-100, 100) for _ in range(3)]))
    f32(e, t + 0x148, *f.get("speed", [rng.uniform(-100, 100) for _ in range(3)]))
    f32(e, t + 0x154, *f.get("angle", [rng.uniform(0, 360) for _ in range(3)]))
    f32(e, t + 0x160, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, t + 0x184, 9)
    u32(e, t + 0x1A0, f.get("ref", 0), f.get("params", 0))
    e.write(t + 0x1A8, bytes([f.get("stopped", 0)]))
    return t


@pytest.mark.parametrize("scale", [(1.0, 2.0, 1.5), (3.0, 2.0, 1.0), (1.0, 1.0, 4.0), (float("nan"), 1.0, 2.0)])
def test_create(scale):
    def setup(e):
        RET[0x4D0950] = 3000.5
        return (rotating(e, random.Random(1), scale=scale),)
    run(emu, 0x438C30, "RotatingObject_Create", setup)


@pytest.mark.parametrize("ref", [0, 0x7900])
@pytest.mark.parametrize("params", [0, 0x7A00])
def test_delete(ref, params):
    run(emu, 0x438DA0, "RotatingObject_Delete", lambda e: (rotating(e, random.Random(2), ref=ref, params=params),))


def sound_ref(e, live):
    ref = e.alloc(4)
    u32(e, ref, 0x7B00 if live else 0)
    return ref


@pytest.mark.parametrize("ref", [None, 0, 1])
@pytest.mark.parametrize("play", [0, 0x7C00])
@pytest.mark.parametrize("scale", [(1.0, 2.0, 1.5), (3.0, 2.0, 1.0)])
def test_changed(ref, play, scale):
    def setup(e):
        RET[0x4E6B00] = play
        RET[0x4D0950] = 1234.0
        r = 0 if ref is None else sound_ref(e, ref)
        return (rotating(e, random.Random(3), ref=r, scale=scale),)
    run(emu, 0x4395D0, "RotatingObject_Changed", setup)


@pytest.mark.parametrize("stoppable", [0, 1])
def test_stop(stoppable):
    run(emu, 0x439750, "RotatingObject_Stop", lambda e: (rotating(e, random.Random(4), stoppable=stoppable),))


def test_get_sound_info():
    run(emu, 0x439770, "RotatingObject_GetSoundInfo",
        lambda e: (rotating(e, random.Random(5), params=0x7A00), e.alloc(12)))


def test_get_params():
    run(emu, 0x439790, "RotatingObject_GetParams", lambda e: (0, e.alloc(4)))


SPEEDS = [
    dict(target=(10.0, -10.0, 0.0), speed=(10.0, -10.0, 0.0)),
    dict(target=(10.0, -10.0, 5.0), speed=(9.99, -9.99, 6.0)),
    dict(target=(10.0, -10.0, 5.0), speed=(-50.0, 50.0, float("nan"))),
    dict(target=(100.0, 100.0, 100.0), speed=(99.0, 100.5, 0.0)),
]


@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("speeds", SPEEDS)
@pytest.mark.parametrize("cond", [(0, 0.0), (1, 0.0), (1, 1.0), (1, 256.0)])
@pytest.mark.parametrize("stopped", [0, 1])
@pytest.mark.parametrize("params", [0, 1])
def test_run(seed, speeds, cond, stopped, params):
    def setup(e):
        rng = random.Random(seed)
        RET[0x4F16C0], RET[0x4F16E0] = cond
        p = 0
        if params:
            p = e.alloc(8)
        angle = rng.choice([(359.9, 0.0, 720.5), (10.0, 359.999, 1e4), (0.0, 0.0, 0.0), (-5.0, 400.0, 359.0)])
        t = rotating(e, rng, stopped=stopped, params=p, angle=angle, **speeds)
        f32(e, t + 0x120, *[rng.choice([1.0, 10.0, 360.0, -5.0, 0.0]) for _ in range(3)])
        f32(e, t + 0x12C, *[rng.uniform(0, 100) for _ in range(3)])
        return (t,)
    run(emu, 0x439090, "RotatingObject_Run", setup)
