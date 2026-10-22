import pytest

from test_menuscreen_util import (RET, BOX, CHILD, add_child, button, cstr, get_params_test, make_emu, register_test,
                                  run, shutdown_test, task, text_new, u32, window)

emu = make_emu()


# ToggleBox, the end of the type before ListBox

@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("orig,ours", [(0x41E5A0, "ToggleBox_Activate"), (0x41E600, "ToggleBox_Deactivate")])
def test_togglebox_register(orig, ours, on):
    register_test(emu, orig, ours, 0xA8, 0x14C, on)


def test_togglebox_shutdown():
    shutdown_test(emu, 0x41E660, "ToggleBox_Shutdown", 0x53B674, 0x57BBD0)


# ListBox

ITEMS = [b"Easy", b"A rather long difficulty name", b"Hard", b""]


def box(e, count=3, index=1, active=15, text=True, width=100, key=0, buttons=True, fade=16, menu=True):
    lb = window(e, 0xD38, fade=fade, menu=menu)
    e.write(lb + 0x54, b"arrow\0")
    e.write(lb + 0x94, b"font1.fnt\0")
    for i in range(count):
        it = lb + 0xE0 + 0x2C * i
        u32(e, it, 10 * i + 5)
        e.write(it + 4, ITEMS[i % len(ITEMS)] + b"\0")
    u32(e, lb + 0xBE0, count, index, active, width)
    if text:
        u32(e, lb + 0xBF4, text_new(e, ()))
    if buttons:
        u32(e, lb + 0xD4, button(e, 16, 12), button(e, 18, 14))
    u32(e, lb + 0x50, key)
    return lb


def test_create():
    run(emu, 0x41E8C0, "ListBox_Create", lambda e: (box(e, buttons=False), 0x1234))


def test_clear():
    run(emu, 0x41E990, "ListBox_Clear", lambda e: (box(e),))


@pytest.mark.parametrize("text", [False, True])
def test_delete(text):
    run(emu, 0x41E9D0, "ListBox_Delete", lambda e: (box(e, text=text),))


@pytest.mark.parametrize("menu", [False, True])
@pytest.mark.parametrize("font", [0, 1])
@pytest.mark.parametrize("text", [False, True])
@pytest.mark.parametrize("sizes", [(16, 12, 18, 14), (20, 30, 18, 14)])
def test_changed(menu, font, text, sizes):
    def setup(e):
        RET[0x418E40] = 0x5555 if font else 0
        lb = box(e, text=text, menu=menu)
        u32(e, e.u32(lb + 0xD4) + 0x28, sizes[0], sizes[1])
        u32(e, e.u32(lb + 0xD8) + 0x28, sizes[2], sizes[3])
        return lb, 0x4321
    run(emu, 0x41EA70, "ListBox_Changed", setup)


@pytest.mark.parametrize("prev,next_,key", [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, -1), (0, 0, -2), (1, 1, -2),
                                             (0, 0, -7)])
@pytest.mark.parametrize("count,index", [(0, 0), (3, 0), (3, 1), (3, 2), (1, 0)])
def test_run(prev, next_, key, count, index):
    def setup(e):
        lb = box(e, count=count, index=index, key=key)
        e.write(e.u32(lb + 0xD4) + 0x4B, bytes([prev]))
        e.write(e.u32(lb + 0xD8) + 0x4B, bytes([next_]))
        return (lb,)
    run(emu, 0x41EC70, "ListBox_Run", setup)


@pytest.mark.parametrize("c", [dict(), dict(fade=4), dict(fade=12), dict(count=0), dict(text=False), dict(index=0),
                               dict(index=1, width=60), dict(index=1, width=10), dict(index=1, width=0),
                               dict(index=3, count=4), dict(index=2, width=33)])
def test_draw(c):
    def setup(e):
        lb = box(e, count=c.get("count", 3), index=c.get("index", 1), text=c.get("text", True),
                 width=c.get("width", 100), fade=c.get("fade", 16))
        e.write(lb + 0x4D, b"\x01")
        return (lb,)
    run(emu, 0x41ED70, "ListBox_Draw", setup)


def test_get_params():
    get_params_test(emu, 0x41F020, "ListBox_GetParams", 0x57BBDC)


@pytest.mark.parametrize("fill", [0, 1])
@pytest.mark.parametrize("select", [0, 1])
@pytest.mark.parametrize("value", [5.0, 15.4, 24.5, 25.0, 99.0])
@pytest.mark.parametrize("children", [0, 2])
def test_layout(fill, select, value, children):
    def setup(e):
        RET[0x4F16C0] = select
        RET[0x4F16E0] = value
        lb = box(e)
        if fill:
            u32(e, lb + 0xBF8, cstr(e, b"$fill"))
        for i in range(children):
            add_child(e, lb, task(e, 0x54, CHILD if i % 2 else BOX))
        return (lb,)
    run(emu, 0x41F030, "ListBox_Layout", setup)


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("children", [0, 3])
def test_forward(has, children):
    def setup(e):
        RET[0x4F16C0] = has
        lb = box(e)
        for i in range(children):
            add_child(e, lb, task(e, 0x54, CHILD if i % 2 else BOX))
        return (lb,)
    run(emu, 0x41F140, "ListBox_Forward918", setup)


@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("orig,ours", [(0x41F1E0, "ListBox_Activate"), (0x41F240, "ListBox_Deactivate")])
def test_register(orig, ours, on):
    register_test(emu, orig, ours, 0xBF0, 0xD38, on)


def test_shutdown():
    shutdown_test(emu, 0x41F2A0, "ListBox_Shutdown", 0x53B694, 0x57BBDC)


@pytest.mark.parametrize("id_", [5, 15, 7])
@pytest.mark.parametrize("count", [0, 2])
def test_add_item(id_, count):
    def setup(e):
        lb = box(e, count=count)
        u32(e, 0x57BBD8, lb)
        return cstr(e, b"Item %d" % id_), id_
    run(emu, 0x41F2D0, "ListBox_AddItem", setup)
