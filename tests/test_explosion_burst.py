import struct

import pytest

from emu import ErrorShown
from test_explosion_util import (RET, STATE, add_child, d64, f32, floatfake, handler, logfake, make_emu,
                                 randoms, rot, run, seeded, set_handler, task, trap, u16, u32, unit, vfake)
from test_menu_util import task_system

CWS = [0x37F, 0x7F]
FIREBALL, FRAGMENT, SMOKE, SIMPOINT, CHILD = 0x41, 0x42, 0x43, 0x44, 0x45
EVENT11, DRAW_EVENT = 0x2B, 0x2C

e = make_emu()


def scaled_model(e, a):
    s = STATE.get("scale", (1.0, 2.0, 0.5))
    u32(e, a[0] + 0x6C, a[1])
    f32(e, a[0] + 0x98, *s)
    return 0


def model(e, a):
    return STATE.get("model", 0x6E00)


def write_out(e, a):
    for i, p in enumerate(a):
        u32(e, p, 0x100 + i)
    return 0x99


def blast_msg(e, a):
    """logs the blast message with its position pointer followed"""
    m = e.read(a[1], 0x1C)
    STATE["log"]().append(("blast", m[:4], e.read(e.u32(a[1] + 4), 24), m[8:]))
    return 0


vfake(e, 0x4012A0, 3, show=lambda e, a: (e.normalize(a[0]), a[1] & 0xFFFF, a[2]),
      effect=lambda e, a: task(e, 0x118, a[1] & 0xFFFF))
logfake(e, 0x4015F0, 1)
vfake(e, 0x4FDF00, 13, show=lambda e, a: tuple(a[:3]) + (e.read(a[3], 12), e.read(a[4], 12), e.read(a[5], 8))
      + tuple(a[6:]), effect=lambda e, a: RET.get(0x4FDF00, 0x7300))
vfake(e, 0x4FE120, 9, show=lambda e, a: (a[0], e.read(a[1], 24), e.read(a[2], 12)) + tuple(a[3:]))
logfake(e, 0x4FE5D0, 2)
logfake(e, 0x4FE5E0, 2)
vfake(e, 0x503CC0, 5, show=lambda e, a: (a[0], a[1] & 0xFFFF, e.read(a[2], 24), e.read(a[3], 0x28), e.read(a[4], 12)),
      effect=lambda e, a: task(e, 0x118, FRAGMENT))
logfake(e, 0x503D50, 2)
logfake(e, 0x4E6B00, 2, strings=(1,))
logfake(e, 0x4E6C30, 2, derefs={1: 24})
vfake(e, 0x4CEC10, 1, show=lambda e, a: (e.read_cstr(a[0]),), effect=model)
vfake(e, 0x4C48D0, 2, effect=scaled_model)
floatfake(e, 0x4D0950, 1, effect=lambda e, a: STATE.get("radius", 1.25))
vfake(e, 0x4C6940, 4, show=lambda e, a: (a[0], a[1], e.read(a[2], 24), a[3]),
      effect=lambda e, a: e.write(a[1] + 0x20, e.read(a[2], 24)))
logfake(e, 0x4C1780, 2)
logfake(e, 0x4C5710, 3)
vfake(e, 0x4C7140, 5, show=lambda e, a: (a[0], a[1], e.read(a[2], 24), a[3], a[4]))
logfake(e, 0x4D0D90, 1)
logfake(e, 0x4D0DA0, 2)
logfake(e, 0x4D0DC0, 3)
vfake(e, 0x4DAA20, 1, show=lambda e, a: (), effect=write_out)
vfake(e, 0x4DA180, 2, show=lambda e, a: (), effect=write_out)
logfake(e, 0x4CFC30, 0, ret=1)
u32(e, 0xA94E84 + 4, trap(e, "render", lambda e: (STATE["log"]().append(("render", e.read(e.arg(0), 20))),
                                                  e.ret(0))))
