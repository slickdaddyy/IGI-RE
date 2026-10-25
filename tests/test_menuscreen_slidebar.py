import struct

import pytest

from test_menuscreen_util import (RET, BOX, CHILD, add_child, button, get_params_test, make_emu, register_test, run,
                                  shutdown_test, task, u32, window)

emu = make_emu()


def bar(e, value=0.5, step=0.05, length=50, horizontal=1, steps=0, knob=True, key=0, fade=16, menu=True,
        sizes=(16, 12, 18, 14)):
    sb = window(e, 0x250, fade=fade, menu=menu)
    e.write(sb + 0x54, b"slide\0")
    if knob:
        u32(e, sb + 0x94, 0x6006)
    u32(e, sb + 0x98, button(e, sizes[0], sizes[1]), button(e, sizes[2], sizes[3]))
    e.write(sb + 0xA0, struct.pack("<dfi", value, step, length))
    e.write(sb + 0xB4, bytes([horizontal]))
    u32(e, sb + 0x248, steps)
    u32(e, sb + 0x50, key)
    return sb


@pytest.mark.parametrize("arg", [0, 0x1234])
def test_create(arg):
    run(emu, 0x41F5A0, "SlideBar_Create", lambda e: (bar(e), arg))


def test_delete():
    run(emu, 0x41F6C0, "SlideBar_Delete", lambda e: (bar(e),))


@pytest.mark.parametrize("menu", [False, True])
@pytest.mark.parametrize("step", [0, 1])
@pytest.mark.parametrize("horizontal", [0, 1])
@pytest.mark.parametrize("knob", [0, 1])
@pytest.mark.parametrize("sizes", [(16, 12, 18, 14), (2, 3, 4, 2), (20, 30, 10, 5), (10, 5, 20, 30)])
def test_changed(menu, step, horizontal, knob, sizes):
    def setup(e):
        RET[0x4F16C0] = step
        RET[0x4F16E0] = 0.125
        RET[0x418DE0] = 0x6006 if knob else 0
        return bar(e, horizontal=horizontal, knob=knob, menu=menu, sizes=sizes), 0x4321
    run(emu, 0x41F740, "SlideBar_Changed", setup)


RUN_CASES = [
    dict(), dict(dec=1), dict(inc=1), dict(key=-1), dict(key=-2), dict(dec=1, value=0.0), dict(inc=1, value=1.0),
    dict(dec=1, value=0.02), dict(inc=1, value=0.98), dict(dec=1, steps=5, value=0.5), dict(inc=1, steps=5),
    dict(inc=1, steps=4, value=0.3), dict(steps=3, value=0.7), dict(steps=1, value=0.3), dict(knob=False),
    dict(held=1, mx=60), dict(held=1, mx=10), dict(held=1, mx=200), dict(held=1, mx=77, steps=6),
    dict(held=1, horizontal=0, my=70), dict(held=1, horizontal=0, my=55, steps=4), dict(horizontal=0),
    dict(held=1, length=9, mx=40), dict(held=1, length=9, mx=59), dict(held=1, horizontal=0, length=11, my=40), dict(value=0.123456789),
    dict(dec=1, value=0.1, step=0.3), dict(value=1.5), dict(value=-0.25),
]


@pytest.mark.parametrize("action", [0, 1])
@pytest.mark.parametrize("c", RUN_CASES)
def test_run(c, action):
    def setup(e):
        RET[0x4F16C0] = action
        RET["mx"] = c.get("mx", 0)
        RET["my"] = c.get("my", 0)
        sb = bar(e, value=c.get("value", 0.5), step=c.get("step", 0.05), steps=c.get("steps", 0),
                 horizontal=c.get("horizontal", 1), knob=c.get("knob", True), length=c.get("length", 50),
                 key=c.get("key", 0))
        e.write(e.u32(sb + 0x98) + 0x4B, bytes([c.get("dec", 0)]))
        e.write(e.u32(sb + 0x9C) + 0x4B, bytes([c.get("inc", 0)]))
        e.write(sb + 0x4A, bytes([c.get("held", 0)]))
        return (sb,)
    run(emu, 0x41FA10, "SlideBar_Run", setup)


@pytest.mark.parametrize("horizontal", [0, 1])
@pytest.mark.parametrize("value", [0.0, 0.3, 0.5, 0.75, 1.0])
@pytest.mark.parametrize("c", [dict(), dict(knob=False), dict(fade=5), dict(fade=12)])
def test_draw(horizontal, value, c):
    def setup(e):
        sb = bar(e, value=value, horizontal=horizontal, knob=c.get("knob", True), fade=c.get("fade", 16))
        e.write(sb + 0x4D, b"\x01")
        return (sb,)
    run(emu, 0x41FCE0, "SlideBar_Draw", setup)


def test_get_params():
    get_params_test(emu, 0x41FF20, "SlideBar_GetParams", 0x57BBE4)


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("steps", [0, 4])
@pytest.mark.parametrize("value", [0.25, 2.5, 3.5])
@pytest.mark.parametrize("children", [0, 2])
def test_layout(has, steps, value, children):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        sb = bar(e, steps=steps)
        for i in range(children):
            add_child(e, sb, task(e, 0x54, CHILD if i % 2 else BOX))
        return (sb,)
    run(emu, 0x41FF30, "SlideBar_Layout", setup)


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("children", [0, 3])
def test_forward(has, children):
    def setup(e):
        RET[0x4F16C0] = has
        sb = bar(e)
        for i in range(children):
            add_child(e, sb, task(e, 0x54, CHILD if i % 2 else BOX))
        return (sb,)
    run(emu, 0x420020, "SlideBar_Save", setup)


@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("orig,ours", [(0x4200C0, "SlideBar_Activate"), (0x420140, "SlideBar_Deactivate")])
def test_register(orig, ours, on):
    register_test(emu, orig, ours, 0xB5, 0x250, on)


def test_shutdown():
    shutdown_test(emu, 0x4201B0, "SlideBar_Shutdown", 0x53B6BC, 0x57BBE4)


def test_new_vertical():
    def setup(e):
        u32(e, 0x53B6BC, 0x43)
        return 0x5000, 0x6000
    run(emu, 0x4201E0, "SlideBar_NewVertical", setup, check_eax=True)


def test_set_range():
    run(emu, 0x420240, "SlideBar_SetRange", lambda e: (bar(e), 11, 22, 33, 140, 7))


@pytest.mark.parametrize("steps", [0, 1, 3, 10])
@pytest.mark.parametrize("value", [0.0, 0.25, 0.5, 0.75, 0.999, 1.0])
def test_get_pos(steps, value):
    run(emu, 0x420290, "SlideBar_GetPos", lambda e: (bar(e, value=value, steps=steps),), check_eax=True)


@pytest.mark.parametrize("steps", [0, 1, 3, 7])
@pytest.mark.parametrize("pos", [0, 1, 2, -1])
def test_set_pos(steps, pos):
    run(emu, 0x4202D0, "SlideBar_SetPos", lambda e: (bar(e, steps=steps), pos))
