import math
import random
import struct

import pytest

from emu import Emu
from test_glass_fakes import Fakes, check, f32, i32

# GlassShatter_AddTri only calls Random_Float and itself, so nothing is faked
e = Emu()
fk = Fakes(e)
e.baseline = e.snapshot()


def cross(u, v):
    return [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]


def setup_tri(em, rng, size, hit_inside, count=0, mode=0, ten=10.0, normal=None):
    t = em.alloc(0x2B048)
    i32(em, t + 0xB8, count)
    i32(em, 0x57C25C, t)
    i32(em, 0x57C27C, t + 0xC0 + 0x60 * count)
    i32(em, 0x57C26C, t + 0x177C0 + 0x50 * count)
    i32(em, 0x57C244, mode)
    em.write(0x57C23C, f32(ten))
    i32(em, 0x57C228, 0x53C8E0)
    em.write(0x57C230, struct.pack("<3I", *[rng.getrandbits(32) for _ in range(3)]))
    base = [rng.uniform(-500, 500) for _ in range(3)]
    pts = [[b + rng.uniform(-size, size) for b in base] for _ in range(3)]
    n = cross([pts[1][i] - pts[0][i] for i in range(3)], [pts[2][i] - pts[0][i] for i in range(3)])
    ln = math.sqrt(sum(x * x for x in n)) or 1.0
    if hit_inside:
        w = [rng.random() for _ in range(3)]
        s = sum(w)
        hit = [sum(w[k] * pts[k][i] for k in range(3)) / s for i in range(3)]
    else:
        hit = [base[i] + rng.uniform(-3 * size, 3 * size) for i in range(3)]
    em.write(0x57C250, f32(*hit))
    em.write(0x57C270, f32(*[h + rng.uniform(-50, 50) for h in hit]))
    em.write(0x57C260, f32(*(normal or [x / ln for x in n])))
    p = [em.alloc(16) for _ in range(3)]
    for q, v in zip(p, pts):
        em.write(q, f32(*v))
    return p


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
@pytest.mark.parametrize("size,inside", [(300, True), (300, False), (1500, True), (1500, False), (4000, False)])
def test_addtri(seed, size, inside, cw):
    def setup(em):
        return setup_tri(em, random.Random(seed * 7 + size), size, inside)
    check(e, fk, 0x44A670, "GlassShatter_AddTri", setup, cw=cw, normalized=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("normal", [(1, 0, 0), (0, 1, 0), (0, 0, 1), (0.6, -0.8, 0), (0, 0.6, -0.8), (-0.6, 0, 0.8),
                                    (0, 0, 0)])
def test_addtri_axis(seed, normal, cw):
    """Each projection plane, whatever the real normal is."""
    def setup(em):
        return setup_tri(em, random.Random(seed), 800, seed % 2 == 0, normal=list(normal))
    check(e, fk, 0x44A670, "GlassShatter_AddTri", setup, cw=cw, normalized=False)


@pytest.mark.parametrize("mode,ten,count", [(1, 10.0, 0), (2, 10.0, 0), (0, 0.0, 0), (0, -1.0, 3), (0, 10.0, 995),
                                            (1, 10.0, 1000)])
def test_addtri_state(mode, ten, count):
    def setup(em):
        return setup_tri(em, random.Random(count + mode), 1200, True, count=count, mode=mode, ten=ten)
    check(e, fk, 0x44A670, "GlassShatter_AddTri", setup, normalized=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
def test_addtri_flat(seed, cw):
    """Degenerate triangles: a repeated corner or all corners on a line."""
    def setup(em):
        rng = random.Random(seed)
        p = setup_tri(em, rng, 200, seed % 2 == 0)
        a = struct.unpack("<3f", em.read(p[0], 12))
        if seed < 3:
            em.write(p[1], f32(*a))
        else:
            b = struct.unpack("<3f", em.read(p[1], 12))
            em.write(p[2], f32(*[2 * y - x for x, y in zip(a, b)]))
        return p
    check(e, fk, 0x44A670, "GlassShatter_AddTri", setup, cw=cw, normalized=False)
