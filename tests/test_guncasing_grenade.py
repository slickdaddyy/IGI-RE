import random
import struct

import pytest

from test_guncasing_util import (CWS, Emu, Fakes, add_child, check, check_error, cstr, f32, f64, new_task, rmatrix,
                                 rvec, set_handler, u32)

e = Emu()
fk = Fakes(e)
GT, PT, EVG = 0x35, 0x36, 0x48
WORLD, OWNER, SND, L3, MODEL, ROOT = 0x5A000000, 0x5B000000, 0x5C000000, 0x5D000000, 0x60000000, 0x61000000
GETTERS = {0x416840: 0x50, 0x416860: 0x51, 0x416850: 0x52, 0x416880: 0x53, 0x4EE0E0: 0x54, 0x4EE0F0: 0x55,
           0x413B20: 0x56, 0x4071D0: 0x57}
R = {}
s = lambda em, p: em.read_cstr(p)
pk = lambda v: struct.unpack("<I", f32(v))[0]


def drop_points(em, a):
    R["hit"] = a[1]
    em.write(a[1], rvec(random.Random(a[3]), 3000) * 4)
    return (a[0], a[1] - a[2] if False else 0, em.read(a[2], 24), a[3], em.read(a[4], 8), a[5] & 0xFF,
            em.read(a[6] + 4, 4), a[7])


def config_find(em, a):
    return R["cfg"].get(s(em, a[1]), 0)


def config_add(em, a):
    p = em.alloc(0x128)
    em.write(p + 0x124, u32(a[2]))
    return p


for addr, v in GETTERS.items():
    fk.add(addr, 0, ret=v)
fk.add(0x401900, 5, ret=GT, fmt=lambda em, a: (s(em, a[0]), a[1], a[2] & 0xFFFF, a[3], a[4]))
fk.add(0x4017C0, 1, ret=EVG, mask=[0xFFFF])
fk.add(0x4018C0, 1, mask=[0xFF])
fk.add(0x401400, 3, mask=[0xFFFF, 0xFF])
fk.add(0x401530, 3, mask=[0xFFFF])
fk.add(0x401A20, 1, mask=[0xFFFF])
fk.add(0x4012A0, 3, ret=lambda em, a: R["task"], mask=[~0, 0xFFFF])
fk.add(0x4015F0, 1)
fk.add(0x401AE0, 1, ret=lambda em, a: a[0] + 0x100)
fk.add(0x401B20, 1)
fk.add(0x4028B0, 0, ret=lambda em, a: R["tick"])
fk.add(0x4168F0, 0, ret=L3)
fk.add(0x416920, 0, ret=WORLD)
fk.add(0x416C90, 5, ret=lambda em, a: R["falloff"], st0="d")
fk.add(0x450CE0, 1, deref={0: 0x1C})
fk.add(0x47E9A0, 4, fmt=lambda em, a: (em.read(a[0], 24), a[1] - R["hit"], a[2], a[3]))
fk.add(0x480FE0, 16, fmt=lambda em, a: (em.read(a[0], 24),) + tuple(a[1:9]) + (a[9] - R["hit"], s(em, a[10]),
                                                                                s(em, a[11])) + tuple(a[12:]))
fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(a[0]))
fk.add(0x4B0D10, 1)
fk.add(0x4B8810, 2, fmt=lambda em, a: (s(em, a[0]), a[1]))
fk.add(0x4B8A20, 2, ret=lambda em, a: R["int"])
fk.add(0x4B8A50, 2, ret=lambda em, a: R["real"], st0="d")
fk.add(0x4C1790, 2)
fk.add(0x4C17C0, 2)
fk.add(0x4C48D0, 2)
fk.add(0x4C5710, 3)
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C7560, 2)
fk.add(0x4CABF0, 8, fmt=drop_points)
fk.add(0x4CEC10, 1, ret=lambda em, a: R["model"], fmt=lambda em, a: (s(em, a[0]),))
fk.add(0x4D0950, 1, ret=lambda em, a: R["radius"], st0="f")
fk.add(0x4E6B00, 2, ret=lambda em, a: R["snd"], fmt=lambda em, a: (a[0], s(em, a[1])))
fk.add(0x4E6C30, 2)
fk.add(0x4ECDB0, 1)
fk.add(0x4EE140, 1, ret=lambda em, a: R["phys"], fmt=lambda em, a: (s(em, a[0]),))
fk.add(0x4F0EE0, 0, ret=OWNER)
fk.add(0x4FB5E0, 0, ret=PT)
fk.add(0x4FB5F0, 1)
fk.add(0x4FB700, 2)
fk.add(0x4FB980, 1)
fk.add(0x4FC0E0, 7, fmt=lambda em, a: tuple(a[:6]) + (em.read(a[6] + 4, 4),))
fk.add(0x4FC7B0, 1, ret=lambda em, a: R["rest"])
fk.add(0x4FE650, 3, ret=config_add, fmt=lambda em, a: (a[0], s(em, a[1]), a[2]))
fk.add(0x4FE730, 2, ret=config_find, fmt=lambda em, a: (a[0], s(em, a[1])))
RUN = fk.handler(1)
COOK = fk.handler(2, fmt=lambda em, a: (em.write(a[1], u32(R["cook"])), a[0])[1:])
e.baseline = e.snapshot()


