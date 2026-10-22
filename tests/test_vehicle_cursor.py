import struct

import pytest

from emu import Emu, compare
from test_vehicle_util import f32, task_system, u32

emu = Emu()


def state(e, full, mx=100, my=50, dx=0.0, dy=0.0, buttons=3, w=640, h=480):
    u32(e, 0x5C8C00, full)
    u32(e, 0x57BC58, mx, my)
    f32(e, 0xBC20F4, dx, dy)
    u32(e, 0xC28F8C, buttons)
    u32(e, 0xC28B44, w, h)


@pytest.mark.parametrize("msg", [0x200, 0x201])
@pytest.mark.parametrize("full", [0, 1])
@pytest.mark.parametrize("lparam", [0x00320064, 0xFFF0FFFE, 0x80007FFF])
def test_on_message(msg, full, lparam):
    def setup(e):
        state(e, full)
        return 0x1234, msg, 7, lparam
    compare(emu, 0x424CA0, "Cursor_WinProc", setup=setup)


@pytest.mark.parametrize("c", [
    dict(full=0), dict(full=0, mx=-5, my=-1), dict(full=0, mx=700, my=480), dict(full=0, mx=639, my=479),
    dict(full=1, dx=0.01, dy=-0.02), dict(full=1, dx=-1.0, dy=-1.0), dict(full=1, dx=2.5, dy=3.0),
    dict(full=1, dx=0.0015625, dy=0.0020833), dict(full=1, dx=-0.0046875, dy=0.0145833, buttons=2),
])
def test_run(c):
    def setup(e):
        state(e, **c)
        cur = e.alloc(0x40)
        u32(e, cur + 0x24, 320, 240)
        u32(e, cur + 0x3C, 1)
        return (cur,)
    compare(emu, 0x424CE0, "Cursor_RunHandler", setup=setup, check_eax=False)


def test_close():
    def setup(e):
        task_system(e)
        t = e.call(0x401900, 0x53BA00, 0x40, e.call(0x4018F0)[0] & 0xFFFF, 0)[0]
        e.write(0x57BC60, struct.pack("<H", t))
    compare(emu, 0x424E10, "Cursor_Close", setup=setup, check_eax=False)


def test_new():
    def setup(e):
        task_system(e)
        t = e.call(0x401900, 0x53BA00, 0x40, e.call(0x4018F0)[0] & 0xFFFF, 0)[0]
        e.write(0x57BC60, struct.pack("<H", t))
        return (0x777,)
    compare(emu, 0x424E20, "Cursor_New", setup=setup)


def test_set_sprite():
    compare(emu, 0x424E40, "Cursor_SetSprite", setup=lambda e: (e.alloc(0x40), 0x5555), check_eax=False)


def test_get_state():
    def setup(e):
        cur = e.alloc(0x40)
        u32(e, cur + 0x24, 11, 22)
        u32(e, cur + 0x38, 33, 44)
        out = e.alloc(16)
        return cur, out, out + 4, out + 8, out + 12
    compare(emu, 0x424E50, "Cursor_GetState", setup=setup, check_eax=False)
