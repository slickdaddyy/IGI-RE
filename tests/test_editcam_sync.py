"""Controller (0x507E70 - 0x507EC0) and Sync (0x508D70 - 0x508F80)."""
import struct

import pytest

from test_editcam_util import RET, handler, logfake, make, run, task, u16, u32

SYNC = 0x64
TYPE, EV0, EV1, CUR = 0xA7B298, 0x54E830, 0x54E831, 0xA7B2A0

emu = make()
logfake(emu, 0x491DC0, 0, ret=0xB117)  # Display_GetBlitPrim
logfake(emu, 0x491D70, 1, derefs={0: 0x28})  # Display_DrawPrimitive
logfake(emu, 0x5183D0, 0)
logfake(emu, 0x508FA0, 0)
CTRL_HANDLER = handler(emu, "ctrl", 2, effect=lambda e, a: e.write(a[0] + 0x1C, struct.pack("<I", RET["bits"])))
emu.baseline = emu.snapshot()


def test_controller_init_shutdown():
    def setup(e):
        e.write(0xA7B254, b"\x07")
    run(emu, 0x507E70, "Controller_Init", setup)
    run(emu, 0x507E80, "Controller_Shutdown", setup)


def test_controller_setup():
    def setup(e):
        c = e.alloc(0x70)
        e.write(c, bytes(range(1, 0x71)))
        return c, 0x4ED000
    run(emu, 0x507E90, "Controller_Setup", setup)


@pytest.mark.parametrize("has_handler", [0, 1])
@pytest.mark.parametrize("old,new", [(0, 0), (0, 0xFFFFFFFF), (0x80000001, 0x80000003), (0x1234, 0xF0F0)])
def test_controller_update(has_handler, old, new):
    def setup(e):
        RET["bits"] = new
        c = e.alloc(0x60)
        u32(e, c, CTRL_HANDLER if has_handler else 0, 1, 2, 3, 4, 0x55, 0x66, old)
        u16(e, c + 0x20, *[(i * 37) & 0xFFFF for i in range(31)], 0xFFFF)
        return c, 0x1234
    run(emu, 0x507EC0, "Controller_Update", setup)


def test_sync_init_shutdown():
    def setup(e):
        u16(e, TYPE, SYNC)
        e.write(EV0, b"\x21\x22")
    run(emu, 0x508D70, "Sync_Init", setup)
    run(emu, 0x508F30, "Sync_Shutdown", setup)
    run(emu, 0x508F60, "Sync_GetEvent0", setup, check_eax=True, eax_mask=0xFF)
    run(emu, 0x508F70, "Sync_GetEvent1", setup, check_eax=True, eax_mask=0xFF)
    run(emu, 0x508F80, "sub_508F80", setup)


def test_sync_create_delete():
    def setup(e):
        t = task(e, 0x994, SYNC)
        e.write(t + 0x20, bytes(i & 0xFF for i in range(0x974)))
        u32(e, CUR, 0x5555)
        return (t,)
    run(emu, 0x508E00, "Sync_Create", setup)
    run(emu, 0x508E30, "Sync_Delete", setup)


@pytest.mark.parametrize("flip", [0, 1, 2])
@pytest.mark.parametrize("count", [0, 1, 3, 50])
def test_sync_draw(flip, count):
    def setup(e):
        t = task(e, 0x994, SYNC)
        img = e.alloc(12)
        u32(e, img, 320, 240, 0x30000000)
        u32(e, t + 0x20, flip)
        for lst in (0x24, 0x348, 0x66C):
            u32(e, t + lst, count)
            for i in range(50):
                u32(e, t + lst + 4 + 16 * i, i * 3 + lst, i * 5, 10 + i, 20 + lst % 7)
        u32(e, t + 0x990, img)
        return t, 0
    run(emu, 0x508E40, "Sync_Draw", setup)
