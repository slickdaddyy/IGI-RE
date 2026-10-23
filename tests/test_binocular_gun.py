import math
import random
import struct

import pytest

from emu import Emu
from test_binocular_util import (Fakes, add_child, check, check_error, common, cstr, event, f32, f64, i32, masked,
                                 matrix_fakes, new_task, rmatrix, rvec, set_events, set_handler, u32, u8)

GT, OT, WT, TT, CT = 0x50, 0x51, 0x52, 0x53, 0x54  # gun, owner, weapon, target, child
V = {}

e = Emu()
fk = Fakes(e)
common(e, fk, {b"AIStationaryGun": GT})
set_events(e)
matrix_fakes(fk)
LOG1 = fk.handler(1)
LOG2 = fk.handler(2, deref={1: 8})
KEY = fk.code(struct.pack("<I", 0x1234))


def point(em, a):
    em.write(a[0], f64(*V.get("point", (10.0, -20.0, 30.0))))


def owner_detect(em, a):
    em.write(a[1], struct.pack("<I", V.get("det", 0)))


fk.add(0x4CEC10, 1, ret=lambda em, a: V.get("model", 0x7700), fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4C5710, 3)
fk.add(0x4D0950, 1, ret=lambda em, a: V.get("radius", 100.0), st0="d")
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4F2050, 1)
fk.add(0x413C00, 1, ret=KEY)
fk.add(0x477C50, 5, fmt=masked(~0, ~0, ~0, ~0, 0xFF))
fk.add(0x4F2140, 3)
fk.add(0x4C4950, 3)
fk.add(0x414E20, 3, ret=point, fmt=lambda em, a: a[1:])
fk.add(0x4F2310, 3, fmt=lambda em, a: (a[0], em.read(a[1], 24), em.read(a[2], 40)))
fk.add(0x4E6B00, 2, ret=lambda em, a: V.get("sound", 0), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4E6C00, 1)
fk.add(0x4E6C30, 2, deref={1: 24})
DETECT = fk.add(fk.code(b"\xc3"), 2, ret=owner_detect, fmt=lambda em, a: (a[0],))
FIRE = fk.add(fk.code(b"\xc3"), 2,
              fmt=lambda em, a: (a[0], em.read(a[1], 2), em.read(a[1] + 4, 16), em.read(a[1] + 0x20, 16)))
set_handler(e, event(0x489630), OT, DETECT)
set_handler(e, event(0x460BF0), WT, FIRE)
set_handler(e, 1, CT, LOG1)
set_handler(e, event(0x4D9FB0), CT, LOG2)
e.baseline = e.snapshot()

CW = pytest.mark.parametrize("cw", [0x37F, 0x7F])


def gun_def(rng, **kw):
    d = e.alloc(0x60)
    e.write(d, b"M60\0")
    vals = dict(pmax=rng.uniform(0.1, 1.0), pmin=rng.uniform(-1.0, -0.1), yaw=rng.uniform(0.5, 7.0),
                ps=rng.uniform(0.001, 0.2), ys=rng.uniform(0.001, 0.2), spin=rng.uniform(0.01, 0.3),
                range=rng.uniform(0, 3000))
    vals.update(kw)
    e.write(d + 0x20, f32(vals["pmax"], vals["pmin"], vals["yaw"]) + struct.pack("<i", 77)
            + f32(vals["ps"], vals["ys"]))
    e.write(d + 0x38, b"gun_turn\0")
    e.write(d + 0x58, f32(vals["spin"], vals["range"]))
    return d


def owner(rng):
    o = new_task(e, OT, 0x200)
    e.write(o + 0x20, rvec(rng))
    e.write(o + 0x70, rmatrix(rng))
    return o


def gun(rng, weapon=True, **kw):
    g = new_task(e, GT, 0x1E8)
    e.write(g + 0x20, rvec(rng))
    e.write(g + 0x68, struct.pack("<II", 0x6800, 0x6C00))
    e.write(g + 0x70, rmatrix(rng))
    e.write(g + 0x98, f32(*[rng.uniform(0.5, 2) for _ in range(3)]))
    e.write(g + 0xF0, rvec(rng, 100))
    e.write(g + 0x108, rmatrix(rng))
    u32(e, g + 0x130, owner(rng))
    u32(e, g + 0x1B8, gun_def(rng, **kw))
    e.write(g + 0x1BC, f32(rng.uniform(-1, 1), rng.uniform(-3, 3)))
    if weapon:
        w = new_task(e, WT, 0x40)
        u32(e, g + 0x1C4, w)
    e.write(g + 0x1E4, f32(rng.uniform(-1, 1)))
    return g


