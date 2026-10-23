import random
import struct

import pytest

from emu import ErrorShown, compare
from test_d3d_util import D3DEmu, check, check_raises, com, pyfake, u32
from test_dxinit_util import VB, check_cw, d3d, device, f32, f64, noise, set_view, vertex_buffer

e = D3DEmu()

pyfake(e, 0x4B0EC0, "RenderMode_SetMethod", 3)
pyfake(e, 0x495200, "Direct3D_AddRestoreCallback", 1)
pyfake(e, 0x401400, "TaskType_SetHandler", 0,
       before=lambda e: (None, (e.arg(0) & 0xFFFF, e.arg(1) & 0xFF, e.arg(2))))
PTRLIST_N = [0]


def ptrlist_add(e):
    PTRLIST_N[0] += 1
    return 0x40 + PTRLIST_N[0] % 7


pyfake(e, 0x4B7430, "PtrList_Add", 2, before=ptrlist_add)

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]


# --- Float_Log2 and the spline cache ------------------------------------------------------

@pytest.mark.parametrize("v", [0, 1.0, 0.5, 1.0000001, 2.0, 3.0, 1e10, -2.0, float("inf"), 0x7FFFFFFF, 0x80000000,
                               0x3F800001, 0xFFFFFFFF])
def test_float_log2(v):
    bits = v if isinstance(v, int) else struct.unpack("<I", struct.pack("<f", v))[0]
    compare(e, 0x499FF0, "Float_Log2", args=(bits,))


@pytest.mark.parametrize("hit", [0, 1])
def test_spline_cache_remove(hit):
    def setup(e):
        e.write(0x720558, struct.pack("<4I", 0x1234, 0x5555, 0x6666, 0x7777))
        return (0x1234 if hit else 0x5555,)
    compare(e, 0x49A010, "SplineCache_Remove", setup=setup, check_eax=False)


def test_spline_cache_reset():
    compare(e, 0x49A340, "SplineCache_Reset", setup=lambda e: e.write(0x720558, b"\x11" * 8), check_eax=False)


# --- SplineMesh_GetVerts --------------------------------------------------------------------

def spline_obj(e, rng, rigid, reversed_, segments, count):
    o = e.alloc(0x5C0)
    e.write(o, noise(0x5C0, rng.randrange(40)))
    start, end = rng.uniform(-500, 200), rng.uniform(300, 3000)
    if rng.random() < 0.2:
        start, end = end, start
    e.write(o + 0x70, f32(*[rng.uniform(-1.2, 1.2) for _ in range(9)]))
    e.write(o + 0xFC, f32(start, end))
    e.write(o + 0x104, f32(*[rng.uniform(-900, 900) for _ in range(63)]))
    e.write(o + 0x200, f32(*[rng.uniform(-1.2, 1.2) for _ in range(230)]))
    u32(e, o + 0x59C, segments)
    e.write(o + 0x5B4, bytes([rigid, reversed_]))
    verts = e.alloc(16 * max(count, 1))
    lo, hi = min(start, end), max(start, end)
    for i in range(count):
        x = rng.choice([rng.uniform(lo, hi), lo, hi, lo - 50, hi + 50])
        e.write(verts + 16 * i, f32(x, rng.uniform(-300, 300), rng.uniform(-300, 300), 7.0))
    parts = e.alloc(0x20 * 3)
    e.write(parts, noise(0x60, 9))
    u32(e, parts + 0x20 + 4, count)
    u32(e, parts + 0x20 + 0x14, verts)
    b = e.alloc(0x30)
    u32(e, b + 0x2C, parts)
    a = e.alloc(0x10)
    u32(e, a + 0xC, b)
    model = e.alloc(0x10)
    u32(e, model + 0xC, a)
    models = e.alloc(8)
    u32(e, models + 4, model)
    u32(e, o + 0x6C, models)
    return o


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(5))
@pytest.mark.parametrize("rigid,reversed_", [(0, 0), (0, 1), (1, 0), (1, 1)])
@pytest.mark.parametrize("count,out", [(0, 1), (1, 0), (40, 1)])
def test_spline_get_verts(seed, rigid, reversed_, count, out, cw):
    def setup(e):
        rng = random.Random(seed)
        o = spline_obj(e, rng, rigid, reversed_, rng.randrange(1, 21), count)
        p = e.alloc(4) if out else 0
        return o, 1, 1, 0x77, 0x88, p
    check_cw(e, 0x49A050, "SplineMesh_GetVerts", cw, setup=setup)


# --- Fog_SetRange ---------------------------------------------------------------------------

@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("radius,density", [(4096.0, 0.3), (4096.0, 0.0), (0.0, 0.5), (100.0, 3.0), (4096.0, 1e6)])
def test_fog_set_range(seed, radius, density, cw):
    def setup(e):
        rng = random.Random(seed)
        set_view(e, rng)
        device(e)
        e.write(0xB46D04, f32(density))
        pos = e.alloc(24)
        view = struct.unpack("<3d", e.read(0xBCAB08, 24))
        e.write(pos, f64(*[c + rng.uniform(-40000, 40000) for c in view]))
        return pos, struct.unpack("<I", f32(radius))[0]
    check_cw(e, 0x49B2D0, "Fog_SetRange", cw, setup=setup, check_eax=False)


# --- mesh batches ----------------------------------------------------------------------------

BATCH = 0x7205C0


