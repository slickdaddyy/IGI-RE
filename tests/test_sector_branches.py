"""Portal_ClipTriangle (0x4DAB50) with view areas whose minimum is above the maximum."""
import random
import struct

import pytest

from test_sector_clip import emu, f, pack
from test_sector_util import check, f32, set_view, with_cw


def vertex(x, y, z, area, near=0.04096):
    x0, y0, x1, y1 = area
    if z < near:
        return [x, y, z, 1234.5, -77.0, 3.0, 0x10]
    rhw = f(1.0 / z)
    sx = f(rhw * x * 320.0 + 320.0)
    sy = f(rhw * y * -320.0 + 240.0)
    code = (sy > y1) << 3 | (sx > x1) << 2 | (sy < y0) << 1 | (sx < x0)
    return [x, y, z, sx, sy, rhw, code]


def inverted(seed):
    r = random.Random(seed)
    x0, x1 = r.uniform(300, 600), r.uniform(0, 280)
    y0, y1 = r.uniform(250, 450), r.uniform(0, 230)
    if seed % 3 == 1:
        x0, x1 = 0.0, 640.0
    elif seed % 3 == 2:
        y0, y1 = 0.0, 480.0
    area = tuple(f(v) for v in (x0, y0, x1, y1))
    verts = [vertex(r.uniform(-900, 900), r.uniform(-700, 700), r.choice([r.uniform(-60, 30), r.uniform(1, 800)]),
                    area) for _ in range(3)]
    return area, verts


def clip_setup(area, verts):
    def setup(e):
        set_view(e, area=area)
        rect = e.alloc(24)
        f32(e, rect + 8, 300.0, 200.0, 310.0, 210.0)
        e.write(rect, struct.pack("<II", rect + 8, rect + 16))
        args = []
        for v in verts:
            args += pack(v)
        return args + [rect, 0x3D27C5AC]
    return setup


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", [0, 3, 10, 19])
def test_clip_triangle_inverted_area(seed, cw):
    """a corner moved onto the left or top edge is still out on the other side"""
    e = emu()
    check(e, with_cw(e, 0x4DAB50, cw), with_cw(e, "Portal_ClipTriangle", cw), setup=clip_setup(*inverted(seed)),
          check_eax=False)
