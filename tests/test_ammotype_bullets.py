"""GunShot_MoveBullets (0x47B750): bullet flight, hits, ricochets and trails."""
import random
import struct

import pytest

from emu import Emu
from test_comprun_util import run as run_cw
from test_menu_util import LOG, RET, f32, logfake, pyfake, set_handler, task, trap, u16, u32
from test_vehicle_util import d64, floatfake

EV7, EV9 = 0x30, 0x31
SHOT_T, WEAPON_T, HIT_T, PLAIN_T = 0x66, 0x68, 0x6A, 0x6B

emu = Emu()


def coll_lines(e):
    world, res, starts, ends, count, ev, flt, flags = [e.arg(i) for i in range(8)]
    a = struct.unpack("<3d", e.read(starts, 24))
    b = struct.unpack("<3d", e.read(ends, 24))
    LOG[-1].append(("coll", world, e.read(starts, 24), e.read(ends, 24), count, ev & 0xFF, e.read(flt + 4, 16), flags))
    hits = RET.get("hits", [])
    h = hits.pop(0) if hits else None
    rec = bytearray(0x70)
    if h is None:
        struct.pack_into("<II3d", rec, 0, 0, 0x55, *b)
        struct.pack_into("<I", rec, 0x58, 0)
    else:
        t = h["t"]
        pos = [a[k] + t * (b[k] - a[k]) for k in range(3)]
        dist = t * sum((b[k] - a[k]) ** 2 for k in range(3)) ** 0.5 + h.get("ddist", 0.0)
        struct.pack_into("<II3d", rec, 0, h.get("task", 0), 0x77, *pos)
        rec[0x20:0x38] = bytes((i * 7 + 3) & 0xFF for i in range(0x18))
        struct.pack_into("<3f", rec, 0x38, *h["normal"])
        struct.pack_into("<IIIIIIId", rec, 0x44, 0x1111, 0x2222, h.get("mat", 0), h.get("back", 0), 0x3333, 1, 0x4444,
                         dist)
        struct.pack_into("<II", rec, 0x68, 0x5555, 0x6666)
    e.write(res, bytes(rec))
    e.ret(0)


def hit_handler(e):
    t, m = e.arg(0), e.arg(1)
    msg = e.read(m, 0x20)
    hit = struct.unpack("<I", msg[:4])[0]
    LOG[-1].append(("hit", t, e.read(hit, 0x70), msg[4:]))
    fx = RET.get("hitfx")
    if fx:
        fx(e, hit)
    e.ret(0)


pyfake(emu, 0x4CC2A0, coll_lines)
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
logfake(emu, 0x4028B0, 0, effect=lambda e, a: RET["ticks"].pop(0) if RET["ticks"] else 777)  # Timer_GetTicks
logfake(emu, 0x416D20, 0, ret=0x1234)  # Game_GetRandom
logfake(emu, 0x4B47C0, 2, effect=lambda e, a: RET["rand"].pop(0) if RET["rand"] else 2)  # Random_Int
floatfake(emu, 0x4B4770, 1, effect=lambda e, a: RET["frand"].pop(0) if RET["frand"] else 0.5)  # Random_Float
logfake(emu, 0x4F0EE0, 0, ret=0x4321)  # Level_GetDynamic
logfake(emu, 0x4E6B00, 2, strings=(1,), effect=lambda e, a: RET.get("snd", 0x7A00))  # Sound_Play
logfake(emu, 0x4E6C30, 2, derefs={1: 24})  # Sound_SetPos
logfake(emu, 0x47C330, 1, derefs={0: 24})  # GunShot_PlayRicochet
logfake(emu, 0x47C4C0, 7, derefs={1: 24, 2: 24})  # GunShot_AddTrail
logfake(emu, 0x4083C0, 5, masks={0: 0xFFFF}, derefs={2: 0x70, 4: 12})  # GameMaterial_Impact
HIT = trap(emu, "handler_hit", hit_handler)
set_handler(emu, EV9, HIT_T, HIT)
emu.write(0x539547, bytes([EV7, 0, EV9]))
emu.baseline = emu.snapshot()


def run(setup, cw):
    return run_cw(emu, 0x47B750, "GunShot_MoveBullets", setup, cw=cw)


def f(v):
    return struct.unpack("<f", struct.pack("<f", v))[0]


def unit(r, zero=False):
    if zero:
        return (0.0, 0.0, 0.0)
    v = [r.uniform(-1, 1) for _ in range(3)]
    n = sum(x * x for x in v) ** 0.5 or 1.0
    return tuple(f(x / n * r.choice([1.0, 1.0, 0.999, 3.7])) for x in v)


def material(e, r, hardness, ricochet, flag):
    m = e.alloc(0x268)
    f32(e, m + 0x40, hardness, ricochet)
    for i in range(6):
        e.write(m + 0x154 + 32 * i, b"impact_%d_%d\0" % (m & 0xFFF, i))
    params, back = e.alloc(0x60), e.alloc(0x60)
    u32(e, params + 0x58, flag)
    u16(e, m + 0x254, r.randrange(0x100, 0x200))
    u32(e, m + 0x258, params)
    u16(e, m + 0x25C, r.randrange(0x200, 0x300))
    u32(e, m + 0x260, back)
    return m


