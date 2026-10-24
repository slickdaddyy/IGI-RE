"""Branch cases for Lightmap_BuildLod (0x523D40) and Dirlight_TraceVertices (0x525460)."""
import random
import struct

import pytest

from emu import Emu
from test_dirlight_lightmap import build_mesh, olm_file, resource_load
from test_dirlight_util import CWS, RET, f32, fbits, logfake, pyfake, run, u8, u16, u32


def alloc(e, a):
    """Mem_Alloc; the calls listed in RET["null"] fail"""
    n = RET.get("allocs", 0)
    RET["allocs"] = n + 1
    return 0 if n in RET.get("null", ()) else e.alloc(a[0])


be = Emu()
logfake(be, 0x4B0C60, 2, effect=alloc)
logfake(be, 0x4B0D60, 1)  # Mem_PushMode
logfake(be, 0x4B0D90, 0)  # Mem_PopMode
logfake(be, 0x4B2610, 1)  # Texture_SetActiveTMU
logfake(be, 0x4B2320, 8)  # Texture_Alloc
logfake(be, 0x4B5B60, 1, strings=(0,))  # Resource_Exists
logfake(be, 0x4B63D0, 1)  # Resource_Flush
logfake(be, 0x52D630, 2, derefs={0: 28, 1: 28})  # SystemTime_Compare
logfake(be, 0x52D440, 1, effect=lambda e, a: u32(e, a[0], 2000, 1, 2, 3, 4, 5, 6))  # SystemTime_Get
pyfake(be, 0x4B5F00, resource_load)
be.baseline = be.snapshot()


def lod_of(e, mesh):
    header = e.alloc(0x28)
    f32(e, header, 0.1)
    u32(e, header + 4, 1999, 1, 2, 3, 4, 5, 6)
    u32(e, header + 0x20, 3, mesh)
    data = e.alloc(0x10)
    u32(e, data + 0xC, header)
    lod = e.alloc(0x10)
    u32(e, lod + 0xC, data)
    return lod


def small_mesh(e, verts, chunks, sizes):
    """verts: (pos, uv); chunks: (texture, [index triples]) in one block; sizes: (w, h) per texture."""
    vp = e.alloc(0x28 * len(verts))
    for i, (pos, uv) in enumerate(verts):
        f32(e, vp + 0x28 * i, *pos)
        f32(e, vp + 0x28 * i + 0x20, *uv)
    sp = e.alloc(8 * max(len(sizes), 1))
    for i, (w, h) in enumerate(sizes):
        u16(e, sp + 8 * i, w, h)
    block = e.alloc(0x400)
    pos = block
    for ci, (texture, tris) in enumerate(chunks):
        idx = [v for t in tris for v in t]
        size = 0x20 + 2 * len(idx)
        u16(e, pos + 0xC, len(idx), 0xFFFF if ci == len(chunks) - 1 else size - 0x20 + 4)
        u8(e, pos + 0x10, 0x10 + ci)
        u16(e, pos + 0x12, texture, 0)
        u8(e, pos + 0x1E, ci & 1)
        if idx:
            u16(e, pos + 0x20, *idx)
        pos += size + 4
    mesh = e.alloc(0x30)
    u32(e, mesh + 4, len(sizes), 1)
    u32(e, mesh + 0x14, vp, sp, block)
    u32(e, mesh + 0x28, 0x77)
    return mesh


def call_args(e, lod, seed=0, full=0):
    name = e.alloc(16)
    e.write(name, b"room\0")
    u32(e, 0xA83B4C, 100)
    u32(e, 0xA839F4, 50)
    return lod, seed % 2, fbits(2048.0), name, seed, full, 1, 0


def mesh_setup(verts, chunks, sizes, null=()):
    def setup(e):
        RET[0x4B5B60] = 0
        RET["null"] = null
        RET["allocs"] = 0
        return call_args(e, lod_of(e, small_mesh(e, verts, chunks, sizes)))
    return setup


