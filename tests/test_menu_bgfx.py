import math
import struct

import pytest

from emu import Emu
from test_menu_util import RET, check, f32, logfake, strip_logger, task, u16, u32

emu = Emu()
strip_logger(emu)
logfake(emu, 0x4012A0, 3, masks={1: 0xFFFF}, ret=0x7777)  # Task_New
emu.baseline = emu.snapshot()


def run(orig, ours, setup, **kw):
    RET.clear()
    return check(emu, orig, ours, setup=setup, normalized=True, **kw)


def transform(e, angle=0.3, scale=1.25, tx=200.0, ty=300.0):
    t = e.alloc(0x1C)
    c, s = math.cos(angle) * scale, math.sin(angle) * scale
    f32(e, t, c, s, -s, c, tx, ty, scale)
    return t


ANGLES = [0.0, 0.7, -2.5, 100.25, 3.14159]


@pytest.mark.parametrize("angle", ANGLES)
@pytest.mark.parametrize("scale", [1.0, 1.4, -0.3])
def test_set_transform(angle, scale):
    def setup(e):
        p = e.alloc(0x1C)
        return (p, struct.unpack("<I", struct.pack("<f", angle))[0], struct.unpack("<I", struct.pack("<f", scale))[0],
                0x43480000, 0x43960000)
    run(0x4192E0, "BackgroundFX_SetTransform", setup, check_eax=False)


@pytest.mark.parametrize("cols,rows", [(20, 20), (0, 3), (-1, 2), (5, -1)])
@pytest.mark.parametrize("angle", [0.0, 0.45, -1.3])
def test_draw_grid(cols, rows, angle):
    def setup(e):
        g = e.alloc(0x18)
        u32(e, g, cols, rows)
        f32(e, g + 8, 10.5, -20.25, 300.0, 213.7)
        return g, transform(e, angle, 1.1)
    run(0x419330, "BackgroundFX_DrawGrid", setup, check_eax=False)


@pytest.mark.parametrize("n", [0, 1, 5])
def test_draw_lines(n):
    def setup(e):
        l = e.alloc(4 + 16 * n)
        u32(e, l, n)
        for i in range(n):
            f32(e, l + 4 + 16 * i, i * 33.3, -i * 7.1, 100.0 / (i + 1), 0.123 * i)
        return l, transform(e, 0.9, 0.77)
    run(0x419540, "BackgroundFX_DrawLines", setup, check_eax=False)


@pytest.mark.parametrize("r", [1.0, 5.0, 15.0, 50.0, 127.0, 128.0, 300.0])
@pytest.mark.parametrize("angle", [0.0, 1.2])
def test_draw_circle(r, angle):
    def setup(e):
        c = e.alloc(12)
        f32(e, c, 20.0, -35.5, r)
        return c, transform(e, angle, 1.3)
    run(0x419630, "BackgroundFX_DrawCircle", setup, check_eax=False)


@pytest.mark.parametrize("shape", [0x539BE8, 0x539BF8, 0x539C08])
def test_draw_shape(shape):
    run(0x419950, "BackgroundFX_DrawShape", lambda e: (shape, transform(e, 0.2, 1.4)), check_eax=False)


@pytest.mark.parametrize("time", [0.0, 1.0, 17.0, 1234.0, 98765.0])
def test_draw_layers(time):
    run(0x419860, "BackgroundFX_DrawLayers", lambda e: (0, struct.unpack("<I", struct.pack("<f", time))[0]),
        check_eax=False)


@pytest.mark.parametrize("time", [0.0, 250.0, 4096.5])
def test_draw(time):
    def setup(e):
        fx = task(e, 0x24)
        f32(e, fx + 0x20, time)
        u32(e, 0xA84A50, 0x1111, 0, 0x2222)
        return (fx,)
    run(0x419820, "BackgroundFX_Draw", setup, check_eax=False)


@pytest.mark.parametrize("time", [0.0, 2.5, 16777216.0, 1e10])
def test_run(time):
    def setup(e):
        fx = task(e, 0x24)
        f32(e, fx + 0x20, time)
        return (fx,)
    run(0x419800, "BackgroundFX_Run", setup, check_eax=False)


def test_create():
    def setup(e):
        fx = task(e, 0x24)
        f32(e, fx + 0x20, 5.0)
        return (fx,)
    run(0x4197F0, "BackgroundFX_Create", setup, check_eax=False)


def test_new():
    run(0x4199D0, "BackgroundFX_New", lambda e: (u16(e, 0x539AF0, 0x31), (0x4444,))[1])
