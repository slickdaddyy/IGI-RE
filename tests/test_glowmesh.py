"""TerrainMesh, BoneDynCubeObj Direct3D handlers and glows (0x50F770 - 0x512BA0)."""
import math
import random
import struct

import pytest

from test_bonemesh_util import (LOG, RET, add_child, d64, f32, handler, logfake, make_emu, pyfake, run, set_handler,
                                task, trap, u16, u32, vfake, with_cw)

CWS = [0x37F, 0x7F]
CWS_INT = [0x37F, 0x7F, 0xC7F, 0x77F]

emu = make_emu()


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def f32s(e, p, n):
    return struct.unpack("<%df" % n, e.read(p, 4 * n))


def stack_vertices(e, n, first):
    """n ClipVertex records passed by value, from argument index first; the screen part
    of a vertex in front of the near plane is stale stack and not logged."""
    out = []
    for k in range(n):
        raw = [e.arg(first + 7 * k + i) for i in range(7)]
        code = raw[6]
        out.append(tuple(raw[:3]) + (code,) + (tuple(raw[3:6]) if not code & 0x10 else ()))
    return out


def clip_depth(e):
    verts = stack_vertices(e, 3, 0)
    minz, near = e.arg(21), e.arg(22)
    LOG[-1].append(("clipdepth", verts, e.normalize(minz), near))
    # lower minZ to the nearest visible vertex so later meshes see it
    cur = struct.unpack("<f", e.read(minz, 4))[0]
    for v in verts:
        if not v[3]:
            z = struct.unpack("<f", struct.pack("<I", v[2]))[0]
            cur = min(cur, z)
    f32(e, minz, cur)
    e.ret(0)


pyfake(emu, 0x4A0340, clip_depth)
logfake(emu, 0x4B0E70, 1, strings=(0,), ret=0x29)
logfake(emu, 0x516600, 0)
logfake(emu, 0x4B0D60, 1)
logfake(emu, 0x4B0D90, 0)
logfake(emu, 0x4B7D30, 1)
logfake(emu, 0x4B7CF0, 3, derefs={1: 1024})
logfake(emu, 0x4C6160, 1)
logfake(emu, 0x4D2A20, 0)
vfake(emu, 0x518240, 0, ret=0x1234)
logfake(emu, 0x512C30, 0)
vfake(emu, 0x4950E0, 0, ret=3)
vfake(emu, 0x4B0F80, 0, ret=3)
# BoneDynCubeObj
vfake(emu, 0x4DAA20, 1, show=lambda e, a: (), effect=lambda e, a: (u32(e, a[0], 0x1111), 0x2222)[1])
vfake(emu, 0x4DA180, 2, show=lambda e, a: (), effect=lambda e, a: (u32(e, a[0], 0x3333), u32(e, a[1], 0x4444), 0)[2])
logfake(emu, 0x4CFC40, 0, ret=6)
logfake(emu, 0x4C56F0, 0, ret=0x22)
DRAW = trap(emu, "draw_bone", lambda e: (LOG[-1].append(("draw", e.normalize(e.u32(e.arg(0))),
                                                           e.read(e.arg(0) + 4, 16))), e.ret(0)))
CHILD = handler(emu, "child", 2, derefs={1: 0x40})
logfake(emu, 0x4D7550, 0)
logfake(emu, 0x4D14D0, 0, ret=0x36)
logfake(emu, 0x4D9610, 0, ret=0x99)
logfake(emu, 0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})
logfake(emu, 0x4D95E0, 0)
vfake(emu, 0x4D03D0, 3, show=lambda e, a: (e.read(a[0], 12), a[1], a[2]))
# glows
logfake(emu, 0x4978D0, 1, strings=(0,), effect=lambda e, a: 0x40 + len(e.read_cstr(a[0])))
logfake(emu, 0x4978E0, 1)
logfake(emu, 0x4978F0, 2)
logfake(emu, 0x4978B0, 2, derefs={1: 0x28})
logfake(emu, 0x497800, 1, effect=lambda e, a: e.alloc(a[0]))


def read_buffer(e):
    buf, x, y, w, h = [e.arg(i) for i in range(5)]
    LOG[-1].append(("readbuf", e.normalize(buf), x, y, w, h))
    if "pixels" in RET:
        e.write(buf, RET["pixels"])
    e.ret(0)


