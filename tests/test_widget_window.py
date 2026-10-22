import itertools

import pytest

from test_widget_util import (BOX, CHILD, OTHER, MENU, WINDOW, add_child, cstr, f32, init_compare, init_emu,
                              input_state, make_emu, run, shutdown_test, task, u32)

emu = make_emu()


def test_create_with_menu():
    def setup(e):
        w = task(e, 0x54, BOX)
        e.write(w + 0x20, b"\xAA" * 0x34)
        return w, 0x1234
    run(emu, 0x4241C0, "MenuWindow_Create", setup)


@pytest.mark.parametrize("depth", [0, 1, 3, None])
def test_create_find_menu(depth):
    def setup(e):
        parent = 0
        if depth is not None:
            parent = task(e, 0x2898, MENU)
            for _ in range(depth):
                parent = task(e, 0x54, CHILD, parent)
        w = task(e, 0x54, BOX, parent)
        e.write(w + 0x20, b"\xAA" * 0x34)
        return w, 0
    run(emu, 0x4241C0, "MenuWindow_Create", setup)


@pytest.mark.parametrize("depth", [0, 2, None])
def test_find_menu(depth):
    def setup(e):
        parent = 0
        if depth is not None:
            parent = task(e, 0x2898, MENU)
            for _ in range(depth):
                parent = task(e, 0x54, CHILD, parent)
        w = task(e, 0x54, BOX, parent)
        u32(e, w + 0x44, 0x5A5A)
        return w, w
    run(emu, 0x424220, "MenuWindow_FindMenu", setup)


@pytest.mark.parametrize("name", [None, b"", b"x"])
@pytest.mark.parametrize("menu", [0, 1])
def test_changed(name, menu):
    def setup(e):
        w = task(e, 0x54, BOX)
        u32(e, w + 0x44, 0x7777 if menu else 0)
        return w, cstr(e, name) if name is not None else 0
    run(emu, 0x424260, "MenuWindow_Changed", setup)


def with_children(e, n):
    w = task(e, 0x54, BOX)
    for i in range(n):
        add_child(e, w, task(e, 0x54, (CHILD, OTHER, BOX)[i % 3]))
    return w


@pytest.mark.parametrize("orig,ours", [(0x424280, "MenuWindow_Draw"), (0x424300, "MenuWindow_ForwardEvent914"),
                                       (0x424380, "MenuWindow_ForwardEvent91C")])
@pytest.mark.parametrize("n", [0, 1, 4])
def test_forward(orig, ours, n):
    run(emu, orig, ours, lambda e: (with_children(e, n),))


def test_get_size():
    def setup(e):
        w = task(e, 0x54, BOX)
        u32(e, w + 0x28, 123, 45)
        out = e.alloc(12)
        e.write(out, b"\x55" * 12)
        return w, out
    run(emu, 0x424400, "MenuWindow_GetSize", setup)


@pytest.mark.parametrize("hor,vert", [(0, 0), (1, 0), (0, 1), (1, 1)])
@pytest.mark.parametrize("cx,cy", [(0, 0), (1, 1), (2, 1)])
def test_place(hor, vert, cx, cy):
    def setup(e):
        p = task(e, 0x54, BOX)
        u32(e, p + 0x20, 50, 60, 301, 207)
        w = task(e, 0x54, CHILD, p)
        u32(e, w + 0x20, 1, 2, 77, 33)
        u32(e, w + 0x3C, cx, cy)
        state = e.alloc(0x18)
        e.write(state, bytes([hor, vert, 0, 0]))
        u32(e, state + 4, 100, 120, 7, 9, p)
        return w, state
    run(emu, 0x424420, "MenuWindow_Place", setup)


def arrange_tree(e, mode, sizes):
    """A window with children of the given sizes, placed by the original size and place handlers.
    Every third child has no size or place handler."""
    from test_widget_util import PLACE, SIZE, set_handler
    w = task(e, 0x54, BOX)
    u32(e, w + 0x20, 20, 30, 400, 300, 6, 5, mode, 1, 1)
    for i, (cw, ch) in enumerate(sizes):
        c = task(e, 0x54, OTHER if i % 3 == 2 else CHILD)
        u32(e, c + 0x20, 0, 0, cw, ch)
        u32(e, c + 0x3C, i % 2, (i + 1) % 2)
        add_child(e, w, c)
    set_handler(e, PLACE, CHILD, 0x424420)
    set_handler(e, SIZE, CHILD, 0x424400)
    set_handler(e, PLACE, OTHER, 0)
    set_handler(e, SIZE, OTHER, 0)
    return w


