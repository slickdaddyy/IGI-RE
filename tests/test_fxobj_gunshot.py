import math
import random

import pytest

from emu import Emu
from test_fxobj_util import (LOG, RET, d64, f32, floatfake, logfake, logger, pyfake, run, set_events, set_handler,
                             task, trap, u32, vfake)

HIT, EXPL, BASE = 0x66, 0x67, 0x68
RANDOM = []
ARGS = {}

emu = Emu()
set_events(emu)
logfake(emu, 0x4D9610, 0, ret=BASE)
logfake(emu, 0x401CF0, 2, masks={0: 0xFFFF, 1: 0xFFFF})  # TaskType_IsA
logfake(emu, 0x416D20, 0, ret=0x5000)  # Game_GetRandom
logfake(emu, 0x4B47C0, 2, ret=2)  # Random_Int
logfake(emu, 0x4B4740, 1)  # Random_Next
floatfake(emu, 0x4B4770, 1, effect=lambda e, a: RANDOM.pop(0))  # Random_Float
vfake(emu, 0x4810A0, 20, show=lambda e, a: (e.read(a[0], 24),) + tuple(a[1:10]) + (e.read_cstr(a[10]),
                                                                                    e.read_cstr(a[11]))
      + tuple(a[12:15]) + (e.read(a[15], 12),) + tuple(a[16:]))
vfake(emu, 0x4FDF00, 13, show=lambda e, a: tuple(a[:3]) + (e.read(a[3], 12), e.read(a[4], 12), e.read(a[5], 8))
      + tuple(a[6:]), ret=0x7300)
vfake(emu, 0x4FE120, 9, show=lambda e, a: (a[0], e.read(a[1], 24), e.read(a[2], 12)) + tuple(a[3:]))
logfake(emu, 0x4F0EE0, 0, ret=0x7200)
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld


def decal(e):
    t, a = e.arg(0), e.arg(1)
    if LOG:
        LOG[-1].append(("decal", e.normalize(t), e.read(a, 16)))
    e.ret(0)


set_handler(emu, 0x2F, HIT, trap(emu, "decal", decal))
# parsing
logfake(emu, 0x4B8A20, 2, effect=lambda e, a: ARGS.get(a[1], 0x1234))  # Script_GetArgInt
floatfake(emu, 0x4B8A50, 2, effect=lambda e, a: ARGS.get(a[1], 2.5))  # Script_GetArgReal


def get_string(e):
    args, i, out, n = (e.arg(k) for k in range(4))
    s = ARGS.get(i, b"str%d" % i)[: n - 1]
    e.write(out, s + b"\0")
    if LOG:
        LOG[-1].append(("str", i, n))
    e.ret(0)


pyfake(emu, 0x4B8A80, get_string)
logfake(emu, 0x416930, 1, strings=(0,), ret=0x6200)  # Game_AddTexture
# aiming


def point(e):
    out, obj, i = e.arg(0), e.arg(1), e.arg(2)
    if LOG:
        LOG[-1].append(("point", e.normalize(obj), i))
    d64(e, out, *ARGS["point"])
    e.ret(0)


pyfake(emu, 0x414E20, point)
logfake(emu, 0x448550, 0, ret=BASE)
set_handler(emu, 0, BASE, trap(emu, "base_create", logger(emu, "base_create", 2)))
emu.baseline = emu.snapshot()


def gdef(e, dust=1, debris=1, tex=0x6300, tex2=0x6400):
    d = e.alloc(0x60)
    e.write(d, bytes([dust, debris]))
    e.write(d + 0x32, b"spark.wav\0")
    f32(e, d + 0x54, 2048.5)
    u32(e, d + 0x58, tex, tex2)
    return d