pyfake(emu, 0x4D3710, read_buffer)
logfake(emu, 0x5182B0, 3, derefs={1: 8, 2: 12})
logfake(emu, 0x4C1780, 1)


def fog(e, a):
    depth = struct.unpack("<f", struct.pack("<I", a[2]))[0]
    f32(e, a[0], (depth % 97) / 97.0)
    return 0


vfake(emu, 0x4DA790, 6, show=lambda e, a: (e.read(a[1], 24),) + tuple(a[2:]), effect=fog)
emu.baseline = emu.snapshot()


def view(e, rng):
    m = [1, 0, 0, 0, 1, 0, 0, 0, 1]
    f32(e, 0xBCAAE0, *[v + rng.uniform(-0.3, 0.3) for v in m])
    u32(e, 0xBCAB04, 7)
    d64(e, 0xBCAB08, *[rng.uniform(-50000, 50000) for _ in range(3)])
    f32(e, 0xBCAB28, rng.uniform(200, 500), rng.uniform(200, 500), rng.uniform(300, 340), rng.uniform(220, 260))
    f32(e, 0xBCABB8, rng.uniform(0, 30), rng.uniform(0, 30), rng.uniform(600, 640), rng.uniform(400, 480),
        rng.uniform(300, 320), rng.uniform(200, 240))
    f32(e, 0xBCABD0, rng.uniform(0, 50), rng.uniform(0, 50), rng.uniform(500, 640), rng.uniform(400, 480))
    f32(e, 0xBCADC0, rng.uniform(1, 10))
    u32(e, 0xB16C6C, rng.choice([0x100, 0x400, 0x1000, 0x4000]) + rng.randrange(0, 0x80))


# --- TerrainMesh_UpdateNearZ ------------------------------------------------

def mesh(e, rng, pos, type_=0x10, n=None):
    n = rng.randrange(0, 40) if n is None else n
    c = [pos[0] + rng.uniform(-3000, 3000), pos[1] + rng.uniform(-3000, 3000), pos[2] + rng.uniform(-2000, 35000)]
    p = e.alloc(0x20 + 16 * n + 16)
    d64(e, p, *c)
    u32(e, p + 0x18, n | rng.randrange(0, 0x10000) << 16, type_)
    for i in range(n):
        f32(e, p + 0x20 + 16 * i, *[c[k] + rng.uniform(-4000, 4000) for k in range(3)], rng.uniform(0, 1))
    nf = rng.randrange(0, 12) if n else 0
    faces = e.alloc(4 * nf + 4)
    for i in range(nf):
        a, b, c_ = (rng.randrange(n) for _ in range(3))
        u32(e, faces + 4 * i, a | b << 8 | c_ << 16 | rng.randrange(256) << 24)
    u32(e, p + 0x20 + 16 * n, 0x77, faces, 0x88, nf)
    return p


def near_setup(e, seed, count=None, types=None, minz=None):
    rng = random.Random(seed)
    view(e, rng)
    pos = struct.unpack("<3d", e.read(0xBCAB08, 24))
    count = rng.randrange(1, 6) if count is None else count
    for i in range(count):
        t = types[i] if types else rng.choice([0x10, 0x10, 0x10, 0x8])
        u32(e, 0xB12C60 + 4 * i, mesh(e, rng, pos, t))
    u32(e, 0xB16C9C, count)
    arg = e.alloc(12)
    f32(e, arg, 1.0, 2.0, rng.uniform(500, 20000) if minz is None else minz)
    return (arg,)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(12))
def test_update_near_z(seed, cw):
    run(emu, with_cw(emu, 0x50F770, cw), with_cw(emu, "TerrainMesh_UpdateNearZ", cw), lambda e: near_setup(e, seed))


@pytest.mark.parametrize("kw", [dict(count=0), dict(count=2, types=[8, 0x18]), dict(minz=-1e6), dict(minz=1e7)])
def test_update_near_z_paths(kw):
    run(emu, 0x50F770, "TerrainMesh_UpdateNearZ", lambda e: near_setup(e, 3, **kw))


def test_init():
    run(emu, 0x50FAC0, "TerrainMesh_Init", lambda e: None)


# --- TerrainMesh_SetupLighting -------------------------------------------------

