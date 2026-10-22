import struct

import pytest

from test_compmap_util import (DRAW_SLOT, FONT, MAP_ENABLED, add_child, check, cstr, make_emu, make_menu, set_handler,
                               task, u16, u32)

OBJECTIVES = 0x5BDC78

e, fk = make_emu()
DRAWN = fk.handler(1)
fk.add(0x401A20, 1, masks={0: 0xFFFF})                  # TaskType_Free
fk.add(0x401900, 4, ret=0x42, strings=(0,), masks={2: 0xFFFF})  # TaskType_New
fk.add(0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})          # TaskType_SetHandler
fk.add(0x401530, 3, masks={0: 0xFFFF})                   # TaskType_SetHandler2
fk.add(0x4B64E0, 1, ret=lambda e, a: e.read_cstr(a[0]).startswith(b"K"), strings=(0,))  # Resource_Find
fk.add(0x4F1030, 1, ret=lambda e, a: 0x5000 + a[0])      # find task by id


def test_init():
    check(e, fk, 0x468E20, "ComputerObjectives_Init", check_eax=False)


def test_small():
    def setup(e):
        u16(e, 0x5BDD70, 0x66)
    check(e, fk, 0x469420, "ComputerObjectives_Shutdown", setup, check_eax=False)
    check(e, fk, 0x469430, "ComputerObjectives_New", setup, args=(0x4444,))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("enabled", [0, 1])
@pytest.mark.parametrize("size", [(800, 600), (1023, 767)])
def test_create(enabled, size, cw):
    def setup(e):
        make_menu(e)
        u32(e, MAP_ENABLED, enabled)
        u32(e, 0xC28B44, *size)
        return (task(e, 0x38),)
    check(e, fk, 0x468E90, "ComputerObjectives_Create", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("pressed", [0, 0x10, 0x30, 0x20])
@pytest.mark.parametrize("id_", [0, 1, 2, 3])
def test_on_tab(pressed, id_):
    def setup(e):
        m = make_menu(e)
        u32(e, m + 0x34, pressed)
        p = e.alloc(4)
        u32(e, p, id_)
        return task(e, 0x90), p
    check(e, fk, 0x469030, "ComputerObjectives_OnTab", setup, check_eax=False)


def objectives(e, recs):
    """recs: (name, task id, complete, failed) for each of 6 objectives."""
    base = e.alloc(0x70 + 6 * 0xCC)
    u32(e, OBJECTIVES, base)
    for i, (name, tid, complete, failed) in enumerate(recs):
        r = base + 0x70 + i * 0xCC
        e.write(r, name + b"\0")
        u32(e, r + 0x20, tid)
        u32(e, r + 0xC4, complete, failed)


RECS = [(b"Kgoal one", 3, 0, 0), (b"", 4, 1, 0), (b"goal three is long text", -1, 1, 0),
        (b"Kfour", 7, 0, 1), (b"five", 9, 1, 1), (b"Ksix", -1, 0, 0)]


@pytest.mark.parametrize("id_", [2, 3, 4, 5, 7, 8, 9])
@pytest.mark.parametrize("match", [True, False])
@pytest.mark.parametrize("buttons", [0, 0x10])
def test_on_select(id_, match, buttons):
    def setup(e):
        m = make_menu(e)
        u32(e, m + 0x30, buttons)
        objectives(e, RECS)
        b = task(e, 0x94)
        u32(e, b + 0x38, id_)
        e.write(b + 0x80, bytes([0x10, 0x77]))
        p = e.alloc(4)
        u32(e, p, id_ if match else id_ + 1)
        return b, p
    check(e, fk, 0x469060, "ComputerObjectives_OnSelect", setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("enabled", [0, 1])
@pytest.mark.parametrize("none", [False, True])
@pytest.mark.parametrize("size", [(800, 600), (1023, 767)])
def test_draw(enabled, none, size, cw):
    def setup(e):
        u32(e, 0xC28B44, *size)
        u32(e, MAP_ENABLED, enabled)
        if none:
            u32(e, OBJECTIVES, 0)
        else:
            objectives(e, RECS)
        o = task(e, 0x38)
        for i in range(6):
            b = task(e, 0x94)
            u32(e, o + 0x20 + 4 * i, b)
        for i in range(2):
            add_child(e, o, task(e, 0x40, 0x31 + i))
        set_handler(e, DRAW_SLOT, 0x31, DRAWN)
        return (o,)
    check(e, fk, 0x469110, "ComputerObjectives_Draw", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("text", [b"", b"one", b"a few words that wrap around the box", b"  double  spaces ",
                                  b"averyveryverylongwordthatdoesnotfit and more"])
@pytest.mark.parametrize("width", [0, 60, 400])
@pytest.mark.parametrize("color", [0, 1, 2, 3])
def test_draw_wrapped(text, width, color, cw):
    def setup(e):
        return 37, 51, struct.unpack("<I", struct.pack("<f", 522.48))[0], width, 0x78, cstr(e, text), FONT, color
    check(e, fk, 0x469450, "Computer_DrawWrapped", setup, cw=cw)
