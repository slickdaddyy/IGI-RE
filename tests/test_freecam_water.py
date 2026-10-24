import math
import random
import struct

import pytest

from test_freecam_util import CWS, FISTP_CWS, d64, f32, new_emu, run, u32
from test_patrol_util import fake

e = new_emu()
fe = new_emu()

SPLITS = 0xA84584
W_SIZE = 0x4AD0


def grid(w, level, x, y):
    return w + 0xC40 + level * 0x300 + x * 0xC0 + y * 0x30


def node(e, p, x, y, size, level=0, active=0, flags=0, edges=0, visible=0, parent=0):
    d64(e, p, x, y)
    u32(e, p + 0x10, 0, flags, edges, level, size, visible, active, parent)


def water(e, cam=(1000.0, 2000.0, 300.0), cam_dir=(0.0, 1.0, 0.0), height=10.0, cam_height=None, env=1, debug=0,
          max_level=0):
    w = e.alloc(W_SIZE)
    e.write(w, b"\0" * W_SIZE)
    d64(e, w + 0x4870, *cam)
    f32(e, w + 0x4A48, *cam_dir)
    d64(e, w + 0x4A78, cam_height if cam_height is not None else cam[2] - height)
    f32(e, w + 0x4A90, height - 0.5, height)
    u32(e, w + 0x4ABC, max_level, env, 0, debug)
    return w


def unit(v):
    n = math.sqrt(sum(c * c for c in v))
    return tuple(c / n for c in v)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(12))
def test_calc_env_uv(seed, cw):
    def setup(e):
        r = random.Random(seed)
        w = water(e, cam=(r.uniform(-5000, 5000), r.uniform(-5000, 5000), r.uniform(50, 2000)))
        pos = e.alloc(24)
        d64(e, pos, r.uniform(-5000, 5000), r.uniform(-5000, 5000), r.uniform(-20, 20))
        n = e.alloc(12)
        if seed == 0:
            f32(e, n, 0.0, 0.0, 1.0)
        else:
            f32(e, n, *unit((r.uniform(-0.3, 0.3), r.uniform(-0.3, 0.3), 1.0)))
        uv = e.alloc(8)
        f32(e, uv, 7.0, 7.0)
        return w, uv, pos, n
    run(e, 0x52F3D0, "WaterMesh_CalcEnvUV", setup, cw=cw)


def test_calc_env_uv_off():
    def setup(e):
        w = water(e, env=0)
        return w, e.alloc(8), e.alloc(24), e.alloc(12)
    run(e, 0x52F3D0, "WaterMesh_CalcEnvUV", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(16))
def test_check_visible(seed, cw):
    def setup(e):
        r = random.Random(seed)
        cam = (r.uniform(-3000, 3000), r.uniform(-3000, 3000), r.uniform(10, 500))
        w = water(e, cam=cam, cam_dir=unit((r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-0.5, 0.1))))
        n = e.alloc(0x30)
        size = 1 << r.randrange(8, 14)
        if seed < 3:
            x, y = cam[0] - size / 2 + r.uniform(-50, 50), cam[1] - size / 2 + r.uniform(-50, 50)
        else:
            x, y = r.uniform(-8000, 8000), r.uniform(-8000, 8000)
        node(e, n, x, y, size, level=2, visible=7)
        return w, n
    run(e, 0x52FE60, "WaterMesh_CheckVisible", setup, cw=cw)


def test_check_visible_zero():
    def setup(e):
        w = water(e, cam=(512.0, 512.0, 10.0), height=10.0)
        n = e.alloc(0x30)
        node(e, n, 0.0, 0.0, 1, visible=7)
        return w, n
    run(e, 0x52FE60, "WaterMesh_CheckVisible", setup)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("seed", range(10))
def test_subdivide(seed, cw):
    def setup(e):
        r = random.Random(seed)
        e.write(SPLITS, b"\0" * 80)
        if seed == 9:
            u32(e, SPLITS, 5)
        size = 1 << r.randrange(18, 21)
        x, y = r.uniform(-4e5, 4e5), r.uniform(-4e5, 4e5)
        if seed % 3 == 0:
            cam = (x + r.uniform(0, size), y + r.uniform(0, size), r.uniform(10, 3000))
        else:
            cam = (x + r.uniform(-size, 2 * size), y + r.uniform(-size, 2 * size), r.uniform(10, 4e5))
        w = water(e, cam=cam, cam_dir=unit((r.uniform(-1, 1), r.uniform(-1, 1), -0.3)), height=r.uniform(-5, 5),
                  max_level=r.randrange(3))
        n = w + 0x40 + 0x30 * r.randrange(64)
        level = 0 if seed % 4 else r.randrange(1, 4)
        node(e, n, x, y, size, level=level)
        return w, n
    run(e, 0x52FFA0, "WaterMesh_Subdivide", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(24))
