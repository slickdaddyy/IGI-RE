"""CubeModifier (0x52B320 - 0x52B6A0)."""
import pytest

from test_terredit_util import CWS, d64, make, run, task, u16, u32, world

TYPE, PARAMS, LIST = 0xA843A8, 0xA843A4, 0xA843AC

emu = make()


def cubemod(e, w, pos=(1234.5, -9876.25, 300.75), shift=10):
    t = task(e, 0x178, 0x56)
    u32(e, t + 0x20, w)
    d64(e, t + 0x28, *pos)
    e.write(t + 0x40, bytes(range(1, 0x31)))
    u32(e, t + 0x70, shift)
    e.write(t + 0x74, b"name" * 64)
    return t


@pytest.mark.parametrize("used", [0, 1, 5, 99])
def test_create(used):
    def setup(e):
        world(e)
        t = task(e, 0x178, 0x56)
        e.write(t + 0x20, bytes(i & 0xFF for i in range(0x158)))
        u32(e, LIST, *[0x1000 + i for i in range(used)])
        u32(e, LIST + 4 * used, 0)
        u32(e, LIST + 4 * used + 4, 0x2222)
        return (t,)
    run(emu, 0x52B320, "CubeModifier_Create", setup)


def test_init():
    def setup(e):
        u32(e, LIST, *range(1, 101))
    run(emu, 0x52B3B0, "CubeModifier_Init", setup)


@pytest.mark.parametrize("where", [0, 1, 7, 99])
def test_delete(where):
    def setup(e):
        t = task(e, 0x178, 0x56)
        u32(e, LIST, *[0x1000 + i for i in range(100)])
        u32(e, LIST + 4 * where, t)
        return (t,)
    run(emu, 0x52B4D0, "CubeModifier_Delete", setup)


def test_get_params_shutdown():
    def setup(e):
        u32(e, PARAMS, 0x1234)
        u16(e, TYPE, 0x56)
        return 0, e.alloc(4)
    run(emu, 0x52B620, "CubeModifier_GetParams", setup)
    run(emu, 0x52B6A0, "CubeModifier_Shutdown", setup)


POSITIONS = [(1234.5, -9876.25, 300.75), (-1e6 - 0.5, 2.5e6 + 0.5, -3333.3), (0.4, -0.6, 1.5), (7e8, -7e8, 1e9)]
OFFSETS = [(0x12345, -0x5000, 0x7000), (0, 0, 0), (-0x40000000, 0x3FFFFFFF, 0x10000001)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("shift", [0, 3, 10, 17, 31])
@pytest.mark.parametrize("pos", POSITIONS)
@pytest.mark.parametrize("off", OFFSETS)
def test_changed(off, pos, shift, cw):
    def setup(e):
        w = world(e, off)
        u32(e, 0xA44344, 0)
        return (cubemod(e, w, pos, shift),)
    run(emu, 0x52B510, "CubeModifier_Changed", setup, cw=cw)


@pytest.mark.parametrize("shift", [0, 1, 10, 31])
def test_get_info(shift):
    def setup(e):
        t = cubemod(e, 0x9999, shift=shift)
        info = e.alloc(0x58)
        e.write(info, b"\xEE" * 0x58)
        return t, info
    run(emu, 0x52B630, "CubeModifier_GetInfo", setup)