for t in (0x60, 0x61, 0x62, CHILD):
    set_handler(e, EVENT11, t, handler(e, "blast_%x" % t, 2, masks={1: 0}, effect=blast_msg))
    set_handler(e, DRAW_EVENT, t, handler(e, "draw_%x" % t, 2, derefs={1: 0x40}))
set_handler(e, 1, SIMPOINT, handler(e, "simpoint_run", 1))
e.write(0x53954B, bytes([EVENT11]))
e.write(0x547F3E, bytes([DRAW_EVENT]))
u16(e, 0xA7A62A, SIMPOINT)
u16(e, 0x5C1154, 0x40)
u16(e, 0x5C1168, FIREBALL)
u16(e, 0x5C1156, FRAGMENT)
u16(e, 0x5C115C, SMOKE)
u32(e, 0x5C1158, 0x6400)
u32(e, 0x5C1160, 0x6500)
u32(e, 0x5C1164, 0x6600)
e.baseline = e.snapshot()


def _log():
    from test_menu_util import LOG
    return LOG[-1] if LOG else []


STATE["log"] = _log


def fval(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(3))
def test_smoke_emit(seed, cw):
    def setup(e):
        rng = seeded(seed)
        randoms(rng)
        s = task(e, 0x48, SMOKE)
        d64(e, s + 0x28, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
        sz = e.alloc(8)
        f32(e, sz, rng.uniform(1, 500), rng.uniform(1, 500))
        return (s, rng.choice((0x1000008, 0x1000010)), sz, fval(rng.uniform(0, 1)), rng.randint(0, 50),
                rng.randint(0, 50), fval(rng.uniform(0, 3)))
    run(e, 0x47EAD0, "ExplosionSmoke_Emit", setup, cw=cw, check_eax=True)


def fragment(e, rng, kind, **kw):
    f = task(e, 0x118, FRAGMENT)
    d64(e, f + 0x20, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
    f32(e, f + 0xF0, *[rng.uniform(-80, 80) * kw.get("speed", 1) for _ in range(3)])
    u32(e, f + 0xFC, kw.get("life", rng.randint(0, 100)), kw.get("start", rng.randint(1, 100)), kind, 0x7310, 0x7320)
    f32(e, f + 0x110, kw.get("size", rng.uniform(0.01, 0.5)))
    return f


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("kind", [0, 1, 2, 3])
@pytest.mark.parametrize("seed", range(2))
def test_fragment_init_smoke(seed, kind, cw):
    def setup(e):
        rng = seeded(seed)
        return (fragment(e, rng, kind), fval(rng.uniform(1, 400)))
    run(e, 0x47EBD0, "Fragment_InitSmoke", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("kind", [0, 1, 2])
@pytest.mark.parametrize("seed", range(8))
def test_fragment_smoke(seed, kind, cw):
    def setup(e):
        rng = seeded(seed)
        randoms(rng, 400)
        size = rng.uniform(0.0005, 0.004) if seed % 2 else rng.uniform(0.01, 0.4)
        speed = 0 if seed == 6 else 1
        life, start = [(60, 100), (80, 100), (90, 100), (55, 100), (151, 200), (9, 10), (10, 100), (75, 100)][seed]
        return (fragment(e, rng, kind, size=size, speed=speed, life=life, start=start),)
    run(e, 0x47ED50, "Fragment_Smoke", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("kind", [0, 1, 2])
@pytest.mark.parametrize("seed", range(2))
def test_fragment_new(seed, kind, cw):
    def setup(e):
        rng = seeded(seed)
        randoms(rng)
        ex = task(e, 0x88, 0x40)
        g = e.alloc(0x40)
        u32(e, g + 0x20, rng.randint(1, 90))
        u32(e, ex + 0x80, g)
        pos, vel = e.alloc(24), e.alloc(12)
        d64(e, pos, 1.0, 2.0, 3.0)
        f32(e, vel, 4.0, 5.0, 6.0)
        return (ex, pos, vel, kind, fval(rng.uniform(1, 400)))
    run(e, 0x47F0D0, "Fragment_New", setup, cw=cw, check_eax=True)


def test_smoke_new():
    def setup(e):
        pos = e.alloc(24)
        d64(e, pos, 1.0, 2.0, 3.0)
        return (pos, fval(3.5), 77)
    run(e, 0x47F1B0, "ExplosionSmoke_New", setup, check_eax=True)


def explosion(e, rng, fireballs, fragments, blast, size):
    ex = task(e, 0x88, 0x40)
    d64(e, ex + 0x20, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
    u32(e, ex + 0x38, 3, 5, 0)
    f32(e, ex + 0x44, *unit(rng))
    f32(e, ex + 0x50, blast, size, rng.uniform(0, 100), rng.uniform(0, 1.6), rng.uniform(100, 300),
        rng.uniform(300, 1400))
    u32(e, ex + 0x68, fragments, fireballs)
    e.write(ex + 0x70, b"explo_03_l\0")
    g = e.alloc(0x40)
    u32(e, g + 0x20, 33)
    u32(e, ex + 0x80, g, 0x5151)
    return ex


BURSTS = [(0, 0, 0.0, 300.0), (1, 0, 20480.0, 300.0), (3, 2, 10240.0, 250.0), (2, 4, 0.0, 512.0),
          (1, 1, 1.0, 0.0), (5, 0, 4096.0, 100.0), (0, 3, 0.0, 1000.0), (2, -1, 5.0, 10.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(BURSTS)))
def test_burst(case, cw):
    def setup(e):
        rng = seeded(case)
        randoms(rng, 200)
        fireballs, fragments, blast, size = BURSTS[case]
        arr = e.alloc(8 + 12)
        u32(e, arr, 3, 0, task(e, 0x40, 0x60), task(e, 0x40, 0x61), task(e, 0x40, 0x62))
        u32(e, 0x57BAA8, arr)
        STATE["scale"] = (1.0, 2.0, 0.5)
        RET[0x4E6B00] = 0x7700 if case % 2 else 0
        return (explosion(e, rng, fireballs, fragments, blast, size),)
    run(e, 0x47F200, "Explosion_Burst", setup, cw=cw)


SCALES = [(1.0, 2.0, 0.5), (3.0, 2.0, 0.5), (1.0, 2.0, 4.5), (2.0, 2.0, 2.0), (1.0, 0.5, 0.5), (0.5, 1.0, 1.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("scale", SCALES)
def test_fireball_new(scale, cw):
    def setup(e):
        STATE["scale"] = scale
        STATE["model"] = 0x6E00
        STATE["radius"] = 1.75
        pos = e.alloc(24)
        d64(e, pos, 1.0, 2.0, 3.0)
        return (pos, 0x54093C, fval(123.4), 75, 3, 60, 9, 70, fval(0.25), fval(0.5))
    run(e, 0x480060, "FireBall_New", setup, cw=cw, check_eax=True)


def test_fireball_new_error():
    msgs = []
    for target in (0x480060, e.symbols["FireBall_New"]):
        e.reset()
        STATE["model"] = 0
        pos = e.alloc(24)
        with pytest.raises(ErrorShown) as err:
            e.call(target, pos, 0x54093C, 0, 75, 0, 60, 0, 75, 0, 0)
        msgs.append(str(err.value))
    STATE["model"] = 0x6E00
    assert msgs[0] == msgs[1]


init = make_emu()
logfake(init, 0x4169D0, 1, strings=(0,), ret=0x6100)
logfake(init, 0x416930, 1, strings=(0,), ret=0x6200)
init.baseline = init.snapshot()


def test_init():
    def setup(e):
        task_system(e)
        for a, v in ((0x539547, 0x24), (0x539548, 0x25), (0x53954D, 0x26), (0x54127C, 0x27)):
            e.write(a, bytes([v]))
        root = e.call(0x4018F0)[0] & 0xFFFF
        u16(e, 0x547F42, e.call(0x401900, 0x540970, 0xF0, root, 0)[0])
        u16(e, 0xA7A62A, e.call(0x401900, 0x540958, 0xA0, root, 0)[0])
    run(init, 0x4801B0, "Explosion_Init", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("ps2", [0, 0x7330])
@pytest.mark.parametrize("timer", [-1, 0, 1, 10, 23, 26, 27, 29, 30])
def test_smoke_run(timer, ps2, cw):
    def setup(e):
        rng = seeded(timer)
        randoms(rng)
        s = task(e, 0x48, SMOKE)
        u32(e, s + 0x20, timer, 30)
        d64(e, s + 0x28, 1.0, 2.0, 3.0)
        f32(e, s + 0x40, 77.5)
        u32(e, s + 0x44, ps2)
        RET[0x4FDF00] = 0x7340
        return (s,)
    run(e, 0x480420, "ExplosionSmoke_Run", setup, cw=cw)


@pytest.mark.parametrize("kind", [0, 1])
def test_fragment_run(kind):
    def setup(e):
        rng = seeded(kind)
        randoms(rng)
        return (fragment(e, rng, kind, size=0.4, life=0, start=10),)
    run(e, 0x4805B0, "Fragment_Run", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("scale", SCALES)
def test_fireball_create(scale, cw):
    def setup(e):
        STATE["radius"] = 3.25
        fb = task(e, 0x118, FIREBALL)
        f32(e, fb + 0x98, *scale)
        u32(e, fb + 0x6C, 0x6E00)
        return (fb,)
    run(e, 0x4805F0, "FireBall_Create", setup, cw=cw)


@pytest.mark.parametrize("case", [(3, 10, 20, 1, 5), (0, 10, 20, 1, 5), (0, 0, 20, 0, 5), (0, 13, 40, 1, 7),
                                  (-2, 0, 20, 1, 5), (0, 5, 20, 1, 1)])
def test_fireball_run(case):
    delay, life, frames, animated, n = case

    def setup(e):
        fb = task(e, 0x118, FIREBALL)
        m = e.alloc(4)
        u32(e, m, 0x6E10)
        u32(e, fb + 0x6C, m)
        u32(e, fb + 0xF0, 7, life, frames, delay, 0, 0, 33)
        RET[0x4D0D90] = animated
        RET[0x4D0DA0] = n
        return (fb,)
    run(e, 0x4806E0, "FireBall_Run", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
def test_fireball_draw(seed, cw):
    def setup(e):
        rng = seeded(seed)
        fb = task(e, 0x118, FIREBALL)
        pos = [rng.uniform(-1e5, 1e5) for _ in range(3)]
        d64(e, fb + 0x20, *pos)
        if seed == 1:
            d64(e, 0xBCAB08, *pos)
        elif seed == 2:
            d64(e, 0xBCAB08, pos[0], pos[1], pos[2] + 1000.0)
        else:
            d64(e, 0xBCAB08, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
        m = e.alloc(4)
        u32(e, m, 0x6E10)
        u32(e, fb + 0x6C, m)
        f32(e, fb + 0x70, *rot(rng))
        u32(e, fb + 0x94, rng.randint(0, 5))
        u32(e, fb + 0xF0, rng.choice((-1, 3)), rng.randint(0, 60), 60, 0 if seed != 5 else 2, 0, 0, 30)
        f32(e, fb + 0x10C, rng.uniform(0, 3), rng.uniform(-3, 3), rng.uniform(-0.2, 0.2))
        for t in (CHILD, 0x70, CHILD):
            c = task(e, 0x40, t)
            add_child(e, fb, c)
        return (fb,)
    run(e, 0x480790, "FireBall_Draw", setup, cw=cw)


