"""QCube_Walk (0x4D14E0): deep walks, lit cells, hash neighbours and odd cache frames."""
import struct

import pytest

from test_lod_util import TASK_HANDLERS, check, u32
from test_lod_walk import CACHE, FRAME, LIGHTMAPS, LIGHTS, MINSIZE, ORIGIN, TYPES, e, fk, near_terrain, scene, visited


def light_up(em, a):
    """a task handler that leaves a visible light map behind"""
    em.write(LIGHTMAPS, struct.pack("<2I", 0x2A, 0x2B))
    u32(em, LIGHTS, 2)
    return 0


H15_LIGHT = fk.func(2, ret=light_up, name="h15_light")
e.baseline = e.snapshot()


def cam_origin(em):
    pos = struct.unpack("<3d", em.read(0xBCAB08, 24))
    return [round(v) for v in pos], list(struct.unpack("<3i", em.read(ORIGIN, 12)))


def walk_setup(seed, kind, mod=None):
    def setup(em):
        w, rng, cur, elems = scene(em, seed, kind)
        u32(em, LIGHTS, 0)
        if mod:
            mod(em, w, rng, cur, elems)
        return (w,)
    return setup


def deep(em, w, rng, cur, elems):
    """cells far below the size limit: pushed until the depth limit"""
    cam, origin = cam_origin(em)
    u32(em, w + 0xC, near_terrain(em, rng, cam, origin, 3000, 18, 0.6))
    u32(em, MINSIZE, 0x7FFFFFFF)


def lit(em, w, rng, cur, elems):
    for t in TYPES:
        u32(em, TASK_HANDLERS + 4 * (15 * 0x180 + t), H15_LIGHT)


def odd_frame(em, w, rng, cur, elems):
    """a cache entry with high frame bits and an old low half"""
    u32(em, FRAME, 40)
    em.write(CACHE + 0x1C * 0x123, struct.pack("<4iiiI", 0x7FFFFFF0, 0, 0, 0, 0x10001, 0, elems[0].pop()))


def neighbours(seed, kind, cw):
    """the slot of each cell visited last frame holds the same x and y but another z"""
    found = visited(seed, kind, cw)

    def mod(em, w, rng, cur, elems):
        prev = (cur - 1) & 0xFFFF
        for i, ent in found:
            em.write(CACHE + 0x1C * i, struct.pack("<4iiiI", ent[0], ent[1], ent[2] + 1, ent[3], prev, 0, 0))
    return mod


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", ["deep", "lit", "odd_frame", "neighbours"])
@pytest.mark.parametrize("seed", [1, 2])
def test_walk_cases(case, seed, cw):
    mod = {"deep": deep, "lit": lit, "odd_frame": odd_frame}.get(case)
    kind = "both" if case == "lit" else "terrain"
    if case == "lit":
        seed += 2 if seed == 1 else 4  # scenes 3 and 6 reach a lit cell
    if case == "neighbours":
        mod = neighbours(seed, kind, cw)
    check(e, fk, 0x4D14E0, "QCube_Walk", setup=walk_setup(seed, kind, mod), check_eax=False, cw=cw)