def gunshot(e, tracer=1, gun=True, owner=True, rate=0):
    s = task(e, 0xC38, SHOT_T)
    g = task(e, 0x1B0, WEAPON_T)
    at = e.alloc(0x168)
    u32(e, at + 0x20, tracer)
    u32(e, g + 0x12C, at)
    ref = e.alloc(4)
    u32(e, ref, g if gun else 0)
    u32(e, s + 0x6C, ref)
    if owner:
        o = e.alloc(4)
        u32(e, o, task(e, 0x40, PLAIN_T))
        u32(e, s + 0x70, o)
    u32(e, s + 0x98, rate)
    return s


def bullet(e, s, i, slot, pos, d, speed, pierce=1.0, ricocheted=0, extra=0x44):
    b = s + 0xE0 + 0x48 * i
    u32(e, b, 1, 0x1234)
    d64(e, b + 8, *pos)
    f32(e, b + 0x20, *d)
    u32(e, b + 0x2C, 0x6E00 + i, 0x30 + i)
    f32(e, b + 0x34, speed)
    u32(e, b + 0x38, 0x38 + i)
    f32(e, b + 0x3C, pierce)
    e.write(b + 0x40, bytes([ricocheted, 0x5A]))
    u32(e, b + 0x44, extra + i)
    u32(e, s + 0xA4 + 4 * slot, b)
    u32(e, s + 0x94, max(e.u32(s + 0x94), slot + 1))
    return b


def random_setup(seed):
    def setup(e):
        r = random.Random(seed)
        s = gunshot(e, tracer=r.random() < 0.85, gun=r.random() < 0.9, owner=r.random() < 0.7,
                    rate=r.choice([0, 0, 1, 4, 10]))
        hard = [f(r.uniform(0.5, 5)) for _ in range(3)]
        mats = [material(e, r, hard[k], r.choice([0.0, f(r.uniform(5, 80)), 45.0, 89.0]), r.random() < 0.8)
                for k in range(3)]
        tasks = [task(e, 0x40, HIT_T), task(e, 0x40, PLAIN_T), 0]
        nb = r.randint(1, 4)
        for i in range(nb):
            pos = tuple(r.uniform(-300000, 300000) for _ in range(3))
            speed = r.choice([f(r.uniform(2000, 400000)), f(r.uniform(2000, 400000)), 0.0, -5.0])
            k = r.randrange(3)
            pierce = f(hard[k] + r.choice([0.0, 0.0, 0.5, 1.0, 2.5, -1.0]))
            bullet(e, s, i * 3 + r.randrange(3), i, pos, unit(r), speed,
                   pierce, r.random() < 0.2)
        hits = []
        for _ in range(r.randint(0, 25)):
            kind = r.choice(["none", "front", "front", "front", "back", "both"])
            if kind == "none":
                hits.append(None)
                continue
            m = r.choice(mats)
            h = {"t": r.choice([r.uniform(0.01, 1.0), r.uniform(0.01, 0.3), 1.0]),
                 "normal": unit(r, r.random() < 0.1), "task": r.choice(tasks)}
            if r.random() < 0.15:
                h["ddist"] = r.uniform(-500, 500)
            if kind in ("front", "both"):
                h["mat"] = m
            if kind in ("back", "both"):
                h["back"] = r.choice(mats)
            hits.append(h)
        RET["hits"] = hits
        RET["ticks"] = [r.randrange(0, 2000) for _ in range(200)]
        RET["rand"] = [r.randrange(6) for _ in range(200)]
        RET["frand"] = [r.random() for _ in range(200)]
        RET["snd"] = r.choice([0, 0x7A00])
        u32(e, 0x540838, r.choice([0xFFFFFFFF, r.randrange(0, 2000)]))
        u32(e, 0x5BE3D8, r.randrange(0, 600))
        if r.random() < 0.2:
            RET["hitfx"] = lambda e, h: u32(e, h + 0x58, 0)
        elif r.random() < 0.2:
            RET["hitfx"] = lambda e, h: d64(e, h + 0x60, 12345.678)
        return (s,)
    return setup


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(80))
def test_random(seed, cw):
    run(random_setup(seed), cw)


