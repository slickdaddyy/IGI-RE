"""Terrain triangle clipper 0x514760 (near plane, then the scene edges, then the TL vertex output)."""
import math
import random
import struct

import pytest

from emu import STACK_BASE, STACK_SIZE, Emu, compare
from test_dxinit_util import with_cw

e = Emu()

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]
COUNT, ZBIAS, ZSCALE = 0xA7E070, 0xA7E074, 0xA7F894
SCENE, RHW, NEAR = 0xBCABD0, 0xBCAD70, 0xBCADC0
SY, SX, CX, CY = 0xC311A0, 0xC311A4, 0xC311B0, 0xC311B4


def f32(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def r32(v):
    return struct.unpack("<f", struct.pack("<f", v))[0]


class Scene:
    def __init__(self, rng):
        self.minx, self.miny = rng.choice([0.0, 10.5, -3.0]), rng.choice([0.0, 20.25])
        self.maxx, self.maxy = rng.choice([639.0, 511.5]), rng.choice([479.0, 383.75])
        self.near = rng.choice([1.0, 1.5, 0.25])
        self.sx, self.sy = rng.choice([320.0, 277.1]), rng.choice([-240.0, -311.3])
        self.cx, self.cy = (self.minx + self.maxx) / 2, (self.miny + self.maxy) / 2

    def write(self, em, rng):
        em.write(SCENE, struct.pack("<4f", self.minx, self.miny, self.maxx, self.maxy))
        em.write(NEAR, struct.pack("<f", self.near))
        em.write(SY, struct.pack("<2f", self.sy, self.sx))
        em.write(CX, struct.pack("<2f", self.cx, self.cy))
        em.write(RHW, struct.pack("<f", rng.uniform(0.001, 2.0)))
        em.write(ZSCALE, struct.pack("<f", rng.uniform(-0.5, 0.5)))
        em.write(ZBIAS, struct.pack("<f", rng.uniform(0.0, 1.0)))
        em.write(COUNT, struct.pack("<i", rng.randrange(-5, 5)))

    def code(self, sx, sy):
        sx, sy = r32(sx), r32(sy)
        return ((sy > self.maxy) << 3 | (sx > self.maxx) << 2 | (not sy >= self.miny) << 1 |
                (not sx >= self.minx))

    def vertex(self, rng, x, y, z):
        """16 dwords: screen or view position, colours, uv, outcode, pos, c[6]."""
        col, spec = rng.getrandbits(32), rng.getrandbits(32)
        u, v = rng.uniform(-2, 2), rng.uniform(-2, 2)
        if z < self.near:
            words = [f32(x), f32(y), f32(z), 0x11111111]
            code = 0x10
        else:
            q = r32(1.0 / z)
            sx, sy = x * self.sx * q + self.cx, y * self.sy * q + self.cy
            words = [f32(sx), f32(sy), f32(q), 0x22222222]
            code = self.code(sx, sy)
        c = [rng.uniform(0, 255) * r32(1.0 / max(z, 0.1)) for _ in range(3)] + [u, v, rng.uniform(0, 255)]
        return words + [col, spec, f32(u), f32(v), code | 0xABCDEF00, 0x33333333] + [f32(x) for x in c]


def output(em, rng):
    verts = em.alloc(0x20 * 256)
    em.write(verts, b"\xcc" * 0x20 * 256)
    vcount, tcount = em.alloc(4), em.alloc(4)
    em.write(vcount, struct.pack("<i", rng.randrange(0, 8)))
    em.write(tcount, struct.pack("<i", rng.randrange(0, 4)))
    idx = em.alloc(2 * 3 * 128)
    em.write(idx, b"\xdd" * 2 * 3 * 128)
    ctx = em.alloc(0x14)
    em.write(ctx, struct.pack("<IIIIi", verts, vcount, idx, tcount, rng.choice([0, 7, 0x7FF0, -3])))
    return ctx


def triangle(seed):
    rng = random.Random(seed)
    sc = Scene(rng)
    kind = seed % 6
    pts = []
    for _ in range(3):
        if kind == 0:      # in front, mostly on screen
            z = rng.uniform(2, 40)
            x, y = rng.uniform(-0.9, 0.9) * z * 2, rng.uniform(-0.9, 0.9) * z * 1.5
        elif kind == 1:    # in front, larger than the screen
            z = rng.uniform(2, 40)
            x, y = rng.uniform(-3, 3) * z * 2, rng.uniform(-3, 3) * z * 1.5
        elif kind == 2:    # crossing the near plane
            z = rng.uniform(-10, 30)
            x, y = rng.uniform(-60, 60), rng.uniform(-40, 40)
        elif kind == 3:    # crossing the near plane, small
            z = rng.uniform(-2, 6)
            x, y = rng.uniform(-3, 3), rng.uniform(-3, 3)
        elif kind == 4:    # one far corner
            z = rng.choice([rng.uniform(1.0, 3), rng.uniform(100, 5000)])
            x, y = rng.uniform(-500, 500), rng.uniform(-500, 500)
        else:
            z = rng.uniform(-1, 50)
            x, y = rng.uniform(-80, 80), rng.uniform(-80, 80)
        pts.append((x, y, z))
    return rng, sc, pts


def args_for(em, seed):
    rng, sc, pts = triangle(seed)
    sc.write(em, rng)
    words = []
    for p in pts:
        words += sc.vertex(rng, *p)
    return tuple(words) + (output(em, rng),)


def param_area(em):
    """The three vertex arguments after the call, without the pos pointers."""
    base = STACK_BASE + STACK_SIZE - 0x1000 - 4 * 49
    words = list(struct.unpack("<48I", em.read(base, 4 * 48)))
    for k in range(3):
        words[16 * k + 9] = 0
        words[16 * k + 8] &= 0xFF
    return words


def check(seed, cw, setup=None):
    setup = setup or (lambda em: args_for(em, seed))
    orig, ours = with_cw(e, 0x514760, cw), with_cw(e, "TerrDraw_ClipTriangle", cw)
    compare(e, orig, ours, setup=setup, check_eax=False)
    areas = []
    for target in (orig, ours):
        e.reset()
        a = setup(e)
        e.call(target, *a)
        areas.append(param_area(e))
    assert areas[0] == areas[1]
    return struct.unpack("<i", e.read(struct.unpack("<I", e.read(a[-1] + 12, 4))[0], 4))[0]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(120))
