"""WaterMesh wave table, vertices, fade and light (0x52D9F0 - 0x52E2D0)."""
import random
import struct

import pytest

from emu import Emu, ErrorShown
from test_menu_util import logger
from test_terredit_util import CWS, LOG, WORLD, d64, f32, logfake, pyfake, run, u32, with_cw

NAN = float("nan")
e = Emu()
logfake(e, 0x4978D0, 1, strings=(0,), ret=0x77)  # Primitive_Register
logfake(e, 0x4978F0, 2)  # Primitive_SetHandler
logfake(e, 0x52FFA0, 2)  # WaterMesh_Subdivide
logfake(e, 0x52FE60, 2)  # WaterMesh_CheckVisible
logfake(e, 0x52FBA0, 4)  # WaterMesh_SetEdgeFlags
logfake(e, 0x52E660, 5)  # WaterMesh_DrawNode
pyfake(e, e.symbols["WaterMesh_DrawNode"], logger(e, 0x52E660, 5))


def tri_log(tag, nargs, index):
    """Logs a WaterTri without its uv, which is stale stack unless the env map pass filled it."""
    def fn(em):
        a = [em.arg(i) for i in range(nargs)]
        t = a[index]
        env = em.u32(t + 0x88)
        if LOG:
            LOG[-1].append((tag,) + tuple(a[:index]) + (em.read(t, 0x6C), em.read(t + 0x84, 0x10),
                                                         em.read(t + 0x6C, 0x18) if env else None) + tuple(a[index + 1:]))
        em.ret(0)
    return fn


ne = Emu()
pyfake(ne, 0x52F630, tri_log("split", 6, 2))  # WaterMesh_DrawSplitTri
pyfake(ne, 0x5116D0, tri_log("tri", 1, 0))
ne.baseline = ne.snapshot()
logfake(e, 0x4C1780, 0)  # Debug_Log
logfake(e, 0x4DA180, 2)  # Haze_GetVisible
e.baseline = e.snapshot()


def water(em, rng, cell=None, extreme=False):
    w = em.alloc(0x4B00)
    em.write(w + 0x4840, bytes(rng.randrange(256) for _ in range(0x2C0)))
    d64(em, w + 0x4870, *[rng.uniform(-2e5, 2e5) for _ in range(3)])
    a = rng.uniform(0, 6.283)
    d64(em, w + 0x4888, rng.uniform(-1, 1), rng.uniform(-1, 1))
    d64(em, w + 0x48A8, rng.uniform(-1, 1), rng.uniform(-1, 1))
    d64(em, w + 0x48B8, *[rng.uniform(-50, 50) for _ in range(50)])
    f32(em, w + 0x4A54, *[rng.uniform(0, 255) for _ in range(3)])
    wl = rng.choice([200.0, 1000.0, 4096.0, 3.5])
    f32(em, w + 0x4A80, rng.uniform(0, 80), wl, wl / 10 if cell is None else cell, rng.uniform(-3, 3),
        rng.uniform(-10, 10), rng.uniform(-500, 500), rng.uniform(0, 0.1))
    u32(em, w + 0x4AA8, rng.randrange(0, 1 << 32) if extreme else rng.randrange(0, 100000),
        rng.randrange(2, 5000), rng.randrange(2, 5000) if not extreme else rng.choice([2, 3, 0xFFFFFFFF, 0x9000000]))
    return w


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(12))
def test_update_waves(seed, cw):
    def setup(em):
        return (water(em, random.Random(seed), extreme=seed >= 8),)
    run(e, 0x52D9F0, "WaterMesh_UpdateWaves", setup, cw=cw, check_eax=True)


def world(em, rng):
    p = em.alloc(0xE00)
    u32(em, p + 0xD64, rng.getrandbits(32))
    u32(em, p + 0xD3C, rng.getrandbits(32))
    u32(em, WORLD, p)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(30))
def test_get_vertex(seed, cw):
    def setup(em):
        rng = random.Random(seed + 50)
        cell = None
        if seed == 27:
            cell = -40.0
        if seed == 28:
            cell = 1e-30
        w = water(em, rng, cell)
        world(em, rng)
        if seed == 26:
            f32(em, w + 0x4A98, 0.0)
        out = em.alloc(0x70)
        em.write(out, b"\xab" * 0x70)
        pos = em.alloc(24)
        p = [rng.uniform(-1e5, 1e5), rng.uniform(-1e5, 1e5), rng.uniform(-600, 600)]
        if seed == 29:
            p[0] = NAN
        d64(em, pos, *p)
        return w, out, pos
    run(e, 0x52DAE0, "WaterMesh_GetVertex", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(30))