def test_get_list():
    def setup(em):
        u32(em, 0x5BE330, 0xABCD)
    check(e, fk, 0x46EAD0, "GunHolder_GetList", setup, check_eax=True)


@CW
@pytest.mark.parametrize("seed", range(4))
def test_create(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        V.clear()
        V["point"] = (rng.uniform(-50, 50), 3.0, 4.0)
        g = new_task(em, GT, 0x1E8)
        em.write(g + 0x98, f32(*[rng.uniform(0.5, 2) for _ in range(3)]))
        em.write(g + 0x6C, struct.pack("<I", 0x6C00))
        u32(em, g + 0x1C4, new_task(em, WT, 0x40) if seed else 0)
        a = em.alloc(0x90)
        u32(em, a, cstr(em, b"turret"), 0x4444)
        em.write(a + 8, rvec(rng) + rvec(rng, 100) + rmatrix(rng) + rmatrix(rng))
        u32(em, a + 0x88, owner(rng), gun_def(rng))
        return g, a
    check(e, fk, 0x46EAE0, "AIStationaryGun_Create", setup, cw=cw)


def test_create_error():
    def setup(em):
        V.clear()
        V["model"] = 0
        a = em.alloc(0x90)
        u32(em, a, cstr(em, b"nothing"))
        return new_task(em, GT, 0x1E8), a
    check_error(e, fk, 0x46EAE0, e.symbols["AIStationaryGun_Create"], setup)


@CW
@pytest.mark.parametrize("seed", range(4))
def test_place_initial(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        V.clear()
        V["point"] = (rng.uniform(-50, 50), 3.0, rng.uniform(-50, 50))
        g = gun(rng, weapon=seed != 3)
        return g, em.u32(g + 0x130) + 0x70
    check(e, fk, 0x46EC90, "AIStationaryGun_PlaceInitial", setup, cw=cw)


def test_open():
    check(e, fk, 0x46F2B0, "AIStationaryGun_Open")


def detection(rng, targets):
    """A detection result whose list holds (active, task or None) nodes."""
    det = e.alloc(0x20)
    nodes = [e.alloc(0x30) for _ in range(len(targets) + 1)]
    for i, (active, t) in enumerate(targets):
        ref = e.alloc(4)
        u32(e, ref, t or 0)
        u32(e, nodes[i], nodes[i + 1], 0, ref)
        u8(e, nodes[i] + 0x29, active)
    u32(e, det + 0x18, nodes[0])
    return det


def run_setup(seed, kind):
    def setup(em):
        rng = random.Random(seed)
        V.clear()
        V["sound"] = rng.choice([0, 0x5050])
        snap = kind in ("fire", "snap")
        kw = {}
        if snap:
            kw = dict(ps=5.0, ys=7.0, pmax=1.5, pmin=-1.5, yaw=rng.choice([3.0, 7.0]),
                      range=0.0 if kind == "fire" else 1e9)
        if kind == "wrap":
            kw = dict(yaw=7.0)
        g = gun(rng, **kw)
        o = em.u32(g + 0x130)
        u32(em, g + 0x1D0, rng.choice([0, 0x5151]))
        u32(em, g + 0x1D4, rng.choice([0, 0, 3]) if kind != "hold" else 5)
        u8(em, g + 0x1D8, rng.choice([0, 1]))
        em.write(g + 0x1DC, f32(rng.uniform(-1, 1), rng.uniform(-3, 3)))
        if kind == "wrap":
            em.write(g + 0x1C0, f32(rng.choice([-3.0, 3.0])))
        for _ in range(rng.randrange(3)):
            c = new_task(em, CT, 0x40)
            add_child(em, g, c)
        if kind == "nohandler":
            set_handler(em, event(0x489630), OT, 0)
        elif kind == "nodet":
            V["det"] = 0
        else:
            targets = []
            if kind != "empty":
                for _ in range(rng.randrange(3)):
                    targets.append(rng.choice([(0, None), (1, None), (0, new_task(em, TT, 0x40))]))
                t = new_task(em, TT, 0x40)
                em.write(t + 0x20, rvec(rng, 3000))
                targets.append((1, t))
            V["det"] = detection(rng, targets)
        return (g,)
    return setup


@CW
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("kind", ["target", "fire", "snap", "wrap", "hold", "empty", "nodet", "nohandler"])
def test_run(kind, seed, cw):
    check(e, fk, 0x46F390, "AIStationaryGun_Run", run_setup(seed, kind), cw=cw)


@pytest.mark.parametrize("fire", [0, 1])
@pytest.mark.parametrize("weapon", [0, 1])
def test_fire(fire, weapon):
    def setup(em):
        g = gun(random.Random(1), weapon=weapon)
        return g, fire | 0x100
    check(e, fk, 0x46F950, "AIStationaryGun_Fire", setup)


@pytest.mark.parametrize("seed", range(30))
def test_clamp(seed):
    def setup(em):
        rng = random.Random(seed)
        g = gun(rng, yaw=rng.choice([1.0, 6.5, 6.2831855]))
        em.write(g + 0x1BC, f32(rng.uniform(-2, 2), rng.uniform(-8, 8)))
        return (g,)
    check(e, fk, 0x46F9D0, "AIStationaryGun_Clamp", setup)


def script_fakes(ints, reals, strs):
    fk.add(0x4B8A20, 2, ret=lambda em, a: ints[a[1] % len(ints)])
    fk.add(0x4B8A50, 2, ret=lambda em, a: reals[a[1] % len(reals)], st0="d")

    def string(em, a):
        em.write(a[2], strs[a[1] % len(strs)] + b"\0")
        return a[2]
    fk.add(0x4B8A80, 4, ret=string, fmt=lambda em, a: (a[0], a[1], a[3]))


script_fakes([5, -7, 12, 99], [12.5, -3.25, 0.1, 400.0, 33.3, 1e-3, 7.0], [b"M2HB", b"turret_snd", b"x"])
e.baseline = e.snapshot()


@CW
@pytest.mark.parametrize("index", [0, 3])
def test_parse_def(index, cw):
    def setup(em):
        d = em.alloc(0x60)
        a = em.alloc(12)
        u32(em, a, d, 0xA000, index)
        return 0, a
    check(e, fk, 0x46FA50, "AIStationaryGun_ParseDef", setup, cw=cw)


def test_def_size():
    check(e, fk, 0x46FB50, "AIStationaryGun_GetDefSize", lambda em: (0, em.alloc(4)))


@pytest.mark.parametrize("case", range(7))
def test_can_see(case):
    def setup(em):
        g = gun(random.Random(case))
        parent = new_task(em, OT, 0x40)
        u32(em, g + 0x14, parent)
        o = em.u32(g + 0x130)
        other = new_task(em, TT, 0x40)
        a, b = [(g, other), (parent, other), (other, g), (other, parent), (o, other), (other, other),
                (other, o)][case]
        q = em.alloc(0x10)
        u32(em, q, 0x5555, 0, a, b)
        return g, q
    check(e, fk, 0x46FB60, "AIStationaryGun_CanSee", setup)


@CW
@pytest.mark.parametrize("seed", range(4))
def test_move(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        V.clear()
        V["point"] = (rng.uniform(-50, 50), 3.0, rng.uniform(-50, 50))
        g = gun(rng)
        for _ in range(seed % 3):
            add_child(em, g, new_task(em, CT, 0x40))
        a = em.alloc(8)
        pos = em.alloc(24)
        em.write(pos, rvec(rng))
        m = em.alloc(40)
        em.write(m, rmatrix(rng))
        u32(em, a, pos, m)
        return g, a
    check(e, fk, 0x46FBA0, "AIStationaryGun_Move", setup, cw=cw)


@CW
@pytest.mark.parametrize("seed", range(4))
def test_place_weapon(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        V.clear()
        V["point"] = (rng.uniform(-50, 50), 3.0, rng.uniform(-50, 50))
        g = gun(rng)
        if seed == 0:
            em.write(g + 0x1E4, f32(0.0))
        return (g,)
    check(e, fk, 0x470050, "AIStationaryGun_PlaceWeapon", setup, cw=cw)


def test_close_and_type():
    def setup(em):
        em.write(0x53FEC0, b"\x34\x12\x99\x99")
    check(e, fk, 0x4703F0, "AIStationaryGun_Close", setup)
    r = check(e, fk, 0x470400, "AIStationaryGun_GetType", setup)
    assert r[0][0][0] & 0xFFFF == r[0][1][0] & 0xFFFF == 0x1234
