"""EditHazinglayer (0x52B040 - 0x52B300)."""
import pytest

from test_terredit_util import CWS, d64, f32, make, run, task, u16, u32

TYPE, PARAMS = 0xA84398, 0xA8439C

emu = make()


def test_init():
    run(emu, 0x52B040, "EditHazeLayer_Init")


def test_get_params_shutdown():
    def setup(e):
        u32(e, PARAMS, 0x1234)
        u16(e, TYPE, 0x55)
        return 0, e.alloc(4)
    run(emu, 0x52B2F0, "EditHazeLayer_GetParams", setup)
    run(emu, 0x52B300, "EditHazeLayer_Shutdown", setup)


def test_delete():
    def setup(e):
        t = task(e, 0x88, 0x55)
        u32(e, t + 0x20, 0x7777)
        return (t,)
    run(emu, 0x52B1C0, "EditHazeLayer_Delete", setup)


POSITIONS = [(1234.5, -9876.25, 300.75), (-1e6 - 0.5, 2.5e6 + 0.5, -3333.3), (0.4, -0.6, 1.5), (7e8, -7e8, 1e9)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("shift", [0, 5, 10, 14, 20, 31])
@pytest.mark.parametrize("pos", POSITIONS)
@pytest.mark.parametrize("layer", [0, 0x4444])
def test_changed(layer, pos, shift, cw):
    def setup(e):
        t = task(e, 0x88, 0x55)
        u32(e, t + 0x20, layer)
        d64(e, t + 0x28, *pos)
        e.write(t + 0x40, bytes(range(0x30)))
        u32(e, t + 0x70, shift)
        f32(e, t + 0x74, 0.375)
        d64(e, t + 0x78, -123.25, 4567.125)
        return (t,)
    run(emu, 0x52B1D0, "EditHazeLayer_Changed", setup, cw=cw)

