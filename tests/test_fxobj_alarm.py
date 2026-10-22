import math
import random

import pytest

from emu import Emu, ErrorShown
from test_fxobj_util import (LOG, RET, add_child, d64, f32, floatfake, handler, logfake, pyfake, run, set_events,
                             set_handler, task, u32, vfake)

REFL, CHILD, BASE = 0x6A, 0x6B, 0x6C
DATA = {}

emu = Emu()
set_events(emu)
logfake(emu, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
logfake(emu, 0x4C5710, 3)  # RigidDynCubeObj_Init
floatfake(emu, 0x4D0950, 1)  # VirModel_GetRadius
logfake(emu, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
logfake(emu, 0x4C4950, 3)
for t in (REFL, CHILD):
    set_handler(emu, 1, t, handler(emu, "run_%x" % t))
u32(emu, 0x547F42, BASE)
set_handler(emu, 4, BASE, handler(emu, "draw", 2))


def point(e):
    out, obj, i = e.arg(0), e.arg(1), e.arg(2)
    if LOG:
        LOG[-1].append(("point", e.normalize(obj), i))
    d64(e, out, *DATA["point"])
    e.ret(0)


def project(e):
    scr, w, p = e.arg(0), e.arg(1), e.arg(2)
    if LOG:
        LOG[-1].append(("project", e.read(p, 24)))
    f32(e, scr, *DATA["scr"])
    f32(e, w, DATA["w"])
    e.ret(0)


def scale(e, a):
    f32(e, a[0], 6144.0 * 0.25, 6144.0 * 0.3)


pyfake(emu, 0x414E20, point)
pyfake(emu, 0x497F20, project)
vfake(emu, 0x498040, 3, show=lambda e, a: (e.read(a[1], 8), a[2], a[0] == a[1]), effect=scale)
vfake(emu, 0x4B53B0, 17, show=lambda e, a: (e.normalize(a[0]),) + tuple(a[1:]))  # QVertex_DrawQuadEx
emu.baseline = emu.snapshot()


def light(e, rng, on=1, destroyed=0):
    l = e.alloc(0x1F8)
    d64(e, l + 0x20, *[rng.uniform(-1e4, 1e4) for _ in range(3)])
    f32(e, l + 0x70, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, l + 0x94, 11)
    f32(e, l + 0x108, *[rng.uniform(-1, 1) for _ in range(9)])
    e.write(l + 0x1A0, bytes([on, destroyed]))
    return l


def reflector(e, rng, **f):
    t = task(e, 0x140, REFL)
    d64(e, t + 0x20, *[rng.uniform(-1e4, 1e4) for _ in range(3)])
    f32(e, t + 0x98, *f.get("scale", (1.0, 2.0, 1.5)))
    f32(e, t + 0x108, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, t + 0x12C, 4)
    u32(e, t + 0x130, light(e, rng, f.get("on", 1), f.get("destroyed", 0)))
    f32(e, t + 0x138, f.get("angle", rng.uniform(0, 100)))
    return t


def create_arg(e, rng):
    a = e.alloc(0x90)
    m = e.alloc(16)
    e.write(m, b"alarm_01\0")
    u32(e, a, m, 0x9998)
    d64(e, a + 8, *[rng.uniform(-1e4, 1e4) for _ in range(6)])
    f32(e, a + 0x38, *[rng.uniform(-1, 1) for _ in range(20)])
    u32(e, a + 0x88, 0x7D00)
    return a


@pytest.mark.parametrize("scale", [(1.0, 2.0, 1.5), (3.0, 2.0, 1.0)])
def test_create(scale):
    def setup(e):
        RET[0x4CEC10] = 0x7600
        RET[0x4D0950] = 512.25
        rng = random.Random(1)
        t = reflector(e, rng, scale=scale)
        return (t, create_arg(e, rng))
    run(emu, 0x439A50, "AlarmLightReflector_Create", setup)


def test_create_no_model():
    def setup(e):
        rng = random.Random(1)
        return (reflector(e, rng), create_arg(e, rng))
    for fn in (0x439A50, "AlarmLightReflector_Create"):
        emu.reset()
        RET.clear()
        args = setup(emu)
        with pytest.raises(ErrorShown, match='VirModel "%s" not available'):
            emu.call(fn, *args)


@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("state", [(1, 0), (0, 0), (1, 1)])
@pytest.mark.parametrize("children", [0, 2])
def test_run(seed, state, children):
    def setup(e):
        rng = random.Random(seed)
        t = reflector(e, rng, on=state[0], destroyed=state[1])
        for i in range(children):
            add_child(e, t, task(e, 0x40, CHILD if i else REFL))
        return (t,)
    run(emu, 0x439B70, "AlarmLightReflector_Run", setup)


@pytest.mark.parametrize("state", [(1, 0), (0, 0), (1, 1), (0, 1)])
def test_draw(state):
    def setup(e):
        rng = random.Random(3)
        DATA.update(point=(10.0, 20.0, 30.0), scr=(320.0, 240.0), w=0.5)
        d64(e, 0xBCAB08, 0.0, 0.0, 0.0)
        return (reflector(e, rng, on=state[0], destroyed=state[1]), 0x4321)
    run(emu, 0x439EC0, "AlarmLightReflector_Draw", setup)


@pytest.mark.parametrize("seed", range(20))
@pytest.mark.parametrize("w", [0.5, 0.0, -1.0, 2.0])
def test_draw_flare(seed, w):
    def setup(e):
        rng = random.Random(seed)
        DATA.update(point=[rng.uniform(-100, 100) for _ in range(3)], scr=(rng.uniform(0, 640), rng.uniform(0, 480)),
                    w=w)
        d64(e, 0xBCAB08, *[rng.uniform(-1e4, 1e4) for _ in range(3)])
        angle = rng.choice([0.0, 1.0, math.pi, 10.0, 123.456, -7.5, rng.uniform(0, 1000)])
        return (reflector(e, rng, angle=angle), rng.randint(0, 3))
    run(emu, 0x439F10, "AlarmLightReflector_DrawFlare", setup)