def lighting_setup(e, seed, count=1, palette=True):
    rng = random.Random(seed)
    lights = e.alloc(0x28 * 3)
    for i in range(3):
        f32(e, lights + 0x28 * i + 0xC, *[rng.uniform(-0.3, 1.6) for _ in range(3)])
    order = e.alloc(16)
    u32(e, order, rng.randrange(3))
    u32(e, 0xB041E0, lights, order, count)
    u32(e, 0xB041F0, 0x28)
    f32(e, 0xA7DA20, *[rng.uniform(-0.2, 1.1) for _ in range(3)])
    f32(e, 0xA7DA18, rng.choice([0.0, rng.uniform(0.5, 30), rng.uniform(-5, 0)]))
    u32(e, 0xB16C6C, rng.choice([0x100, 0x400, 0x1000, 0x4000, 0x3FFFFF]) + rng.randrange(0, 0x80))
    t = e.alloc(0xE00)
    u32(e, t + 0xDF4, 0x5555 if palette else 0)
    return (t,)


@pytest.mark.parametrize("cw", CWS_INT)
@pytest.mark.parametrize("seed", range(6))
def test_setup_lighting(seed, cw):
    run(emu, with_cw(emu, 0x50FAE0, cw), with_cw(emu, "TerrainMesh_SetupLighting", cw),
        lambda e: lighting_setup(e, seed))


@pytest.mark.parametrize("kw", [dict(count=0), dict(palette=False)])
def test_setup_lighting_paths(kw):
    run(emu, 0x50FAE0, "TerrainMesh_SetupLighting", lambda e: lighting_setup(e, 1, **kw))


def test_set_unk_dbc():
    def setup(e):
        t = e.alloc(0xE00)
        u32(e, t + 0xDBC, 0x12345678)
        return (t,)
    run(emu, 0x50FD00, "TerrainMesh_SetTextures", setup)
    run(emu, 0x5116B0, "TerrainMesh_SetUnusedTextures", setup)


@pytest.mark.parametrize("vals", [(0.5, 0.25, 0.75, 100.0), (-1.0, 1.5, 1.0, -3.0), (0.0, float("nan"), 2.0, 16384.0),
                                  (float("nan"), -0.0, 1.0000001, 20000.0), (0.2, 0.3, 0.4, float("nan")),
                                  (float("inf"), float("-inf"), 0.9, float("inf"))])
def test_set_fog(vals):
    def setup(e):
        p = e.alloc(12)
        f32(e, p, *vals[:3])
        return (p, fbits(vals[3]))
    run(emu, 0x50FD20, "TerrainMesh_SetFog", setup)


def test_set_ambient():
    def setup(e):
        p = e.alloc(12)
        f32(e, p, 0.1, 0.2, 0.3)
        return (p,)
    run(emu, 0x50FE40, "TerrainMesh_SetAmbient", setup)


# --- frame setup -----------------------------------------------------------------

def frame_setup(e, seed, build=0, count=1, mode=3):
    rng = random.Random(seed)
    view(e, rng)
    u32(e, 0xB16CA4, build)
    u32(e, 0xB16C9C, count)
    RET[0x4950E0] = mode
    u32(e, 0xC28FBC, 0x99)
    u32(e, 0xC31090, 0x98)
    return ()


@pytest.mark.parametrize("cw", CWS_INT)
@pytest.mark.parametrize("seed", range(6))
def test_update(seed, cw):
    run(emu, with_cw(emu, 0x50FE60, cw), with_cw(emu, "TerrainMesh_Update", cw), lambda e: frame_setup(e, seed))


@pytest.mark.parametrize("cw", CWS_INT)
@pytest.mark.parametrize("seed", range(6))
def test_render(seed, cw):
    run(emu, with_cw(emu, 0x511500, cw), with_cw(emu, "TerrainMesh_Render", cw), lambda e: frame_setup(e, seed))


@pytest.mark.parametrize("kw", [dict(count=0), dict(mode=4)])
def test_frame_paths(kw):
    run(emu, 0x50FE60, "TerrainMesh_Update", lambda e: frame_setup(e, 2, **kw))
    run(emu, 0x511500, "TerrainMesh_Render", lambda e: frame_setup(e, 2, **kw))


# --- near plane clipping of terrain triangles -----------------------------------------