def test_set_edge_flags(seed, cw):
    level = random.Random(seed).choice([0, 1, 4])
    x, y = random.Random(seed + 100).randrange(4), random.Random(seed + 200).randrange(4)

    def setup(e):
        r = random.Random(seed)
        w = water(e, max_level=level + (1 if seed % 5 == 0 else r.randrange(2, 5)))
        for lv in (level, level + 1):
            for i in range(4):
                for j in range(4):
                    size = 1024 >> lv
                    node(e, grid(w, lv, i, j), i * size + r.choice([0, 0, 0.5, -0.5]) * size,
                         j * size + r.choice([0, 0, 0.5, -0.5]) * size, size, level=lv + 1,
                         active=int(r.random() < 0.7), edges=0x12345)
        return w, level, x, y
    run(e, 0x52FBA0, "WaterMesh_SetEdgeFlags", setup, cw=cw)


def test_close():
    def setup(e):
        u32(e, 0xA84578, 77)
    run(fe, 0x530210, "WaterMesh_Close", setup)


# WaterMesh_DrawSplitTri with sub_52DAE0, sub_52E180 and TerrainMesh_DrawTriangle faked


def vertex(e, args):
    w, out, pos = args
    x, y, z = struct.unpack("<3d", e.read(pos, 24))
    h = math.sin(x * 0.01) * 3 + math.cos(y * 0.013) * 2
    d64(e, out + 8, x, y, h)
    f32(e, out + 0x38, *unit((math.sin(x * 0.02) * 0.2, math.cos(y * 0.03) * 0.2, 1.0)))
    return 0


def color(e, args):
    w, out, n = args
    nx, ny, nz = struct.unpack("<3f", e.read(n, 12))
    f32(e, out, nx * 0.5 + 0.5, ny * 0.25 + 0.5, nz)
    return 0


fake(fe, 0x52DAE0, 3, skip=(1,), derefs={2: 24}, effect=vertex)
fake(fe, 0x52E180, 3, skip=(1,), derefs={2: 12}, effect=color)
fake(fe, 0x5116D0, 1, derefs={0: 0x8C})
fake(fe, 0x4978E0, 1)


def tri(e, r, env):
    t = e.alloc(0x90)
    x0, y0 = r.uniform(-1000, 1000), r.uniform(-1000, 1000)
    s = 2 ** r.randrange(4, 9)
    d64(e, t, x0, y0, 1.0, x0 + s, y0, 2.0, x0 + s, y0 + s, 3.0)
    f32(e, t + 0x48, *[r.uniform(0, 1) for _ in range(9)])
    f32(e, t + 0x6C, *[r.uniform(-2, 2) for _ in range(6)])
    u32(e, t + 0x84, 0x1234, env, 0x55)
    return t


CASES = [(2, 3, 0, 1, 1, 0), (8, 4, 1, 1, 1, 0), (0xA, 2, 2, 0, 0, 0), (4, 5, 0, 1, 1, 1), (0x10, 3, 3, 1, 1, 0),
         (0x14, 2, 1, 0, 1, 1), (0x1, 3, 0, 1, 1, 0), (2, 1, 0, 1, 1, 0), (8, 6, 2, 0, 0, 1), (0x10, 4, 0, 0, 0, 0),
         (2, 7, 1, 1, 1, 1), (0x18, 3, 1, 1, 1, 0), (6, 2, 0, 1, 1, 0), (0x22, 2, 0, 0, 0, 0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("edges,count,base,recalc,env,debug", CASES)
def test_draw_split_tri(edges, count, base, recalc, env, debug, cw):
    def setup(e):
        r = random.Random(edges * 100 + count * 10 + base)
        w = water(e, cam=(r.uniform(-500, 500), r.uniform(-500, 500), r.uniform(50, 400)), debug=debug)
        n = e.alloc(0x30)
        node(e, n, 0.0, 0.0, 64, edges=edges)
        return w, n, tri(e, r, env), base, count, recalc
    run(fe, 0x52F630, "WaterMesh_DrawSplitTri", setup, cw=cw)
