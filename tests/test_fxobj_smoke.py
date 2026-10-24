import random

import pytest

from emu import Emu
from test_fxobj_util import (LOG, RET, add_child, d64, f32, floatfake, handler, logfake, pyfake, run, set_events,
                             set_handler, task, u32, vfake)

SMOKE, CHILD = 0x64, 0x65
RANDOM = []

emu = Emu()
set_events(emu)
for a, n in ((0x4F1320, 1), (0x4F1340, 1), (0x4F1400, 2), (0x4F16C0, 1), (0x4F1050, 1), (0x4B4720, 1),
             (0x4015F0, 1), (0x4FE5D0, 2)):
    logfake(emu, a, n)
floatfake(emu, 0x4F16E0, 2)  # VarString_EvalReal
floatfake(emu, 0x4B4770, 1, effect=lambda e, a: RANDOM.pop(0))  # Random_Float
logfake(emu, 0x4FE610, 2, derefs={1: 24})
vfake(emu, 0x4FE210, 12, show=lambda e, a: (a[0], e.read(a[1], 24), e.normalize(a[2])) + tuple(a[3:]))
# Particle2D_New reads a start and an end size
vfake(emu, 0x4FDF00, 13, show=lambda e, a: tuple(a[:3]) + (e.read(a[3], 12), e.read(a[4], 12), e.read(a[5], 8))
      + tuple(a[6:]), ret=0x7300)
