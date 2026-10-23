"""GameScreen (0x4851B0 - 0x485320) and StatusScreen (0x486900 - 0x4872B0)."""
import pytest

from test_mission_util import (RET, add_child, display, f32, handler, make_emu, run, set_handler, task,
                               tick_info, u16, u32)

SS_TYPE, SS_EVENT, SS_CUR = 0x5C8A74, 0x540B5C, 0x5C8A48
CHILD = 0x50
# floats around 2 - 1/15, where the unrounded sum decides the reset
NEAR_TWO = [float.fromhex("0x1.eeeee%xp+0" % d) for d in range(0xC, 0x10)] + [float.fromhex("0x1.eeeef0p+0")]

emu = make_emu()
for slot in (1, 0x30):
    set_handler(emu, slot, CHILD, handler(emu, "child_%x" % slot, 1))
HUD = handler(emu, "hud", 2, derefs={1: 4})
set_handler(emu, 0x2A, 0x51, HUD)
emu.baseline = emu.snapshot()


@pytest.mark.parametrize("orig,ours", [(0x4851B0, "GameScreen_Init"), (0x4852F0, "GameScreen_Close"),
                                       (0x486900, "StatusScreen_Init"), (0x487130, "StatusScreen_Close")])
def test_init_close(orig, ours):
    def setup(e):
        u16(e, 0x5C8A28, 0x45)
        u16(e, SS_TYPE, 0x46)
        e.write(SS_EVENT, b"\x2C")
        e.write(0x5C8A4C, b"\xEE" * 40)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("small", [0, 1])
def test_game_screen_create(small):
    def setup(e):
        e.write(0x5C8E00, bytes([small]))
        RET[0x416D40] = 0x41600
        u16(e, SS_TYPE, 0x46)
        return (task(e, 0x28, 0x45),)
    run(emu, 0x485220, "GameScreen_Create", setup)


@pytest.mark.parametrize("orig,ours,glob", [(0x485300, "GameScreen_New", 0x5C8A28),
                                            (0x487160, "StatusScreen_New", SS_TYPE)])
def test_new(orig, ours, glob):
    def setup(e):
        u16(e, glob, 0x47)
        return (0x1234,)
    run(emu, orig, ours, setup, check_eax=True)


def test_get_view():
    def setup(e):
        t = task(e, 0x28, 0x45)
        u32(e, t + 0x24, 0xABCD)
        return (t,)
    run(emu, 0x485320, "GameScreen_GetView", setup, check_eax=True)


def screen(e, blink=0.0, children=0):
    s = task(e, 0x4F0, 0x46)
    e.write(s + 0x20, bytes(range(0x50)))
    u32(e, s + 0x70, 0x7070)
    u32(e, s + 0x4DC, e.alloc(0x28), e.alloc(0x28), 0x4E4)
    f32(e, s + 0x4E8, blink)
    for i in range(children):
        add_child(e, s, task(e, 0x40, CHILD if i != 1 else 0x52))
    return s


def test_status_screen_create():
    def setup(e):
        display(e, 800, 600)
        cres = e.alloc(0x20)
        u32(e, cres + 0x10, 0xF0F1)
        RET[0x46B4D0] = cres
        u16(e, 0x540B08, 0x48)
        return (screen(e),)
    run(emu, 0x4869B0, "StatusScreen_Create", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("blink", [0.0, 1.0, 1.9, 2.0, 5.0] + NEAR_TWO)
@pytest.mark.parametrize("children", [0, 3])
def test_status_screen_run(blink, children, cw):
    run(emu, 0x486CD0, "StatusScreen_Run", lambda e: (screen(e, blink, children),), cw=cw)


def test_status_screen_delete():
    run(emu, 0x486D70, "StatusScreen_Delete", lambda e: (screen(e),))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("watching,computer,timer", [(0, 0, 1), (1, 1, 1), (1, 0, 0), (1, 0, 1)])
@pytest.mark.parametrize("time", [(0, 0), (1800, 1830), (3600 + 30, 3600 + 30 * 61), (54321, 54321), (-100, -95)])
def test_status_screen_draw(watching, computer, timer, time, cw):
    def setup(e):
        display(e, 1024, 768)
        tick_info(e, timer=timer)
        game = e.alloc(0x200)
        human = task(e, 0x40, 0x51)
        ref = e.alloc(4)
        u32(e, ref, human)
        cam = task(e, 0x128, 0x53)
        u32(e, cam + 0x68, ref)
        u32(e, game + 0x5C, cam)
        u32(e, game + 0xB4, cam if watching else 0x777)
        RET[0x416D40] = game
        RET[0x46C060] = computer
        RET["time"] = list(time)
        e.write(SS_EVENT, b"\x2A")
        return (screen(e, children=3),)
    run(emu, 0x486F30, "StatusScreen_Draw", setup, cw=cw)


@pytest.mark.parametrize("orig,ours,size", [(0x487180, "StatusScreen_Get", 4), (0x487190, "StatusScreen_GetEvent", 1),
                                            (0x4871A0, "StatusScreen_GetPortraitNames", 4),
                                            (0x4871B0, "StatusScreen_GetActionSprites", 4)])
def test_getters(orig, ours, size):
    run(emu, orig, ours, lambda e: u32(e, SS_CUR, 0x3456) if size == 4 else e.write(SS_EVENT, b"\x9C"),
        check_eax=True, eax_mask=0xFFFFFFFF if size == 4 else 0xFF)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("sprite", [0, 0x6000])
@pytest.mark.parametrize("blink", [0.0, 0.3, 0.999, 1.0, 1.7])
@pytest.mark.parametrize("size", [(33, 17), (-5, 64), (0, 0)])
def test_draw_action_icon(sprite, blink, size, cw):
    def setup(e):
        display(e, 1023, 767)
        RET[0x4B6CE0], RET[0x4B6D00] = size
        return screen(e, blink), sprite
    run(emu, 0x4871C0, "StatusScreen_DrawActionIcon", setup, cw=cw)


def test_reset_blink():
    run(emu, 0x4872B0, "StatusScreen_ResetBlink", lambda e: (screen(e, 1.5),))

