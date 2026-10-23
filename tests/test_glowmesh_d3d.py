"""TerrainMesh_DrawD3D (0x512C30): the terrain meshes of a frame through Direct3D."""
import math
import random
import struct

import pytest

from test_d3d_util import D3DEmu, check, com, out, u32

DRAW = 0x512C30
G_DEVICE, VB, VB_DATA = 0x5CA114, 0xA7F890, 0xA7F89C
POOL = 0xA7F878
MESHES, MESH_COUNT = 0xB12C60, 0xB16C9C
DEV = {0: 3, 1: 1, 2: 1, 11: 3, 13: 2, 20: 3, 32: 8, 35: 3, 37: 4}
CWS = [0x37F, 0x7F, 0xC7F, 0x77F]

e = D3DEmu()
_tramps = {}


def with_cw(target, cw):
    addr = e.symbols[target] if isinstance(target, str) else target
    if (addr, cw) not in _tramps:
        p = e.fake_top
        e.fake_top += 32
        e.write(p, struct.pack("<H", cw) + b"\0\0" + b"\xd9\x2d" + struct.pack("<I", p) + b"\xe9" +
                struct.pack("<i", addr - (p + 15)))
        _tramps[(addr, cw)] = p + 4
    return _tramps[(addr, cw)]


def f32(em, p, *v):
    em.write(p, struct.pack("<%df" % len(v), *v))


def f64(em, p, *v):
    em.write(p, struct.pack("<%dd" % len(v), *v))


def rotation(rng):
    a, b, c, d = (rng.gauss(0, 1) for _ in range(4))
    n = math.sqrt(a * a + b * b + c * c + d * d)
    a, b, c, d = a / n, b / n, c / n, d / n
    return [a * a + b * b - c * c - d * d, 2 * (b * c - a * d), 2 * (b * d + a * c),
            2 * (b * c + a * d), a * a - b * b + c * c - d * d, 2 * (c * d - a * b),
            2 * (b * d - a * c), 2 * (c * d + a * b), a * a - b * b - c * c + d * d]


def strips(rng, n, count):
    """count faces in strips over n vertices: byte0 new corner, byte1 and byte2 the strip start."""
    faces = []
    while len(faces) < count:
        length = rng.randrange(1, 7)
        for i in range(length):
            c, b, a = (rng.randrange(n) for _ in range(3))
            if rng.random() < 0.08:
                b = c
            word = c | b << 8 | a << 16 | rng.randrange(0x10) << 24
            if i == length - 1 or len(faces) == count - 1:
                word |= 0x10000000
            faces.append(word)
            if len(faces) == count:
                break
    if rng.random() < 0.2:
        faces[-1] &= ~0x10000000
    return faces


def vert_counts(n0, n):
    """Vertices the do-while loops of the original visit: first kind, then all."""
    j1 = max(n0, 1)
    return j1, (j1 if j1 == n else max(n, j1 + 1))


def mesh(em, rng, cfg, cam, fwd, textures):
    n0 = cfg.get("n0", rng.randrange(1, 24))
    n = cfg.get("n", n0 + rng.randrange(0, 40 - n0 if n0 < 40 else 1))
    level = cfg.get("level", rng.randrange(14, 21))
    spread = cfg.get("spread", rng.choice([50.0, 300.0, 1200.0]))
    d = rng.uniform(*cfg.get("depth", (-300.0, 4000.0)))
    side = [rng.uniform(-1.5, 1.5) * max(d, 200.0) for _ in range(2)]
    right, up = cfg["right"], cfg["up"]
    center = [cam[k] + fwd[k] * d + right[k] * side[0] + up[k] * side[1] for k in range(3)]
    faces_n = cfg.get("faces", rng.choice([1, 4, 12, 30, 70, 110]))
    cnt = cfg.get("materials", rng.randrange(1, 4))
    size = 32 + 16 * (n + 2) + 16 + (4 + 20 * (n + 2)) * (2 * max(cnt, 1) + 1) + 0x400
    p = em.alloc(size)
    f64(em, p, *center)
    u32(em, p + 0x18, n0 << 16 | n)
    u32(em, p + 0x1C, level)
    q = p + 0x20
    j1, total = vert_counts(n0, n)
    for j in range(total):
        v = [center[k] + rng.uniform(-spread, spread) for k in range(3)]
        if j < j1:
            f32(em, q, *v, rng.uniform(-400.0, 400.0) if rng.random() < 0.7 else 0.0)
        else:
            parent = rng.randrange(j1) if rng.random() < 0.95 else rng.randrange(total)
            f32(em, q, *v)
            u32(em, q + 12, parent)
        q += 16
    faces = em.alloc(4 * max(faces_n, 1))
    for i, w in enumerate(strips(rng, total, max(faces_n, 1))):
        u32(em, faces + 4 * i, w)
    u32(em, q, rng.choice([0, 4, 5, 3]))
    u32(em, q + 4, faces)
    u32(em, q + 8, cnt)
    u32(em, q + 12, faces_n)
    q += 16
    for s_ in range(max(cnt, 1) * 2 + 1):
        lightmap = s_ == max(cnt, 1) * 2
        u32(em, q, 0 if lightmap and (cfg.get("no_lightmap") or rng.random() < 0.25) else rng.choice(textures))
        q += 4
        for j in range(total):
            k = j if j < j1 else rng.randrange(j1)
            flag = rng.choice([0, 8, 0x10, 0x18]) + rng.choice([0, 4])
            u32(em, q, k << 16 | (k * 32 + flag))
            f32(em, q + 4, rng.uniform(-2, 2), rng.uniform(-2, 2))
            q += 12
            if j >= j1:
                f32(em, q, rng.uniform(-1, 1), rng.uniform(-1, 1))
                q += 8
    return p