logfake(emu, 0x4F0EE0, 0, ret=0x7200)
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
logfake(emu, 0x401CC0, 2, masks={1: 0xFFFF})  # Task_FindAncestor
logfake(emu, 0x4B7FC0, 2, strings=(0,))  # Script_RegisterReal
logfake(emu, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol


def get_name(e):
    e.write(e.arg(0), b"Smoke_7\0")
    if LOG:
        LOG[-1].append(("name", e.normalize(e.arg(1))))
    e.ret(0)


pyfake(emu, 0x4F1270, get_name)
set_handler(emu, 0x25, SMOKE, handler(emu, "deactivate"))
for t in (SMOKE, CHILD):
    set_handler(emu, 1, t, handler(emu, "run_%x" % t))
u32(emu, 0x547F42, 0x66)
for i, a in enumerate((0x57BDC8, 0x57BE44, 0x57BE48, 0x57BE4C)):
    u32(emu, a, 0x6100 + 0x100 * i)
u32(emu, 0x57BE10, 0x6500)
emu.baseline = emu.snapshot()


def smoke(e, rng, **f):
    t = task(e, 0x178, SMOKE)
    d64(e, t + 0x28, *[rng.uniform(-1e5, 1e5) for _ in range(9)])
    f32(e, t + 0x70, *[rng.uniform(-1, 1) for _ in range(3)])
    f32(e, t + 0x7C, rng.uniform(0, 3.14), rng.uniform(-3.14, 3.14), rng.uniform(0, 5), rng.uniform(0, 14),
        rng.uniform(0, 14), rng.uniform(0, 1), rng.uniform(0, 5))
    d64(e, t + 0x98, f.get("rate", rng.uniform(0, 150)))
    f32(e, t + 0xA0, f.get("acc", rng.uniform(0, 30)))
    u32(e, t + 0xF4, f.get("ps", 0x7000))
    u32(e, t + 0xF8, rng.randint(1, 255))
    f32(e, t + 0xFC, rng.uniform(0, 4), rng.uniform(0, 4), *[rng.uniform(0, 4) for _ in range(3)])
    u32(e, t + 0x110, f.get("blend", rng.randint(0, 2)))
    f32(e, t + 0x114, rng.uniform(-150, 150))
    u32(e, t + 0x118, f.get("sprite", rng.randint(0, 3)))
    f32(e, t + 0x11C, rng.uniform(-3, 3), rng.uniform(-3, 3), f.get("u124", 0.5), f.get("u128", 0.5))
    d64(e, t + 0x130, *[rng.uniform(-1e4, 1e4) for _ in range(3)])
    e.write(t + 0x20, bytes([f.get("registered", 0)]))
    e.write(t + 0x170, bytes([f.get("enabled", 1), f.get("moved", 1)]))
    return t


def test_create():
    run(emu, 0x437B90, "Smoke_Create", lambda e: (smoke(e, random.Random(1)),))


@pytest.mark.parametrize("seed", range(6))
def test_move(seed):
    def setup(e):
        rng = random.Random(seed)
        t = smoke(e, rng)
        pos = e.alloc(24)
        d64(e, pos, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
        m = e.alloc(40)
        f32(e, m, *[rng.uniform(-1, 1) for _ in range(9)])
        a = e.alloc(8)
        u32(e, a, pos, m)
        return (t, a)
    run(emu, 0x437AD0, "Smoke_Move", setup)


@pytest.mark.parametrize("registered", [0, 1])
def test_activate(registered):
    run(emu, 0x437CA0, "Smoke_Activate", lambda e: (smoke(e, random.Random(2), registered=registered),))
    run(emu, 0x437D00, "Smoke_Deactivate", lambda e: (smoke(e, random.Random(2), registered=registered),))


@pytest.mark.parametrize("id_", [-1, 0, 3])
@pytest.mark.parametrize("handled", [0, 1])
def test_delete(id_, handled):
    def setup(e):
        RET[0x4F1050] = id_
        if not handled:
            set_handler(e, 0x25, SMOKE, 0)
        return (smoke(e, random.Random(3)),)
    run(emu, 0x437D50, "Smoke_Delete", setup)


def test_get_params():
    run(emu, 0x437DA0, "Smoke_GetParams", lambda e: (0, e.alloc(4)))


@pytest.mark.parametrize("seed", range(10))
@pytest.mark.parametrize("flags", [(1, 1), (0, 1), (1, 0)])
@pytest.mark.parametrize("has", [0, 1])
def test_run(seed, flags, has):
    def setup(e):
        rng = random.Random(seed)
        RET[0x4F16C0] = has
        RET[0x4F16E0] = rng.choice([0.0, 29.99, 30.0, 45.0, 150.0, 1000.0])
        RANDOM[:] = [rng.random() for _ in range(3 * 200)]
        t = smoke(e, rng, enabled=flags[0], moved=flags[1], acc=rng.choice([0.0, 0.01, 15.0, 29.9]),
                  rate=rng.choice([0.0, 30.0, 31.5, 75.0]), u124=rng.choice([0.5, 1.0, 0.01]))
        for i in range(seed % 3):
            add_child(e, t, task(e, 0x40, CHILD if i else SMOKE))
        return (t,)
    run(emu, 0x437DB0, "Smoke_Run", setup)


@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("ps", [0, 0x7000])
@pytest.mark.parametrize("same", [0, 1])
@pytest.mark.parametrize("blend", [0, 1, 2])
@pytest.mark.parametrize("sprite", [0, 1, 2, 3, 7])
@pytest.mark.parametrize("parent", [0, 1])
def test_changed(seed, ps, same, blend, sprite, parent):
    def setup(e):
        rng = random.Random(seed)
        t = smoke(e, rng, ps=ps, blend=blend, sprite=sprite, u124=0.5, u128=0.5 if same else 0.75)
        RET[0x401CC0] = 0
        if parent:
            p = e.alloc(0xF0)
            d64(e, p + 0x20, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
            f32(e, p + 0x70, *[rng.uniform(-1, 1) for _ in range(9)])
            RET[0x401CC0] = p
        return (t,)
    run(emu, 0x438000, "Smoke_Changed", setup)


@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("parent", [0, 1])
def test_update_local(seed, parent):
    def setup(e):
        rng = random.Random(seed)
        t = smoke(e, rng)
        RET[0x401CC0] = 0
        if parent:
            p = e.alloc(0xF0)
            d64(e, p + 0x20, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
            f32(e, p + 0x70, *[rng.uniform(-1, 1) for _ in range(9)])
            RET[0x401CC0] = p
        return (t,)
    run(emu, 0x438240, "Smoke_UpdateLocal", setup)
