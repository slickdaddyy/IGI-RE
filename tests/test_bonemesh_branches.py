"""RigidDynCubeObj_NearZHandler (0x4A1570) and Direct3DRender_ClipTriangleDepth (0x4A0340) branch cases."""
import pytest

from test_bonemesh_render import emu, groups, lightmap_groups, near_setup
from test_bonemesh_util import d64, f32, handler, run, set_handler, u32, with_cw


def unlink_next(e, a):
    """the handler drops the next sibling's link, as a deleting handler would"""
    nxt = e.u32(a[0])
    if nxt:
        u32(e, nxt, 0)


UNLINK = handler(emu, "near_unlink", 2, effect=unlink_next)
ID = (1, 0, 0, 0, 1, 0, 0, 0, 1)


def parts(e, t):
    vm = e.u32(t + 0x6C)
    header = e.u32(e.u32(e.u32(vm) + 0xC) + 0xC)
    return vm, header, e.u32(header + 0x24)


def near_case(seed, fmt=0, mod=None, **kw):
    def setup(e):
        t, arg = near_setup(e, seed, fmt, **kw)
        if mod:
            mod(e, t, arg)
        return t, arg
    return setup


def no_lod(e, t, arg):
    u32(e, parts(e, t)[0], 0)


def no_verts(e, t, arg):
    _, h, r = parts(e, t)
    u32(e, r + (0xC if e.u32(h + 0x20) == 0 else 0x10), 0)


def short_group(e, t, arg):
    _, h, r = parts(e, t)
    if e.u32(h + 0x20) == 0:
        u32(e, r + 0x14, groups(e, [(0, 1, 0, [1, 2])]))
    else:
        u32(e, r + 0x1C, lightmap_groups(e, [(1, [1, 2])]))


def touching(e, t, arg):
    """identity frames and whole numbers: the near side lands exactly on the depth so far"""
    _, h, r = parts(e, t)
    f32(e, 0xBCAAE0, *ID)
    d64(e, 0xBCAB08, 0.0, 0.0, 0.0)
    d64(e, t + 0x20, 10.0, 20.0, 100.0)
    f32(e, t + 0x70, *ID)
    f32(e, t + 0x98, 1.0, 1.0, 1.0)
    f32(e, h + 0x30, 0.0, 0.0, 0.0, 30.0)
    f32(e, arg + 8, 70.0)


def detached(e, t, arg):
    u32(e, t + 0xE8, 0)


def unlinking_kid(e, t, arg):
    set_handler(e, 0x36, 0x60, UNLINK)


def no_kid_handler(e, t, arg):
    set_handler(e, 0x36, 0x61, 0)


NEAR = [near_case(1, 0, no_lod), near_case(2, 0, no_verts), near_case(3, 3, no_verts),
        near_case(4, 0, short_group, min_z=1e6), near_case(5, 3, short_group, min_z=1e6),
        near_case(6, 0, touching), near_case(7, 3, touching), near_case(8, 0, detached),
        near_case(9, 0, unlinking_kid, kids=3), near_case(10, 3, no_kid_handler, kids=3)]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", range(len(NEAR)))
def test_near_z(case, cw):
    run(emu, with_cw(emu, 0x4A1570, cw), with_cw(emu, "RigidDynCubeObj_NearZHandler", cw), NEAR[case])


# --- Direct3DRender_ClipTriangleDepth --------------------------------------------

import random  # noqa: E402
import struct  # noqa: E402

from test_bonemesh_render import clip_vertex  # noqa: E402


def inverted_clip_setup(e, seed):
    """clip_setup with a scene box whose minimum is above its maximum on x or y"""
    rng = random.Random(seed)
    near = rng.uniform(0.5, 10)
    minx, maxx = rng.uniform(300, 600), rng.uniform(0, 280)
    miny, maxy = rng.uniform(250, 450), rng.uniform(0, 230)
    if seed % 3 == 1:
        minx, maxx = rng.uniform(0, 50), rng.uniform(500, 800)
    elif seed % 3 == 2:
        miny, maxy = rng.uniform(0, 50), rng.uniform(400, 600)
    ctx = [rng.uniform(200, 500), rng.uniform(200, 500), rng.uniform(250, 400), rng.uniform(200, 300),
           minx, miny, maxx, maxy]
    ctx = [struct.unpack("<f", struct.pack("<f", v))[0] for v in ctx]
    f32(e, 0xBCAB28, *ctx[:4])
    f32(e, 0xBCABD0, *ctx[4:])
    u32(e, 0x7AF1E0, 3)
    out = e.alloc(4)
    f32(e, out, rng.choice([1e9, rng.uniform(1, 300)]))
    args = []
    for _ in range(3):
        v, code = clip_vertex(rng, near, ctx)
        args += list(struct.unpack("<6I", struct.pack("<6f", *v))) + [code]
    return args + [out, struct.unpack("<I", struct.pack("<f", near))[0]]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", [0, 1, 24, 52])
def test_clip_triangle_depth_inverted_box(seed, cw):
    """a corner moved onto the left or top edge is still out on the other side"""
    run(emu, with_cw(emu, 0x4A0340, cw), with_cw(emu, "Direct3DRender_ClipTriangleDepth", cw),
        lambda e: inverted_clip_setup(e, seed))