def test_get_fade(seed, cw):
    def setup(em):
        rng = random.Random(seed + 90)
        w = water(em, rng)
        cam = struct.unpack("<3d", em.read(w + 0x4870, 24))
        size = rng.choice([0, 1, 64, 4096, 65536, -50])
        reach = max(abs(size), 1) * 2
        p = [c + rng.uniform(-reach, reach) for c in cam]
        if seed == 29:
            p[1] = NAN
        pos = em.alloc(24)
        d64(em, pos, *p)
        return w, pos, size & 0xFFFFFFFF
    run(e, 0x52DCB0, "WaterMesh_GetFade", setup, cw=cw, check_st0=True)


def test_open():
    run(e, 0x52DD70, "WaterMesh_Open", None)


def lights(em, rng, n):
    stride = rng.choice([0x18, 0x40])
    data = em.alloc(stride * 8)
    em.write(data, bytes(rng.randrange(256) for _ in range(stride * 8)))
    for i in range(8):
        f32(em, data + stride * i + 12, *[rng.uniform(-50, 255) for _ in range(3)])
    idx = em.alloc(4 * 8)
    u32(em, idx, *[rng.randrange(8) for _ in range(8)])
    u32(em, 0xB041E0, data, idx, n)
    u32(em, 0xB041F0, stride)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(20))
def test_light(seed, cw):
    def setup(em):
        rng = random.Random(seed + 200)
        w = water(em, rng)
        lights(em, rng, rng.choice([0, 1, 3, 8, -1]))
        normal = em.alloc(12)
        n = [rng.uniform(-1, 1) for _ in range(3)]
        if seed in (16, 17):
            n = [0.0, 0.0, 0.0] if seed == 16 else [0.0, -0.0, NAN]
        if seed == 18:
            n[2] = abs(n[2]) * 1e-20
        f32(em, normal, *n)
        color = em.alloc(12)
        return w, color, normal
    run(e, 0x52E180, "WaterMesh_Light", setup, cw=cw)


SIZES = [0, 1, 4096, 1 << 28, (1 << 28) + 1, int(2.5 * (1 << 28)), 3 << 28, int(5.5 * (1 << 28)), 1 << 31,
         0xFFFFFFFF]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("size", SIZES)
@pytest.mark.parametrize("nlights", [0, 2, 8])
def test_init(size, nlights, cw):
    def setup(em):
        rng = random.Random(size % 1000 + nlights)
        w = water(em, rng)
        u32(em, w + 0x4A9C, size)
        d64(em, w + 0x4840, rng.uniform(-1e6, 1e6), rng.uniform(-1e6, 1e6))
        if nlights == 2:
            d64(em, w + 0x4888, 0.0, 0.0)
        lights(em, rng, nlights)
        return (w,)
    run(e, 0x52D6C0, "WaterMesh_Init", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(24))
def test_draw(seed, cw):
    def setup(em):
        rng = random.Random(seed + 300)
        w = water(em, rng)
        size = rng.choice([1 << 28, 2 << 28, 3 << 28, 1 << 31, 4096 * 64])
        u32(em, w + 0x4A9C, size)
        org = (rng.uniform(-1e6, 1e6), rng.uniform(-1e6, 1e6))
        d64(em, w + 0x4840, *org)
        lights(em, rng, rng.choice([0, 3]))
        em.call(0x52D6C0, w)
        span = size * rng.choice([1.0, 1.3])
        cam = [org[0] + rng.uniform(-0.2, 1.0) * span, org[1] + rng.uniform(-0.2, 1.0) * span,
               rng.uniform(-300, 3000)]
        d64(em, 0xBCAB08, *cam)
        f32(em, 0xBCAAF8, rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))
        u32(em, w + 0x4AA8, rng.randrange(0, 1 << 20), rng.randrange(1, 5000))
        node = em.alloc(16)
        u32(em, node + 0xC, w)
        return (node,)
    run(e, 0x52DD90, "WaterMesh_Draw", setup, cw=cw)