def scene(em, seed, **cfg):
    rng = random.Random(seed)
    u32(em, G_DEVICE, com(em, "dev", DEV, size=64))
    vbmem = em.alloc(0x40000)
    em.write(vbmem, bytes([0xAB]) * 0x40000)
    lock = cfg.get("lock", 0)
    u32(em, VB, com(em, "vb", {0: 3, 1: 1, 2: 1, 3: 4, 4: 1}, methods={3: (lock, out(2, vbmem))}))
    # render chunks
    em.call(0x516600)
    em.call(0x4B0B00, POOL, 0x2EF4, 0x80, 0)
    for a in (0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4):
        u32(em, a, em.alloc(8))
    u32(em, 0xC28B44, 640)
    u32(em, 0xC28B48, 480)
    f32(em, 0xA7DA30, rng.random(), rng.random(), rng.random())
    f32(em, 0xA7DA18, cfg.get("fog", rng.choice([0.0001, 0.001, 0.003])))
    u32(em, 0xBCAD78, rng.randrange(4))
    f32(em, 0xBCADA0, 10.0)
    f32(em, 0xBCADC8, 40000.0)
    f32(em, 0xBCAD8C, 1.0)
    f32(em, 0xBCADB4, 0.0)
    f32(em, 0xBCAD70, rng.choice([1.0, 0.5]))
    f32(em, 0xBCABEC, rng.choice([1.0, 0.01, 0.0005]))
    f32(em, 0xBCADC0, cfg.get("near", rng.choice([10.0, 100.0, 409.6])))
    f32(em, 0xBCADCC, cfg.get("far", rng.choice([1e6, 1e6, 40000.0])))
    f32(em, 0xBCABD0, 0.0, 0.0, 639.0, 479.0)
    f32(em, 0xC311A0, -300.0, 300.0)
    f32(em, 0xC311B0, 320.0, 240.0)
    u32(em, 0xC31088, cfg.get("morph_start", rng.randrange(0, 20)))
    f32(em, 0xC28FC0, cfg.get("morph_scale", rng.choice([0.05, 0.3, 1.0, 4.0])))
    u32(em, 0xC28FBC, 7)
    # view
    m = rotation(rng)
    f32(em, 0xBCAAE0, *m, 0.0)
    cam = [rng.uniform(-50000, 50000) for _ in range(3)]
    f64(em, 0xBCAB08, *cam)
    cfg = dict(cfg, right=m[0:3], up=m[3:6])
    fwd = m[6:9]
    textures = []
    for i in range(6):
        t = em.alloc(0x24)
        u32(em, t + 0x20, 0x5000 + i)
        textures.append(t)
    count = cfg.get("count", rng.randrange(1, 6))
    for i in range(count):
        p = 0 if cfg.get("null") and i == 1 else mesh(em, rng, cfg, cam, fwd, textures)
        u32(em, MESHES + 4 * i, p)
    u32(em, MESH_COUNT, count)


def run(seed, cw, **cfg):
    return check(e, with_cw(DRAW, cw), with_cw("TerrainMesh_DrawD3D", cw), setup=lambda em: scene(em, seed, **cfg),
                 check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(30))
def test_draw(seed, cw):
    run(seed, cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(20))
def test_draw_near(seed, cw):
    # meshes around the near plane
    run(100 + seed, cw, depth=(-400.0, 600.0), spread=300.0)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(10))
def test_draw_many_faces(seed, cw):
    run(200 + seed, cw, faces=120, depth=(-200.0, 1500.0))


@pytest.mark.parametrize("over", [
    # a NULL mesh crashes the original: it copies the centre before the check. With n0 = 0 a skipped
    # stream is skipped by 20n - 8n0 + 4 bytes, not by what the loops read, so keep the factors near 0.5.
    {"lock": 0x88760000}, {"count": 0}, {"far": 1.0}, {"no_lightmap": 1},
    {"morph_scale": 0.0}, {"morph_scale": 100.0, "morph_start": 0}, {"morph_scale": -1.0},
    {"n0": 0, "n": 0, "morph_scale": 1e-6, "morph_start": -500000},
    {"n0": 0, "n": 5, "morph_scale": 1e-6, "morph_start": -500000}, {"n0": 5, "n": 5}, {"n0": 1, "n": 1, "faces": 0},
    {"faces": 0}, {"materials": 0}, {"level": 26}, {"level": 30}, {"fog": math.nan}, {"fog": -math.inf},
    {"faces": 0, "depth": (-300.0, 300.0)}, {"n0": 0, "n": 0, "depth": (-300.0, 300.0), "morph_scale": 1e-6, "morph_start": -500000},
])
@pytest.mark.parametrize("seed", range(3))
def test_draw_paths(over, seed):
    run(300 + seed, 0x37F, **over)
