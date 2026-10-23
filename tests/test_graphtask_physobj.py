import math
import random
import struct

import pytest

from emu import Emu
from test_graphtask_util import CWS, Fakes, check, check_error, f32, f64, i32, set_handler

emu = Emu()
fk = Fakes(emu)
R = {}
WORLD, MSG = 0x5A5A0000, 0x3C3C0000
HIT_EVENT = 0x4E


def hit_bytes(e, h):
    return e.read(h, 4) + e.read(h + 0x38, 12) + e.read(h + 0x50, 4) + e.read(h + 0x58, 4) + e.read(h + 0x60, 8)


def coll_lines(e, a):
    """Coll_Lines(world, hits, starts, ends, n, event, msg, flags): hits from R["hits"]."""
    for j in range(a[4]):
        h = a[1] + 0x70 * j
        task, normal, mat, found, dist = R["hits"][j % len(R["hits"])]
        e.write(h, i32(task))
        e.write(h + 0x38, f32(*normal))
        e.write(h + 0x50, i32(mat))
        e.write(h + 0x58, i32(found))
        e.write(h + 0x60, f64(dist))


def drop_points(e, a):
    """Coll_DropPointsMat(world, hits, points, n, dz, event, msg, flags)."""
    for j in range(a[3]):
        h = a[1] + 0x70 * j
        found, mat = R["drops"][j % len(R["drops"])]
        e.write(h + 0x50, i32(mat))
        e.write(h + 0x58, i32(found))


def model_vertex(e, a):
    e.write(a[0], f32(*R["verts"][a[2] % len(R["verts"])]))


fk.add(0x4C48C0, 0, 0x33)
fk.add(0x4EE100, 0, 0x44)
fk.add(0x401900, 5, 0x77, fmt=lambda e, a: (e.read_cstr(a[0]), a[1], a[2] & 0xFFFF, a[3]))
fk.add(0x4017C0, 1, lambda e, a: R.setdefault("ev", iter(range(0x30, 0x40))).__next__())
fk.add(0x4018C0, 1, fmt=lambda e, a: (a[0] & 0xFF,))
fk.add(0x401A20, 1, fmt=lambda e, a: (a[0] & 0xFFFF,))
fk.add(0x401400, 3, fmt=lambda e, a: (a[0] & 0xFFFF, a[1] & 0xFF, e.normalize(a[2])))
fk.add(0x401530, 3, fmt=lambda e, a: (a[0] & 0xFFFF, a[1], e.normalize(a[2])))
fk.add(0x4CC2A0, 8, coll_lines, fmt=lambda e, a: (a[0], e.read(a[2], 24 * a[4]), e.read(a[3], 24 * a[4]), a[4],
                                                   a[5], a[6], a[7]))
fk.add(0x4CB9F0, 8, drop_points, fmt=lambda e, a: (a[0], e.read(a[2], 24 * a[3]), a[3], e.read(a[4], 8 * a[3]),
                                                    a[5], a[6], a[7]))
fk.add(0x4C6940, 4, fmt=lambda e, a: (a[0], a[1], e.read(a[2], 24), a[3]))
fk.add(0x4D0950, 1, lambda e, a: R["radius"], st0="f")
fk.add(0x4D09E0, 4, model_vertex, fmt=lambda e, a: (a[1], a[2], e.read_cstr(a[3])))
fk.add(0x4EE1B0, 1, lambda e, a: a[0] + 0xC)
HIT = fk.handler(2, ret=lambda e, a: e.write(a[1] + 8, i32(R["ignore"].pop(0) if R["ignore"] else 0)),
                 fmt=lambda e, a: (a[0], e.normalize(e.u32(a[1])), hit_bytes(e, e.u32(a[1] + 4))))
emu.baseline = emu.snapshot()


def rotation(rng):
    a, b, c = rng.uniform(-3, 3), rng.uniform(-3, 3), rng.uniform(-3, 3)
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    return [cb * cc, -cb * sc, sb, sa * sb * cc + ca * sc, -sa * sb * sc + ca * cc, -sa * cb,
            -ca * sb * cc + sa * sc, ca * sb * sc + sa * cc, ca * cb]


def matrix(rng):
    return f32(*rotation(rng)) + i32(rng.randint(0, 40))


def vec(rng, k):
    return [rng.uniform(-k, k) for _ in range(3)]