def test_clip(seed, cw):
    check(seed, cw)


EDGE = [(0.0, 0.0), (639.0, 479.0), (-1.0, 240.0), (700.0, 240.0), (320.0, -5.0), (320.0, 500.0), (-50.0, -50.0),
        (900.0, 900.0), (float("nan"), 100.0), (100.0, float("nan")), (639.0, 0.0), (0.0, 479.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("i", range(len(EDGE)))
@pytest.mark.parametrize("j", [1, 5, 7])
def test_clip_screen_points(i, j, cw):
    """Corners given directly in screen space, including NaN and points on the edges."""
    def setup(em):
        rng = random.Random(i * 31 + j)
        sc = Scene(rng)
        sc.minx, sc.miny, sc.maxx, sc.maxy = 0.0, 0.0, 639.0, 479.0
        sc.write(em, rng)
        words = []
        for p in (EDGE[i], EDGE[(i + j) % len(EDGE)], EDGE[(i + 2 * j) % len(EDGE)]):
            v = sc.vertex(rng, 0.0, 0.0, 5.0)
            v[0], v[1] = f32(p[0]), f32(p[1])
            v[8] = sc.code(*p) if p[0] == p[0] and p[1] == p[1] else (0x1 if p[0] != p[0] else 0) | (
                0x2 if p[1] != p[1] else 0) | sc.code(p[0] if p[0] == p[0] else 1.0, p[1] if p[1] == p[1] else 1.0)
            words += v
        return tuple(words) + (output(em, rng),)
    check(0, cw, setup)


RECTS = [(639.0, 479.0, 0.0, 0.0), (320.0, 240.0, 320.0, 240.0), (639.0, 0.0, 0.0, 479.0), (0.0, 479.0, 639.0, 0.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("rect", range(len(RECTS)))
@pytest.mark.parametrize("seed", range(0, 120, 5))
def test_clip_odd_rect(seed, rect, cw):
    """Inverted and zero size clip rectangles, so a clipped corner can lie outside another edge."""
    def setup(em):
        rng, sc, pts = triangle(seed)
        sc.minx, sc.miny, sc.maxx, sc.maxy = RECTS[rect]
        sc.write(em, rng)
        words = []
        for pt in pts:
            words += sc.vertex(rng, *pt)
        return tuple(words) + (output(em, rng),)
    check(seed, cw, setup)


def test_clip_coverage():
    """The random triangles reach the split paths and produce more than one triangle."""
    counts = [check(seed, 0x37F) for seed in range(0, 120, 7)]
    assert max(counts) > 4
