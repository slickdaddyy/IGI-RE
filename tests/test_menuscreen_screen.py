import pytest

from test_menuscreen_util import (RET, BOX, get_params_test, logfake, make_emu, run, strip_logger, task, text_new, u16,
                                  u32, window)

emu = make_emu()
strip_logger(emu)
PICS = {}
logfake(emu, 0x418D80, 2, strings=(1,), effect=lambda e, a: PICS.get(e.read_cstr(a[1]), 0))  # MenuManager_FindPic


# MenuButton

def test_button_create():
    run(emu, 0x421510, "MenuButton_Create", lambda e: (window(e, 0x98), 0x1234))


@pytest.mark.parametrize("menu", [False, True])
@pytest.mark.parametrize("found", [0, 1])
def test_button_changed(menu, found):
    def setup(e):
        RET[0x418DE0] = 0x6006 if found else 0
        b = window(e, 0x98, menu=menu)
        e.write(b + 0x54, b"arrow_1.spr\0")
        return b, 0x4321
    run(emu, 0x421550, "MenuButton_Changed", setup)


def test_button_run():
    run(emu, 0x4215D0, "MenuButton_Run", lambda e: (window(e, 0x98),))


@pytest.mark.parametrize("image", [0, 0x6006])
@pytest.mark.parametrize("fade", [16, 11, 3])
def test_button_draw(image, fade):
    def setup(e):
        b = window(e, 0x98, fade=fade, x=-7, y=500)
        u32(e, b + 0x94, image)
        return (b,)
    run(emu, 0x421600, "MenuButton_Draw", setup)


def test_button_get_params():
    get_params_test(emu, 0x4216F0, "MenuButton_GetParams", 0x57BBFC)


@pytest.mark.parametrize("parent", [0, 0x5000])
def test_button_new(parent):
    def setup(e):
        u16(e, 0x53B7F0, 0x44)
        return (parent,)
    run(emu, 0x421730, "MenuButton_New", setup, check_eax=True)


# MenuScreen

def screen(e, pic=True, logo=True, fade=16, frame=(30, 520, 60, 400)):
    ms = window(e, 0x1A4, fade=fade)
    e.write(ms + 0x54, b"back.pic\0")
    if pic:
        u32(e, ms + 0x94, text_new(e, ()))
    if logo:
        u32(e, ms + 0x98, text_new(e, ()))
    u32(e, ms + 0x9C, frame[0], frame[1], frame[2], frame[3])
    return ms


def test_create():
    run(emu, 0x4219B0, "MenuScreen_Create", lambda e: (screen(e), 0x1234))


@pytest.mark.parametrize("index", [-1, 0, 3])
@pytest.mark.parametrize("handled", [0, 1])
def test_delete(index, handled):
    def setup(e):
        RET[0x4F1050] = index
        ms = screen(e)
        u16(e, ms + 0x1C, BOX if handled else 0x47)
        return (ms,)
    run(emu, 0x421A10, "MenuScreen_Delete", setup)


@pytest.mark.parametrize("menu", [False, True])
@pytest.mark.parametrize("usa", [0, 1])
@pytest.mark.parametrize("pic", [0, 1])
@pytest.mark.parametrize("logo", [0, 1])
@pytest.mark.parametrize("width", [301, 300, 77])
def test_update(menu, usa, pic, logo, width):
    def setup(e):
        RET[0x48F2D0] = usa
        RET[0x4B6E70] = width
        ms = screen(e)
        if not menu:
            u32(e, ms + 0x44, 0)
        PICS.clear()
        PICS[b"back.pic"] = text_new(e, ()) if pic else 0
        PICS[b"logo.pic"] = text_new(e, ()) if logo else 0
        PICS[b"logo_us.pic"] = text_new(e, ()) if logo else 0
        return ms, 0x4321
    run(emu, 0x421A70, "MenuScreen_UpdateInternalDataHandler", setup)


@pytest.mark.parametrize("pic", [0, 1])
@pytest.mark.parametrize("logo", [0, 1])
@pytest.mark.parametrize("fx", [0, 1])
def test_draw(pic, logo, fx):
    def setup(e):
        ms = screen(e, pic=pic, logo=logo)
        if fx:
            RET[0x419040] = task(e, 0x24, BOX)
        return (ms,)
    run(emu, 0x421C20, "MenuScreen_DrawHandler", setup)


@pytest.mark.parametrize("pic", [0, 1])
@pytest.mark.parametrize("fade", [16, 8, 7, 3, 0, 20])
@pytest.mark.parametrize("frame", [(30, 520, 60, 400), (-5, 33, 400, 17), (0, 640, 0, 480)])
def test_draw_frame(pic, fade, frame):
    run(emu, 0x421CD0, "MenuScreen_DrawFrame", lambda e: (screen(e, pic=pic, fade=fade, frame=frame),))