def body(e, rng, b):
    e.call(0x4ECEA0, b, *struct.unpack("<4I", f32(rng.uniform(10, 900), 9000, 20000, 6000)), 32)
    e.write(b + 8, f32(rng.uniform(0, 80)))
    e.write(b + 0x18, f32(*vec(rng, 60)))
    d = vec(rng, 1)
    k = 1 / math.sqrt(sum(x * x for x in d))
    e.write(b + 0x24, f32(*[x * k for x in d]))
    e.write(b + 0x30, f32(*vec(rng, 5000)))
    e.write(b + 0x48, f32(*vec(rng, 0.05)))


def phys(e, rng, joints=6):
    p = e.alloc(0x2D8)
    e.write(p + 8, f32(rng.uniform(10, 900), *[rng.uniform(0.1, 2) for _ in range(3)]))
    e.write(p + 0x138, b"crate_model\0")
    e.write(p + 0x148, i32(joints))
    for j in range(16):
        e.write(p + 0x14C + 0x18 * j, i32(rng.randint(0, 30), rng.randint(0, 9), rng.randint(0, 9)) +
                f32(rng.uniform(0, 1), rng.uniform(0, 1), rng.uniform(0, 1)))
    e.write(p + 0x2CC, f32(rng.uniform(5, 50), rng.uniform(0.0001, 0.002)))
    return p


def obj(e, rng, active=0.7, collide=1, upright=0, ncontacts=None):
    o = e.alloc(0x720)
    e.write(o + 0x20, f64(*vec(rng, 1e5)))
    e.write(o + 0x6C, i32(0x6C6C0000))
    e.write(o + 0x70, matrix(rng))
    e.write(o + 0x98, f32(*[rng.choice([0.5, 1.0, 1.5, rng.uniform(0.5, 2)]) for _ in range(3)]))
    body(e, rng, o + 0xF0)
    e.write(o + 0x1B4, i32(phys(e, rng)))
    e.write(o + 0x1B8, i32(rng.randint(0, 3)))
    for i in range(22):
        c = o + 0x1BC + 0x38 * i
        e.write(c, i32(rng.randint(0, 9), rng.randint(0, 9)) + f32(rng.uniform(0, 1), *vec(rng, 3000)))
        e.write(c + 0x18, bytes([rng.random() < active, rng.random() < active, rng.randint(0, 1), 0]))
        e.write(c + 0x1C, f32(rng.uniform(0, 1), rng.uniform(0, 1), *vec(rng, 1), rng.uniform(0, 100)))
    e.write(o + 0x68C, i32(rng.randint(1, 22) if ncontacts is None else ncontacts))
    e.write(o + 0x690, bytes([rng.randint(0, 1)]))
    e.write(o + 0x694, matrix(rng))
    e.write(o + 0x6C0, f64(*[x + rng.uniform(-50, 50) for x in struct.unpack("<3d", e.read(o + 0x20, 24))]))
    e.write(o + 0x6D8, bytes([collide, upright]))
    e.write(o + 0x6DC, matrix(rng))
    e.write(o + 0x708, f64(*vec(rng, 9)))
    return o


def base(e):
    R.clear()
    R["ignore"] = []
    e.call(0x4B0BA0)  # Mem_Init
    e.write(0x54E4CC, bytes([0x4C, 0x4D, HIT_EVENT]))
    e.write(0xA774B0, struct.pack("<H", 0x77))


def test_getters():
    for addr, name in [(0x4FB5E0, "PhysicsObj_GetType"), (0x4FC840, "PhysicsObj_GetEvent0"),
                       (0x4FC850, "PhysicsObj_GetMotionEvent"), (0x4FC860, "PhysicsObj_GetHitEvent")]:
        check(emu, fk, addr, name, base, eax_mask=0xFFFF if addr == 0x4FB5E0 else 0xFF)


def test_open_close():
    check(emu, fk, 0x4FB1B0, "PhysicsObj_Open", base, check_eax=False)
    check(emu, fk, 0x4FB5A0, "PhysicsObj_Close", base, check_eax=False)


@pytest.mark.parametrize("addr,name", [(0x4FB290, "PhysicsObj_CreateHandler"), (0x4FB2A0, "PhysicsObj_DeleteHandler"),
                                       (0x4FB2B0, "PhysicsObj_RunHandler")])
def test_error_handlers(addr, name):
    check_error(emu, addr, name, base)


def test_init_free():
    def setup(e):
        base(e)
        return (obj(e, random.Random(1)),)
    check(emu, fk, 0x4FB5F0, "PhysicsObj_Init", setup, check_eax=False)
    check(emu, fk, 0x4FB980, "PhysicsObj_Free", setup, check_eax=False)


