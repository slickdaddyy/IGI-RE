import math
import random
import struct

import pytest

from emu import ErrorShown
from test_render_util import (D3D, DEVICE, NEAR_PLANE, VB, VB_NORMAL, VIEW, Rig, device_slots, f32, f64,
                              make_state_fns, rmatrix, setup_states, setup_view, u32)

rig = Rig()
e0 = rig.e
dev = rig.com("dev", device_slots())
vbs = [VB(rig, "vb%d" % i) for i in range(6)]
VB_NEW = VB(rig, "vbnew")
d3d = rig.com("d3d", {5: (4, lambda e, a: (u32(e, a[2], VB_NEW.obj), 0)[1], lambda e, a: (e.read(a[0], 16), a[2]))})
make_state_fns(rig)

SORT = {}


def sort_add(e, a):
    p = SORT["next"]
    SORT["next"] += a[1]
    return p


def fogfmt(e, a):
    return (e.read(a[0], 24), a[1])


def clipfmt(e, a):
    return (rig.norm(a[0]), "stack")


INSIDE = {"ret": 0, "inside": 1}


def clip_ret(e, a):
    u32(e, a[1], INSIDE["inside"])
    return INSIDE["ret"]


LOD = {"lod": 0}
FRUSTUM = {"ret": 0}

rig.fake(0x49B2D0, "SetFog", 2, fmt=fogfmt)
rig.fake(0x49B4B0, "MeshBatch_Flush", 1)
rig.fake(0x4D0510, "Mesh3D_ExcludeRigid", 2, fmt=clipfmt, ret=clip_ret)
rig.fake(0x4D0790, "Mesh3D_ExcludeSpline", 2, fmt=clipfmt, ret=clip_ret)
rig.fake(0x4CFCB0, "ModelObj_GetLOD", 1, ret=lambda e, a: LOD["lod"])
rig.fake(0x4D0180, "Mesh3D_GetSplineMeshLOD", 1, ret=lambda e, a: LOD["lod"])
rig.fake(0x4D03D0, "Mesh3D_Exclude", 3, fmt=lambda e, a: (e.read(a[0], 12), a[1], a[2]), ret=lambda e, a: FRUSTUM["ret"])
rig.fake(0x491D40, "Display_GetAspect", 0, ret=4.0 / 3.0, st0=True)
rig.fake(0x497E70, "TransContext_Select", 1, fmt=lambda e, a: (e.read(a[0], 0xA8),))
HANDLER = rig.func("handler", 2)


def near_fmt(e, a):
    """sub_4A0340(ProjVert a, b, c, float *nearZ, float near): the screen fields of vertices behind the
    near plane are stale and not compared."""
    out = []
    for v in range(3):
        w = a[v * 7:v * 7 + 7]
        out.append(tuple(w[:3]) + ((None,) * 3 if w[6] == 0x10 else tuple(w[3:6])) + (w[6],))
    return tuple(out) + (e.read(a[21], 4), a[22])


def near_ret(e, a):
    z = struct.unpack("<f", struct.pack("<I", a[2]))[0]
    cur = struct.unpack("<f", e.read(a[21], 4))[0]
    if z * 0.75 < cur:
        e.write(a[21], f32(z * 0.75))
    return 0


rig.fake(0x4A0340, "Direct3DRender_ClipTriangleDepth", 23, fmt=near_fmt, ret=near_ret)


def sort_fmt(e, a):
    return (rig.norm(a[0]), a[1], a[2])


rig.fake(0x4B72E0, "SortRender_Add", 3, fmt=sort_fmt, ret=sort_add)


def base_setup(e, rng):
    u32(e, DEVICE, dev)
    u32(e, D3D, d3d)
    setup_states(rig, e, rng)
    SORT["next"] = e.alloc(0x2000)


# --- state functions ---------------------------------------------------------------

LIGHT_BASE, LIGHT_INDEX, LIGHT_COUNT, LIGHT_STRIDE = 0xB041E0, 0xB041E4, 0xB041E8, 0xB041F0


