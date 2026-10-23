import struct

import pytest

from test_explosion_util import (RET, STATE, d64, f32, handler, logfake, make_emu, pool, pool_take, rot, run,
                                 seeded, set_handler, task, u16, u32, unit, vfake)

CWS = [0x37F, 0x7F]
HOLE_POOL, POLY_POOL = 0x5C1108, 0x5C1120
MESH = 0x31
OBJ_TYPE = 0x40

e = make_emu()


def new_task(e, a):
    t = e.alloc(0xE148)
    u32(e, t + 8, t + 0xC, 0, t + 8)
    u32(e, t + 0x14, a[0])
    u16(e, t + 0x1C, a[1] & 0xFFFF)
    return t


def coll(e, a):
    seq = STATE.get("coll", [])
    return seq.pop(0) if seq else 0


vfake(e, 0x4012A0, 3, show=lambda e, a: (e.normalize(a[0]), a[1] & 0xFFFF, a[2]), effect=new_task)
logfake(e, 0x4015F0, 1)
vfake(e, 0x49A450, 11, show=lambda e, a: (a[0], a[1], e.read(a[2], 24), e.read(a[3], 0x28)) + tuple(a[4:]),
      effect=lambda e, a: RET.get("mesh", 0))
logfake(e, 0x49A510, 2)
logfake(e, 0x490370, 0, ret=5555)
vfake(e, 0x4CE410, 8, show=lambda e, a: (a[0], e.read(a[1], 24), e.read(a[2], 24)) + tuple(a[3:]), effect=coll)
logfake(e, 0x49A430, 2)
logfake(e, 0x4E0810, 2)
set_handler(e, 4, MESH, handler(e, "mesh_draw", 2))
logfake(e, 0x4B0B70, 1)
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})
logfake(e, 0x4018C0, 1, masks={0: 0xFF})
e.baseline = e.snapshot()


def hit(e, rng, near=None):
    h = e.alloc(0x70)
    u32(e, h, near or 0)
    f32(e, h + 0x20, *[rng.uniform(-50000, 50000) for _ in range(3)])
    f32(e, h + 0x2C, *[x * rng.uniform(0.2, 3) for x in unit(rng)])
    f32(e, h + 0x38, *[rng.uniform(-300, 300) for _ in range(3)])
    return h


def msg(e, rng, h):
    m = e.alloc(16)
    u32(e, m, h, 0, 0x6600)
    f32(e, m + 12, rng.uniform(20, 300))
    return m


