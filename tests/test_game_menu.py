import struct

import pytest

from emu import Emu, compare
from test_game_stubs import PyFake, fakes, u32

emu = Emu()

HANDLERS = 0xA96AE0
TYPE = 0x539824
UNK_C28C61, UNK_C28F80 = 0xC28C61, 0xC28F80

# script arguments by index, set by each test
ARGS = {}

F = fakes(emu, {
    0x4B12F0: 2, 0x4018F0: (0, 7), 0x401530: 3, 0x4F1A60: (0, 0x31), 0x4F1A70: (0, 0x32),
    0x4E7530: (0, 0x33), 0x4F1210: (0, 0x34), 0x4F1220: (0, 0x35), 0x4F1950: (1, 0x1950),
    0x4F0EF0: (0, 0xEF0), 0x4F0FA0: (0, 0xFA0), 0x4F0FF0: (0, 0xFF0), 0x4F1000: (0, 0x1000),
    0x4B8890: 4, 0x4B8810: 2, 0x4061C0: (13, 0x61C0), 0x4061E0: (1, 0x61E0), 0x4ED5E0: 16, 0x48A710: 0,
    0x497570: 2, 0x424E20: (1, 0x4E20), 0x4062E0: (0, 1), 0x4062F0: 1, 0x424860: (0, 2), 0x424870: (0, 5),
    0x424880: (0, 3), 0x424890: (0, 4), 0x489460: (0, 0), 0x4F1030: (1, 0),
})
PyFake(emu, 0x401900, log=(0, 1, ("w", 2), 3), ret=0x1234)
PyFake(emu, 0x401400, log=(("w", 0), ("b", 1), 2))
PyFake(emu, 0x4F1A80, log=(0, lambda e: e.arg(2) - e.arg(1), 3, 4, 5))
PyFake(emu, 0x497680, log=(0, (1, 4)))
PyFake(emu, 0x4B8A20, log=(0, 1), ret=lambda e: ARGS.get(e.arg(1), 0))
DISPLAY = emu.alloc(16)
emu.write(DISPLAY, u32(0) + u32(800) + u32(600))
PyFake(emu, 0x491CF0, ret=DISPLAY)
HANDLER = emu.alloc(16)
PyFake(emu, HANDLER, log=(0,))
emu.baseline = emu.snapshot()

OUT = emu.alloc(24)
IGNORE = [(OUT + 4, OUT + 8), (OUT + 20, OUT + 24)]


def new_task(e, size, type_=0):
    t = e.alloc(size)
    e.write(t + 8, u32(t + 0xC) + u32(0) + u32(t + 8))
    e.write(t + 0x1C, struct.pack("<H", type_))
    return t


def add_child(e, parent, child):
    tailpred = e.u32(parent + 0x10)
    e.write(child, u32(parent + 0xC) + u32(tailpred))
    e.write(tailpred, u32(child))
    e.write(parent + 0x10, u32(child))


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), u32(fn))


def menu(e, screen_type=0x41, kids=(0x41, 0x42), language=1, depth=3):
    mm = new_task(e, 0x2898, 0x40)
    e.write(mm + 0x20, b"\x5a" * 0x2878)
    e.write(mm + 0x20, u32(7) + u32(8) + u32(9) + u32(10))
    if screen_type is not None:
        e.write(mm + 0x30, u32(new_task(e, 0x54, screen_type)))
    for k in kids:
        add_child(e, mm, new_task(e, 0x54, k))
    e.write(mm + 0x27E4, u32(language) + u32(depth))
    e.write(mm + 0x27EC, b"".join(u32(0x100 + i) for i in range(16)))
    for slot in (2, 3, 4, 5):
        set_handler(e, slot, 0x41, HANDLER)
    set_handler(e, 2, 0x42, HANDLER)  # event 910 has no NULL check
    return mm


def test_init():
    compare(emu, 0x4170A0, "MenuManager_Init", check_eax=False, normalized=True)


@pytest.mark.parametrize("keys,joy", [(0, 0), (1, 0), (1, 1)])
def test_create(keys, joy):
    def setup(e):
        e.write(UNK_C28C61, bytes([keys]))
        e.write(UNK_C28F80, bytes([joy]))
        return (menu(e),)
    compare(emu, 0x417260, "MenuManager_Create", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("screen_type", [None, 0x41, 0x42])
@pytest.mark.parametrize("language,old", [(1, 1), (2, 1)])
def test_apply_language(screen_type, language, old):
    def setup(e):
        F[0x4062E0].ret = old
        mm = menu(e, screen_type if screen_type else 0x41, language=language)
        if screen_type is None:
            e.write(mm + 0x30, u32(0))
        return (mm,)
    if screen_type is None and language != old:
        return  # the original then sends to a NULL window
    compare(emu, 0x417520, "MenuManager_ApplyLanguage", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("all_", [0, 1])
@pytest.mark.parametrize("screen_type", [0x41, 0x42])
@pytest.mark.parametrize("kids", [(), (0x41,), (0x41, 0x42, 0x41)])
def test_refresh(all_, screen_type, kids):
    compare(emu, 0x417590, "MenuManager_Refresh", setup=lambda e: (menu(e, screen_type, kids), all_),
            check_eax=False, normalized=True)


@pytest.mark.parametrize("window", [0, 1])
def test_send_event(window):
    def setup(e):
        mm = menu(e)
        return mm, e.u32(mm + 0x30) if window else 0
    compare(emu, 0x417660, "MenuManager_SendEvent910", setup=setup, check_eax=False, normalized=True)


SCRIPT = [(0x4174A0, "MenuManager_RequestScreen"), (0x417690, "MenuManager_PushScreen"),
          (0x417710, "MenuManager_PopScreen"), (0x417790, "MenuManager_LeaveMenus"),
          (0x417810, "MenuManager_SetLanguage"), (0x417880, "MenuManager_ForceUpdateWindow"),
          (0x417900, "MenuManager_ActivatePopupScreen"), (0x417960, "MenuManager_DeactivatePopuScreen"),
          (0x4179B0, "MenuManager_SetEnabled")]


@pytest.mark.parametrize("addr,name", SCRIPT)
@pytest.mark.parametrize("a0,a1", [(0, 0), (1, 1), (5, 9), (12, 0)])
@pytest.mark.parametrize("unk", [0, 1])
def test_script(addr, name, a0, a1, unk):
    def setup(e):
        ARGS.clear()
        ARGS.update({0: a0, 1: a1})
        F[0x489460].ret = unk
        F[0x4062E0].ret = 1 + unk
        mm = menu(e)
        F[0x4F1030].ret = e.u32(mm + 0x30) if unk else 0
        return OUT, mm, 0x5A00
    compare(emu, addr, name, setup=setup, ignore=IGNORE, normalized=True)
