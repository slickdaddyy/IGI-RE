import random
import struct

import pytest

from emu import Emu
from test_shadow_fakes import CWS, Fakes, check, f32, f64, i32

e = Emu()
fk = Fakes(e)


def project(em, a):
    x, y, z = struct.unpack("<3d", em.read(a[2], 24))
    d = struct.unpack("<f", em.read(a[1], 4))[0]
    em.write(a[0], f32(x * 0.5 + d, y * 0.25 - d))
    em.write(a[1], f32(d * 0.001))
    return 0


fk.add(0x497F20, 3, ret=project, fmt=lambda em, a: (em.read(a[1], 4), em.read(a[2], 24)))
DRAW = fk.add(fk.code(b"\xc3"), 1, fmt=lambda em, a: (em.read(a[0], 16), em.read(em.u32(a[0] + 16), 0x48)))
i32(e, 0xA84A54, 5)
i32(e, 0xA94E84 + 5 * 4, DRAW)
i32(e, 0xA84A50, 0x1234)
i32(e, 0xA84A58, 0x5678)
e.write(0xBCAAF8, f32(0.6, 0.0, 0.8))
e.write(0xBCAB08, f64(100.0, -50.0, 10.0))
e.baseline = e.snapshot()

ENDS = [((1000, 0, 1000), (2000, 5, 3000)), ((100, 0, 10), (2000, 5, 3000)), ((2000, 5, 3000), (100, 0, 10)),
        ((100, 0, 10), (90, 3, 11)), ((300.3, 7.7, 400.1), (-200.5, 1e3, 120.25))]
ALPHAS = [(0.0, 0.0), (1.0, 1.0), (0.5, 0.0), (0.0, 1.0), (1.0, 0.25)]


@pytest.mark.parametrize("addr,name", [(0x4E0E20, "Line3D_Draw"), (0x4E1130, "Line3D_Draw2")])
@pytest.mark.parametrize("ends", ENDS)
@pytest.mark.parametrize("alphas", ALPHAS)
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_line(addr, name, ends, alphas, cw):
    def setup(em):
        a, b = em.alloc(24), em.alloc(24)
        em.write(a, f64(*ends[0]))
        em.write(b, f64(*ends[1]))
        ca, cb = em.alloc(12), em.alloc(12)
        em.write(ca, f32(0.1, 0.2, 0.3))
        em.write(cb, f32(0.9, 0.8, 0.7))
        return (a, b, ca, struct.unpack("<I", f32(alphas[0]))[0], cb, struct.unpack("<I", f32(alphas[1]))[0])
    check(e, fk, addr, name, setup=setup, cw=cw)


def spline(em, rng, n, straight, reversed_):
    s = em.alloc(0x5C0)
    em.write(s + 0x70, f32(*[rng.uniform(-1, 1) for _ in range(9)]))
    em.write(s + 0xFC, f32(-30.0, 70.0))
    for k in range(n + 1):
        em.write(s + 0x104 + 12 * k, f32(*[rng.uniform(-100, 100) for _ in range(3)]))
        em.write(s + 0x200 + 0x28 * k, f32(*[rng.uniform(-1, 1) for _ in range(9)]))
    i32(em, s + 0x59C, n)
    em.write(s + 0x5B4, bytes([straight, reversed_]))
    return s


@pytest.mark.parametrize("straight,reversed_", [(1, 0), (0, 0), (0, 1)])
@pytest.mark.parametrize("seed", [1, 2])
@pytest.mark.parametrize("cw", CWS)
def test_bend(straight, reversed_, seed, cw):
    rng = random.Random(seed)
    xs = [-30.0, 70.0, 20.0, -45.0, 90.0] + [rng.uniform(-35, 75) for _ in range(20)]
    verts = b"".join(f32(x, rng.uniform(-9, 9), rng.uniform(-9, 9), 1, 2, 3) for x in xs)

    def setup(em):
        s = spline(em, random.Random(seed + 10), 6, straight, reversed_)
        v = em.alloc(len(verts))
        em.write(v, verts)
        lst = em.alloc(8)
        em.write(lst, struct.pack("<II", v, len(xs)))
        return s, lst
    check(e, fk, 0x4E1440, "SplineObj_BendVerts", setup=setup, cw=cw)
