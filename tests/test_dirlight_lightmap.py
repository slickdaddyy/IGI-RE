"""LightmapInfo and the lightmap upload helpers (0x523900 - 0x526860)."""
import random
import struct

import pytest

from emu import Emu
from test_dirlight_util import (CWS, EVENT4, LOAD_EVENT, LOG, MATCH_EVENT, RET, f32, fbits, handler, logfake, logger, pyfake,
                                run, set_handler, task, trap, u8, u16, u32)
from test_vehicle_util import init_emu, setup_init

PARENT_TYPE, OBJ_TYPE = 0x50, 0x51
MATCH, LOAD, EV4 = 0x30, 0x31, 0x32
INFO = []

e = Emu()
logfake(e, 0x4B0D60, 1)  # Mem_PushMode
logfake(e, 0x4B0D90, 0)  # Mem_PopMode
logfake(e, 0x4B22F0, 1)  # Texture_Free
logfake(e, 0x4B2610, 1)  # Texture_SetActiveTMU
logfake(e, 0x4B2320, 8)  # Texture_Alloc
logfake(e, 0x4B2450, 7)  # Texture_Download
logfake(e, 0x5237E0, 1)
logfake(e, 0x523670, 1)
logfake(e, 0x48F230, 0, ret=1)
logfake(e, 0x523110, 1, ret=0x6110)
logfake(e, 0x5233A0, 1)


def lightmaps(e, rng, count, unk38=None):
    lm = e.alloc(0x3C + 0x2C * count)
    e.write(lm, bytes(rng.randrange(256) for _ in range(0x3C + 0x2C * count)))
    u32(e, lm + 0x2C, count)
    if unk38 is not None:
        u32(e, lm + 0x38, unk38)
    for i in range(count):
        u16(e, lm + 0x3C + 0x2C * i + 0x28, rng.randrange(1, 300), rng.randrange(1, 300))
    return lm


def build(e, args):
    """sub_523D40: a new Lightmaps, or NULL every third call; also counts in the globals it fills."""
    n = RET.setdefault("built", 0)
    RET["built"] = n + 1
    u32(e, 0xA83B4C, e.u32(0xA83B4C) + 3)
    u32(e, 0xA839F4, e.u32(0xA839F4) + 5)
    if n % 3 == 2:
        return 0
    return lightmaps(e, random.Random(n), n % 4)


pyfake(e, 0x523D40, logger(e, 0x523D40, 8, effect=build))


def vertex_lights(e, args):
    if RET.get("novl"):
        return 0
    rng = random.Random(7)
    n = rng.randint(0, 4)
    vl = e.alloc(4 + 12 * n)
    u32(e, vl, n)
    for i in range(n):
        u32(e, vl + 4 + 12 * i, 0x100 + i, 0x200 + i, rng.randrange(0, 50))
    return vl


pyfake(e, 0x4BD070, logger(e, 0x4BD070, 1, effect=vertex_lights))


def nothing(e):
    args = (e.arg(0), e.arg(1)) if INFO and e.arg(0) == INFO[0] else (e.arg(0),)
    if LOG:
        LOG[-1].append(("nothing",) + args)
    e.ret(0)


pyfake(e, 0x4D3710, nothing)


def match(e):
    arg = e.arg(1)
    if LOG:
        LOG[-1].append(("match", e.arg(0), e.u32(arg)))
    u32(e, arg + 4, RET.get("match", 0))
    e.ret(0)


def load(e):
    arg = e.arg(1)
    if LOG:
        LOG[-1].append(("load", e.arg(0), e.u32(arg)))
    if "load" in RET:
        u32(e, arg, RET["load"])
    e.ret(0)


MATCH_FN = trap(e, "match_h", match)
LOAD_FN = trap(e, "load_h", load)
EV4_FN = handler(e, "ev4", 2)
e.baseline = e.snapshot()


def events(e):
    u8(e, MATCH_EVENT, MATCH)
    u8(e, LOAD_EVENT, LOAD)
    u8(e, EVENT4, EV4)
    set_handler(e, MATCH, PARENT_TYPE, MATCH_FN)
    set_handler(e, LOAD, PARENT_TYPE, LOAD_FN)
    set_handler(e, EV4, OBJ_TYPE, EV4_FN)


def tex(e, rng):
    t = e.alloc(0x2C)
    e.write(t, bytes(rng.randrange(256) for _ in range(0x2C)))
    return t


