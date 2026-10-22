import random
import struct

import pytest

from emu import Emu, compare

emu = Emu()

POINT_INSIDE = 0x4C9020
BUMPS, BUMP_COUNT = 0xBA2030, 0xBA2058

CUBE = [(x, y, z) for z in (10, 40) for y in (10, 40) for x in (10, 40)]
CUBE_TRIS = [(0, 2, 1), (1, 2, 3), (4, 5, 6), (5, 7, 6), (0, 1, 4), (1, 5, 4),
             (2, 6, 3), (3, 6, 7), (0, 4, 2), (2, 4, 6), (1, 3, 5), (3, 7, 5)]


def leaf(e, verts, tris, split=None, ntris=None):
    split = len(verts) // 2 if split is None else split
    body = b"".join(struct.pack("<I", a | b << 8 | c << 16 | 0xAB000000) for a, b, c in tris)
    vb = b"".join(struct.pack("<I", (x << 26) | (y << 20) | (z << 14) | 0x1234) for x, y, z in verts)
    head = struct.pack("<4h", len(tris) if ntris is None else ntris, len(body), split, len(verts) - split)
    p = e.alloc(len(head) + len(body) + len(vb))
    e.write(p, head + body + vb)
    return p


def vec(e, x, y, z):
    p = e.alloc(24)
    e.write(p, struct.pack("<3d", x, y, z))
    return p


def random_mesh(rng):
    n = rng.randrange(4, 14)
    verts = [tuple(rng.randrange(64) for _ in range(3)) for _ in range(n)]
    tris = [tuple(rng.randrange(n) for _ in range(3)) for _ in range(rng.randrange(1, 20))]
    if rng.random() < 0.3:
        tris.append((0, 0, 1))
    return verts, tris


def set_bump(e, rng, base, unit):
    heights = e.alloc(66 * 66)
    e.write(heights, bytes(rng.randrange(256) for _ in range(66 * 66)))
    rec = BUMPS + 0x30
    scale = 40.0 / (64 * unit)
    e.write(rec + 0xC, struct.pack("<if", 3, scale))
    e.write(rec + 0x18, struct.pack("<2dI", base[0] - 45 * unit, base[1] - 45 * unit, heights))
    e.write(BUMP_COUNT, struct.pack("<i", 1))


def point_in(rng, base, unit, flags):
    return [b + rng.uniform(-30, 30) * unit for b in base]


@pytest.mark.parametrize("seed", range(40))
@pytest.mark.parametrize("bump", [0, 1])
def test_point_inside_random(seed, bump):
    def setup(e):
        rng = random.Random(seed * 2 + bump)
        verts, tris = random_mesh(rng)
        shift = rng.choice([12, 18, 20, 24])
        unit = (0x40000000 >> shift) / 24.0
        base = [rng.uniform(-1e5, 1e5) for _ in range(3)]
        if bump:
            set_bump(e, rng, base, unit)
        flags = rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 0x13])
        out = e.alloc(4)
        e.write(out, b"\x77\x77\x77\x77")
        return leaf(e, verts, tris), vec(e, *base), vec(e, *point_in(rng, base, unit, flags)), shift | 0x300, out, \
            flags
    compare(emu, POINT_INSIDE, "Coll_LeafPointInside", setup=setup)


@pytest.mark.parametrize("seed", range(30))
def test_point_inside_cube(seed):
    def setup(e):
        rng = random.Random(seed)
        shift = 20
        unit = (0x40000000 >> shift) / 24.0
        base = [rng.uniform(-1e4, 1e4) for _ in range(3)]
        flags = rng.randrange(8) | rng.choice([0, 0, 8])
        tris = CUBE_TRIS[:]
        rng.shuffle(tris)
        # the cube spans -14..16 units around base before rotation
        p = [b + rng.uniform(-20, 20) * unit for b in base]
        out = e.alloc(4)
        return leaf(e, CUBE, tris, split=rng.randrange(9)), vec(e, *base), vec(e, *p), shift, out, flags
    compare(emu, POINT_INSIDE, "Coll_LeafPointInside", setup=setup)


@pytest.mark.parametrize("ntris", [0, -3])
def test_point_inside_empty(ntris):
    def setup(e):
        out = e.alloc(4)
        e.write(out, b"\x77\x77\x77\x77")
        return leaf(e, CUBE, CUBE_TRIS, ntris=ntris), vec(e, 0, 0, 0), vec(e, 1, 2, 3), 20, out, 0
    compare(emu, POINT_INSIDE, "Coll_LeafPointInside", setup=setup)


def test_point_inside_degenerate():
    def setup(e):
        out = e.alloc(4)
        e.write(out, b"\x77\x77\x77\x77")
        return leaf(e, CUBE, [(0, 0, 1), (2, 2, 2)]), vec(e, 0, 0, 0), vec(e, 1, 2, 3), 20, out, 0
    compare(emu, POINT_INSIDE, "Coll_LeafPointInside", setup=setup)
