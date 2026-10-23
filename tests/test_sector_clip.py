import random
import struct

import pytest

from test_sector_util import check, f32, new_emu, set_view, with_cw

E = None


def emu():
    global E
    if E is None:
        E = new_emu()
    return E


def f(v):
    return struct.unpack("<f", struct.pack("<f", v))[0]


def vertex(x, y, z, near=0.04096):
    if z < near:
        return [x, y, z, 1234.5, -77.0, 3.0, 0x10]
    rhw = f(1.0 / z)
    sx = f(rhw * x * 320.0 + 320.0)
    sy = f(rhw * y * -320.0 + 240.0)
    code = (sy > 480) << 3 | (sx > 640) << 2 | (sy < 0) << 1 | (sx < 0)
    return [x, y, z, sx, sy, rhw, code]


def pack(v):
    return list(struct.unpack("<6I", struct.pack("<6f", *v[:6]))) + [v[6]]


def tris(seed, n):
    r = random.Random(seed)
    out = []
    for _ in range(n):
        out.append([vertex(r.uniform(-900, 900), r.uniform(-700, 700), r.choice([r.uniform(-60, 30),
                                                                                     r.uniform(1, 800)]))
                    for _ in range(3)])
    return out


CASES = tris(1, 60) + [
    [vertex(10, 10, 100), vertex(50, 10, 100), vertex(10, 50, 100)],
    [vertex(-900, 10, 100), vertex(900, 10, 100), vertex(10, 700, 100)],
    [vertex(10, 10, -5), vertex(50, 10, 100), vertex(10, 50, 100)],
    [vertex(10, 10, -5), vertex(50, 10, -100), vertex(10, 50, 100)],
    [vertex(10, 10, -5), vertex(50, 10, -100), vertex(10, 50, -1)],
]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("i", range(len(CASES)))
def test_clip_triangle(i, cw):
    e = emu()

    def setup(e):
        set_view(e)
        rect = e.alloc(24)
        f32(e, rect + 8, 300.0, 200.0, 310.0, 210.0)
        e.write(rect, struct.pack("<II", rect + 8, rect + 16))
        args = []
        for v in CASES[i]:
            args += pack(v)
        return args + [rect, 0x3D27C5AC]
    check(e, with_cw(e, 0x4DAB50, cw), with_cw(e, "Portal_ClipTriangle", cw), setup=setup, check_eax=False)
