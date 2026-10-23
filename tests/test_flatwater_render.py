"""FlatWater_Render (0x51C5B0)."""
import pytest

from test_flatwater_util import CWS, add_child, d64, f32, fake_emu, project, pyfake, run, task, u16, u32, view, with_cw
from test_menu_util import strip_logger

LAYER = 0x35

emu = fake_emu({})
pyfake(emu, 0x498000, project)
strip_logger(emu)
emu.baseline = emu.snapshot()


def water(e, layers, height=-1000.0, reach=30000.0, cam=(1234.5, -987.25, 0.0), fog=0.004):
    d64(e, 0xBCAB08, *cam)
    f32(e, 0xA7DA18, fog)
    w = task(e, 0x90)
    d64(e, w + 0x70, height)
    f32(e, w + 0x78, reach)
    f32(e, w + 0x80, 0.1, 0.3, 0.5)
    for i, (loaded, scale) in enumerate(layers):
        t = task(e, 0x26C, LAYER)
        u32(e, t + 0x44, loaded)
        f32(e, t + 0x248, scale, 0.01, -0.02, 0.25 + 0.1 * i, 0.75, 0.6, 0.9, 0.8, 0.7 - 0.1 * i)
        add_child(e, w, task(e, 0x40, 0x34))
        add_child(e, w, t)
    return w


VIEWS = [(0.3, -0.2, 0.0), (1.0, -0.4, 0.1), (-2.0, -0.05, -0.3), (0.0, -1.2, 0.0), (0.7, 0.3, 0.0),
         (2.5, 0.0, 0.0), (0.0, -0.6, 0.0), (0.4, -1.5707963267948966, 0.0), (0.0, -0.3, 1.4), (0.2, -0.1, -0.9),
         (3.0, -0.25, 0.6)]
LAYERS = [[(1, 0.0001)], [(0, 0.0003), (1, 0.00007)]]


def setup_for(v, layers, **kw):
    def setup(e):
        u16(e, 0xBA2018, LAYER)
        view(e, *v)
        return (water(e, layers, **kw),)
    return setup


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("v", VIEWS)
@pytest.mark.parametrize("layers", LAYERS)
@pytest.mark.parametrize("height", [-1000.0, 5000.0])
def test_render(cw, v, layers, height):
    run(emu, with_cw(emu, 0x51C5B0, cw), with_cw(emu, "FlatWater_Render", cw), setup_for(v, layers, height=height))


@pytest.mark.parametrize("height,reach", [(-100.0, 30000.0), (0.0, 30000.0), (-1000.0, 0.5), (-1000.0, 3.0)])
def test_render_other(height, reach):
    for v in VIEWS[:3]:
        run(emu, 0x51C5B0, "FlatWater_Render", setup_for(v, LAYERS[1], height=height, reach=reach))


def test_render_no_layers():
    run(emu, 0x51C5B0, "FlatWater_Render", setup_for(VIEWS[0], []))
