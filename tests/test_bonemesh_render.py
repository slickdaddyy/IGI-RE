"""Direct3D mesh methods and rigid object handlers (0x49F700 - 0x4A23D0)."""
import random
import struct

import pytest

from emu import ErrorShown
from test_bonemesh_util import (LOG, RET, add_child, com, d64, f32, floatfake, handler, logfake, make_emu, run,
                                set_handler, task, trap, u16, u32, vfake, with_cw)

emu = make_emu()
# DrawBoneMesh
vfake(emu, 0x4D06C0, 2, show=lambda e, a: (e.normalize(a[0]),),
      effect=lambda e, a: (u32(e, a[1], RET.get("clip", 0)), RET.get("hidden", 0))[1])
logfake(emu, 0x4CFF70, 1, effect=lambda e, a: RET.get("lod", 0))
floatfake(emu, 0x491D40, 0)
vfake(emu, 0x497E70, 1, show=lambda e, a: (e.read(a[0], 0xA8),))
vfake(emu, 0x49B2D0, 2, show=lambda e, a: (e.read(a[0], 24), a[1]))
vfake(emu, 0x49B700, 4, effect=lambda e, a: RET.get("base", 0))
DEV = com(emu, "dev", {11: 3, 32: 8, 35: 3, 37: 4}, derefs={11: {2: 64}})
LISTS = []
for name in "ABCD":
    lst = emu.alloc(0x14 + 4 * 8)
    for i in range(8):
        u32(emu, lst + 0x14 + 4 * i, handler(emu, "%s%d" % (name, i), 1))
    LISTS.append(lst)
# LoadModel
CHUNKS = emu.alloc(0x10000)
logfake(emu, 0x4B7490, 1, effect=lambda e, a: a[0] + 0x10)
logfake(emu, 0x4B7470, 1, effect=lambda e, a: a[0] + 1)
logfake(emu, 0x4B7590, 2, effect=lambda e, a: 0 if a[1] in RET.get("missing", ()) else
        0x2000 + ((a[0] * 3 + (a[1] & 0xFF)) & 0xFFF))
logfake(emu, 0x4B74D0, 2, effect=lambda e, a: CHUNKS + (a[0] & 0xFFF) * 8)
vfake(emu, 0x49B430, 3)
# Shutdown, rigid object handlers
logfake(emu, 0x4B7450, 2)
vfake(emu, 0x4DAA20, 1, show=lambda e, a: (), effect=lambda e, a: (u32(e, a[0], 0x1111), 0x2222)[1])
vfake(emu, 0x4DA180, 2, show=lambda e, a: (), effect=lambda e, a: (u32(e, a[0], 0x3333), u32(e, a[1], 0x4444), 0)[2])
logfake(emu, 0x4CFC30, 0, ret=5)
logfake(emu, 0x4C56F0, 0, ret=0x22)
DRAW = trap(emu, "draw_rigid", lambda e: (LOG[-1].append(("draw", e.normalize(e.u32(e.arg(0))),
                                                            e.read(e.arg(0) + 4, 16))), e.ret(0)))


def scribble(e, a):
    e.write(a[1], b"\x55" * 8)


CHILD = handler(emu, "child", 2, derefs={1: 0x40}, effect=lambda e, a: scribble(e, a))
logfake(emu, 0x4C3450, 0)
logfake(emu, 0x4D14D0, 0, ret=0x36)
logfake(emu, 0x4D97E0, 0, ret=0x37)
logfake(emu, 0x4C48C0, 0, ret=0x99)
logfake(emu, 0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})
logfake(emu, 0x4C4820, 0)
emu.baseline = emu.snapshot()


def rnd_f32(rng, n, lo=-1.0, hi=1.0):
    return [rng.uniform(lo, hi) for _ in range(n)]


def groups(e, items):
    """REND chunk: (texture, start, count, indices) per group."""
    blob = b""
    for i, (tex, start, count, idx) in enumerate(items):
        nxt = 0xFFFF if i == len(items) - 1 else 2 * len(idx)
        blob += b"\xAA" * 12 + struct.pack("<HHHHH", len(idx), nxt, tex, start, count) + b"\xBB" * 6
        blob += struct.pack("<%dH" % len(idx), *idx)
    p = e.alloc(len(blob))
    e.write(p, blob)
    return p


