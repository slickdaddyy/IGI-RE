"""ComputerStartup: the boot text of the map computer (0x466240 - 0x466CF0)."""
import struct

import pytest

from emu import compare
from test_computer_util import (LOG, RET, com, cstr, f32, handler, logfake, make_emu, run, task, u16, u32, vfake,
                                with_cw)

TYPE = 0x48
D3D = 0x5CA110
STEP_FN = 0x6100

emu = make_emu()
logfake(emu, 0x4012A0, 3, masks={1: 0xFFFF}, effect=lambda e, a: RET.get("new", 0))  # Task_New
logfake(emu, 0x4B5F00, 2, strings=(0,), effect=lambda e, a: 0x5000 + len(e.read_cstr(a[0])))  # Resource_Load
logfake(emu, 0x4B0DB0, 0, effect=lambda e, a: RET.get("mode", 0))  # Mem_GetMode
logfake(emu, 0x4B0D40, 0)  # Mem_SetDefaultMode
logfake(emu, 0x4B0D50, 0)  # Mem_ClearMode
logfake(emu, 0x4B7000, 1, strings=(0,), ret=0xF047)  # Sprite_New
logfake(emu, 0x4B7030, 1)  # Sprite_Delete
logfake(emu, 0x4B72E0, 3, effect=lambda e, a: RET.get("item", 0))  # SortRender_Add
logfake(emu, 0x4B70F0, 2, strings=(1,), ret=17)  # Sprite_GetHeight
logfake(emu, 0x4B6EC0, 4, strings=(3,), effect=lambda e, a: RET.get("text", 0))  # Text_New
vfake(emu, 0x4B6F50, 1, show=lambda e, a: (e.read(a[0], 0x28),))  # Text_DrawAlt
logfake(emu, 0x4B6F30, 1)  # Text_Free
STEP = handler(emu, "step", 2)
DRAW = handler(emu, "draw", 1)
VB = com(emu, "vb", {})
D3D_OBJ = com(emu, "d3d", {5: 4}, derefs={5: {1: 16}})
BUF = emu.alloc(0x1000)


def lock(e, a):
    n = RET.get("locks", 0)
    RET["locks"] = n + 1
    u32(e, a[2], BUF + 0x400 * n)
    u32(e, a[3], 0x3E8)


def transform(e, a):
    f32(e, a[2], *[0.5 + 0.25 * i for i in range(16)])


VB2 = com(emu, "vb2", {3: 4}, effects={3: lock})
DEV = com(emu, "dev", {12: 3, 20: 3, 31: 6, 35: 3, 37: 4}, effects={12: transform, 31: lambda e, a: 0x31 + a[4]})
emu.baseline = emu.snapshot()


def startup(e, flags=1, step=0, count=3, timer=0.0, frames=(5.0, 2.0, 7.5, 1.0, 3.0), fns=(1, 0, 1, 1, 1)):
    t = task(e, 0x90, TYPE)
    u32(e, t + 0x14, 0x1414)
    u32(e, t + 0x20, step, count)
    f32(e, t + 0x28, 0.1, 0.2, 0.3, 0.9, 0.5, 0.25, timer, *frames)
    f32(e, t + 0x58, 300.0)
    u32(e, t + 0x5C, *[cstr(e, b"line %d text" % i) for i in range(5)])
    u32(e, t + 0x70, flags, *[STEP if f else 0 for f in fns])
    u32(e, t + 0x88, VB, 0xF047)
    return t


@pytest.mark.parametrize("count,mode", [(0, 0), (1, 1), (3, 0), (5, 1)])
def test_new(count, mode):
    def setup(e):
        RET["new"] = task(e, 0x90)
        e.write(RET["new"] + 0x20, b"\xAB" * 0x70)
        RET["mode"] = mode
        u32(e, D3D, D3D_OBJ)
        u16(e, 0x53F520, TYPE)
        frames, names, c1, c2 = e.alloc(20), e.alloc(20), e.alloc(12), e.alloc(12)
        f32(e, frames, 1.0, 2.0, 3.5, 4.0, 5.0)
        u32(e, names, *[cstr(e, b"name%d" % (i * 11)) for i in range(5)])
        f32(e, c1, 0.1, 0.2, 0.3)
        f32(e, c2, 0.7, 0.8, 0.9)
        return 0x7777, count, frames, names, c1, c2, 0x43960000
    run(emu, 0x466240, "ComputerStartup_New", setup, check_eax=True)