def simple(hits, frand=(), ricocheted=0, pierce=1.0, speed=100000.0, rate=0, tracer=1, gun=True, normal=None,
           sound=0xFFFFFFFF, ticks=(), delay=8, d=(0.6, 0.8, 0.0), params=1, hardness=1.0, ricochet=30.0):
    def setup(e):
        r = random.Random(1)
        s = gunshot(e, tracer=tracer, gun=gun, rate=rate)
        m = material(e, r, hardness, ricochet, params)
        t = task(e, 0x40, HIT_T)
        bullet(e, s, 2, 0, (1000.25, -2000.5, 300.125), tuple(f(x) for x in d), speed, pierce, ricocheted)
        out = []
        for h in hits:
            if h is None:
                out.append(None)
                continue
            h = dict(h)
            h.setdefault("normal", normal or (f(0.1), f(-0.9), f(0.3)))
            if h.pop("front", True):
                h["mat"] = m
            if h.pop("backside", False):
                h["back"] = m
            if h.pop("hittask", False):
                h["task"] = t
            out.append(h)
        RET["hits"] = out
        RET["frand"] = list(frand)
        RET["ticks"] = list(ticks)
        RET["rand"] = [3, 1, 4, 1, 5]
        u32(e, 0x540838, sound)
        u32(e, 0x5BE3D8, delay)
        return (s,)
    return setup


CASES = {
    "nohit": dict(hits=[None]),
    "zero_speed": dict(hits=[None], speed=0.0),
    "ricochet": dict(hits=[{"t": 0.3}], frand=[0.0]),
    "ricochet_zero_normal": dict(hits=[{"t": 0.3, "normal": (0.0, 0.0, 0.0)}], frand=[0.0], ricochet=100.0),
    "already_ricocheted": dict(hits=[{"t": 0.3}], ricocheted=1, pierce=3.0),
    "no_ricochet_material": dict(hits=[{"t": 0.3}], ricochet=0.0, pierce=1.0, frand=[0.1]),
    "pierce_equal_lucky": dict(hits=[{"t": 0.3, "hittask": True}, {"t": 0.5}], frand=[0.99, 0.1, 0.9]),
    "pierce_equal_unlucky": dict(hits=[{"t": 0.3, "hittask": True}], frand=[0.99, 0.7]),
    "pierce_harder": dict(hits=[{"t": 0.3, "hittask": True}, {"t": 0.2}, None], frand=[0.99, 0.99], pierce=2.5),
    "pierce_no_task": dict(hits=[{"t": 0.3}], frand=[0.99], pierce=2.5),
    "pierce_weak": dict(hits=[{"t": 0.3}], frand=[0.99], pierce=1.5),
    "back_then_front": dict(hits=[{"t": 0.3, "front": False, "backside": True}, {"t": 0.4}]),
    "both_sides": dict(hits=[{"t": 0.3, "backside": True}, None]),
    "back_only_none": dict(hits=[{"t": 0.3, "front": False}, None]),
    "sound_recent": dict(hits=[{"t": 0.3}], frand=[0.99], sound=100, ticks=[105, 105], delay=8),
    "sound_old": dict(hits=[{"t": 0.3}], frand=[0.99], sound=100, ticks=[120, 120, 130], delay=8),
    "sound_back_in_time": dict(hits=[{"t": 0.3}], frand=[0.99], sound=100, ticks=[50, 50, 60], delay=8),
    "sound_wrap": dict(hits=[{"t": 0.3}], frand=[0.99], sound=0x7FFFFFF0, ticks=[0x80000010, 0x80000010, 1]),
    "trail_rate": dict(hits=[None], rate=3),
    "trail_rate_pierced": dict(hits=[{"t": 0.3, "hittask": True}, None], frand=[0.99], pierce=2.5, rate=3),
    "no_tracer": dict(hits=[{"t": 0.3}], tracer=0),
    "no_gun": dict(hits=[{"t": 0.3}], gun=False),
    "no_params": dict(hits=[{"t": 0.3}], frand=[0.99], params=0),
    "many_impacts": dict(hits=[{"t": 0.05, "hittask": True}] * 14, frand=[0.99] * 14, pierce=4.0),
    "many_backs": dict(hits=[{"t": 0.05, "front": False, "backside": True}] * 13 + [None]),
    "zero_length": dict(hits=[{"t": 0.0, "hittask": True}, None], frand=[0.99], pierce=2.5),
    "grazing": dict(hits=[{"t": 0.5, "normal": (f(0.6), f(-0.8), 0.0)}], frand=[0.9], d=(0.8, 0.6, 0.0)),
}


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", sorted(CASES))
def test_case(case, cw):
    run(simple(**CASES[case]), cw)


@pytest.mark.parametrize("fx", ["lost", "dist"])
def test_handler_changes_hit(fx):
    def setup(e):
        RET["hitfx"] = (lambda e, h: u32(e, h + 0x58, 0)) if fx == "lost" else (lambda e, h: d64(e, h + 0x60, 1.5))
        return simple([{"t": 0.3, "hittask": True}, {"t": 0.6}, None], frand=[0.99, 0.99], pierce=2.5)(e)
    run(setup, 0x7F)


def test_no_bullets():
    def setup(e):
        return (gunshot(e),)
    run(setup, None)


def test_swaps_bullets():
    def setup(e):
        s = gunshot(e)
        for i in range(5):
            bullet(e, s, 10 - i, i, (i * 100.0, 5.0, 7.0), (f(0.0), f(1.0), f(0.0)), [0.0, 5000.0][i & 1])
        RET["hits"] = [None] * 5
        return (s,)
    run(setup, None)