def phys(em, fuse, kind):
    p, d = em.alloc(0x2D8), em.alloc(8)
    em.write(d, f32(fuse) + u32(kind))
    em.write(p + 0x2D4, u32(d))
    return p


def grenade(em, rng):
    g = new_task(em, GT, 0x738)
    em.write(g + 0x20, rvec(rng))
    em.write(g + 0x98, f32(*[rng.choice([1.0, rng.uniform(0.5, 2)]) for _ in range(3)]))
    em.write(g + 0x108, f32(*[rng.uniform(-300, 300) for _ in range(3)]))
    return g


def weapon(em, cfg):
    wt = em.alloc(0x6E8)
    em.write(wt + 0x61C, b"grenade01\0")
    if cfg:
        c = em.alloc(12)
        em.write(c, f32(*cfg))
        em.write(wt + 0x6E0, u32(c))
    return wt


def test_open_close_event():
    check(e, fk, 0x476730, "Grenade_Open", check_eax=False)

    def setup(em):
        em.write(0x54066C, struct.pack("<H", GT))
        em.write(0x5BE3B8, bytes([EVG]))
    check(e, fk, 0x477010, "Grenade_Close", setup, check_eax=False)
    check(e, fk, 0x477030, "Grenade_GetEvent", setup, eax_mask=0xFF)


@pytest.mark.parametrize("old,have", [(0, ()), (1, ()), (0, (b"Grenade - Explosion radius (meters)",)),
                                      (1, (b"Grenade - Explosion falloff (meters)",
                                           b"Grenade - Explosion damage factor",
                                           b"Grenade - Explosion radius (meters)"))])
def test_weapon_config(old, have):
    def setup(em):
        wt = weapon(em, (1, 2, 3) if old else None)
        R["cfg"] = {}
        for i, name in enumerate(have):
            p = em.alloc(0x128)
            em.write(p + 0x124, f32(7.5 + i))
            R["cfg"][name] = p
        msg = em.alloc(8)
        em.write(msg, u32(0x6E6E0000, wt))
        return (0, msg)
    check(e, fk, 0x4768A0, "Grenade_WeaponConfigHandler", setup, check_eax=False)


