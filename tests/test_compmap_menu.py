import pytest

from test_compmap_util import (COMPUTER, DRAW_SLOT, MENU, MOUSE, add_child, check, check_error, f32, make_emu,
                               make_menu, set_handler, task, u16, u32)

GAME_OBJ = 0x57BABC
NAN = float("nan")

e, fk = make_emu()
PARAMS = e.alloc(0x40)
e.baseline = e.snapshot()
DRAWN = fk.handler(1)
RUN = fk.handler(1)
fk.add(0x401A20, 1, masks={0: 0xFFFF})                           # TaskType_Free
fk.add(0x401900, 4, ret=0x42, strings=(0,), masks={2: 0xFFFF})   # TaskType_New
fk.add(0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})                  # TaskType_SetHandler
fk.add(0x401530, 3, masks={0: 0xFFFF})                           # TaskType_SetHandler2
fk.add(0x4B12F0, 2, strings=(0, 1))                              # Alias_Set
fk.add(0x4B13A0, 1, strings=(0,))                                # Alias_Remove
fk.add(0x401780, 1)                                              # Task_DeleteChildren
fk.add(0x485320, 1, ret=lambda e, a: a[0] + 0x100)               # parent of the menu
fk.add(0x406220, 0, ret=lambda e, a: e.u32(PARAMS))              # Profile_GetActive
fk.add(0x5055D0, 2, deref={1: 12})                               # panel mouse


def world(e, paused=0, profile=1, dx=0.0, dy=0.0):
    comp = e.alloc(0x200)
    u32(e, COMPUTER, comp)
    inp = e.alloc(0x200)
    u32(e, comp + 0x14, inp)
    f32(e, inp + 0x1B0, dx, dy)
    game = e.alloc(0x100)
    u32(e, GAME_OBJ, game)
    e.write(game + 0xE0, bytes([paused]))
    u32(e, game + 0x44, 0x6600)
    p = e.alloc(0x600)
    u32(e, p + 0x538, profile)
    u32(e, PARAMS, p)


def test_init():
    check(e, fk, 0x46AB60, "ComputerMenu_Init", check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("size", [(800, 600), (1023, 767), (16777217, 3)])
def test_create(size, cw):
    def setup(e):
        u32(e, 0xC28B44, *size)
        return task(e, 0x3C), 0x1234
    check(e, fk, 0x46ABE0, "ComputerMenu_Create", setup, check_eax=False, cw=cw)


MOVES = [(0.0, 0.0), (0.25, -0.125), (-3.0, 2.0), (3.0, -2.0), (0.0123, 0.0456), (NAN, NAN), (-0.0, 1e-30)]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("move", MOVES)
@pytest.mark.parametrize("profile", [0, 1])
@pytest.mark.parametrize("pos", [(400.0, 300.0), (37.0, 31.0), (770.0, 545.0)])
@pytest.mark.parametrize("size", [(800, 600), (1023, 767)])
def test_run_move(move, profile, pos, size, cw):
    def setup(e):
        world(e, 0, profile, *move)
        u32(e, 0xC28B44, *size)
        m = make_menu(e)
        f32(e, m + 0x24, *pos)
        return (m,)
    check(e, fk, 0x46AC30, "ComputerMenu_Run", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("paused", [0, 1])
@pytest.mark.parametrize("screen", [0, 1, 2, 3])
@pytest.mark.parametrize("dragging", [0, 1])
@pytest.mark.parametrize("mouse,old", [(0, 0), (1, 0), (2, 0x10), (3, 0x10), (0x103, 0x30)])
def test_run(paused, screen, dragging, mouse, old):
    def setup(e):
        world(e, paused, 1, 0.5, 0.25)
        m = make_menu(e)
        f32(e, m + 0x24, 401.7, 299.5)
        e.write(m + 0x2C, bytes([dragging]))
        u32(e, m + 0x30, old, 0x77, screen)
        u32(e, MOUSE, mouse)
        add_child(e, m, task(e, 0x40, 0x61))
        add_child(e, m, task(e, 0x40, 0x62))
        set_handler(e, 1, 0x61, RUN)
        return (m,)
    check(e, fk, 0x46AC30, "ComputerMenu_Run", setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("dragging", [0, 1])
@pytest.mark.parametrize("pos", [(400.0, 300.0), (37.25, 31.5)])
def test_draw(dragging, pos, cw):
    def setup(e):
        world(e)
        m = make_menu(e)
        f32(e, m + 0x24, *pos)
        e.write(m + 0x2C, bytes([dragging]))
        add_child(e, m, task(e, 0x40, 0x31))
        add_child(e, m, task(e, 0x40, 0x32))
        set_handler(e, DRAW_SLOT, 0x31, DRAWN)
        return (m,)
    check(e, fk, 0x46AE30, "ComputerMenu_Draw", setup, check_eax=False, cw=cw)


def test_small():
    def setup(e):
        make_menu(e)
        u16(e, 0x5BDDA8, 0x66)
    check(e, fk, 0x46AE20, "ComputerMenu_Delete", setup, check_eax=False)
    check(e, fk, 0x46AF90, "ComputerMenu_Shutdown", setup, check_eax=False)
    check(e, fk, 0x46B030, "ComputerMenu_Get", setup)
    check(e, fk, 0x46B040, "ComputerMenu_Get2", setup)

    def setup2(e):
        m = make_menu(e)
        u32(e, m + 0x20, 0x111, 0x222, 0x333, 0x444, 0x555, 0x666, 0x777)
    check(e, fk, 0x46B050, "ComputerMenu_GetPanel", setup2)
    check(e, fk, 0x46B4F0, "ComputerMenu_GetButtons", setup2)
    check(e, fk, 0x46B500, "ComputerMenu_GetPressed", setup2)
    check(e, fk, 0x46B060, "ComputerMenu_SetPanel", setup2, args=(0x9999,), check_eax=False)
    check(e, fk, 0x46B4E0, "ComputerMenu_SetScreen", setup2, args=(3,), check_eax=False)


@pytest.mark.parametrize("screen", [1, 2, 3])
@pytest.mark.parametrize("menu", [False, True])
def test_open_screen(screen, menu):
    def setup(e):
        world(e)
        u16(e, 0x5BDDA8, 0x66)
        if menu:
            make_menu(e)
        else:
            u32(e, MENU, 0)
        return (screen,)
    check(e, fk, 0x46AFB0, "ComputerMenu_OpenScreen", setup, check_eax=False)


@pytest.mark.parametrize("screen", [0, 4])
def test_open_screen_error(screen):
    def setup(e):
        world(e)
        make_menu(e)
        return (screen,)
    assert check_error(e, 0x46AFB0, "ComputerMenu_OpenScreen", setup) == "No New function for menu!!"