def test_delete():
    run(emu, 0x466360, "ComputerStartup_Delete", lambda e: (startup(e),))


def test_set_callback():
    compare(emu, 0x466390, "ComputerStartup_SetCallback", setup=lambda e: (startup(e), 2, 0x1234), check_eax=False)


@pytest.mark.parametrize("flags", [0, 1, 2, 3, 0xF0])
def test_start(flags):
    compare(emu, 0x4663B0, "ComputerStartup_Start", setup=lambda e: (startup(e, flags=flags, step=2, timer=3.0),),
            check_eax=False)


@pytest.mark.parametrize("flags,fn", [(0, 1), (1, 1), (1, 0), (3, 1)])
def test_sort_draw(flags, fn):
    def setup(e):
        u32(e, 0x5BDC54, 9)
        u32(e, 0xA94E84 + 9 * 4, DRAW if fn else 0)
        d = e.alloc(16)
        u32(e, d, 0, 0, 0, startup(e, flags=flags))
        return (d,)
    run(emu, 0x466C60, "ComputerStartup_SortDraw", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("flags,step,count,timer,fns", [
    (0, 0, 3, 9.0, (1, 1, 1, 1, 1)),
    (1, 0, 3, 3.0, (1, 1, 1, 1, 1)),
    (1, 0, 3, 4.0, (1, 1, 1, 1, 1)),
    (1, 0, 3, 5.0, (0, 1, 1, 1, 1)),
    (1, 1, 3, 1.5, (1, 1, 1, 1, 1)),
    (1, 2, 3, 7.0, (1, 1, 1, 1, 1)),
    (1, 2, 3, 6.5, (1, 1, 0, 1, 1)),
    (5, 4, 5, 2.0, (1, 1, 1, 1, 1)),
    (1, 0, 3, 16777215.0, (1, 1, 1, 1, 1)),
    (1, 0, 3, 4.00000047, (1, 1, 1, 1, 1)),
])
def test_run(flags, step, count, timer, fns, cw):
    def setup(e):
        return (startup(e, flags=flags, step=step, count=count, timer=timer, fns=fns,
                        frames=(5.0, 2.0, 7.5, 3.0, 16777216.0) if timer < 1e6 else (16777216.0,) * 5),)
    run(emu, with_cw(emu, 0x466C90, cw), with_cw(emu, "ComputerStartup_Run", cw), setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("flags,mode,w,h", [(0, 0, 640, 480), (1, 0, 640, 480), (1, 1, 800.4, 600.6),
                                            (3, 0, 1023.5, 767.5), (1, 0, 333.0, 257.0)])
def test_draw(flags, mode, w, h, cw):
    def setup(e):
        RET["mode"] = mode
        RET["item"] = e.alloc(16)
        RET["text"] = e.alloc(0x30)
        e.write(RET["text"], bytes(range(0x30)))
        u32(e, 0x5BDC50, 0x50)
        f32(e, 0xBCABC0, w, h)
        return (startup(e, flags=flags, step=1),)
    run(emu, with_cw(emu, 0x466CF0, cw), with_cw(emu, "ComputerStartup_Draw", cw), setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("step,timer,colors", [
    (0, 0.0, (0.1, 0.2, 0.3, 0.9, 0.5, 0.25)),
    (1, 1.0, (1.0, 0.0, 0.5, 0.0, 1.0, 0.3)),
    (2, 3.3, (1.5, -0.2, 0.7, 2.0, 0.01, 1.0)),
])
@pytest.mark.parametrize("w,h,x,y", [(640, 480, 0, 0), (800.4, 600.6, 10.5, 20.5), (1023.5, 767.5, -3, 7)])
def test_render(step, timer, colors, w, h, x, y, cw):
    def setup(e):
        RET["locks"] = 0
        t = startup(e, step=step, timer=timer)
        f32(e, t + 0x28, *colors)
        u32(e, t + 0x88, VB2)
        u32(e, 0x5CA114, DEV)
        f32(e, 0xBCABB8, x, y, w, h)
        f32(e, 0xBCABEC, 0.001)
        f32(e, 0xBCADA0, 0.25)
        f32(e, 0xBCADB4, 0.125)
        f32(e, 0xBCAD8C, 0.875)
        for g in (0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4):
            u32(e, g, e.alloc(8))
        return (t,)
    run(emu, with_cw(emu, 0x466480, cw), with_cw(emu, "ComputerStartup_Render", cw), setup, check_eax=True)