def lightmap_set(e, rng, n):
    s = e.alloc(0x18 + 0x24 * n)
    e.write(s, bytes(rng.randrange(256) for _ in range(0x18 + 0x24 * n)))
    u32(e, s, n)
    for i in range(n):
        r = s + 0x18 + 0x24 * i
        kind = rng.randrange(3)
        lm = 0 if kind == 0 else lightmaps(e, rng, rng.randrange(3), rng.choice([0, 0x55]))
        u32(e, r, lm, 0x700 + i, task(e, 0x20, OBJ_TYPE))
        u32(e, r + 0x18, rng.randrange(5))
    return s


# --- small helpers -----------------------------------------------------------

def test_tex_free():
    run(e, 0x523900, "LightmapTex_Free", lambda e: (tex(e, random.Random(1)),))


@pytest.mark.parametrize("seed", range(4))
def test_tex_alloc(seed):
    def setup(e):
        t = tex(e, random.Random(seed))
        if seed == 3:
            u16(e, t + 0x28, 0x8001, 0xFFFF)
        return (t,)
    run(e, 0x525320, "LightmapTex_Alloc", setup)


@pytest.mark.parametrize("seed", range(3))
def test_tex_upload(seed):
    run(e, 0x525410, "LightmapTex_Upload", lambda e: (tex(e, random.Random(seed)),))


@pytest.mark.parametrize("count", [None, 0, 1, 3])
def test_upload_ref(count):
    def setup(e):
        r = e.alloc(0x24)
        u32(e, r, 0 if count is None else lightmaps(e, random.Random(2), count))
        return (r,)
    run(e, 0x5253D0, "Lightmap_UploadRef", setup)


@pytest.mark.parametrize("lods,uploaded", [(0, 0), (1, 0), (3, 0), (3, 1)])
def test_upload_object(lods, uploaded):
    def setup(e):
        rng = random.Random(lods)
        obj = e.alloc(0xE0)
        model = e.alloc(0x40)
        u16(e, model + 0x3C, lods)
        refs = e.alloc(0x24 * max(lods, 1))
        for i in range(lods):
            u32(e, refs + 0x24 * i, lightmaps(e, rng, i + 1) if i != 1 else 0)
        u32(e, refs + 0x20, uploaded)
        u32(e, obj + 0x6C, model)
        u32(e, obj + 0xDC, refs)
        return (obj,)
    run(e, 0x525380, "Lightmap_UploadObject", setup)


def test_get_params():
    def setup(e):
        u32(e, 0xA83B3C, 0x2468)
        return 0, e.alloc(4)
    run(e, 0x525370, "LightmapInfo_GetParams", setup)


def test_is_light_source():
    run(e, 0x525450, "Dirlight_IsLightSource", lambda e: (0, e.alloc(4)))


# --- Line2_Intersect ---------------------------------------------------------