def batch(e, rng, lst, n, total, formats=(0, 1, 3)):
    vb, _ = vertex_buffer(e, 0x20000)
    d3d(e, vb)
    base = BATCH + lst * 0xFA8
    for i in range(n):
        fmt = rng.choice(formats)
        item = e.alloc(0x28)
        m = e.alloc(0x40)
        e.write(m, noise(0x40, i))
        cnt = rng.randrange(1, 20)
        data = e.alloc(cnt * 0x28)
        e.write(data, noise(cnt * 0x28, i + 3))
        if fmt == 3:
            u32(e, m + 0x10, cnt)
            u32(e, m + 0x14, data)
        else:
            u32(e, m + 0xC, cnt)
            u32(e, m + 0x10, data)
        u32(e, item + 0x20, fmt)
        u32(e, item + 0x24, m)
        u32(e, base + 4 * i, item)
    e.write(base + 0xFA0, struct.pack("<ii", total, n))
    u32(e, 0x72350C, rng.randrange(5))


@pytest.mark.parametrize("lst", [0, 1, 2])
@pytest.mark.parametrize("tnl", [0, 1])
@pytest.mark.parametrize("seed", range(3))
def test_mesh_batch_flush(lst, tnl, seed):
    def setup(e):
        rng = random.Random(seed)
        u32(e, 0xBCAD74, tnl)
        batch(e, rng, lst, rng.choice([0, 1, 12]), rng.randrange(1000))
        return (lst,)
    check(e, 0x49B4B0, "MeshBatch_Flush", setup=setup, check_eax=False)


def test_mesh_batch_flush_bad_format():
    def setup(e):
        batch(e, random.Random(1), 0, 3, 50, formats=(2,))
        return (0,)
    check_raises(e, 0x49B4B0, "MeshBatch_Flush", setup=setup)


@pytest.mark.parametrize("count,total,verts", [(0, 0, 10), (5, 0xFFF0, 0xF), (5, 0xFFF0, 0x10), (999, 10, 5),
                                               (998, 10, 5), (3, 100, 0x10000)])
def test_mesh_batch_add(count, total, verts):
    def setup(e):
        rng = random.Random(count)
        batch(e, rng, 1, 1, total, formats=(1,))
        item = e.u32(BATCH + 0xFA8)
        e.write(BATCH + 0xFA8, struct.pack("<1000I", *[item] * 1000))
        u32(e, BATCH + 0xFA8 + 0xFA4, count)
        return 1, item, verts
    check(e, 0x49B430, "MeshBatch_Add", setup=setup, check_eax=False)


# --- BoneMesh_Skin ----------------------------------------------------------------------------

def bone_mesh(e, rng, count, extra):
    mesh = e.alloc(0x20)
    e.write(mesh, noise(0x20, 4))
    verts = e.alloc(0x28 * (count + extra) + 4)
    for i in range(count + extra):
        e.write(verts + 0x28 * i, f32(*[rng.uniform(-500, 500) for _ in range(3)] +
                                       [rng.uniform(-1, 1) for _ in range(3)] +
                                       [rng.uniform(0, 1), rng.uniform(0, 1), rng.uniform(0, 1)]) +
                struct.pack("<HH", rng.randrange(max(count, 1)), rng.randrange(6)))
    e.write(mesh + 0xC, struct.pack("<iiI", extra, count, 0) + struct.pack("<I", verts))
    bones = e.alloc(0x84 * 6)
    for b in range(6):
        e.write(bones + 0x84 * b, noise(0x84, b) [:0x18] + f32(*[rng.uniform(-2000, 2000) for _ in range(3)]) +
                f32(*[rng.uniform(-1.1, 1.1) for _ in range(9)]) + noise(0x3C, b))
    holder = e.alloc(0x10)
    u32(e, holder + 8, bones)
    model = e.alloc(0x40)
    e.write(model + 0x14, b"soldier_bone\0")
    return mesh, model, holder


def normal_vb(e, rng, used, size, lock_ret=0):
    vb, buf = vertex_buffer(e, 0x20 * 4200, lock_ret=lock_ret)
    e.write(0xBCAD90, struct.pack("<Iii", vb, size, used))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("rigid", [0, 1])
@pytest.mark.parametrize("used,count,extra", [(0, 30, 10), (100, 30, 0), (3990, 30, 7), (0, 0, 5)])
def test_bone_mesh_skin(seed, rigid, used, count, extra, cw):
    def setup(e):
        rng = random.Random(seed)
        normal_vb(e, rng, used, 4000)
        return (*bone_mesh(e, rng, count, extra), rigid)
    check_cw(e, 0x49B700, "BoneMesh_Skin", cw, setup=setup)


@pytest.mark.parametrize("used", [0, 3990])
def test_bone_mesh_skin_lock_fails(used):
    def setup(e):
        rng = random.Random(1)
        normal_vb(e, rng, used, 4000, lock_ret=0x80004005)
        return (*bone_mesh(e, rng, 20, 2), 0)
    check(e, 0x49B700, "BoneMesh_Skin", setup=setup)


def test_bone_mesh_too_large():
    def setup(e):
        rng = random.Random(1)
        normal_vb(e, rng, 0, 9000)
        mesh, model, holder = bone_mesh(e, rng, 4, 0)
        u32(e, mesh + 0x10, 4001)
        return mesh, model, holder, 0
    assert "soldier_bone" in check_raises(e, 0x49B700, "BoneMesh_Skin", setup=setup)


def test_direct3d_render_init():
    def setup(e):
        PTRLIST_N[0] = 0
        for p in (0x721560, 0x721560 + 0xFA8, 0x721560 + 0x1F50):
            e.write(p, b"\x33" * 8)
        u32(e, 0x72350C, 9)
        u32(e, 0xB817A0, 0x51)
        u32(e, 0xB8170C, 0x52)
        u32(e, 0x5CA0E0, 0x3)
        e.write(0x548440, b"\x07")
        e.write(0xA5E3F4, b"\x34\x12")
    check(e, 0x49BB40, "Direct3DRender_Init", setup=setup, check_eax=False)