def obj(e, rng):
    o = task(e, 0xF0, OBJ_TYPE)
    d64(e, o + 0x20, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
    f32(e, o + 0x70, *rot(rng))
    return o


def mesh(e):
    m = task(e, 0xA0, MESH)
    RET["mesh"] = m
    return m


def set_hole(e, rng, p, h, mode):
    """Hole at p: 'dead', 'far', 'near' (other direction), 'same' (close and facing the same way)."""
    hp = struct.unpack("<3f", e.read(h + 0x20, 12))
    hn = struct.unpack("<3f", e.read(h + 0x2C, 12))
    size = rng.uniform(20, 200)
    if mode == "far":
        pos = [x + rng.choice((-1, 1)) * rng.uniform(400, 900) for x in hp]
    else:
        pos = [x + rng.uniform(-10, 10) for x in hp]
    if mode == "same":
        n = [x * rng.uniform(0.5, 2) + rng.uniform(-0.05, 0.05) for x in hn]
    elif mode == "near":
        n = [-x for x in hn]
    else:
        n = unit(rng)
    f32(e, p, *n)
    f32(e, p + 0xC, *pos)
    f32(e, p + 0x18, *[rng.uniform(-1e4, 1e4) for _ in range(12)])
    u32(e, p + 0x48, 0xFFFFFFFF if mode == "dead" else 0, 0x6700)
    f32(e, p + 0x50, size)
    u32(e, p + 0x54, 0)


def bullethole(e, rng, o, modes, last, nxt=0):
    bh = e.alloc(0xE148)
    u32(e, bh + 8, bh + 0xC, 0, bh + 8)
    u16(e, bh + 0x1C, 0x33)
    for i, mode in enumerate(modes):
        set_hole(e, rng, bh + 0x20 + 0x58 * i, STATE["hit"], mode)
    u32(e, bh + 0x44E0, last, nxt, len(modes))
    u32(e, bh + 0xE12C, mesh(e), o)
    u32(e, bh + 0xE134, pool_take(e, HOLE_POOL, bh))
    u32(e, bh + 0xE144, rng.randint(0, 1000))
    return bh


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
def test_place(seed, cw):
    def setup(e):
        rng = seeded(seed)
        STATE["coll"] = [rng.choice((0, 0, 0, 1)) for _ in range(4)]
        h = hit(e, rng)
        pos = e.alloc(24)
        d64(e, pos, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
        m = e.alloc(0x28)
        f32(e, m, *rot(rng))
        bh = e.alloc(0xE148)
        return (pos, m, bh, bh + 0x20 + 0x58 * rng.randint(0, 199), h, rng.choice((0, 0xFFFFFFFF)), 0x6600,
                struct.unpack("<I", struct.pack("<f", rng.uniform(10, 300)))[0])
    run(e, 0x47D920, "BulletHole_Place", setup, cw=cw)


CASES = ["new", "full", "same", "other", "ring"]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("case", CASES)
@pytest.mark.parametrize("orig,ours", [(0x47D380, "BulletHole_AddRigid"), (0x47E0C0, "BulletHole_AddSpline")])
def test_add(orig, ours, case, seed, cw):
    def setup(e):
        rng = seeded(seed * 31 + CASES.index(case))
        STATE["coll"] = [rng.choice((0, 0, 1)) for _ in range(4)]
        pool(e, HOLE_POOL, 4, 3)
        pool(e, POLY_POOL, 0x40, 8)
        o = obj(e, rng)
        h = hit(e, rng, o)
        STATE["hit"] = h
        mesh(e)
        if case == "full":
            for i in range(3):
                b = bullethole(e, rng, obj(e, rng), ["far"], 0)
                u32(e, b + 0xE144, [7, 3, 3][i])
        elif case in ("same", "other"):
            bullethole(e, rng, obj(e, rng), ["far"], 0)
            modes = [rng.choice(("dead", "far", "near", "same" if case == "same" else "near")) for _ in range(6)]
            if case == "same":
                modes[rng.randint(0, 5)] = "same"
            bullethole(e, rng, o, modes, rng.randint(0, 5))
        elif case == "ring":
            modes = [rng.choice(("dead", "far", "near")) for _ in range(200)]
            nxt = rng.choice((199, rng.randint(0, 198)))
            b = bullethole(e, rng, o, modes, rng.randint(0, 199), nxt)
            prev = 0
            for _ in range(3):
                p = pool_take(e, POLY_POOL)
                u32(e, p + 0x3C, prev)
                prev = p
            u32(e, b + 0x20 + 0x58 * nxt + 0x54, prev)
            pool_take(e, POLY_POOL)
        return (o, msg(e, rng, h))
    run(e, orig, ours, setup, cw=cw)


@pytest.mark.parametrize("dirty", [0, 1])
@pytest.mark.parametrize("seed", range(4))
def test_draw(seed, dirty):
    def setup(e):
        rng = seeded(seed)
        bh = e.alloc(0xE148)
        n = rng.choice((3, 12, 200))
        for i in range(n):
            p = bh + 0x20 + 0x58 * i
            u32(e, p, *[rng.getrandbits(32) for _ in range(18)])
            u32(e, p + 0x48, rng.choice((0, 0, 0xFFFFFFFF)), 0x6700 + i, 0)
            if rng.random() < 0.3:
                prev = 0
                for _ in range(rng.randint(1, 4)):
                    q = e.alloc(0x44)
                    u32(e, q, *[rng.getrandbits(32) for _ in range(15)])
                    u32(e, q + 0x3C, prev)
                    prev = q
                u32(e, p + 0x54, prev)
        u32(e, bh + 0x44E8, n)
        m = mesh(e)
        u32(e, m + 0x9C, 77)
        u32(e, bh + 0xE12C, m)
        e.write(bh + 0xE138, bytes([dirty]))
        a = e.alloc(0x40)
        u32(e, a, *range(16))
        return (bh, a)
    run(e, 0x47D0B0, "BulletHole_Draw", setup)


@pytest.mark.parametrize("which", range(3))
def test_delete(which):
    def setup(e):
        pool(e, HOLE_POOL, 4, 5)
        items = [pool_take(e, HOLE_POOL, 0x100 + i) for i in range(3)]
        bh = e.alloc(0xE148)
        u32(e, bh + 0xE134, items[which])
        return (bh,)
    run(e, 0x47D320, "BulletHole_Delete", setup)


def test_close():
    def setup(e):
        e.write(0x540904, b"\x2f")
        u16(e, 0x5C1138, 0x55)
    run(e, 0x47E6B0, "BulletHole_Close", setup)


def test_get_event():
    def setup(e):
        e.write(0x540904, b"\x2f")
    run(e, 0x47E6F0, "BulletHole_GetEvent", setup, check_eax=True, eax_mask=0xFF)