def grids(em, rng, w, count):
    u32(em, w + 0x4ABC, count)
    for g in range(count):
        size = rng.choice([1 << 23, 1 << 24, 1 << 26])
        flip = rng.random() < 0.5
        x0, y0 = rng.uniform(-1e6, 1e6), rng.uniform(-1e6, 1e6)
        for a in range(4):
            for b in range(4):
                p = w + 0xC40 + g * 0x300 + (a * 4 + b) * 0x30
                em.write(p, bytes(rng.randrange(256) for _ in range(0x30)))
                ax, by = (3 - a, 3 - b) if flip else (a, b)
                d64(em, p, x0 + ax * size, y0 + by * size)
                u32(em, p + 0x18, rng.getrandbits(32), rng.randrange(0, 20), size, rng.choice([0, 1]),
                    rng.choice([0, 0, 1]))
        if rng.random() < 0.2:
            d64(em, w + 0xC40 + g * 0x300, NAN)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(20))
def test_draw_cells(seed, cw):
    def setup(em):
        rng = random.Random(seed + 400)
        w = water(em, rng)
        u32(em, w + 0x4A9C, rng.choice([1 << 28, 3 << 28]))
        d64(em, w + 0x4840, rng.uniform(-1e6, 1e6), rng.uniform(-1e6, 1e6))
        em.call(0x52D6C0, w)
        for i in range(8):
            for j in range(8):
                u32(em, w + 0x40 + (i * 8 + j) * 0x30 + 0x28, rng.choice([0, 1]))
        grids(em, rng, w, rng.choice([0, 1, 3, 6]))
        return (w,)
    run(e, 0x52E2D0, "WaterMesh_DrawCells", setup, cw=cw)


def node_scene(em, rng, case):
    w = water(em, rng)
    world(em, rng)
    lights(em, rng, rng.choice([0, 2]))
    lvl = rng.randrange(0, 3)
    u32(em, w + 0x4AB8, lvl if case != "nolevel" else 0)
    u32(em, w + 0x4ABC, rng.choice([lvl + 2, lvl + 3, 8]))
    u32(em, w + 0x4AC0, rng.choice([0, 1]), 0x4AC4, rng.choice([0, 0, 1]))
    u32(em, w + 0x4AA0, 0x7E01, 0x7E02)
    u32(em, w + 0x4ACC, 0x4ACC, 0x4AD0)
    S = rng.choice([1 << 12, 1 << 14])
    x0, y0 = float(rng.randrange(-1 << 20, 1 << 20)), float(rng.randrange(-1 << 20, 1 << 20))
    for a in range(4):
        for b in range(4):
            d64(em, w + 0xC40 + lvl * 0x300 + (a * 4 + b) * 0x30, x0 + a * S, y0 + b * S)
    node = em.alloc(0x30)
    nlevel = {"same": lvl + 1, "deeper": lvl + 2, "coarse": lvl, "nolevel": 5, "hidden": lvl + 1}[case]
    size = 3 * S if case != "deeper" else 4 * S
    far = 3 * S if case != "deeper" else 4 * S
    d64(em, node, rng.choice([x0, x0, x0 + S, x0 + far]), rng.choice([y0, y0, y0 - S, y0 + far]))
    f32(em, node + 0x10, rng.choice([0.0, 0.3, 1.0, rng.uniform(0, 1), NAN]))
    if rng.random() < 0.3:
        f32(em, w + 0x4A98, 0.0)
    u32(em, node + 0x14, 0, rng.choice([0, 0x14, 0xA, 0x1FE, rng.getrandbits(9)]), nlevel, size,
        0 if case == "hidden" else 1, 0, 0)
    return w, node, lvl


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", ["same", "deeper", "coarse", "nolevel", "hidden"])
@pytest.mark.parametrize("seed", range(12))
def test_draw_node(seed, case, cw):
    def setup(em):
        w, node, lvl = node_scene(em, random.Random(seed * 7 + len(case)), case)
        return w, node, lvl, 1, 2
    orig, ours = with_cw(ne, 0x52E660, cw), with_cw(ne, "WaterMesh_DrawNode", cw)
    ne.reset()
    try:
        ne.call(orig, *setup(ne))
    except ErrorShown as err:
        # a flat normal makes the env map's atan2 fail in the original too
        ne.reset()
        with pytest.raises(ErrorShown) as mine:
            ne.call(ours, *setup(ne))
        assert str(mine.value) == str(err)
        return
    run(ne, 0x52E660, "WaterMesh_DrawNode", setup, cw=cw)
