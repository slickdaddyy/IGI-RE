"""WaterMesh wave table, vertices, fade and light (0x52D9F0 - 0x52E2D0)."""
import random
import struct

import pytest

from emu import Emu
from test_terredit_util import CWS, WORLD, d64, f32, logfake, run, u32

NAN = float("nan")
e = Emu()
logfake(e, 0x4978D0, 1, strings=(0,), ret=0x77)  # Primitive_Register
logfake(e, 0x4978F0, 2)  # Primitive_SetHandler
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