def setup_lighting(e, rng, nlights):
    stride = 0x20
    base = e.alloc(stride * 6)
    for i in range(6):
        e.write(base + stride * i, f32(*[rng.uniform(-1, 1) for _ in range(3)], *[rng.uniform(0, 300) for _ in range(3)]))
    idx = e.alloc(4 * max(nlights, 1))
    for i in range(nlights):
        u32(e, idx + 4 * i, rng.randrange(6))
    u32(e, LIGHT_BASE, base, idx, nlights)
    u32(e, LIGHT_STRIDE, stride)
    e.write(0xB818EC, f32(*[rng.choice([0.0, 0.5, 1.0, rng.random()]) for _ in range(3)]))
    e.write(0xB818AC, f32(*[rng.choice([0.0, 0.25, 1.0, rng.random()]) for _ in range(3)]))
    e.write(0xB46D04, f32(rng.random()))
    u32(e, 0x5470B4, rng.randrange(2))
    u32(e, 0xBCAD78, rng.randrange(3))
    e.write(0xBCABEC, f32(0.001))


STATE_FUNCS = [(0x49D480, "Direct3DRender_SetLitState"), (0x49D730, "Direct3DRender_SetTextureStages"),
               (0x49D810, "Direct3DRender_SetUnlitState"), (0x49D960, "Direct3DRender_SetLightmapStages"),
               (0x49DB10, "Direct3DRender_AlphaBlendOff"), (0x49DB20, "Direct3DRender_SetLitAlphaState"),
               (0x49DDF0, "Direct3DRender_SetUnlitAlphaState")]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("addr,name", STATE_FUNCS)
