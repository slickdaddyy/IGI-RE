import struct

import pytest

from test_compmap_util import RES, check, cstr, f32, make_emu, task, u16, u32

e, fk = make_emu()
fk.add(0x401A20, 1, masks={0: 0xFFFF})                           # TaskType_Free
fk.add(0x401900, 4, ret=0x42, strings=(0,), masks={2: 0xFFFF})   # TaskType_New
fk.add(0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})                  # TaskType_SetHandler
fk.add(0x401530, 3, masks={0: 0xFFFF})                           # TaskType_SetHandler2
fk.add(0x504DC0, 2)                                              # list add
fk.add(0x4B7000, 1, ret=lambda e, a: 0xA000 + len(e.read_cstr(a[0])), strings=(0,))  # Sprite_New
fk.add(0x4B6720, 2, strings=(1,))                                # Sprite_Load
fk.add(0x4B66A0, 1)                                              # Sprite_Free
fk.add(0x4B7030, 1)                                              # Sprite_Delete
fk.add(0x4B63D0, 1)                                              # Resource_Flush


def bits(f):
    return struct.unpack("<I", struct.pack("<f", f))[0]


def test_init():
    def setup(e):
        u16(e, 0x54E720, 0x77)
        e.write(0x54E70B, b"\x2a")
    check(e, fk, 0x46B510, "ComputerButtonItem_Init", setup, check_eax=False)
    check(e, fk, 0x46B7F0, "ComputerTextItem_Init", setup, check_eax=False)


def test_small():
    def setup(e):
        u16(e, 0x5BE2B4, 0x66)
        u16(e, 0x5BE2BC, 0x67)
    check(e, fk, 0x46B650, "ComputerButtonItem_Shutdown", setup, check_eax=False)
    check(e, fk, 0x46B960, "ComputerTextItem_Shutdown", setup, check_eax=False)
    check(e, fk, 0x46B940, "ComputerItem_AddToList", lambda e: (task(e, 0x94), 0x5555), check_eax=False)
    check(e, fk, 0x46B4D0, "ComputerRes_Get")
    check(e, fk, 0x46B7D0, "ComputerButtonItem_SetPos", lambda e: (task(e, 0x94), bits(12.5), bits(-3.25)),
          check_eax=False)


@pytest.mark.parametrize("text", [False, True])
def test_delete(text):
    def setup(e):
        b = task(e, 0x94)
        u32(e, b + 0x84, 0x3333 if text else 0)
        return (b,)
    check(e, fk, 0x46B5A0, "ComputerButtonItem_Delete", setup, check_eax=False)
    check(e, fk, 0x46B880, "ComputerTextItem_Delete", lambda e: (task(e, 0x90),), check_eax=False)


@pytest.mark.parametrize("hover", [0, 1])
@pytest.mark.parametrize("pressed", [0, 1])
@pytest.mark.parametrize("text", [False, True])
def test_button_draw(hover, pressed, text):
    def setup(e):
        b = task(e, 0x94)
        e.write(b + 0x4E, bytes([hover]))
        f32(e, b + 0x78, 123.5, 77.25)
        e.write(b + 0x80, bytes([pressed]))
        u32(e, b + 0x84, e.alloc(0x28) if text else 0, 0x1110, 0x2220, 0x3330)
        return (b,)
    check(e, fk, 0x46B5C0, "ComputerButtonItem_Draw", setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("hover", [0, 1])
@pytest.mark.parametrize("color", [0, 1, 2, 3])
def test_text_draw(hover, color, cw):
    def setup(e):
        t = task(e, 0x90)
        e.write(t + 0x4E, bytes([hover]))
        u32(e, t + 0x78, e.alloc(0x28), color)
        return (t,)
    check(e, fk, 0x46B890, "ComputerTextItem_Draw", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("text", [b"", b"label"])
@pytest.mark.parametrize("e_", [0, -3])
@pytest.mark.parametrize("pos", [(10.0, 20.0), (123.456, -77.7), (1e8, 3.0)])
@pytest.mark.parametrize("sprite", [0x2000, 0x201F])
def test_button_new(text, e_, pos, sprite, cw):
    def setup(e):
        return (task(e, 0x40), 5, 1, 2, 3, 4, e_ & 0xFF, bits(pos[0]), bits(pos[1]), 0x101, 0x7100, cstr(e, text),
                sprite, sprite + 0x18, sprite + 0x30, 0x4321)
    check(e, fk, 0x46B660, "ComputerButtonItem_New", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("e_", [0, -3])
@pytest.mark.parametrize("pos", [(10.0, 20.0), (123.456, -77.7)])
def test_text_new(e_, pos, cw):
    def setup(e):
        return (task(e, 0x40), 5, 1, 2, 3, 4, e_ & 0xFF, bits(pos[0]), bits(pos[1]), 0x7100, cstr(e, b"text"), 2,
                0x4321)
    check(e, fk, 0x46B970, "ComputerTextItem_New", setup, cw=cw)


def test_res_load():
    check(e, fk, 0x46B070, "ComputerRes_Load", check_eax=False)


def test_res_free():
    def setup(e):
        u32(e, RES, 0x111, 0x222, 0x333, 0x444, 0x555, 0x666)
    check(e, fk, 0x46B310, "ComputerRes_Free", setup, check_eax=False)
