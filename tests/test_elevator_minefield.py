import struct

import pytest

from emu import compare
from test_elevator_util import (LOG, RET, f32, init_emu, make_emu, run, set_handler, setup_types, strz, task,
                                u32)
from test_menu_util import trap

MF, TARGET, BARE = 0x62, 0x63, 0x64

emu = make_emu((MF,))
init = init_emu()


def get_info(e):
    t, info = e.arg(0), e.arg(1)
    LOG[-1].append(("info", e.normalize(t), e.read(info + 0x50, 4)))
    e.write(info, struct.pack("<3d", 100.5, -7.25, 3.0) + bytes(range(0x38)))
    e.ret(0)


set_handler(emu, 7, TARGET, trap(emu, "get_info", get_info))
emu.baseline = emu.snapshot()


def field(e, timer=0, drop=1, target=5):
    t = task(e, 0xD0, MF)
    f32(e, t + 0x88, 3.0, 1.5, 3.25, 0.4)
    u32(e, t + 0x98, 3, 4)
    strz(e, t + 0xA0, b"explo_03_l")
    u32(e, t + 0xC0, target)
    f32(e, t + 0xC4, 5.1)
    e.write(t + 0xC8, bytes([drop]))
    u32(e, t + 0xCC, timer)
    return t


def test_init():
    compare(init, 0x43A840, "MineField_Init", setup=setup_types, check_eax=False, normalized=True)


def test_create():
    run(emu, 0x43AB50, "MineField_Create", lambda e: (field(e, timer=7, drop=0),))


@pytest.mark.parametrize("timer", [-1, 0, 1, 20])
@pytest.mark.parametrize("has,value", [(0, 0.0), (1, 0.0), (1, -0.0), (1, 1.0), (1, -2.5), (1, float("nan"))])
def test_run(timer, has, value):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        RET[0x4F1030] = task(e, 0x60, TARGET)
        return (field(e, timer=timer),)
    run(emu, 0x43ABC0, "MineField_Run", setup)


@pytest.mark.parametrize("found", ["none", "bare", "target"])
@pytest.mark.parametrize("drop", [0, 1])
@pytest.mark.parametrize("t94", [0.0, 0.4, 1.0166, 2.5])
def test_explode(found, drop, t94):
    def setup(e):
        RET[0x4F1030] = {"none": 0, "bare": task(e, 0x60, BARE), "target": task(e, 0x60, TARGET)}[found]
        t = field(e, drop=drop)
        f32(e, t + 0x94, t94)
        return (t,)
    run(emu, 0x43AC40, "MineField_Explode", setup)


@pytest.mark.parametrize("orig,ours", [(0x43AD20, "MineField_Delete"), (0x43AD40, "MineField_Changed")])
def test_varstring(orig, ours):
    run(emu, orig, ours, lambda e: (field(e),))


def test_get_params():
    def setup(e):
        u32(e, 0x57BEF4, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x43AD30, "MineField_GetParams", setup)


def test_close():
    def setup(e):
        u32(e, 0x57BEF4, 0x9C9C)
        u32(e, 0x53C2D0, 0x0033)
    run(emu, 0x43AD60, "MineField_Close", setup)