@pytest.mark.parametrize("joints", [None, 0, 1, 7, 16])
@pytest.mark.parametrize("cw", CWS)
def test_set_phys_type(joints, cw):
    def setup(e):
        base(e)
        rng = random.Random(joints or 99)
        R["verts"] = [vec(rng, 3000) for _ in range(5)]
        o = obj(e, rng)
        return o, 0 if joints is None else phys(e, rng, joints)
    check(emu, fk, 0x4FB700, "PhysicsObj_SetPhysType", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("seed", range(3))
@pytest.mark.parametrize("cw", CWS)
def test_point_motion(seed, cw):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        o = obj(e, rng)
        p = e.alloc(24)
        e.write(p, f64(*[x + rng.uniform(-3000, 3000) for x in struct.unpack("<3d", e.read(o + 0x20, 24))]))
        msg = e.alloc(0x48)
        e.write(msg, i32(p))
        return o, msg
    check(emu, fk, 0x4FB2C0, "PhysicsObj_GetPointMotion", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("scale", [0.001, 0.02, 1.0, 30.0])
@pytest.mark.parametrize("cw", CWS)
def test_classify(scale, cw):
    def setup(e):
        base(e)
        rng = random.Random(int(scale * 1000))
        b = e.alloc(0x64)
        body(e, rng, b)
        e.write(b + 0x18, f32(*vec(rng, scale)))
        e.write(b + 0x48, f32(*vec(rng, scale / 3000)))
        n, r, out = e.alloc(12), e.alloc(12), e.alloc(4)
        e.write(n, f32(*rotation(rng)[:3]))
        e.write(r, f32(*vec(rng, 3000)))
        return n, b, r, 0x12345678, out
    check(emu, fk, 0x4FC870, "Contact_Classify", setup, cw=cw)


@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("cw", CWS)
def test_apply_impulse(seed, cw):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        b = e.alloc(0x64)
        body(e, rng, b)
        e.write(b + 4, f32(rng.uniform(0.001, 0.1)))
        e.write(b + 0xC, f32(*[rng.uniform(1e-8, 1e-6) for _ in range(3)]))
        m, n, r = e.alloc(40), e.alloc(12), e.alloc(12)
        e.write(m, matrix(rng))
        e.write(n, f32(*rotation(rng)[3:6]))
        e.write(r, f32(*vec(rng, 3000)))
        return (b, m, n, r) + struct.unpack("<3I", f32(rng.uniform(-30, -1), rng.uniform(0, 50), rng.uniform(0, 1)))
    check(emu, fk, 0x4FC8E0, "Body_ApplyImpulse", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("case", ["flat", "lim", "slide", "parallel", "zero"])
@pytest.mark.parametrize("cw", CWS)
def test_apply_friction(case, cw):
    def setup(e):
        base(e)
        rng = random.Random(case)
        b = e.alloc(0x64)
        body(e, rng, b)
        n = e.alloc(12)
        nv = rotation(rng)[6:9]
        if case == "flat":
            nv[2] = 0.0
        if case == "parallel":
            e.write(b + 0x24, f32(*nv))
        if case == "zero":
            e.write(b + 0x24, f32(0, 0, 0))
        e.write(n, f32(*nv))
        push = 44.6 if case != "slide" else 0.01
        return (b, n) + struct.unpack("<3I", f32(push, rng.uniform(0.2, 1), rng.uniform(0.8, 1)))
    check(emu, fk, 0x4FCCA0, "Body_ApplyFriction", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("seed", range(3))
@pytest.mark.parametrize("cw", CWS)
def test_integrate(seed, cw):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        return (obj(e, rng),) + struct.unpack("<3I", f32(44.6, rng.uniform(0.9, 1), rng.uniform(0.9, 1))) + (WORLD,)
    check(emu, fk, 0x4FBF90, "PhysicsObj_Integrate", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
def test_contact_impulse_friction(cw):
    def setup(e):
        base(e)
        rng = random.Random(5)
        o = obj(e, rng)
        n, r, dot = e.alloc(12), e.alloc(12), e.alloc(4)
        e.write(n, f32(*rotation(rng)[:3]))
        e.write(r, f32(*vec(rng, 3000)))
        e.write(dot, f32(-7.5))
        R["args"] = (o, n, r, dot)
        return o, n, r, dot, struct.unpack("<I", f32(44.6))[0], 3, 1
    check(emu, fk, 0x4FBF00, "PhysicsObj_ContactImpulse", setup, check_eax=False, cw=cw)

    def setup2(e):
        o, n, r, dot = setup(e)[:4]
        return o, WORLD, n, struct.unpack("<I", f32(44.6))[0], 5, r, struct.unpack("<I", f32(0.95))[0]
    check(emu, fk, 0x4FBF50, "PhysicsObj_ContactFriction", setup2, check_eax=False, cw=cw)


def hits(rng, n, task=0, ignore=False):
    out = []
    for j in range(n):
        normal = vec(rng, 1) if rng.random() < 0.8 else [0.0, 0.0, 0.0]
        found = rng.random() < 0.7
        out.append((task if rng.random() < 0.5 else 0, normal, R["mat"][rng.randint(0, 2)], found,
                    rng.uniform(0, 200)))
    return out


SCENARIOS = ["none", "nohit", "hits", "task", "ignore", "fast", "empty"]


@pytest.mark.parametrize("scenario", SCENARIOS)
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("cw", [0x37F, 0x7F, 0xC7F])
def test_collide_contacts(scenario, seed, cw):
    def setup(e):
        base(e)
        rng = random.Random(seed * 100 + SCENARIOS.index(scenario))
        o = obj(e, rng, active=0 if scenario == "empty" else 0.7, ncontacts=0 if scenario == "none" else None)
        if scenario == "fast":
            e.write(o + 0xF0 + 0x18, f32(*vec(rng, 3000)))
        m0, m1 = e.alloc(16), e.alloc(16)
        e.write(m0 + 0xC, f32(0.0))
        e.write(m1 + 0xC, f32(0.4))
        R["mat"] = [0, m0, m1]
        t = e.alloc(0x40)
        e.write(t + 0x1C, struct.pack("<H", 0x55))
        set_handler(e, HIT_EVENT, 0x55, HIT)
        R["hits"] = hits(rng, 22, t if scenario in ("task", "ignore") else 0)
        if scenario == "nohit":
            R["hits"] = [(0, [0, 0, 1], 0, 0, 1.0)]
        if scenario == "ignore":
            R["ignore"] = [rng.randint(0, 1) for _ in range(22)]
        return (o,) + struct.unpack("<2I", f32(44.6, 0.95)) + (WORLD, 0xC, MSG)
    check(emu, fk, 0x4FB9A0, "PhysicsObj_CollideContacts", setup, cw=cw)


@pytest.mark.parametrize("seed", range(6))
def test_check_contacts(seed):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        o = obj(e, rng, active=0 if seed == 5 else 0.7)
        R["drops"] = [(rng.random() < 0.3, rng.choice([0, 0x4444])) for _ in range(22)]
        return o, WORLD, 0xC, MSG
    for cw in (0x37F, 0x7F):
        check(emu, fk, 0x4FC620, "PhysicsObj_CheckContacts", setup, cw=cw)


@pytest.mark.parametrize("seed", range(8))
def test_is_resting(seed):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        o = obj(e, rng)
        e.write(o + 0x1B8, i32(rng.choice([0, 1, 2, -1])))
        e.write(o + 0x690, bytes([rng.random() < 0.2]))
        e.write(o + 0xF0 + 0x5C, i32(rng.random() < 0.2))
        e.write(o + 0xF0 + 8, f32(rng.uniform(0, 30)))
        e.write(o + 0xF0 + 0x48, f32(*vec(rng, 0.03)))
        return (o,)
    check(emu, fk, 0x4FC7B0, "PhysicsObj_IsResting", setup)


@pytest.mark.parametrize("flags", [(0, 0), (1, 0), (0, 1), (1, 1)])
@pytest.mark.parametrize("seed", range(3))
@pytest.mark.parametrize("cw", [0x37F, 0x7F, 0xC7F])
def test_collide(flags, seed, cw):
    def setup(e):
        base(e)
        rng = random.Random(seed * 10 + flags[0] * 2 + flags[1])
        o = obj(e, rng, collide=flags[0], upright=flags[1])
        m0 = e.alloc(16)
        e.write(m0 + 0xC, f32(0.5))
        R["mat"] = [0, m0, 0]
        R["hits"] = hits(rng, 22)
        R["drops"] = [(rng.random() < 0.05 * seed, 0x4444) for _ in range(22)]
        R["radius"] = rng.uniform(100, 3000)
        return (o,) + struct.unpack("<3I", f32(44.6, 0.96, 0.92)) + (WORLD, 0xC, MSG)
    check(emu, fk, 0x4FC0E0, "PhysicsObj_Collide", setup, check_eax=False, cw=cw)