def impact_arg(e, rng, task_=1, exit_=0, **kw):
    hit = e.alloc(0x50)
    if task_:
        u32(e, hit, task(e, 0x40, HIT))
    d64(e, hit + 8, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
    f32(e, hit + 0x38, *[rng.uniform(-1, 1) for _ in range(3)])
    a = e.alloc(0x18)
    u32(e, a, hit, gdef(e, **kw), 0x11, 0x22, 0x33, exit_)
    return a


@pytest.mark.parametrize("seed", range(3))
@pytest.mark.parametrize("task_", [0, 1])
@pytest.mark.parametrize("excluded", [0, 1])
@pytest.mark.parametrize("flags", [(1, 1), (0, 1), (1, 0), (0, 0)])
@pytest.mark.parametrize("exit_", [0, 1])
@pytest.mark.parametrize("tex", [(0x6300, 0x6400), (0, 0x6400), (0x6300, 0), (0, 0)])
@pytest.mark.parametrize("odd", [0, 1])
def test_impact(seed, task_, excluded, flags, exit_, tex, odd):
    def setup(e):
        rng = random.Random(seed)
        RET[0x401CF0] = excluded
        RET[0x4B4740] = 0x1234 + odd
        RANDOM[:] = [rng.random() for _ in range(5)]
        return (0, impact_arg(e, rng, task_, exit_, dust=flags[0], debris=flags[1], tex=tex[0], tex2=tex[1]))
    run(emu, 0x438440, "GunShotEffect_Impact", setup)


def test_impact_no_handler():
    def setup(e):
        set_handler(e, 0x2F, HIT, 0)
        RANDOM[:] = [0.5] * 5
        return (0, impact_arg(e, random.Random(5)))
    run(emu, 0x438440, "GunShotEffect_Impact", setup)


@pytest.mark.parametrize("strings", [{}, {5: b""}, {6: b""}, {5: b"", 6: b""}, {5: b"x" * 40}])
@pytest.mark.parametrize("ints", [{}, {3: 0x1FF, 4: -1}])
def test_parse_args(strings, ints):
    def setup(e):
        ARGS.clear()
        ARGS.update(strings)
        ARGS.update(ints)
        ARGS[7] = -0.123
        out = e.alloc(0x60)
        a = e.alloc(16)
        u32(e, a, out, 0x4444, 3, 0)
        return (0, a)
    run(emu, 0x4387B0, "GunShotEffect_ParseArgs", setup)


def explosive(e, rng, active=1, range_=0.75):
    t = task(e, 0x2B0, EXPL)
    d64(e, t + 0x20, *[rng.uniform(-1e4, 1e4) for _ in range(3)])
    e.write(t + 0x240, bytes([active]))
    f32(e, t + 0x2A8, range_)
    u32(e, t + 0x2AC, rng.randint(-1, 100))
    return t


def test_get_target_id():
    run(emu, 0x4388A0, "PlaceExplosiveTBA_GetTargetId", lambda e: (explosive(e, random.Random(1)),),
        check_eax=True)


def test_create():
    run(emu, 0x4389A0, "PlaceExplosiveTBA_Create", lambda e: (explosive(e, random.Random(1)), 0x4567))


def test_get_kind():
    run(emu, 0x438BF0, "PlaceExplosiveTBA_GetKind", lambda e: (0, e.alloc(4)))


def test_get_params():
    run(emu, 0x438C00, "PlaceExplosiveTBA_GetParams", lambda e: (0, e.alloc(4)))


def rot(a, b):
    ca, sa, cb, sb = math.cos(a), math.sin(a), math.cos(b), math.sin(b)
    return [ca, sa * cb, sa * sb, -sa, ca * cb, ca * sb, 0.0, -sb, cb]


@pytest.mark.parametrize("seed", range(30))
@pytest.mark.parametrize("active", [0, 1])
@pytest.mark.parametrize("found", [0, 1])
def test_check_aim(seed, active, found):
    def setup(e):
        rng = random.Random(seed)
        t = explosive(e, rng, active, rng.choice([0.75, 3.0, 10.0]))
        ARGS["point"] = [rng.uniform(-2000, 2000) for _ in range(3)]
        a = e.alloc(0x88)
        d64(e, a + 8, *[e_ + rng.uniform(-8000, 8000) for e_ in struct_pos(e, t)])
        f32(e, a + 0x20, *rot(rng.uniform(-3, 3), rng.uniform(-1, 1)))
        f32(e, a + 0x70, rng.choice([0.5, 1.0, 3.0]), rng.choice([0.5, 1.0, 3.0]))
        u32(e, a + 0x80, 0x7777 if found else 0)
        return (t, a)
    run(emu, 0x4389E0, "PlaceExplosiveTBA_CheckAim", setup)


def struct_pos(e, t):
    import struct
    return struct.unpack("<3d", e.read(t + 0x20, 24))