def run_build(setup, cw=0x37F):
    run(be, 0x523D40, "Lightmap_BuildLod", setup, cw=cw, check_eax=True)


def tri(pos, uvs=((0.0, 0.0), (1.0, 0.0), (0.0, 1.0))):
    return list(zip(pos, uvs))


ZP = [(0, 0, 5), (10, 0, 5), (0, 10, 5)]
YP = [(0, 3, 0), (10, 3, 0), (0, 3, 10)]
LINE = [(0, 0, 0), (1, 1, 1), (2, 2, 2)]
ONE = [(0, [(0, 1, 2)])]

BUILDS = [
    mesh_setup(tri(ZP), ONE, []),                               # no textures
    mesh_setup(tri(ZP), ONE, [(2, 2)], null=(1,)),              # no room for the texel info
    mesh_setup(tri(ZP), ONE, [(2, 0), (2, 2)]),                 # empty textures
    mesh_setup(tri(ZP), ONE, [(0, 2), (2, 2)]),
    mesh_setup(tri(ZP), [(0, []), (0, [(0, 1, 2)])], [(2, 2)]),  # a chunk without triangles
    mesh_setup(tri(ZP), ONE, [(2, 2)]),                         # normals along z, y and none
    mesh_setup(tri(YP), ONE, [(2, 2)]),
    mesh_setup(tri(LINE), ONE, [(2, 2)]),
    # positions on a line whose Heron product rounds below zero
    mesh_setup(tri([(3.90625, 4.84375, -0.4296875), (3.6875, 2.65625, 0.171875), (3.625, 2.03125, 0.34375)]),
               ONE, [(2, 2)]),
    # texture coordinates on a line: no texture area but clipped area left
    mesh_setup(tri(ZP, [(0.208984375, 0.3125), (0.390625, 0.40625), (0.572265625, 0.5)]), ONE, [(2, 2)]),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(BUILDS)))
def test_build(case, cw):
    run_build(BUILDS[case], cw)


def test_build_load_empty():
    def setup(e):
        RET[0x4B5B60] = 1
        RET[0x52D630] = -1
        RET["file"] = olm_file(e, random.Random(1), 0)
        return call_args(e, lod_of(e, small_mesh(e, tri(ZP), ONE, [])))
    run_build(setup)


# --- Dirlight_TraceVertices ----------------------------------------------------

from test_dirlight_lightmap import te, trace_setup  # noqa: E402


def trace_case(seed, mod):
    base = trace_setup(seed)

    def setup(e):
        dl, arg = base(e)
        mod(e, dl, e.u32(e.u32(arg + 24) + 0x88), arg)
        return dl, arg
    return setup


def light_dir(d):
    return lambda e, dl, lset, arg: f32(e, dl + 0x24, *d)


def no_objects(e, dl, lset, arg):
    u32(e, lset, 0)


def facing(e, dl, lset, arg):
    """small set, every vertex facing the light: the facing-angle light wins by far"""
    f32(e, lset + 8, 1.0)
    f32(e, e.u32(lset + 0x18 + 0xC), 1, 0, 0, 0, 1, 0, 0, 0, 1)
    f32(e, dl + 0x24, 0.0, 0.0, 1.0)
    for i in range(8):
        f32(e, e.u32(e.u32(arg + 12) + 4 * i), 1000.0)
        f32(e, e.u32(e.u32(arg + 20) + 4 * i), 0.0, 0.0, 1.0)


TRACES = [trace_case(1, no_objects), trace_case(2, light_dir((0.0, 0.7, 0.0))),
          trace_case(4, light_dir((0.0, 0.0, -0.4))), trace_case(2, facing), trace_case(4, facing)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(TRACES)))
def test_trace(case, cw):
    run(te, 0x525460, "Dirlight_TraceVertices", TRACES[case], cw=cw)
