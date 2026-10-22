import random
import struct

import pytest

from emu import Emu
from test_glass_fakes import Fakes, base, check, common, f32, f64, i32, new_task, rmatrix, rvec

HT = 0x56  # GlassShatter type id

e = Emu()
fk = Fakes(e)
common(e, fk, {b"GlassShatter": HT})
VALUE = {}
fk.add(0x4015F0, 1)
fk.add(0x4012A0, 3, ret=lambda em, a: VALUE["task"], fmt=lambda em, a: (a[0], a[1] & 0xFFFF, a[2]))
fk.add(0x44A670, 3, deref={0: 12, 1: 12, 2: 12})
fk.add(e.symbols["GlassShatter_AddTri"], 3, deref={0: 12, 1: 12, 2: 12}, key=0x44A670)
fk.add(0x416920, 0, ret=0x4444)
fk.add(0x49A450, 11, ret=0x9A45, deref={2: 24, 3: 40})
fk.add(0x4E6B00, 2, ret=lambda em, a: VALUE.get("sound", 0), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4E6C30, 2, deref={1: 24})
RANDOM = e.alloc(0x10)
fk.add(0x416D20, 0, ret=RANDOM)
e.baseline = e.snapshot()


def hbase(em):
    VALUE.clear()
    base(em)
    em.write(0x57C240, struct.pack("<H", HT))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_open(cw):
    def setup(em):
        hbase(em)
        em.write(0x57C230, bytes(range(16)))
    _, log = check(e, fk, 0x449D30, "GlassShatter_Open", setup, cw=cw)
    assert len(log) == 3


def unit(rng, n=3):
    return [rng.uniform(-1, 1) for _ in range(n)]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed,frames", [(0, 30), (1, 1), (2, 12), (3, 2)])
def test_run(seed, frames, cw):
    def setup(em):
        hbase(em)
        rng = random.Random(seed)
        for i in range(30):
            em.write(0x57C280 + 0x28 * i, rmatrix(rng))
        g = new_task(em, HT, 0x2B048)
        em.write(g + 0x90, rmatrix(rng))
        n = rng.randrange(1, 20)
        i32(em, g + 0xB8, n)
        i32(em, g + 0xBC, frames)
        for i in range(n):
            s = g + 0xC0 + 0x60 * i
            em.write(s, f32(*[rng.uniform(-3000, 3000) for _ in range(18)]))
            em.write(s + 0x48, struct.pack("<i3ffi", rng.randrange(30), *unit(rng), rng.uniform(0, 9),
                                           rng.choice([0, 1, 0x100])))
            em.write(g + 0x177C0 + 0x50 * i + 0x3C, f32(*unit(rng)))
        mesh = em.alloc(0xA0)
        polys = em.alloc(0x50 * n)
        i32(em, mesh + 0x6C, polys)
        i32(em, mesh + 0x9C, n)
        i32(em, g + 0x2B040, mesh)
        return (g,)
    check(e, fk, 0x449E50, "GlassShatter_Run", setup, cw=cw)


def test_small():
    def setup(em):
        hbase(em)
        g = new_task(em, HT, 0x2B048)
        i32(em, g + 0x2B040, 0x7777)
        em.write(0x57C240, struct.pack("<H", 0x66))
        return (g,)
    check(e, fk, 0x44A1A0, "GlassShatter_Delete", setup)
    check(e, fk, 0x44A1C0, "GlassShatter_Close", setup)


@pytest.mark.parametrize("sound", [0, 0x6B00])
@pytest.mark.parametrize("seed", range(4))
def test_play_sound(sound, seed):
    def setup(em):
        hbase(em)
        em.write(RANDOM, bytes(random.Random(seed).randrange(256) for _ in range(16)))
        VALUE["sound"] = sound
        p = em.alloc(24)
        em.write(p, f64(1.5, 2.5, 3.5))
        return (p,)
    check(e, fk, 0x44A630, "GlassShatter_PlaySound", setup)


def mesh_model(em, rng, ntri):
    m, lod, a, info = em.alloc(8), em.alloc(0x20), em.alloc(0x20), em.alloc(0x80)
    i32(em, m, lod)
    i32(em, lod + 0xC, a)
    i32(em, a + 0xC, info)
    mesh = em.alloc(0x20)
    i32(em, info + 0x2C, mesh)
    nv = 12
    tris, verts = em.alloc(8 * max(ntri, 1)), em.alloc(16 * nv)
    for i in range(nv):
        em.write(verts + 16 * i, f32(*[rng.uniform(-200, 200) for _ in range(4)]))
    for i in range(ntri):
        em.write(tris + 8 * i, struct.pack("<4H", *[rng.randrange(nv) for _ in range(4)]))
    em.write(mesh, struct.pack("<i12xII", ntri, tris, verts))
    return m


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed,ntri", [(0, 0), (1, 1), (2, 8), (3, 20), (4, 13)])
def test_spawn(seed, ntri, cw):
    def setup(em):
        hbase(em)
        rng = random.Random(seed)
        g = new_task(em, HT, 0x2B048)
        VALUE["task"] = g
        model = mesh_model(em, rng, ntri)
        d = em.alloc(12)
        em.write(d, f32(*[rng.uniform(-5000, 5000) for _ in range(3)]))
        pos = em.alloc(24)
        em.write(pos, rvec(rng, 1e5))
        m = em.alloc(40)
        em.write(m, rmatrix(rng))
        hit = em.alloc(0x48)
        em.write(hit + 8, rvec(rng, 1e5))
        em.write(hit + 0x38, f32(*unit(rng)))
        return model, d, pos, m, hit
    check(e, fk, 0x44A1D0, "GlassShatter_Spawn", setup, cw=cw)