@CWS
@pytest.mark.parametrize("seed", range(4))
def test_create(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        R.update(tick=1000 + seed, radius=rng.uniform(100, 900))
        return (grenade(em, rng),)
    check(e, fk, 0x4769B0, "Grenade_CreateHandler", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("kind", [0, 1, 2])
@pytest.mark.parametrize("cfg", [None, (1.5, 6.0, 9.0)])
@pytest.mark.parametrize("has_weapon", [0, 1])
def test_run_explode(kind, cfg, has_weapon):
    def setup(em):
        rng = random.Random(kind)
        g = grenade(em, rng)
        em.write(g + 0x1B4, u32(phys(em, 2.0, kind)))
        if has_weapon:
            em.write(g + 0x730, u32(weapon(em, cfg)))
        return (g,)
    check(e, fk, 0x476AC0, "Grenade_RunHandler", setup, check_eax=False)


@CWS
@pytest.mark.parametrize("seed", range(12))
def test_run_fuse(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        g = grenade(em, rng)
        em.write(g + 0x1B4, u32(phys(em, 2.0, 0)))
        em.write(g + 0x720, u32(rng.randrange(1, 50), rng.randrange(900, 1100)) + bytes([rng.choice([0, 1])]))
        em.write(g + 0x1B8, u32(rng.choice([0, 1])))
        R.update(rest=rng.choice([0, 0, 1]), snd=rng.choice([0, SND]), tick=1005 + rng.randrange(10))
        for i in range(rng.randrange(3)):
            c = new_task(em, 0x70 + i, 0x40)
            add_child(em, g, c)
            set_handler(em, 1, 0x70 + i, RUN)
        return (g,)
    check(e, fk, 0x476AC0, "Grenade_RunHandler", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("seed", range(3))
def test_delete_hit(seed):
    def setup(em):
        rng = random.Random(seed)
        g = grenade(em, rng)
        em.write(g + 0x720, u32(5))
        em.write(g + 0x72C, u32(rng.choice([0, 0x7700])))
        return (g,)
    check(e, fk, 0x476E50, "Grenade_DeleteHandler", setup, check_eax=False)
    check(e, fk, 0x476EA0, "Grenade_HitHandler", setup, check_eax=False)


@CWS
@pytest.mark.parametrize("fuse,falloff", [(0, 1.0), (4, 0.0), (4, 0.25), (4, -0.0), (4, float("nan"))])
def test_blast(fuse, falloff, cw):
    def setup(em):
        R.update(radius=512.5, falloff=falloff)
        g = grenade(em, random.Random(1))
        em.write(g + 0x720, u32(fuse))
        return (g, 0x7300)
    check(e, fk, 0x476EB0, "Grenade_BlastHandler", setup, check_eax=False, cw=cw)


@CWS
def test_parse_args(cw):
    def setup(em):
        R.update(real=2.123456789, int=1)
        cfg, msg = em.alloc(8), em.alloc(12)
        em.write(msg, u32(cfg, 0x7400, 5))
        return (0, msg)
    check(e, fk, 0x476F70, "Grenade_ParseArgsHandler", setup, check_eax=False, cw=cw)


def throw_state(em, rng, fuse=2.0):
    R.update(task=grenade(em, rng), model=MODEL, phys=phys(em, fuse, 0), radius=rng.uniform(100, 900))
    pos, mat, vel = em.alloc(24), em.alloc(40), em.alloc(12)
    em.write(pos, rvec(rng))
    em.write(mat, rmatrix(rng))
    em.write(vel, f32(*[rng.uniform(-300, 300) for _ in range(3)]))
    em.write(0x54066C, struct.pack("<H", GT))
    return pos, mat, vel


@CWS
@pytest.mark.parametrize("seed", range(10))
def test_throw(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        pos, mat, vel = throw_state(em, rng, rng.choice([2.0, 0.1, 1.5, 0.0333]))
        if rng.random() < 0.5:
            em.call(0x4B0D60, 3)  # Mem_PushMode
        return (ROOT, rng.choice([0, 0x7500]), pos, mat, vel, weapon(em, None), rng.choice([0, 5, 30, 100, -3]))
    check(e, fk, 0x477040, "Grenade_Throw", setup, cw=cw)


def test_throw_no_model():
    def setup(em):
        pos, mat, vel = throw_state(em, random.Random(3))
        R["model"] = 0
        return (ROOT, 0x7500, pos, mat, vel, weapon(em, None), 0)
    check_error(e, fk, 0x477040, e.symbols["Grenade_Throw"], setup)


@pytest.mark.parametrize("handler", [0, 1])
def test_fire(handler):
    def setup(em):
        rng = random.Random(handler)
        pos, mat, vel = throw_state(em, rng)
        em.write(0x5BE3B8, bytes([EVG]))
        R["cook"] = 12
        thrower = new_task(em, 0x77, 0x40)
        if handler:
            set_handler(em, EVG, 0x77, COOK)
        gun = new_task(em, 0x78, 0x40)
        add_child(em, thrower, gun)
        msg = em.alloc(0x30)
        em.write(msg, u32(gun, 0x7500, pos, mat, vel, 0, 0, 0, weapon(em, None)))
        return (0, msg)
    check(e, fk, 0x476FA0, "Grenade_FireHandler", setup, check_eax=False)


@CWS
@pytest.mark.parametrize("hit,bounced,tick,snd", [(0, 1, 1005, 0), (0, 1, 1020, 0), (1, 1, 1020, SND), (1, 0, 1020, 0),
                                                  (1, 0, 1020, SND)])
def test_bounce(hit, bounced, tick, snd, cw):
    def setup(em):
        g = grenade(em, random.Random(tick))
        em.write(g + 0x1B8, u32(hit))
        em.write(g + 0x724, u32(1000) + bytes([bounced]))
        R.update(tick=tick, snd=snd)
        return (g,)
    check(e, fk, 0x476CC0, "Grenade_UpdateBounce", setup, check_eax=False, cw=cw)


def test_noise():
    def setup(em):
        return (grenade(em, random.Random(1)), 0x12, 2, pk(20480.0))
    check(e, fk, 0x476D40, "Grenade_MakeNoise", setup, check_eax=False)


@CWS
@pytest.mark.parametrize("seed,rest,hit", [(0, 1, 0), (1, 0, 1), (2, 0, 0), (3, 0, 0)])
def test_physics(seed, rest, hit, cw):
    def setup(em):
        g = grenade(em, random.Random(seed))
        em.write(g + 0x1B8, u32(hit))
        R.update(rest=rest)
        return (g,)
    check(e, fk, 0x476D90, "Grenade_UpdatePhysics", setup, check_eax=False, cw=cw)