def test_states(addr, name, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        base_setup(e, rng)
        setup_lighting(e, rng, [0, 1, 3, 5][seed])
        return (0,)
    log = rig.check(addr, name, setup, cw=cw)
    assert log


# --- vertex buffers ----------------------------------------------------------------

MODEL_VB_COUNT, MODEL_VBS = 0x72350C, 0x7AC0A0


def setup_model_vbs(e, rng, n):
    u32(e, MODEL_VB_COUNT, n)
    for i in range(n):
        ent = MODEL_VBS + 0x24 * i
        stride = rng.choice([0x20, 0x28])
        nodes = rng.randrange(0, 4)
        holder = e.alloc(4)
        u32(e, holder, vbs[i % 4].obj)
        prev = e.alloc(0x18)
        first = prev
        for k in range(nodes):
            nxt = e.alloc(0x18)
            src = e.alloc(stride * 8)
            e.write(src, bytes(rng.randrange(256) for _ in range(stride * 8)))
            u32(e, prev, nxt)
            e.write(prev + 0xC, struct.pack("<HH", rng.randrange(8), rng.randrange(1, 8)))
            u32(e, prev + 0x10, holder, src)
            prev = nxt
        u32(e, ent, vbs[(i + 1) % 4].obj)
        e.write(ent + 4, struct.pack("<4I", 0x10, rng.randrange(4), 0x112, 1000))
        u32(e, ent + 0x14, stride, first)
    for k in range(4):
        vbs[k].set(e, e.alloc(0x400), rng.choice([0, 0, 0x80004005]))


@pytest.mark.parametrize("n", range(4))
@pytest.mark.parametrize("seed", range(3))
def test_restore_vertex_buffers(n, seed):
    def setup(e):
        rng = random.Random(seed * 10 + n)
        base_setup(e, rng)
        setup_model_vbs(e, rng, n)
    rig.check(0x49BD30, "Direct3DRender_RestoreVertexBuffers", setup)


@pytest.mark.parametrize("n", range(3))
def test_deinit_models(n):
    def setup(e):
        rng = random.Random(n)
        base_setup(e, rng)
        setup_model_vbs(e, rng, n)
    rig.check(0x49C800, "Direct3DRender_DeinitModels", setup)


@pytest.mark.parametrize("mask", range(8))
def test_init_models(mask):
    def setup(e):
        for i in range(3):
            u32(e, 0x7205C0 + 0xFA8 * i + 0xFA4, (mask >> i & 1) * (i + 5))
    rig.check(0x49C7D0, "Direct3DRender_InitModels", setup)


# --- models ----------------------------------------------------------------------

def make_textures(e, n):
    arr = e.alloc(4 * n)
    for i in range(n):
        t = e.alloc(0x24)
        u32(e, t + 0x20, 0x51000000 + i * 0x10)
        u32(e, arr + 4 * i, t)
    return arr


def make_lod(e, header, ntex=4, anims=()):
    an = e.alloc(0x14 * len(anims)) if anims else 0
    for i, (src, dst) in enumerate(anims):
        u32(e, an + 0x14 * i, 0, src, dst, 0, 0)
    mesh = e.alloc(0x18)
    u32(e, mesh + 0xC, header, len(anims), an)
    lod = e.alloc(0x1C)
    u32(e, lod + 0xC, mesh, make_textures(e, ntex))
    return lod


def make_virmodel(e, lods, count=None):
    vm = e.alloc(0x48)
    for i, l in enumerate(lods):
        u32(e, vm + 4 * i, l)
    e.write(vm + 0x14, b"testmodel\0")
    e.write(vm + 0x3C, struct.pack("<H", len(lods) if count is None else count))
    return vm


def make_header(e, rng, kind, data):
    h = e.alloc(0x40)
    u32(e, h + 0x20, kind, data)
    e.write(h + 0x30, f32(*[rng.uniform(-50, 50) for _ in range(3)], rng.uniform(10, 200)))
    return h


def make_groups(e, rng, specs, lm=False):
    """specs: dicts with sorted, texture, base, vcount, ntri, flags, alpha, lightmap."""
    hdr = 0x20 if lm else 0x1C
    blobs = []
    for s in specs:
        idx = [rng.randrange(s["vcount"]) for _ in range(3 * s["ntri"])]
        b = bytearray(hdr)
        b[0:12] = f32(*[rng.uniform(-30, 30) for _ in range(3)])
        if lm:
            struct.pack_into("<7H", b, 0xC, len(idx), 0, s["texture"], s.get("lightmap", 0), s["base"], s["vcount"],
                             s["sorted"])
            b[0x1C] = s.get("flags", 0)
            b[0x1F] = s.get("alpha", 0)
        else:
            struct.pack_into("<6H", b, 0xC, len(idx), 0, s["texture"], s["base"], s["vcount"], s["sorted"])
            b[0x18] = s.get("flags", 0)
            b[0x1B] = s.get("alpha", 0)
        blobs.append(b + struct.pack("<%dH" % len(idx), *idx))
    for i, b in enumerate(blobs):
        struct.pack_into("<H", b, 0xE, 0xFFFF if i == len(blobs) - 1 else len(b) - hdr)
    data = b"".join(blobs)
    p = e.alloc(len(data))
    e.write(p, data)
    return p


def rand_groups(rng, nverts, n, lm=False, sorted_p=0.5):
    out = []
    for _ in range(n):
        base = rng.randrange(nverts)
        out.append(dict(sorted=rng.random() < sorted_p, texture=rng.choice([0, 1, 2, 3, 0xFFFF]), base=base,
                        vcount=max(1, nverts - base), ntri=rng.randrange(1, 5), flags=rng.randrange(4),
                        alpha=rng.randrange(256), lightmap=rng.choice([0, 1, 2, 0xFFFF])))
    return out


def make_vertices(e, rng, n, lo, hi, stride=8):
    vs = []
    for i in range(n):
        x = rng.choice([lo, hi, rng.uniform(lo - 20, hi + 20), rng.uniform(lo, hi)])
        v = [x, rng.uniform(-120, 120), rng.uniform(-120, 120)]
        v += [rng.uniform(-1, 1) for _ in range(3)] + [rng.random() for _ in range(stride - 6)]
        vs.append(v)
    p = e.alloc(4 * stride * n)
    e.write(p, b"".join(f32(*v) for v in vs))
    return p


def place(rng, view_m, view_pos, dist, spread=300):
    lat = [rng.uniform(-spread, spread) for _ in range(2)]
    return [view_pos[i] + dist * view_m[6 + i] + lat[0] * view_m[i] + lat[1] * view_m[3 + i] for i in range(3)]


def make_obj(e, rng, view, model, dist, layer=0, spline=None, spread=300):
    view_m, view_pos = view
    o = e.alloc(0x5C0)
    e.write(o + 0x1C, struct.pack("<H", rng.randrange(1, 0x180)))
    e.write(o + 0x20, f64(*place(rng, view_m, view_pos, dist, spread)))
    e.write(o + 0x44, f32(rng.uniform(50, 500)))
    u32(e, o + 0x6C, model)
    e.write(o + 0x70, f32(*rmatrix(rng)) + struct.pack("<I", rng.randrange(1000)))
    e.write(o + 0x98, f32(*[rng.choice([1.0, rng.uniform(0.5, 2)]) for _ in range(3)]))
    u32(e, o + 0xD8, rng.randrange(1 << 32))
    u32(e, o + 0xEC, layer)
    if spline:
        n, lo, hi, straight, reverse, offset = spline
        e.write(o + 0xFC, f32(lo, hi))
        for i in range(n + 1):
            x = lo + (hi - lo) * i / n
            e.write(o + 0x104 + 12 * i, f32(x + rng.uniform(-5, 5), rng.uniform(-20, 20), rng.uniform(-20, 20)))
            e.write(o + 0x200 + 0x28 * i, f32(*rmatrix(rng)))
        u32(e, o + 0x59C, n)
        if offset:
            off = e.alloc(16)
            e.write(off, f32(rng.uniform(-50, 50), rng.uniform(-50, 50), rng.uniform(-50, 50), rng.uniform(50, 400)))
            u32(e, o + 0x5A0, off)
        e.write(o + 0x5B4, bytes([straight, reverse]))
    return o


def spline_model(e, rng, view, nverts=10, ngroups=3, dist=None, straight=0, reverse=0, offset=0, nseg=4,
                 spread=300):
    lo, hi = sorted([rng.uniform(-200, 200), rng.uniform(-200, 200)])
    if hi - lo < 10:
        hi = lo + 50
    verts = make_vertices(e, rng, nverts, lo, hi)
    groups = make_groups(e, rng, rand_groups(rng, nverts, ngroups)) if ngroups else 0
    d = e.alloc(0x30)
    u32(e, d, 4, 0, 0, nverts, verts, groups)
    h = make_header(e, rng, 0, d)
    lod = make_lod(e, h)
    vm = make_virmodel(e, [lod])
    o = make_obj(e, rng, view, vm, dist if dist is not None else rng.uniform(100, 3000),
                 spline=(nseg, lo, hi, straight, reverse, offset), spread=spread)
    return o, h, d, lod


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
def test_set_object_fog(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        view = setup_view(e, rng)
        o, h, d, lod = spline_model(e, rng, view)
        return o, h
    rig.check(0x49CBE0, "Direct3DRender_SetObjectFog", setup, cw=cw)


def setup_vb(e, rng, used, size, hr=0, null=False):
    buf = e.alloc(0x20 * 64)
    vbs[5].set(e, 0 if null else buf, hr)
    u32(e, VB_NORMAL, vbs[5].obj, size, used)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("straight", [0, 1])
@pytest.mark.parametrize("reverse", [0, 1])
def test_fill_spline_vb(seed, straight, reverse, cw):
    def setup(e):
        rng = random.Random(seed * 4 + straight * 2 + reverse)
        base_setup(e, rng)
        view = setup_view(e, rng)
        o, h, d, lod = spline_model(e, rng, view, nverts=rng.randrange(1, 12), straight=straight, reverse=reverse,
                                    nseg=rng.randrange(1, 21))
        setup_vb(e, rng, 0 if seed == 4 else rng.choice([0, 20, 40, 55]), 64, hr=0x80004005 if seed == 5 else 0, null=seed == 4)
        return d, o
    rig.check(0x49CC80, "Direct3DRender_FillSplineVB", setup, cw=cw, check_eax=True)


PROJ = 0x723518


def leading_behind(e, target, setup, n=40):
    """Vertices at the start that end up behind the near plane keep stale screen fields."""
    e.reset()
    args = setup(e)
    e.call(target, *args)
    k = 0
    while k < n and e.u32(PROJ + 0x1C * k + 0x18) == 0x10:
        k += 1
    return k


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(30))
@pytest.mark.parametrize("straight", [0, 1])
def test_spline_near_z(seed, straight, cw):
    def setup(e):
        rng = random.Random(seed * 2 + straight)
        base_setup(e, rng)
        view = setup_view(e, rng)
        near = struct.unpack("<f", e.read(NEAR_PLANE, 4))[0]
        dist = rng.choice([near + rng.uniform(-150, 150), near + rng.uniform(0, 400), rng.uniform(100, 3000)])
        o, h, d, lod = spline_model(e, rng, view, nverts=rng.randrange(3, 30), ngroups=rng.randrange(0, 4) if seed else 3,
                                    dist=dist, straight=straight, reverse=rng.randrange(2), offset=rng.randrange(2),
                                    nseg=rng.randrange(1, 21), spread=rng.choice([300, 800]))
        if seed == 11:
            u32(e, h + 0x20, 3)
        FRUSTUM["ret"] = 1 if seed == 10 else 0
        q = e.alloc(12)
        e.write(q + 8, f32(rng.choice([1e9, 1e9, near * 2, 5000.0, near + 50])))
        return o, q
    k = leading_behind(rig.e, rig.with_cw(0x49BE10, cw), setup)
    span = 12 if straight else 4
    ignore = [(PROJ + 0x1C * i + 0xC, PROJ + 0x1C * i + 0xC + span) for i in range(k)]
    rig.check(0x49BE10, "Direct3DRender_SplineNearZ", setup, cw=cw, ignore=ignore)
    FRUSTUM["ret"] = 0


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", ["nomodel", "culled", "nolod", "lightmap", "out", "in", "in2", "in3"])
def test_draw_spline_mesh(case, cw):
    def setup(e):
        rng = random.Random(case)
        base_setup(e, rng)
        view = setup_view(e, rng)
        o, h, d, lod = spline_model(e, rng, view, nverts=rng.randrange(3, 20), ngroups=rng.randrange(1, 5),
                                    straight=rng.randrange(2), nseg=rng.randrange(1, 21))
        if case == "nomodel":
            u32(e, o + 0x6C, 0)
        if case == "lightmap":
            u32(e, h + 0x20, 3)
        INSIDE["ret"] = 1 if case == "culled" else 0
        INSIDE["inside"] = 0 if case == "out" else rng.randrange(1, 5)
        LOD["lod"] = -1 if case == "nolod" else 0
        setup_vb(e, rng, rng.choice([0, 30, 60]), 64)
        p = e.alloc(4)
        u32(e, p, o)
        return (p,)
    rig.check(0x49C850, "Direct3DRender_DrawSplineMesh", setup, cw=cw)
    INSIDE["ret"], LOD["lod"] = 0, 0


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
def test_draw_sorted_spline(seed, cw):
    def setup(e):
        rng = random.Random(100 + seed)
        base_setup(e, rng)
        view = setup_view(e, rng)
        o, h, d, lod = spline_model(e, rng, view, nverts=rng.randrange(3, 20), ngroups=1,
                                    straight=rng.randrange(2), nseg=rng.randrange(1, 21))
        g = e.u32(d + 0x14)
        e.write(g + 0x10, struct.pack("<H", 0xFFFF if seed & 1 else seed % 4))
        e.write(g + 0x18, bytes([seed >> 1 & 1]))
        setup_vb(e, rng, rng.choice([0, 30, 60]), 64)
        p = e.alloc(0x30)
        e.write(p, e.read(o + 0x20, 24))
        u32(e, p + 0x18, rng.randrange(1 << 32), lod, d, g, o)
        return (p,)
    rig.check(0x49D0B0, "Direct3DRender_DrawSortedFaceGroup_Spline", setup, cw=cw)


# --- rigid and lightmapped meshes ------------------------------------------------------

def vb_holder(e, k):
    p = e.alloc(4)
    u32(e, p, vbs[k].obj)
    return p


def make_lightmap_set(e, rng, n=3):
    s = e.alloc(0x3C + 0x2C * n)
    for i in range(n):
        t = s + 0x3C + 0x2C * i
        e.write(t + 0x10, f32(rng.uniform(0.1, 2), rng.uniform(0.1, 2)))
        u32(e, t + 0x20, 0x52000000 + i * 0x10)
    return s


def rigid_model(e, rng, view, kind=0, nverts=12, ngroups=3, layer=0, nlods=2, anims=((0, 1), (2, 3)), sorted_p=0.5,
                lightmaps="set"):
    if kind == 3:
        verts = make_vertices(e, rng, nverts, -100, 100, stride=10)
        groups = make_groups(e, rng, rand_groups(rng, nverts, ngroups, lm=True, sorted_p=sorted_p), lm=True) \
            if ngroups else 0
        d = e.alloc(0x34)
        u32(e, d, 5, 0, 0, 0, nverts, verts, 0, groups, 0, 0, rng.randrange(2) if lightmaps == "set" else 0)
        e.write(d + 0x2C, struct.pack("<H", rng.randrange(50)))
        u32(e, d + 0x30, vb_holder(e, 1))
        vbs[1].set(e, e.alloc(0x28 * 80))
    else:
        verts = make_vertices(e, rng, nverts, -100, 100)
        groups = make_groups(e, rng, rand_groups(rng, nverts, ngroups, sorted_p=sorted_p)) if ngroups else 0
        d = e.alloc(0x2C)
        u32(e, d, 4, 0, 0, nverts, verts, groups)
        e.write(d + 0x24, struct.pack("<H", rng.randrange(50)))
        u32(e, d + 0x28, vb_holder(e, 0))
    h = make_header(e, rng, kind, d)
    lods = [make_lod(e, h, anims=anims) for _ in range(nlods)]
    vm = make_virmodel(e, lods)
    o = make_obj(e, rng, view, vm, rng.uniform(100, 3000), layer=layer)
    if lightmaps:
        refs = e.alloc(0x24 * nlods)
        for i in range(nlods):
            u32(e, refs + 0x24 * i, make_lightmap_set(e, rng) if lightmaps == "set" else 0)
            e.write(refs + 0x24 * i + 0x10, f32(rng.uniform(0.1, 2), rng.uniform(0.1, 2)))
        u32(e, o + 0xDC, refs)
    slot = e.read(0x548368, 1)[0]
    typ = struct.unpack("<H", e.read(o + 0x1C, 2))[0]
    u32(e, 0xA96AE0 + 4 * (slot * 0x180 + typ), HANDLER)
    return o, h, d, lods


RIGID_CASES = ["nomodel", "culled", "nolod", "nodata", "rigid", "rigid_out", "rigid_layer", "rigid_nogroups",
               "rigid_static", "lm", "lm_layer", "lm_noset", "lm_none", "lm_none_nogroups", "lm_static", "other"]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", RIGID_CASES)
@pytest.mark.parametrize("seed", range(3))
def test_draw_rigid_mesh(case, seed, cw):
    def setup(e):
        rng = random.Random(case + str(seed))
        base_setup(e, rng)
        view = setup_view(e, rng)
        kind = 3 if case.startswith("lm") else 0
        lm = {"lm_noset": "null", "lm_none": None, "lm_none_nogroups": None}.get(case, "set")
        o, h, d, lods = rigid_model(e, rng, view, kind=kind, nverts=rng.randrange(3, 20),
                                    ngroups=0 if case.endswith("nogroups") else rng.randrange(1, 5),
                                    layer=1 if case in ("nolod", "rigid_layer", "lm_layer") else 0,
                                    sorted_p=0 if case.endswith("static") else 0.5, lightmaps=lm,
                                    anims=((0, 1), (2, 3)) if seed else ())
        if case == "nomodel":
            u32(e, o + 0x6C, 0)
        if case == "nodata":
            u32(e, h + 0x24, 0)
        if case == "other":
            u32(e, h + 0x20, 1)
        INSIDE["ret"] = 1 if case == "culled" else 0
        INSIDE["inside"] = 0 if case == "rigid_out" else rng.randrange(1, 5)
        LOD["lod"] = -1 if case == "nolod" else rng.randrange(2)
        p = e.alloc(4)
        u32(e, p, o)
        return (p,)
    rig.check(0x49DF60, "Direct3DRender_DrawRigidMesh", setup, cw=cw)
    INSIDE["ret"], LOD["lod"] = 0, 0


def test_draw_rigid_mesh_not_converted():
    msgs = []
    for target in (0x49DF60, "Direct3DRender_DrawRigidMesh"):
        e = rig.e
        e.reset()
        rng = random.Random(5)
        base_setup(e, rng)
        view = setup_view(e, rng)
        o, h, d, lods = rigid_model(e, rng, view)
        e.write(e.u32(o + 0x6C) + 0x3C, b"\0\0")
        p = e.alloc(4)
        u32(e, p, o)
        with pytest.raises(ErrorShown) as err:
            e.call(target, p)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1] == "model %s not converted"


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", ["done", "lockfail", "nolm", "noset", "lm", "lm2"])
def test_upload_lightmap_mesh(case, cw):
    def setup(e):
        rng = random.Random(case)
        base_setup(e, rng)
        view = setup_view(e, rng)
        o, h, d, lods = rigid_model(e, rng, view, kind=3, nverts=rng.randrange(3, 20), ngroups=rng.randrange(1, 5),
                                    lightmaps="null" if case == "noset" else "set")
        u32(e, d + 0x28, 1 if case == "done" else 0)
        vbs[1].set(e, e.alloc(0x28 * 80), 0x80004005 if case == "lockfail" else 0)
        return d, 0 if case == "nolm" else e.u32(o + 0xDC) + (0x24 if case == "lm2" else 0)
    rig.check(0x49EB50, "Direct3DRender_UploadLightmapMesh", setup, cw=cw)