def bone_setup(e, seed, layer=0, lod=1, fmt=1, model=True, hidden=0, clip=0, cur=(0, 1, 2, 3)):
    rng = random.Random(seed)
    RET.update(clip=clip, hidden=hidden, lod=lod, base=rng.randrange(0, 500))
    RET[0x491D40] = rng.uniform(0.5, 2.0)
    u32(e, 0x5CA114, DEV)
    f32(e, 0xBCAAE0, *rnd_f32(rng, 42, -3, 3))
    d64(e, 0xBCAB08, *rnd_f32(rng, 3, -5000, 5000))
    f32(e, 0xBCABB8, 3.0, 5.0, rng.uniform(300, 1000), rng.uniform(200, 800), rng.uniform(150, 500))
    f32(e, 0xB81700, rng.uniform(0.5, 1.5), 2.0, 3.0)
    f32(e, 0xBCABEC, rng.uniform(0.001, 0.1))
    for i, lst in enumerate(LISTS):
        u32(e, [0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4][i], lst)
        u32(e, lst + 4, cur[i])
    u32(e, 0x7205B0, 1)
    u32(e, 0x7AC09C, 2)
    u32(e, 0x7234B8, 3)
    u32(e, 0xBCACEC, 4)
    u32(e, 0xBCADE0, 5)
    u32(e, 0xBCAD90, 0xB0B0)
    texs = []
    for i in range(4):
        t = e.alloc(0x24)
        u32(e, t + 0x20, 0x7E00 + i)
        texs.append(t)
    tex_arr = e.alloc(16)
    u32(e, tex_arr, *texs)
    render = e.alloc(0x38)
    u32(e, render, 4)
    u32(e, render + 0x1C, groups(e, [(2, 10, 30, [0, 1, 2]), (0xFFFF, 5, 7, [3, 4, 5, 6, 7, 8]),
                                     (0, 0, 3, [1, 2, 0]), (0xFFFF, 1, 2, [0, 1, 2])][:rng.randrange(1, 5)]))
    header = e.alloc(0x40)
    u32(e, header + 0x20, fmt, render)
    f32(e, header + 0x3C, rng.uniform(1, 100))
    vm = e.alloc(0x48)
    for i in range(5):
        mesh = e.alloc(0x14)
        u32(e, mesh + 0xC, header)
        ml = e.alloc(0x1C)
        u32(e, ml + 0xC, mesh, tex_arr)
        u32(e, vm + 4 * i, ml)
    obj = e.alloc(0xE0)
    d64(e, obj + 0x20, *rnd_f32(rng, 3, -5000, 5000))
    u32(e, obj + 0x74, vm if model else 0)
    u32(e, obj + 0xD4, layer)
    arg = e.alloc(0x14)
    u32(e, arg, obj, 1, 2, 3, 4)
    return (arg,)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("kw", [dict(), dict(layer=1, lod=2), dict(layer=2, lod=-1), dict(layer=0, lod=-1),
                                dict(model=False), dict(hidden=1), dict(clip=1), dict(fmt=0), dict(layer=1, fmt=3),
                                dict(cur=(1, 2, 3, 4)), dict(cur=(1, 2, 3, 5), clip=1)])
def test_draw_bone_mesh(seed, kw, cw):
    run(emu, with_cw(emu, 0x49F700, cw), with_cw(emu, "Direct3DRender_DrawBoneMesh", cw),
        lambda e: bone_setup(e, seed, **kw))


def test_copy_uv():
    def setup(e):
        p = e.alloc(0x18)
        u32(e, p, *range(1, 7))
        return (p,)
    run(emu, 0x49FAF0, "Direct3DRender_CopyUV", setup)


def load_setup(e, fmt, unk28=0, missing=()):
    RET["missing"] = missing
    header = e.alloc(0x40)
    u32(e, header + 0x20, fmt, 0x77, unk28, 0x99)
    e.write(CHUNKS, bytes(range(256)) * 256)
    arg = e.alloc(8)
    u32(e, arg, header, 0x40)
    return (arg,)


@pytest.mark.parametrize("fmt", [0, 1, 3])
@pytest.mark.parametrize("missing", [(), (0x434D5348,)])
def test_load_model(fmt, missing):
    run(emu, 0x49FB10, "Direct3DRender_LoadModel", lambda e: load_setup(e, fmt, missing=missing))


