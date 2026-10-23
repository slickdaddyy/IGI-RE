import random
import struct

import pytest

from test_d3d_util import D3DEmu, logged, pyfake, trap, u32
from test_dxinit_util import check_cw, f32

e = D3DEmu()

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]
SCENE_MIN = 0xBCABD0
KEYS = 0xC28C62
DEPTH = 0xA7DA08


def node_bytes(e, p):
    return e.read(p, 0x28 + 0x1C * min(e.u32(p + 0x1C), 64))


pyfake(e, 0x497860, "Primitive_Draw", 1, before=lambda e: (None, node_bytes(e, e.arg(0))))


def emitted(e):
    """The callback logs the three vertices it gets and the strip."""
    return (None, ([e.read(e.u32(e.arg(0) + 4 * i), 0x28) for i in range(3)], e.read(e.arg(1), 0x14)))


def outcode(x, y, box):
    x0, y0, x1, y1 = box
    return (int(not y <= y1 + 1) << 3 | int(not x <= x1 + 1) << 2 | int(not y >= y0) << 1 | int(not x >= x0))


def f(v):
    return struct.unpack("<f", struct.pack("<f", v))[0]


def clip_args(em, rng, spread=1.0, code_override=None):
    x0, y0 = f(rng.uniform(-5, 60)), f(rng.uniform(-5, 60))
    x1, y1 = f(x0 + rng.uniform(100, 600)), f(y0 + rng.uniform(100, 400))
    em.write(SCENE_MIN, f32(x0, y0, x1, y1))
    em.write(0xBCADA0, f32(rng.uniform(0.5, 4)))
    em.write(0xBCADC8, f32(rng.uniform(500, 9000)))
    em.write(0xBCAD8C, f32(0.95))
    em.write(0x6E5BB0, struct.pack("<I", 1))
    args = []
    for i in range(3):
        w, h = x1 - x0, y1 - y0
        x = f(rng.uniform(x0 - w * spread, x1 + w * spread))
        y = f(rng.uniform(y0 - h * spread, y1 + h * spread))
        attrs = [rng.uniform(0.0005, 2)] + [rng.uniform(0, 1.5) for _ in range(6)]
        code = outcode(x, y, (x0, y0, x1, y1)) if code_override is None else code_override[i]
        args += list(struct.unpack("<9I", f32(x, y, *attrs))) + [code]
    s = em.alloc(0x14)
    em.write(s, struct.pack("<iIiiI", 0, 0x20000, -1, 3, em.alloc(0x6C)))
    cb = trap(em, "emit", logged("emit", 2, pop=0, before=emitted))
    return args + [struct.unpack("<I", f32(1.5))[0], cb, s]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(40))
def test_clip_triangle(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        u32(em, DEPTH, 5)
        return clip_args(em, rng, spread=[0.2, 0.6, 1.0, 2.0][seed % 4])
    check_cw(e, 0x50BF20, "QVertex_ClipTriangle", cw, setup=setup, check_eax=False)


@pytest.mark.parametrize("codes", [(1, 1, 1), (0x10, 0, 0), (0, 0, 0), (3, 2, 6)])
def test_clip_trivial(codes):
    def setup(em):
        return clip_args(em, random.Random(7), code_override=codes)
    check_cw(e, 0x50BF20, "QVertex_ClipTriangle", 0x7F, setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
def test_clip_debug_outline(seed, cw):
    def setup(em):
        rng = random.Random(seed + 50)
        for k in (0x1D, 0x2A, 0x07):
            em.write(KEYS + k, b"\x80")
        u32(em, 0xA84A50, 0x1234)
        return clip_args(em, rng, spread=0.5)
    check_cw(e, 0x50BF20, "QVertex_ClipTriangle", cw, setup=setup, check_eax=False)