def sorted_prim(e, rng, o, lod, d, g, extra):
    p = e.alloc(0x80)
    e.write(p, e.read(o + 0x20, 24) + e.read(o + 0x70, 0x28) + e.read(o + 0x98, 12))
    u32(e, p + 0x4C, e.u32(o + 0xD8), lod, d, g, e.u32(o + 0xEC), extra)
    return p


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(10))
def test_draw_sorted_rigid(seed, cw):
    def setup(e):
        rng = random.Random(200 + seed)
        base_setup(e, rng)
        view = setup_view(e, rng)
        o, h, d, lods = rigid_model(e, rng, view, nverts=rng.randrange(3, 20), ngroups=1, layer=seed % 3 == 2,
                                    anims=((0, 1), (2, 3)) if seed & 1 else ())
        g = e.u32(d + 0x14)
        e.write(g + 0x10, struct.pack("<H", 0xFFFF if seed % 4 == 3 else seed % 4))
        e.write(g + 0x18, bytes([rng.randrange(4)]))
        anim = 0
        if seed & 1:
            anim = e.alloc(8)
            u32(e, anim, 3, 2)
        return (sorted_prim(e, rng, o, lods[0], d, g, anim),)
    rig.check(0x49EC70, "Direct3DRender_DrawSortedFaceGroup_Rigid", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_draw_sorted_lightmap(seed, cw):
    def setup(e):
        rng = random.Random(300 + seed)
        base_setup(e, rng)
        view = setup_view(e, rng)
        o, h, d, lods = rigid_model(e, rng, view, kind=3, nverts=rng.randrange(3, 20), ngroups=1,
                                    layer=seed % 3 == 2)
        g = e.u32(d + 0x1C)
        e.write(g + 0x10, struct.pack("<HH", 0xFFFF if seed % 4 == 3 else seed % 4, rng.randrange(3)))
        e.write(g + 0x1C, bytes([seed >> 1 & 1]))
        lm = e.u32(e.u32(o + 0xDC)) if seed & 4 else 0
        return (sorted_prim(e, rng, o, lods[0], d, g, lm),)
    rig.check(0x49F1C0, "Direct3DRender_DrawSortedFaceGroup_Lightmap", setup, cw=cw)