def check_error(e, orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        RET.clear()
        LOG.append([])
        args = setup(e)
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append((str(err.value), LOG[-1]))
    assert msgs[0] == msgs[1]


@pytest.mark.parametrize("fmt,unk28", [(2, 0), (4, 0), (-1, 0), (0, 1), (1, 5)])
def test_load_model_error(fmt, unk28):
    check_error(emu, 0x49FB10, "Direct3DRender_LoadModel", lambda e: load_setup(e, fmt, unk28))


def unload_setup(e, fmt):
    render = e.alloc(0x40)
    u32(e, render, *range(0x100, 0x110))
    header = e.alloc(0x40)
    u32(e, header + 0x20, fmt, render)
    arg = e.alloc(8)
    u32(e, arg, header, 0)
    return (arg,)


@pytest.mark.parametrize("fmt", [0, 1, 3])
def test_unload_model(fmt):
    run(emu, 0x49FE60, "Direct3DRender_UnloadModel", lambda e: unload_setup(e, fmt))


@pytest.mark.parametrize("fmt", [2, 7])
def test_unload_model_error(fmt):
    check_error(emu, 0x49FE60, "Direct3DRender_UnloadModel", lambda e: unload_setup(e, fmt))


def exclude_setup(e, seed, inside=True):
    rng = random.Random(seed)
    f32(e, 0xBCADDC, rng.uniform(0.5, 20))
    f32(e, 0xBCADCC, rng.uniform(1000, 6000))
    for a in (0xB46CE4, 0xB46D1C, 0xB816FC, 0xB8179C):
        f32(e, a, rng.uniform(-1.2, 1.2))
    for a in (0xB817A4, 0xB46D20, 0xB46D18, 0xB46CE8):
        f32(e, a, rng.uniform(0.3, 1.5))
    c = e.alloc(12)
    z = rng.choice([rng.uniform(-200, 7000), rng.uniform(-50, 50), rng.uniform(400, 1000)])
    f32(e, c, rng.uniform(-3000, 3000) * rng.random(), rng.uniform(-2000, 2000) * rng.random(), z)
    arg = e.alloc(16)
    out = e.alloc(4) if inside else 0
    if out:
        u32(e, out, 0x77)
    u32(e, arg, c)
    f32(e, arg + 4, rng.choice([rng.uniform(0, 600), rng.uniform(0, 10)]))
    u32(e, arg + 8, out, 0x99)
    return (arg,)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(150))
def test_exclude_object(seed, cw):
    run(emu, with_cw(emu, 0x49FEB0, cw), with_cw(emu, "Direct3DRender_ExcludeObject", cw),
        lambda e: exclude_setup(e, seed, seed % 4 != 3))


def exclude_exact(e, cx, cy, cz, r, k=0.0, s=1.0, near=1.0, far=1000.0):
    f32(e, 0xBCADDC, near)
    f32(e, 0xBCADCC, far)
    for a in (0xB46CE4, 0xB46D1C, 0xB816FC, 0xB8179C):
        f32(e, a, k)
    for a in (0xB817A4, 0xB46D20, 0xB46D18, 0xB46CE8):
        f32(e, a, s)
    c = e.alloc(12)
    f32(e, c, cx, cy, cz)
    arg = e.alloc(16)
    out = e.alloc(4)
    u32(e, arg, c)
    f32(e, arg + 4, r)
    u32(e, arg + 8, out, 0x99)
    return (arg,)


@pytest.mark.parametrize("vals", [(0, 0, 0, 1), (0, 0, -1, 1), (0, 0, 999, 1), (0, 0, 1001, 1), (1, 0, 500, 1),
                                  (-1, 0, 500, 1), (0, 1, 500, 1), (0, -1, 500, 1), (0, 0, 410.6, 1),
                                  (0, 0, 410.7, 1), (0, 0, 2, 1), (0, 0, 998.5, 1.5), (2, 0, 500, 1),
                                  (0, 0, -2, 1)])
def test_exclude_object_edges(vals):
    run(emu, 0x49FEB0, "Direct3DRender_ExcludeObject", lambda e: exclude_exact(e, *vals))


def test_shutdown():
    def setup(e):
        u32(e, 0xBCADE8, 0xA000)
        u32(e, 0xBCAD7C, 0xB000)
        u32(e, 0xBCADAC, 0xC000)
        for i, a in enumerate((0x7205B8, 0x723508, 0x7205BC, 0x723504, 0x7ADDD4, 0x7205B0, 0x7AC09C, 0x7234B8)):
            u32(e, a, i + 1)
    run(emu, 0x4A0110, "Direct3DRender_Shutdown", setup)