def clip_slots(e, rng, n=4):
    for i in range(n):
        z = rng.choice([rng.uniform(-500, 400), rng.uniform(420, 3000)])
        d64(e, 0xA7DAB0 + 24 * i, rng.uniform(-3000, 3000), rng.uniform(-3000, 3000), z)
        d64(e, 0xA7DA70 + 16 * i, rng.uniform(-2, 2), rng.uniform(-2, 2))
        f32(e, 0xA7DB10 + 48 * i, *[rng.uniform(-500, 500) for _ in range(12)])


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(8))
def test_clip_edge(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        view(e, rng)
        clip_slots(e, rng)
        a = rng.randrange(4)
        return (a, (a + rng.randrange(1, 4)) % 4)
    run(emu, with_cw(emu, 0x5122B0, cw), with_cw(emu, "TerrainMesh_ClipEdge", cw), setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("codes", range(8))
def test_clip_triangle(codes, cw):
    def setup(e):
        rng = random.Random(codes)
        view(e, rng)
        clip_slots(e, rng)
        args = []
        for size in (48, 24, 16):
            for i in range(3):
                p = e.alloc(size)
                vals = [rng.uniform(-500, 500) for _ in range(size // 4)]
                if size == 24:
                    d64(e, p, rng.uniform(-3000, 3000), rng.uniform(-3000, 3000), rng.uniform(-500, 3000))
                elif size == 16:
                    d64(e, p, rng.uniform(-2, 2), rng.uniform(-2, 2))
                else:
                    f32(e, p, *vals)
                args.append(p)
        for i in range(3):
            args.append((0x10 if codes >> i & 1 else 0) | rng.choice([0, 1, 2, 4, 8, 0x20, 0xFFFFFF00]))
        return tuple(args)
    run(emu, with_cw(emu, 0x512040, cw), with_cw(emu, "TerrainMesh_ClipTriangle", cw), setup)


# --- BoneDynCubeObj ----------------------------------------------------------------------

def bone_obj(e, rng, kids=2):
    t = task(e, 0x100, 0x31)
    d64(e, t + 0x20, *[rng.uniform(-5000, 5000) for _ in range(3)])
    f32(e, t + 0x78, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, t + 0x9C, 5)
    for i in range(kids):
        c = task(e, 0x40, 0x40 + i)
        add_child(e, t, c)
        set_handler(e, 0x22, 0x40 + i, CHILD if i != 1 else 0)
    return t


@pytest.mark.parametrize("kids", [0, 1, 2, 3])
def test_enter_handler(kids):
    def setup(e):
        rng = random.Random(kids)
        u32(e, 0xA94E84 + 4 * 6, DRAW)
        return (bone_obj(e, rng, kids),)
    run(emu, 0x5124C0, "BoneDynCubeObj_EnterHandler", setup)


def test_open_close():
    run(emu, 0x5125A0, "BoneDynCubeObj_OpenD3D", lambda e: None)
    run(emu, 0x5127D0, "BoneDynCubeObj_CloseD3D", lambda e: None)


def nearz_setup(e, seed, d4=1, model=True, excluded=0, minz=None):
    rng = random.Random(seed)
    view(e, rng)
    pos = struct.unpack("<3d", e.read(0xBCAB08, 24))
    o = bone_obj(e, rng, 0)
    d64(e, o + 0x20, *[p + rng.uniform(-300, 300) for p in pos[:2]], pos[2] + rng.uniform(-100, 600))
    f32(e, o + 0xA4, *[rng.choice([1.0, 2.0, rng.uniform(0.2, 3)]) for _ in range(3)])
    u32(e, o + 0xD4, d4)
    hdr = e.alloc(0x40)
    f32(e, hdr + 0x30, *[rng.uniform(-50, 50) for _ in range(3)], rng.uniform(1, 200))
    lod = e.alloc(0x10)
    u32(e, lod + 0xC, hdr)
    vm = e.alloc(0x10)
    u32(e, vm + 0xC, lod)
    m = e.alloc(4)
    u32(e, m, vm)
    u32(e, o + 0x74, m if model else 0)
    RET[0x4D03D0] = excluded
    arg = e.alloc(12)
    f32(e, arg, 0, 0, rng.uniform(-100, 600) if minz is None else minz)
    return (o, arg)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(12))
def test_near_z_handler(seed, cw):
    run(emu, with_cw(emu, 0x5125C0, cw), with_cw(emu, "BoneDynCubeObj_NearZHandler", cw),
        lambda e: nearz_setup(e, seed))


@pytest.mark.parametrize("kw", [dict(d4=0), dict(d4=2), dict(model=False), dict(excluded=1), dict(minz=-1e9),
                                dict(minz=1e9)])
def test_near_z_handler_paths(kw):
    run(emu, 0x5125C0, "BoneDynCubeObj_NearZHandler", lambda e: nearz_setup(e, 4, **kw))


def test_near_z_scales():
    for s in [(1, 2, 3), (3, 2, 1), (2, 3, 1), (2, 1, 3), (1, 1, 1), (2, 2, 1), (float("nan"), 1, 2), (1, float("nan"), 2)]:
        def setup(e, s=s):
            r = nearz_setup(e, 7)
            f32(e, r[0] + 0xA4, *s)
            return r
        run(emu, 0x5125C0, "BoneDynCubeObj_NearZHandler", setup)


# --- glows -----------------------------------------------------------------------------------

@pytest.mark.parametrize("cw", CWS_INT)
def test_glow_read_buffer(cw):
    for seed in range(8):
        def setup(e, seed=seed):
            rng = random.Random(seed)
            p = e.alloc(0x18)
            f32(e, p + 0xC, rng.uniform(-100, 700), rng.choice([16.5, 17.5, rng.uniform(-100, 500)]))
            u32(e, p + 0x14, 0x7777)
            return (p,)
        run(emu, with_cw(emu, 0x5127E0, cw), with_cw(emu, "Glow_ReadBufferHandler", cw), setup)


@pytest.mark.parametrize("cw", CWS)
def test_glow_init(cw):
    run(emu, with_cw(emu, 0x512820, cw), with_cw(emu, "Glow_Init", cw), lambda e: None)


def test_glow_shutdown():
    def setup(e):
        u32(e, 0xA7DBD8, 0x44)
        u32(e, 0xA7DC60, 0x45)
    run(emu, 0x512920, "Glow_Shutdown", setup)


def glow_pool(e, rng, count=10, used=None):
    stride, size = 0x804, 0x800
    data = e.alloc(stride * count)
    free = e.alloc(4 * count)
    order = list(range(count))
    rng.shuffle(order)
    used = rng.randrange(2, count) if used is None else used
    u32(e, free, *order)
    for i in range(used):
        u32(e, data + order[i] * stride + size, i)
    u32(e, 0xB015A0, data, free, used, count, stride, size)
    return [data + order[i] * stride for i in range(used)]


def instance_setup(e, seed, active=1, same=None):
    rng = random.Random(seed)
    u32(e, 0xBCAB38, active)
    e.call(0x512820)
    elems = glow_pool(e, rng)
    buf = rng.choice(elems)
    pix = bytes(rng.randrange(4) for _ in range(0x800))
    e.write(buf, pix)
    new = bytearray(pix)
    if same is None:
        for _ in range(rng.randrange(0, 1500)):
            new[rng.randrange(0x800)] ^= 1
    elif not same:
        new = bytes(b ^ 0xFF for b in pix)
    RET["pixels"] = bytes(new)
    p = e.alloc(0x28)
    u32(e, p + 0xC, 0x1234)
    f32(e, p + 0x10, rng.uniform(-50, 700), rng.uniform(-50, 500), *[rng.uniform(0, 2) for _ in range(3)])
    u32(e, p + 0x24, buf)
    return (p,)


@pytest.mark.parametrize("cw", CWS_INT)
@pytest.mark.parametrize("seed", range(6))
def test_glow_instance(seed, cw):
    run(emu, with_cw(emu, 0x512940, cw), with_cw(emu, "Glow_InstanceHandler", cw),
        lambda e: instance_setup(e, seed))


@pytest.mark.parametrize("kw", [dict(active=0), dict(same=True), dict(same=False)])
@pytest.mark.parametrize("seed", range(4))
def test_glow_instance_paths(kw, seed):
    run(emu, 0x512940, "Glow_InstanceHandler", lambda e: instance_setup(e, seed, **kw))


@pytest.mark.parametrize("count", [0, 1, 3])
def test_glow_queue_all(count):
    def setup(e):
        rng = random.Random(count)
        u32(e, 0xA7DC60, 0x46)
        for i in range(count):
            u32(e, 0xAFA840 + 0x1C * i, *[rng.randrange(1 << 32) for _ in range(7)])
        u32(e, 0xA800EC, count)
    run(emu, 0x512BA0, "Glow_QueueAll", setup)


# --- TerrainMesh_DrawTriangle ----------------------------------------------------------------------------------

# ClipTriangle copies three words of each vertex that nobody sets
POLY_IGNORE = [(0xA7DB10 + 48 * i + o, 0xA7DB10 + 48 * i + o + 4) for i in range(4) for o in (0x1C, 0x28, 0x2C)]


def poly_setup(e, seed, mode=None, near=None):
    rng = random.Random(seed)
    view(e, rng)
    clip_slots(e, rng)
    pos = struct.unpack("<3d", e.read(0xBCAB08, 24))
    t = e.alloc(0x94)
    for i in range(3):
        z = near[i] if near else rng.choice([rng.uniform(-800, 400), rng.uniform(420, 3000), rng.uniform(1e6, 3e6)])
        d64(e, t + 24 * i, pos[0] + rng.uniform(-2000, 2000), pos[1] + rng.uniform(-2000, 2000), pos[2] + z)
    f32(e, t + 0x48, *[rng.uniform(-0.5, 2.0) for _ in range(9)])
    f32(e, t + 0x6C, *[rng.uniform(-3, 3) for _ in range(6)])
    u32(e, t + 0x84, 0x5150, rng.randrange(2) if mode is None else mode, 0x99, rng.randrange(5))
    f32(e, 0xBA2030, *[rng.uniform(0, 1.5) for _ in range(3)])
    f32(e, 0xA7DA18, rng.uniform(0, 20))
    return (t,)


@pytest.mark.parametrize("cw", CWS_INT)
@pytest.mark.parametrize("seed", range(16))
def test_poly(seed, cw):
    run(emu, with_cw(emu, 0x5116D0, cw), with_cw(emu, "TerrainMesh_DrawTriangle", cw), lambda e: poly_setup(e, seed),
        ignore=POLY_IGNORE)


@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("near", [(100, 200, 300), (1000, 2000, 3000), (100, 1000, 2000), (1000, 100, 2000),
                                  (1000, 2000, 100), (100, 200, 2000), (100, 2000, 200), (2000, 100, 200),
                                  (-10, 500, 600), (0, 0, 0), (2e6, 500, 100)])
def test_poly_paths(mode, near):
    run(emu, 0x5116D0, "TerrainMesh_DrawTriangle", lambda e: poly_setup(e, 5, mode, near), ignore=POLY_IGNORE)


# --- TerrainMesh_Build ----------------------------------------------------------------------------

emu2 = make_emu()

def make_pool(e, base, rng, size, count, used):
    stride = size + 4
    data = e.alloc(stride * count)
    free = e.alloc(4 * count)
    order = list(range(count))
    rng.shuffle(order)
    u32(e, free, *order)
    for i in range(used):
        u32(e, data + order[i] * stride + size, i)
    u32(e, base, data, free, used, count, stride, size)


def shape(e, rng, n0, n1):
    n = n0 + n1
    nf = rng.randrange(1, 6)
    codes = []
    for j in range(n):
        c = rng.randrange(1 << 32) & ~0x3F
        if j >= n0:
            c |= rng.randrange(min(j, n0 + 2))
        codes.append(c)
    p = e.alloc(8 + 4 * nf + 4 * n)
    u16(e, p, nf, 4 * nf, n0, n1)
    u32(e, p + 8, *[rng.randrange(1 << 32) for _ in range(nf)])
    u32(e, p + 8 + 4 * nf, *codes)
    return p


def build_setup(e, seed, cells=None, pools=None, level=None, lm=None, hm=None, key=None, n=None, flags=None):
    rng = random.Random(seed)
    view(e, rng)
    sizes = (0x640, 0xC80, 8000)
    pools = pools or [(rng.randrange(4, 8), rng.randrange(0, 3)), (4, rng.randrange(0, 2)), (3, 0)]
    for k, (base, (count, used)) in enumerate(zip((0xB12B60, 0xB12C40, 0xB16CC0), pools)):
        make_pool(e, base, rng, sizes[k], count, used)
    d64(e, 0xB12BA0, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
    u32(e, 0xC30060, 0x30000)
    e.write(0xC30080, bytes(rng.randrange(256) for _ in range(0x800)))
    for mat in range(6):
        for lv in range(32):
            p = 0xC28FE4 + mat * 900 + lv * 28
            f32(e, p + 0xC, rng.uniform(0.001, 0.05))
            u32(e, p + 0x10, rng.randrange(4))
            f32(e, p + 0x14, rng.uniform(0.001, 0.05))
            u32(e, p + 0x18, rng.randrange(4))
    count = rng.randrange(1, 4) if cells is None else cells
    u32(e, 0xB16CA4, count)
    u32(e, 0xC31090, 5)
    u32(e, 0xB16C9C, rng.randrange(3))
    for i in range(count):
        c = 0xB16CE0 + 0x30 * i
        lev = rng.randrange(11, 18) if level is None else level
        s = 2.0 ** ((0x1B - lev) & 31)
        pos = [rng.uniform(-2e5, 2e5) for _ in range(3)]
        d64(e, c, *pos)
        n0 = rng.randrange(1, 14)
        n1 = rng.randrange(0, 14) if n is None else n
        lmi = rng.choice([0xFFFF, 1, 2]) if lm is None else lm
        hmi = rng.choice([0, 1, 2]) if hm is None else hm
        u32(e, c + 0x18, lev, rng.randrange(8) | rng.randrange(256) << 8 if flags is None else flags,
            shape(e, rng, n0, n1), lmi | hmi << 16 | rng.randrange(256) << 24,
            (rng.choice([0, 0x01, 0x0201, 0x030201, 0x00030102, 0x04030201]) if key is None else key),
            e.alloc(0x1C))
        lo = [p - 24 * s / 3 - 10 for p in pos]
        span = 64 * s / 3 + 20
        # lightmaps
        for k in (1, 2):
            r = 0xBA80B0 + 0x30 * k
            f32(e, r, rng.uniform(0.5, 300) / span * 256, rng.uniform(-50, 50))
            d64(e, r + 8, lo[0] + rng.uniform(-1, 1), lo[1] + rng.uniform(-1, 1))
            u32(e, r + 0x18, rng.randrange(1 << 32))
        # height maps: 129 x 129 samples
        for k in (1, 2):
            r = 0xBA2030 + 0x30 * k
            data = e.alloc(130 * 130)
            e.write(data, bytes(rng.randrange(256) for _ in range(130 * 130)))
            u32(e, r + 0xC, 4)
            f32(e, r + 0x10, 120 / span)
            d64(e, r + 0x18, lo[0], lo[1])
            u32(e, r + 0x28, data)
        # material maps: 64 x 64 bits
        for k in range(1, 5):
            r = 0xBA5050 + 0x30 * k
            bits = e.alloc(8 * 64)
            e.write(bits, bytes(rng.choice([0, 0xFF, rng.randrange(256)]) for _ in range(8 * 64)))
            u32(e, r, rng.randrange(5))
            u32(e, r + 0xC, 3)
            f32(e, r + 0x10, 60 / span)
            d64(e, r + 0x18, lo[0], lo[1])
            u32(e, r + 0x28, bits)
    return ()


@pytest.mark.parametrize("cw", CWS_INT)
@pytest.mark.parametrize("seed", range(12))
def test_build(seed, cw):
    run(emu2, with_cw(emu2, 0x510010, cw), with_cw(emu2, "TerrainMesh_Build", cw), lambda e: build_setup(e, seed))


@pytest.mark.parametrize("kw", [dict(level=13), dict(level=14), dict(level=15), dict(lm=0xFFFF), dict(hm=0),
                                dict(key=0), dict(n=0), dict(n=40), dict(cells=0),
                                dict(pools=[(3, 1), (3, 1), (3, 1)]), dict(pools=[(3, 0), (5, 2), (3, 0)]),
                                dict(pools=[(3, 0), (3, 0), (5, 2)]), dict(pools=[(5, 0), (3, 1), (5, 0)], n=30),
                                dict(pools=[(5, 0), (3, 1), (3, 1)], n=30), dict(pools=[(5, 0), (5, 0), (5, 0)], n=45)]
                         + [dict(flags=f) for f in range(8)])
@pytest.mark.parametrize("seed", range(3))
def test_build_paths(kw, seed):
    run(emu2, 0x510010, "TerrainMesh_Build", lambda e: build_setup(e, seed, **kw))


@pytest.mark.parametrize("cw", CWS_INT)
@pytest.mark.parametrize("seed", range(3))
def test_update_with_build(seed, cw):
    run(emu2, with_cw(emu2, 0x50FE60, cw), with_cw(emu2, "TerrainMesh_Update", cw), lambda e: build_setup(e, seed))
