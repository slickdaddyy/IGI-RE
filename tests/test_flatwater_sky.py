"""FlatSky_Render (0x51E1F0)."""
import math
import struct

import pytest

from test_flatwater_util import CWS, LOG, add_child, f32, fake_emu, project, pyfake, run, task, u16, u32, view, with_cw
from test_menu_util import strip_logger

LAYER = 0x33


emu = fake_emu({0x52D060: 1})  # SkyDome_Render
pyfake(emu, 0x498000, project)
strip_logger(emu)
emu.baseline = emu.snapshot()


def sky(e, layers, fade=0.5, near=5.0, far=100.0):
    s = task(e, 0xD0)
    f32(e, s + 0x6C, near, far, fade)
    u32(e, s + 0x84, 0x8484)
    for i, (loaded, scale, alpha) in enumerate(layers):
        t = task(e, 0x26C, LAYER)
        u32(e, t + 0x44, loaded)
        f32(e, t + 0x248, scale, 0.01, -0.02, 0.25 + 0.1 * i, 0.75, alpha, 0.9, 0.8, 0.7 - 0.1 * i)
        add_child(e, s, t)
        add_child(e, s, task(e, 0x40, 0x34))
    return s


VIEWS = [(0.3, -0.2, 0.0), (1.0, 0.4, 0.1), (-2.0, 0.05, -0.3), (0.0, 1.2, 0.0), (0.7, -1.2, 0.0),
         (2.5, 0.0, 0.0), (0.0, 0.0, 0.0), (0.4, math.pi / 2, 0.0), (0.0, 0.3, 1.4)]
LAYERS = [[], [(1, 0.0001, 1.0)], [(0, 0.0003, 0.5), (1, 0.00007, 0.8)]]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("yaw,pitch,roll", VIEWS)
@pytest.mark.parametrize("layers", LAYERS)
@pytest.mark.parametrize("fade", [0.5, 0.95])
def test_render(cw, yaw, pitch, roll, layers, fade):
    def setup(e):
        u16(e, 0xBA1FDC, LAYER)
        view(e, yaw, pitch, roll)
        return (sky(e, layers, fade),)
    run(emu, with_cw(emu, 0x51E1F0, cw), with_cw(emu, "FlatSky_Render", cw), setup)


def test_render_reaches_strips():
    drawn = 0
    for yaw, pitch, roll in VIEWS:
        emu.reset()
        u16(emu, 0xBA1FDC, LAYER)
        view(emu, yaw, pitch, roll)
        s = sky(emu, LAYERS[1])
        LOG.append([])
        emu.call(0x51E1F0, s)
        drawn += any(x[0] == "strip" for x in LOG[-1])
    assert drawn >= 4


# horizon test fails, nothing above the horizon on screen, alpha clamped to 0.999
EDGES = [((0.3, -0.2, 0.0), 0.5, 0.01, 0.01), ((-0.18558571330701756, -0.810966935616543, 0.13128257770779128), 0.2,
                                               5.0, 0.05),
         ((2.768867933700501, 0.12551510038659397, 0.5334914317517772), 0.9, 0.5, 0.05),
         ((1.4433509674948546, -0.4997347157884715, -0.7493734257892125), 0.999, 5.0, 0.05)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("v,fade,near,far", EDGES)
def test_render_edges(cw, v, fade, near, far):
    def setup(e):
        u16(e, 0xBA1FDC, LAYER)
        view(e, *v)
        return (sky(e, LAYERS[2], fade, near, far),)
    run(emu, with_cw(emu, 0x51E1F0, cw), with_cw(emu, "FlatSky_Render", cw), setup)


def test_render_edges_reached():
    seen = []
    for v, fade, near, far in EDGES:
        emu.reset()
        u16(emu, 0xBA1FDC, LAYER)
        view(emu, *v)
        s = sky(emu, [(1, 0.0001, 1.0)], fade, near, far)
        LOG.append([])
        emu.call(0x51E1F0, s)
        strips = [x for x in LOG[-1] if x[0] == "strip"]
        alphas = {struct.unpack("<f", x[5][36 * i + 20:36 * i + 24])[0] for x in strips for i in range(26)}
        seen.append((len(LOG[-1]), any(abs(a - 0.999) < 1e-6 for a in alphas)))
    assert seen[0] == (1, False) and seen[1] == (3, False) and seen[2][1] and seen[3][1]