def rigid(e, rng, kids=2, type0=0x40):
    t = task(e, 0xF0, 0x33)
    d64(e, t + 0x20, *rnd_f32(rng, 3, -5000, 5000))
    f32(e, t + 0x70, *rnd_f32(rng, 9))
    u32(e, t + 0x94, rng.randrange(100))
    for i in range(kids):
        c = task(e, 0x40, type0 + i)
        add_child(e, t, c)
        set_handler(e, 0x22, type0 + i, CHILD if i != 1 else 0)
    return t


@pytest.mark.parametrize("kids,attached,att_kids", [(0, False, 0), (1, False, 0), (3, False, 0), (2, True, 0),
                                                    (2, True, 3), (0, True, 1)])
def test_enter_handler(kids, attached, att_kids):
    def setup(e):
        rng = random.Random(kids * 7 + att_kids)
        u32(e, 0xA94E84 + 5 * 4, DRAW)
        t = rigid(e, rng, kids)
        if attached:
            u32(e, t + 0xE8, rigid(e, rng, att_kids, 0x50))
        return (t,)
    run(emu, 0x4A01B0, "RigidDynCubeObj_EnterHandler", setup)


def test_open():
    run(emu, 0x4A1470, "RigidDynCubeObj_OpenD3D", None)


def test_close():
    run(emu, 0x4A23D0, "RigidDynCubeObj_CloseD3D", None)


def sphere_setup(e, seed, model=True, count=1):
    rng = random.Random(seed)
    t = rigid(e, rng, 0)
    header = e.alloc(0x40)
    f32(e, header + 0x30, *rnd_f32(rng, 4, -50, 50))
    mesh = e.alloc(0x14)
    u32(e, mesh + 0xC, header)
    ml = e.alloc(0x1C)
    u32(e, ml + 0xC, mesh)
    vm = e.alloc(0x48)
    u32(e, vm, ml)
    e.write(vm + 0x14, b"crate_big\0")
    u16(e, vm + 0x3C, count)
    u32(e, t + 0x6C, vm if model else 0)
    out = e.alloc(0x20)
    e.write(out, b"\xCD" * 0x20)
    return t, out


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(20))
def test_get_sphere(seed, cw):
    run(emu, with_cw(emu, 0x4A14B0, cw), with_cw(emu, "RigidDynCubeObj_GetSphereHandler", cw),
        lambda e: sphere_setup(e, seed))


def test_get_sphere_no_model():
    run(emu, 0x4A14B0, "RigidDynCubeObj_GetSphereHandler", lambda e: sphere_setup(e, 1, model=False))


def test_get_sphere_not_converted():
    check_error(emu, 0x4A14B0, "RigidDynCubeObj_GetSphereHandler", lambda e: sphere_setup(e, 1, count=0))


def clip_vertex(rng, near, ctx):
    x, y = rng.uniform(-400, 400), rng.uniform(-300, 300)
    z = rng.choice([rng.uniform(-100, near), rng.uniform(near, 50), rng.uniform(near, 600)])
    if z < near:
        return [x, y, z, rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)], 0x10
    sx_, sy_, cx, cy, minx, miny, maxx, maxy = ctx
    rhw = 1.0 / z
    sx = rhw * x * sx_ + cx
    sy = sy_ * rhw * y + cy
    code = (sx < minx) | (sy < miny) << 1 | (sx > maxx) << 2 | (sy > maxy) << 3
    return [x, y, z, sx, sy, rhw], code


def clip_setup(e, seed):
    rng = random.Random(seed)
    near = rng.uniform(0.5, 10)
    ctx = [rng.uniform(200, 500), rng.uniform(200, 500), rng.uniform(250, 400), rng.uniform(200, 300),
           rng.uniform(0, 50), rng.uniform(0, 50), rng.uniform(500, 800), rng.uniform(400, 600)]
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
@pytest.mark.parametrize("seed", range(400))
def test_clip_triangle_depth(seed, cw):
    run(emu, with_cw(emu, 0x4A0340, cw), with_cw(emu, "Direct3DRender_ClipTriangleDepth", cw),
        lambda e: clip_setup(e, seed))


vfake(emu, 0x4D03D0, 3, show=lambda e, a: (e.read(a[0], 12), a[1], a[2]), effect=lambda e, a: RET.get("cull", 0))
NEAR_CHILD = handler(emu, "near_child", 2, derefs={1: 12})
emu.baseline = emu.snapshot()