SIZES = [[], [(50, 20)], [(50, 20), (60, 25), (10, 300)], [(50, 20), (60, 25), (10, 300), (3, 4)]]


@pytest.mark.parametrize("mode", range(10))
@pytest.mark.parametrize("sizes", SIZES)
def test_arrange(mode, sizes):
    run(emu, 0x4244B0, "MenuWindow_Arrange", lambda e: (arrange_tree(e, mode, sizes),))


@pytest.mark.parametrize("orig,ours", [(0x424850, "MenuWindow_GetType"), (0x424860, "MenuWindow_GetEvent910"),
                                       (0x424870, "MenuWindow_GetEvent914"), (0x424880, "MenuWindow_GetEvent918"),
                                       (0x424890, "MenuWindow_GetEvent91C"), (0x424B80, "MenuWindow_GetPlaceEvent")])
def test_getters(orig, ours):
    mask = {0x424850: 0xFFFF, 0x424B80: 0xFF}.get(orig, 0xFFFFFFFF)
    run(emu, orig, ours, lambda e: None, check_eax=True, eax_mask=mask)


BOX_XY = (100, 80, 50, 40)
POINTS = [(99, 90), (100, 90), (101, 81), (149, 119), (150, 100), (120, 80), (120, 120), (125, 100)]


@pytest.mark.parametrize("point", POINTS)
@pytest.mark.parametrize("flags", list(itertools.product([0, 1], repeat=5)))
def test_hit_test(point, flags):
    button, pressed, released, entered, held = flags

    def setup(e):
        w = task(e, 0x54, BOX)
        mm = task(e, 0x2898, MENU)
        u32(e, w + 0x44, mm)
        e.write(w + 0x48, bytes([0xAA, held, held, 0xAA]))
        input_state(e, w, button, pressed, released, entered, 0, *point)
        return (w,) + BOX_XY + (e.alloc(4), e.alloc(4))
    run(emu, 0x4248A0, "MenuWindow_HitTest", setup)


@pytest.mark.parametrize("hover", [0, 1])
def test_hit_test_no_out(hover):
    def setup(e):
        w = task(e, 0x54, BOX)
        mm = task(e, 0x2898, MENU)
        u32(e, w + 0x44, mm)
        e.write(w + 0x49, bytes([hover]))
        input_state(e, w, 0, 0, 0, 0, 0, 120, 100)
        return (w,) + BOX_XY + (0, 0)
    run(emu, 0x4248A0, "MenuWindow_HitTest", setup)


@pytest.mark.parametrize("alpha", [0.0, 0.5, 0.99, 0.991, 1.0])
@pytest.mark.parametrize("fill", [0.0, 0.3])
@pytest.mark.parametrize("focused", [0, 1, 7])
def test_draw_end(alpha, fill, focused):
    def setup(e):
        w = task(e, 0x54, BOX)
        u32(e, w + 0x20, 33, 44, 155, 66)
        a = e.alloc(8)
        f32(e, a, fill, alpha)
        return w, e.u32(a), e.u32(a + 4), focused
    run(emu, 0x424980, "MenuWindow_DrawEnd", setup)


def test_new():
    from test_menu_util import logfake
    e = make_emu()
    logfake(e, 0x4012A0, 3, masks={1: 0xFFFF}, effect=lambda e, a: 0x4444)
    e.baseline = e.snapshot()
    run(e, 0x424B60, "MenuWindow_New", lambda e: (0x1111, 0x2222), check_eax=True)


def test_shutdown():
    from test_menu_util import logfake
    e = make_emu()
    logfake(e, 0x4018E0, 1)
    logfake(e, 0x4018C0, 1, masks={0: 0xFF})
    e.baseline = e.snapshot()
    shutdown_test(e, 0x424790, "MenuWindow_Shutdown", 0x53B90C)


init, init_setup = init_emu()


def test_init():
    init_compare(init, 0x423FC0, "MenuWindow_Init", init_setup)

