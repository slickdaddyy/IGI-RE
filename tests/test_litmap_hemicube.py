"""Hemicube triangle drawing and clipping (0x521110 - 0x521E40)."""
import random
import struct

import pytest

from test_litmap_util import CWS, FISTP_CWS, f32, new_emu, run, u32

e = new_emu()
SIZE = 32


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def frame(e, rng, affine, clamp):
    """Id and depth buffers, an 8x8 id texture and the clip rectangle."""
    color = e.alloc(4 * SIZE * SIZE)
    depth = e.alloc(4 * SIZE * SIZE)
    far = 1e9 if affine else 0.0
    f32(e, depth, *[far if rng.random() < 0.8 else rng.uniform(0, 2) for _ in range(SIZE * SIZE)])
    ids = e.alloc(4 * 64)
    u32(e, ids, *range(0x100, 0x140))
    rect = [rng.randrange(4), rng.randrange(4), SIZE - rng.randrange(4), SIZE - rng.randrange(4)] if clamp \
        else [0, 0, SIZE, SIZE]
    return [color, depth, SIZE, ids, 8, 8] + rect


def vec(e, *v):
    p = e.alloc(4 * len(v))
    f32(e, p, *v)
    return p


def triangle(rng, affine):
    pts = []
    for _ in range(3):
        x, y = rng.uniform(-8, SIZE + 8), rng.uniform(-8, SIZE + 8)
        if rng.random() < 0.2:
            x, y = float(int(x)), float(int(y)) + 0.5
        z = rng.uniform(1, 50) if affine else 1 / rng.uniform(1, 50)
        u, v = rng.uniform(-2, 10), rng.uniform(-2, 10)
        if not affine:
            u, v = u * z, v * z
        pts.append(((x, y, z), (u, v)))
    return pts


SPECIAL = {
    "flat_top": [((2.0, 3.0, 5.0), (0, 0)), ((20.0, 3.0, 6.0), (4, 0)), ((10.0, 25.0, 7.0), (2, 6))],
    "flat_bottom": [((10.0, 2.0, 5.0), (0, 0)), ((2.0, 20.0, 6.0), (4, 0)), ((25.0, 20.0, 7.0), (2, 6))],
    "flat": [((1.0, 5.0, 5.0), (0, 0)), ((9.0, 5.0, 6.0), (4, 0)), ((20.0, 5.0, 7.0), (2, 6))],
    "line": [((1.0, 1.0, 5.0), (0, 0)), ((5.0, 5.0, 6.0), (4, 0)), ((9.0, 9.0, 7.0), (2, 6))],
    "thin_top": [((10.0, 4.2, 5.0), (0, 0)), ((3.0, 4.6, 6.0), (4, 0)), ((25.0, 28.0, 7.0), (2, 6))],
    "nan": [((10.0, float("nan"), 5.0), (0, 0)), ((3.0, 4.6, 6.0), (4, 0)), ((25.0, 28.0, 7.0), (2, 6))],
}


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("affine", [0, 1])
@pytest.mark.parametrize("case", list(range(12)) + list(SPECIAL))
def test_draw_triangle(case, affine, cw):
    def setup(e):
        rng = random.Random(str(case) + str(affine))
        if isinstance(case, int):
            pts = triangle(rng, affine)
        else:
            pts = [((x, y, z if affine else 1 / z), (u, v)) for (x, y, z), (u, v) in SPECIAL[case]]
        args = frame(e, rng, affine, isinstance(case, int) and case % 3 == 0)
        args += [vec(e, *p) for p, _ in pts] + [vec(e, *t) for _, t in pts] + [affine]
        return tuple(args)
    run(e, 0x521110, "Hemicube_DrawTriangle", setup, cw=cw)


ZS = [(10.0, 20.0, 30.0), (0.0, 5.0, 1.0), (-1.0, 20.0, 30.0), (10.0, -2.5, 30.0), (10.0, 20.0, -0.5),
      (-1.0, -2.0, 30.0), (10.0, -2.0, -3.0), (-1.0, 20.0, -7.0), (-1.0, -2.0, -3.0), (float("nan"), 5.0, 6.0),
      (-0.001, 4.0, 4.5), (0.0, -1.0, 0.0), (-100.0, 1000.0, 0.1)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("zs", ZS)
@pytest.mark.parametrize("seed", [1, 2])
def test_clip_triangle(zs, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        affine = seed - 1
        ps = [vec(e, rng.uniform(-50, 50), rng.uniform(-50, 50), z) for z in zs]
        ts = [vec(e, rng.uniform(0, 8), rng.uniform(0, 8)) for _ in range(3)]
        view = [fbits(SIZE / 2), fbits(SIZE / 2 - 0.5), fbits(rng.uniform(10, 40)), fbits(-rng.uniform(10, 40))]
        return tuple(frame(e, rng, affine, False) + view + ps + ts + [affine])
    run(e, 0x521960, "Hemicube_ClipTriangle", setup, cw=cw)