@pytest.mark.parametrize("seed", range(30))
@pytest.mark.parametrize("cw", CWS)
def test_line_intersect(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        pts = [[rng.uniform(-100, 100), rng.uniform(-100, 100)] for _ in range(4)]
        if seed < 8:
            a, b = divmod(seed % 4, 2)
            pts[2 + b] = list(pts[a])
            if seed >= 4:
                pts[2 + b][1] += 1.0
        if seed == 9:
            pts[3] = [pts[2][0] + pts[1][0] - pts[0][0], pts[2][1] + pts[1][1] - pts[0][1]]
        ps = []
        for p in pts:
            q = e.alloc(8)
            f32(e, q, *p)
            ps.append(q)
        return (e.alloc(8),) + tuple(ps)
    run(e, 0x5251D0, "Line2_Intersect", setup, cw=cw)


# --- Dirlight_AddVertexLight -------------------------------------------------

@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("cw", CWS)
def test_add_vertex_light(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        dl = e.alloc(0x64)
        f32(e, dl + 0x30, *[rng.uniform(0, 2) for _ in range(6)])
        info = e.alloc(0xE0)
        lset = e.alloc(0x18)
        f32(e, lset + 8, rng.uniform(0, 30000))
        u32(e, info + 0x88, lset)
        nl = 3
        vls = e.alloc(4 + 12 * nl)
        u32(e, info + 0xC8, vls)
        nv = 6
        verts = [e.alloc(12) for _ in range(2 * nv)]
        for v in verts:
            f32(e, v, *[rng.uniform(-1, 3) for _ in range(3)])
        a, b = e.alloc(4 * nv), e.alloc(4 * nv)
        u32(e, a, *verts[:nv])
        u32(e, b, *(verts[nv:] if seed % 2 else verts[:nv]))
        u32(e, vls, nl)
        for i in range(nl):
            n = rng.randrange(0, 6) if i != 1 else 5
            w, idx = e.alloc(4 * max(n, 1)), e.alloc(4 * max(n, 1))
            if n:
                f32(e, w, *[rng.uniform(0, 1) for _ in range(n)])
                u32(e, idx, *[rng.randrange(nv) for _ in range(n)])
            u32(e, vls + 4 + 12 * i, w, idx, n)
        arg = e.alloc(16)
        u32(e, arg, seed % nl if seed < 6 else 1, info, a, b)
        return dl, arg
    run(e, 0x526150, "Dirlight_AddVertexLight", setup, cw=cw)


# --- LightmapInfo_Changed ----------------------------------------------------

@pytest.mark.parametrize("seed", range(60))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_changed(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        events(e)
        parent = task(e, 0x40, PARENT_TYPE)
        info = e.alloc(0xE0)
        e.write(info, bytes(rng.randrange(256) for _ in range(0xE0)))
        INFO[:] = [info]
        u32(e, info + 0x14, parent)
        flags = rng.randrange(8) if seed % 5 else 0
        u8(e, info + 0x6F, flags & 1, (flags >> 1) & 1, (flags >> 2) & 1)
        e.write(info + 0x78, b"lmap\0")
        lset = lightmap_set(e, rng, rng.randrange(4)) if seed % 7 else 0
        u32(e, info + 0x88, lset)
        f32(e, info + 0x94, rng.uniform(0, 10))
        f32(e, info + 0x98, rng.choice([1.0, 1.0, 0.5]))
        u32(e, info + 0xC8, rng.choice([0, 0xC8C8]))
        u32(e, info + 0xCC, rng.randrange(2))
        u32(e, info + 0xDC, rng.choice([0, 0, 1]))
        RET["match"] = rng.choice([0, 1, 1])
        RET["built"] = 0
        if rng.random() < 0.3:
            RET["load"] = rng.choice([0, lightmap_set(e, rng, rng.randrange(1, 4))])
        else:
            RET.pop("load", None)
        RET[0x48F230] = rng.choice([0, 1, 1])
        if rng.random() < 0.3:
            RET["novl"] = 1
        else:
            RET.pop("novl", None)
        return (info,)
    run(e, 0x523930, "LightmapInfo_Changed", setup, cw=cw)


# --- Init / Shutdown -----------------------------------------------------------

ie = init_emu()


def test_init():
    run(ie, 0x526330, "LightmapInfo_Init", setup_init)


def test_shutdown():
    def setup(e):
        setup_init(e)
        e.call(0x526330)
    run(ie, 0x526860, "LightmapInfo_Shutdown", setup)


# --- Dirlight_TraceVertices --------------------------------------------------

te = Emu()
logfake(te, 0x4B0C60, 2, effect=lambda e, a: e.alloc(a[0]))
logfake(te, 0x4B0D10, 1)
NIDS = 8


def raster(e):
    info, origin, m, ids, depth, w, h = (e.arg(i) for i in range(7))
    rest = [e.arg(i) for i in range(7, 15)]
    if LOG:
        LOG[-1].append(("raster", info, e.read(origin, 24), e.read(m, 36), w, h) + tuple(rest))
    n = RET.setdefault("raster", 0)
    RET["raster"] = n + 1
    rng = random.Random(1000 * RET["seed"] + n)
    for i in range(w * h):
        u32(e, ids + 4 * i, rng.choice([0xFFFFFFFF, rng.randrange(NIDS)]))
        f32(e, depth + 4 * i, rng.uniform(0, 120))
    e.ret(0)


def line_of_sight(e):
    world, a, b = e.arg(0), e.arg(1), e.arg(2)
    rest = [e.arg(i) for i in range(3, 7)]
    if LOG:
        LOG[-1].append(("los", world, e.read(a, 24), e.read(b, 24)) + tuple(rest))
    n = RET.setdefault("los", 0)
    RET["los"] = n + 1
    rng = random.Random(7919 * RET["seed"] + n)
    f32(e, e.arg(7), rng.choice([0.0, -0.5, rng.uniform(0, 1), rng.uniform(0, 1)]))
    e.ret(1)


pyfake(te, 0x521E40, raster)
pyfake(te, 0x4CE410, line_of_sight)


def trace_setup(seed):
    def setup(e):
        rng = random.Random(seed)
        RET.update(seed=seed, raster=0, los=0)
        u32(e, 0xA44344, 0x4444)
        dl = e.alloc(0x64)
        d = [0.0, 0.0, 0.0] if seed == 3 else [rng.uniform(-1, 1) for _ in range(3)]
        f32(e, dl + 0x24, *d)
        n = rng.randint(1, 3)
        lset = e.alloc(0x18 + 0x24 * n)
        f32(e, lset + 8, rng.uniform(10, 3000), *[rng.uniform(-50, 50) for _ in range(3)])
        u32(e, lset, n)
        mat, pos = e.alloc(36), e.alloc(24)
        f32(e, mat, *[rng.uniform(-1, 1) for _ in range(9)])
        e.write(pos, struct.pack("<3d", *[rng.uniform(-1e5, 1e5) for _ in range(3)]))
        for i in range(n):
            obj = e.alloc(0xB0)
            u32(e, obj + 0xA4, rng.randrange(1 << 32))
            u32(e, lset + 0x18 + 0x24 * i + 8, obj, mat, pos)
        info = e.alloc(0xE0)
        u32(e, info + 0x88, lset)
        u32(e, info + 0xA0, [0, 1, 3, 4, 5][seed % 5])
        vls = e.alloc(4 + 12 * 3)
        u32(e, vls, 3)
        index = e.alloc(4)
        u32(e, index, seed % 3)
        ids, weights = e.alloc(4 * NIDS), e.alloc(4 * NIDS)
        areas, normals = e.alloc(4 * NIDS), e.alloc(4 * NIDS)
        for i in range(NIDS):
            a, nv = e.alloc(4), e.alloc(12)
            f32(e, a, rng.choice([0.0, rng.uniform(0, 5000), rng.uniform(0, 5000)]))
            f32(e, nv, *[rng.uniform(-1, 1) for _ in range(3)])
            u32(e, areas + 4 * i, a)
            u32(e, normals + 4 * i, nv)
        arg = e.alloc(28)
        u32(e, arg, vls, index, ids, areas, weights, normals, info)
        return dl, arg
    return setup


@pytest.mark.parametrize("seed", range(12))
@pytest.mark.parametrize("cw", CWS)
def test_trace_vertices(seed, cw):
    run(te, 0x525460, "Dirlight_TraceVertices", trace_setup(seed), cw=cw)


# --- Lightmap_BuildLod -------------------------------------------------------

be = Emu()
logfake(be, 0x4B0C60, 2, effect=lambda e, a: e.alloc(a[0]))
logfake(be, 0x4B0D60, 1)  # Mem_PushMode
logfake(be, 0x4B0D90, 0)  # Mem_PopMode
logfake(be, 0x4B2610, 1)  # Texture_SetActiveTMU
logfake(be, 0x4B2320, 8)  # Texture_Alloc
logfake(be, 0x4B5B60, 1, strings=(0,))  # Resource_Exists
logfake(be, 0x4B63D0, 1)  # Resource_Flush
logfake(be, 0x52D630, 2, derefs={0: 28, 1: 28})  # SystemTime_Compare
logfake(be, 0x52D440, 1, effect=lambda e, a: u32(e, a[0], 2000, 1, 2, 3, 4, 5, 6))  # SystemTime_Get


def resource_load(e):
    if LOG:
        LOG[-1].append(("load", e.read_cstr(e.arg(0)), e.arg(1)))
    e.ret(RET.get("file", 0))


pyfake(be, 0x4B5F00, resource_load)


def olm_file(e, rng, count, magic=0x3DF5C28F, version=0.1):
    sizes = [(rng.randint(1, 4), rng.randint(1, 4)) for _ in range(count)]
    n = 0x3C + 0x2C * count + sum(4 * w * h for w, h in sizes)
    f = e.alloc(n)
    e.write(f, bytes(rng.randrange(256) for _ in range(n)))
    u32(e, f, magic)
    f32(e, f + 4, version)
    u32(e, f + 0x2C, count, rng.randrange(100), rng.randrange(100))
    for i, (w, h) in enumerate(sizes):
        u16(e, f + 0x3C + 0x2C * i + 0x28, w, h)
    return f


def build_mesh(e, rng, kind):
    """A lightmapped mesh with ntex textures; returns (lod, header, mesh)."""
    ntex = 2
    nverts = 24
    verts = e.alloc(0x28 * nverts)
    for i in range(nverts):
        e.write(verts + 0x28 * i, struct.pack("<3f", *[rng.uniform(-50, 50) for _ in range(3)]))
        uv = [rng.uniform(-0.2, 1.2), rng.uniform(-0.2, 1.2)]
        if i % 6 == 5:
            uv = [round(uv[0] * 4) / 4, round(uv[1] * 3) / 3]
        f32(e, verts + 0x28 * i + 0x20, *uv)
    f32(e, verts + 0x28 * 7 + 0x20, *rd_uv(e, verts, 3))  # a zero length edge
    sizes = e.alloc(8 * ntex)
    for t in range(ntex):
        u16(e, sizes + 8 * t, rng.randint(1, 4), rng.randint(1, 3))
    tris = [[rng.randrange(nverts) for _ in range(3)] for _ in range(14)]
    tris += [[0, 1, 2], [2, 1, 0], [3, 7, 9], [4, 4, 5]]
    chunks = []
    split = [tris[:7], tris[7:12], tris[12:]]
    for ci, part in enumerate(split):
        ch = e.alloc(0x20 + 6 * len(part) + 8)
        e.write(ch, bytes(rng.randrange(256) for _ in range(0x20)))
        u16(e, ch + 0xC, 3 * len(part))
        u8(e, ch + 0x10, rng.randrange(256))
        u16(e, ch + 0x12, ci % ntex if ci != 2 else 0)
        u16(e, ch + 0x14, 0)
        u8(e, ch + 0x1E, rng.choice([0, 1]))
        for j, (a, b, c) in enumerate(part):
            u16(e, ch + 0x20 + 6 * j, a, b, c)
        chunks.append(ch)
    # the chunks are found by offset from the previous one, so copy them into one block
    block = e.alloc(0x400)
    pos = block
    for ci, ch in enumerate(chunks):
        size = 0x20 + 6 * (e.read(ch + 0xC, 2)[0] | e.read(ch + 0xD, 1)[0] << 8) // 3
        size = 0x20 + 2 * (e.read(ch + 0xC, 1)[0] | e.read(ch + 0xD, 1)[0] << 8)
        e.write(pos, e.read(ch, size))
        u16(e, pos + 0xE, 0xFFFF if ci == len(chunks) - 1 else size - 0x20 + 4)
        pos += size + 4
    mesh = e.alloc(0x30)
    u32(e, mesh + 4, ntex, 1)
    u32(e, mesh + 0x14, verts, sizes, block)
    u32(e, mesh + 0x28, 0x77)
    if kind == "nomesh":
        u32(e, mesh + 8, 0)
    header = e.alloc(0x28)
    f32(e, header, 0.1)
    u32(e, header + 4, 1999, 1, 2, 3, 4, 5, 6)
    u32(e, header + 0x20, 2 if kind == "format" else 3, mesh)
    data = e.alloc(0x10)
    u32(e, data + 0xC, header)
    lod = e.alloc(0x10)
    u32(e, lod + 0xC, data)
    return lod


def rd_uv(e, verts, i):
    return struct.unpack("<2f", e.read(verts + 0x28 * i + 0x20, 8))


def build_setup(seed, kind):
    def setup(e):
        rng = random.Random(seed)
        lod = build_mesh(e, rng, kind)
        name = e.alloc(16)
        e.write(name, b"room\0")
        u32(e, 0xA83B4C, 100, )
        u32(e, 0xA839F4, 50)
        RET[0x4B5B60] = 0 if kind in ("build", "format", "nomesh") else 1
        RET[0x52D630] = 1 if kind == "old" else -1
        if kind != "build":
            count = 3 if kind == "count" else 2
            RET["file"] = olm_file(e, rng, count, magic=0x1234 if kind == "magic" else 0x3DF5C28F,
                                   version=0.2 if kind == "version" else 0.1)
        return lod, seed % 2, fbits(2048.0), name, seed, 1 if kind == "full" else 0, 1, 0
    return setup


@pytest.mark.parametrize("kind", ["format", "nomesh", "load", "magic", "version", "old", "count", "full"])
def test_build_lod_paths(kind):
    run(be, 0x523D40, "Lightmap_BuildLod", build_setup(1, kind), check_eax=True)


@pytest.mark.parametrize("seed", range(10))
@pytest.mark.parametrize("cw", CWS)
def test_build_lod(seed, cw):
    run(be, 0x523D40, "Lightmap_BuildLod", build_setup(seed, "build"), cw=cw, check_eax=True)