def lightmap_groups(e, items):
    blob = b""
    for i, (start, idx) in enumerate(items):
        nxt = 0xFFFF if i == len(items) - 1 else 2 * len(idx)
        blob += b"\xAA" * 12 + struct.pack("<HH", len(idx), nxt) + b"\xBB" * 4 + struct.pack("<H", start)
        blob += b"\xCC" * 10 + struct.pack("<%dH" % len(idx), *idx)
    p = e.alloc(len(blob))
    e.write(p, blob)
    return p


def near_setup(e, seed, fmt=0, ec=1, model=True, cull=0, min_z=None, kids=2, faces=True):
    rng = random.Random(seed)
    RET["cull"] = cull
    m = [1, 0, 0, 0, 1, 0, 0, 0, 1]
    f32(e, 0xBCAAE0, *[v + rng.uniform(-0.2, 0.2) for v in m])
    u32(e, 0xBCAB04, 7)
    d64(e, 0xBCAB08, *rnd_f32(rng, 3, -5000, 5000))
    f32(e, 0xBCAB28, rng.uniform(200, 500), rng.uniform(200, 500), 320.0, 240.0)
    f32(e, 0xBCABD0, rng.uniform(0, 50), rng.uniform(0, 50), rng.uniform(500, 640), rng.uniform(400, 480))
    near = rng.uniform(1, 10)
    f32(e, 0xBCADC0, near)
    t = rigid(e, rng, 0)
    off = [rng.uniform(-80, 80), rng.uniform(-60, 60), rng.uniform(20, 150)]
    d64(e, t + 0x20, *[e_ + v for e_, v in zip(struct.unpack("<3d", e.read(0xBCAB08, 24)), off)])
    f32(e, t + 0x70, *[v + rng.uniform(-0.3, 0.3) for v in m])
    f32(e, t + 0x98, *rnd_f32(rng, 3, 0.5, 2.0))
    u32(e, t + 0xEC, ec)
    n = rng.randrange(3, 30)
    stride = 0x20 if fmt == 0 else 0x28
    verts = e.alloc(stride * n)
    for i in range(n):
        p = [0, 0, 0] if i == 0 else [rng.uniform(-60, 60), rng.uniform(-60, 60), rng.uniform(-90, 60)]
        f32(e, verts + stride * i, *p)
    faces_ = []
    for k in range(rng.randrange(1, 4)):
        start = rng.randrange(0, 3)
        idx = [rng.randrange(0, n - start) for _ in range(3 * rng.randrange(1, 6))]
        faces_.append((start, idx))
    render = e.alloc(0x38)
    if fmt == 0:
        u32(e, render + 0xC, n, verts)
        u32(e, render + 0x14, groups(e, [(0, s, 0, i) for s, i in faces_]) if faces else 0)
    else:
        u32(e, render + 0x10, n, verts)
        u32(e, render + 0x1C, lightmap_groups(e, faces_) if faces else 0)
    header = e.alloc(0x40)
    u32(e, header + 0x20, fmt, render)
    f32(e, header + 0x30, *rnd_f32(rng, 3, -5, 5), rng.uniform(10, 100))
    mesh = e.alloc(0x14)
    u32(e, mesh + 0xC, header)
    ml = e.alloc(0x1C)
    u32(e, ml + 0xC, mesh)
    vm = e.alloc(0x48)
    u32(e, vm, ml)
    u32(e, t + 0x6C, vm if model else 0)
    att = rigid(e, rng, 0)
    for i in range(kids):
        c = task(e, 0x40, 0x60 + i)
        add_child(e, att, c)
        set_handler(e, 0x36, 0x60 + i, NEAR_CHILD)
    u32(e, t + 0xE8, att)
    arg = e.alloc(12)
    u32(e, arg, 0x1234, 0x5678)
    f32(e, arg + 8, rng.choice([1e6, rng.uniform(0, 200)]) if min_z is None else min_z)
    return t, arg


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(60))
@pytest.mark.parametrize("fmt", [0, 3])
def test_near_z(seed, fmt, cw):
    run(emu, with_cw(emu, 0x4A1570, cw), with_cw(emu, "RigidDynCubeObj_NearZHandler", cw),
        lambda e: near_setup(e, seed, fmt))


@pytest.mark.parametrize("kw", [dict(fmt=1), dict(fmt=2), dict(ec=0), dict(model=False), dict(cull=1),
                                dict(min_z=-1000.0), dict(faces=False), dict(fmt=3, faces=False), dict(kids=0)])
def test_near_z_paths(kw):
    run(emu, 0x4A1570, "RigidDynCubeObj_NearZHandler", lambda e: near_setup(e, 5, **kw))
