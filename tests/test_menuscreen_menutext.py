import pytest

from test_menuscreen_util import (RET, BOX, CHILD, add_child, cstr, get_params_test, make_emu, run, shutdown_test,
                                  task, text_new, u32, window)

emu = make_emu()

COND, PLAIN = 0x218, 0x170


def text(e, size=COND, label=True, color=2, action=True, enabled=1, visible=1, key=0, sound=b"", fade=16,
         menu=True, focused=0, glow=True, x=40, y=30):
    mt = window(e, size, fade=fade, menu=menu, x=x, y=y, w=120, h=17)
    e.write(mt + 0x54, b"MENU_START\0")
    e.write(mt + 0x94, b"font2.fnt\0")
    e.write(mt + 0xD4, sound + b"\0")
    if label:
        u32(e, mt + 0x114, text_new(e, ()))
    u32(e, mt + 0x118, color)
    if action:
        u32(e, mt + 0x11C, cstr(e, b"$action"))
    if size == COND:
        u32(e, mt + 0x20C, 0x6060 if glow else 0, enabled, visible)
    else:
        u32(e, mt + 0x16C, 0x6060 if glow else 0)
    u32(e, mt + 0x50, key)
    e.write(mt + 0x4D, bytes([focused]))
    return mt


# MenuTextConditional

@pytest.mark.parametrize("visible", [0, 1])
def test_cond_place(visible):
    run(emu, 0x420500, "MenuTextConditional_Place", lambda e: (text(e, visible=visible), 0x2222))


def test_cond_create():
    run(emu, 0x420550, "MenuTextConditional_Create", lambda e: (text(e), 0x1234))


@pytest.mark.parametrize("label", [False, True])
def test_cond_delete(label):
    run(emu, 0x4205F0, "MenuTextConditional_Delete", lambda e: (text(e, label=label),))


@pytest.mark.parametrize("menu", [False, True])
@pytest.mark.parametrize("font", [0, 1])
@pytest.mark.parametrize("label", [False, True])
@pytest.mark.parametrize("action", [False, True])
@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("value", [0.0, 1.0, 2.5, 257.0])
def test_cond_changed(menu, font, label, action, has, value):
    def setup(e):
        RET[0x418E40] = 0x5555 if font else 0
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        return text(e, label=label, action=action, menu=menu, enabled=7, visible=9), 0x4321
    run(emu, 0x420640, "MenuTextConditional_Changed", setup)


RUN_CASES = [dict(), dict(over=1), dict(over=1, was=1), dict(clicked=1), dict(key=-7), dict(key=-1),
             dict(clicked=1, sound=b"menu_back"), dict(clicked=1, action=False), dict(clicked=1, enabled=0),
             dict(clicked=1, has=0)]


@pytest.mark.parametrize("c", RUN_CASES)
@pytest.mark.parametrize("value", [None, 0.0, 1.0])
def test_cond_run(c, value):
    def setup(e):
        RET[0x4F16C0] = c.get("has", 1) if value is None else 1
        RET[0x4F16E0] = value or 0.0
        mt = text(e, action=c.get("action", True), enabled=c.get("enabled", 1), key=c.get("key", 0),
                  sound=c.get("sound", b""))
        e.write(mt + 0x48, bytes([c.get("was", 0), c.get("over", 0), 0, c.get("clicked", 0)]))
        return (mt,)
    run(emu, 0x4207E0, "MenuTextConditional_Run", setup)


DRAW_CASES = [dict(), dict(focused=1), dict(focused=1, fade=15), dict(fade=4), dict(fade=12), dict(color=0x12),
              dict(color=0x13, focused=1), dict(color=1, enabled=0), dict(color=3, focused=1, enabled=0),
              dict(label=False), dict(visible=0), dict(focused=1, x=-50, y=400), dict(color=0xF)]


@pytest.mark.parametrize("c", DRAW_CASES)
def test_cond_draw(c):
    def setup(e):
        return (text(e, label=c.get("label", True), color=c.get("color", 2), enabled=c.get("enabled", 1),
                     visible=c.get("visible", 1), fade=c.get("fade", 16), focused=c.get("focused", 0),
                     x=c.get("x", 40), y=c.get("y", 30)),)
    run(emu, 0x4208F0, "MenuTextConditional_Draw", setup)


def test_cond_get_params():
    get_params_test(emu, 0x420BB0, "MenuTextConditional_GetParams", 0x57BBEC)


@pytest.mark.parametrize("size", [COND, PLAIN])
@pytest.mark.parametrize("menu", [False, True])
@pytest.mark.parametrize("label", [False, True])
@pytest.mark.parametrize("children", [0, 2])
def test_update_text(size, menu, label, children):
    def setup(e):
        mt = text(e, size=size, menu=menu, label=label)
        for i in range(children):
            add_child(e, mt, task(e, 0x54, CHILD if i % 2 else BOX))
        return (mt,)
    run(emu, 0x420BC0, "MenuText_UpdateText", setup)


def test_cond_shutdown():
    shutdown_test(emu, 0x420C90, "MenuTextConditional_Shutdown", 0x53B700, 0x57BBEC)


# MenuText

def test_create():
    run(emu, 0x420E90, "MenuText_Create", lambda e: (text(e, size=PLAIN), 0x1234))


@pytest.mark.parametrize("label", [False, True])
def test_delete(label):
    run(emu, 0x420F00, "MenuText_Delete", lambda e: (text(e, size=PLAIN, label=label),))


@pytest.mark.parametrize("menu", [False, True])
@pytest.mark.parametrize("font", [0, 1])
@pytest.mark.parametrize("label", [False, True])
@pytest.mark.parametrize("action", [False, True])
def test_changed(menu, font, label, action):
    def setup(e):
        RET[0x418E40] = 0x5555 if font else 0
        return text(e, size=PLAIN, label=label, action=action, menu=menu), 0x4321
    run(emu, 0x420F40, "MenuText_Changed", setup)


@pytest.mark.parametrize("c", RUN_CASES)
def test_run(c):
    def setup(e):
        RET[0x4F16C0] = c.get("has", 1)
        mt = text(e, size=PLAIN, action=c.get("action", True), key=c.get("key", 0), sound=c.get("sound", b""))
        e.write(mt + 0x48, bytes([c.get("was", 0), c.get("over", 0), 0, c.get("clicked", 0)]))
        return (mt,)
    run(emu, 0x421070, "MenuText_Run", setup)


@pytest.mark.parametrize("c", DRAW_CASES)
def test_draw(c):
    def setup(e):
        return (text(e, size=PLAIN, label=c.get("label", True), color=c.get("color", 2), fade=c.get("fade", 16),
                     focused=c.get("focused", 0), x=c.get("x", 40), y=c.get("y", 30)),)
    run(emu, 0x421130, "MenuText_Draw", setup)


def test_get_params():
    get_params_test(emu, 0x4213D0, "MenuText_GetParams", 0x57BBF4)
