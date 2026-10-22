"""MenuScreen handlers, MenuFrame, ModelViewBox and Cursor."""
import pytest

from test_menu_util import logfake
from test_widget_util import (BOX, CHILD, OTHER, RET, add_child, alloc_effect, f32, init_compare, init_emu,
                              input_state, make_emu, run, shutdown_test, task, u32, window)

emu = make_emu()
logfake(emu, 0x4028B0, 0, ret=1234)  # Timer_GetTicks
logfake(emu, 0x4C5B20, 1, effect=alloc_effect(0x100))  # World_New
logfake(emu, 0x4C5CE0, 1)  # World_Free
logfake(emu, 0x4D9870, 10, derefs={1: 24, 2: 40})  # Camera_Set
logfake(emu, 0x4E8110, 12, effect=alloc_effect(0xD0))  # View_New
logfake(emu, 0x4E8370, 2)  # View_SetWorld
logfake(emu, 0x4E8250, 2, masks={1: 0xFF})  # View_SetEnabled
logfake(emu, 0x4015F0, 1)  # Task_Delete
logfake(emu, 0x4012A0, 3, masks={1: 0xFFFF}, effect=alloc_effect(0x40))  # Task_New
logfake(emu, 0x4CEC10, 1, strings=(0,), ret=0x6666)  # VirModel_Find
logfake(emu, 0x4C5710, 3)  # RigidDynCubeObj_Init
logfake(emu, 0x4C6890, 3)  # DynCube_ListAdd
emu.baseline = emu.snapshot()


def with_children(e, w, n=3):
    for i in range(n):
        add_child(e, w, task(e, 0x54, (CHILD, OTHER, BOX)[i % 3]))
    return w


# --- MenuScreen -------------------------------------------------------------

def screen(e, registered=0, menu=True):
    s = window(e, 0x1A4)
    u32(e, s + 0x19C, 77, registered)
    if not menu:
        u32(e, s + 0x44, 0)
    return s


@pytest.mark.parametrize("menu", [0, 1])
@pytest.mark.parametrize("back", [0, 1])
@pytest.mark.parametrize("has", [0, 1])
def test_screen_run(menu, back, has):
    def setup(e):
        RET[0x4F16C0] = has
        s = screen(e, menu=menu)
        if menu:
            input_state(e, s, back=back)
        return (s,)
    run(emu, 0x422110, "MenuScreen_Run", setup)


@pytest.mark.parametrize("orig,ours", [(0x422190, "MenuScreen_Activate"), (0x422200, "MenuScreen_Deactivate")])
@pytest.mark.parametrize("registered", [0, 1])
def test_screen_activate(orig, ours, registered):
    run(emu, orig, ours, lambda e: (screen(e, registered),))


@pytest.mark.parametrize("orig,ours", [(0x422260, "MenuScreen_Open"), (0x422300, "MenuScreen_Apply")])
@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("n", [0, 3])
def test_screen_events(orig, ours, has, n):
    def setup(e):
        RET[0x4F16C0] = has
        return (with_children(e, screen(e), n),)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("orig,ours,addr", [(0x422180, "MenuScreen_GetParams", 0x57BC04),
                                            (0x422B00, "MenuFrame_GetParams", 0x57BC10),
                                            (0x422FB0, "ModelViewBox_GetParams", 0x57BC28),
                                            (0x423CA0, "ControlsMenu_GetParams", 0x57BC40)])
def test_get_params(orig, ours, addr):
    def setup(e):
        u32(e, addr, 0x9876)
        return 0, e.alloc(4)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("orig,ours,type_,params", [
    (0x4223A0, "MenuScreen_Shutdown", 0x53B800, 0x57BC04), (0x422B10, "MenuFrame_Shutdown", 0x53B838, 0x57BC10),
    (0x423050, "ModelViewBox_Shutdown", 0x53B854, 0x57BC28)])
def test_shutdown(orig, ours, type_, params):
    shutdown_test(emu, orig, ours, type_, params)


def test_controls_shutdown():
    shutdown_test(emu, 0x423F90, "ControlsMenu_Shutdown", 0x53B874)


# --- MenuFrame --------------------------------------------------------------

SPRITE_SIZES = [(8, 6), (5, 4), (9, 7), (3, 11), (10, 12), (6, 5), (7, 8), (4, 9)]


def frame(e, sprites=0xFF, sizes=SPRITE_SIZES, w=200, h=100):
    f = window(e, 0xB4, w=w, h=h)
    e.write(f + 0x54, b"frame\0")
    for i in range(8):
        if sprites & (1 << i):
            s = e.alloc(8)
            u32(e, s, *sizes[i])
            u32(e, f + 0x94 + 4 * i, s)
    return f


def test_frame_create():
    def setup(e):
        f = frame(e)
        return f, 0x1234
    run(emu, 0x4225F0, "MenuFrame_Create", setup)


@pytest.mark.parametrize("menu", [0, 1])
def test_frame_changed(menu):
    def setup(e):
        RET[0x418DE0] = 0x3333
        f = frame(e)
        if not menu:
            u32(e, f + 0x44, 0)
        return f, 0x4321
    run(emu, 0x422640, "MenuFrame_Changed", setup)


@pytest.mark.parametrize("sprites", [0xFF, 0x00, 0xF8, 0x05, 0xFA, 0x7F, 0x77])
@pytest.mark.parametrize("w,h", [(200, 100), (10, 10), (0, 0), (37, 53)])
def test_frame_draw(sprites, w, h):
    run(emu, 0x4226D0, "MenuFrame_Draw", lambda e: (frame(e, sprites, w=w, h=h),))


@pytest.mark.parametrize("sprites", [0xFF, 0x01, 0x02, 0x04, 0x06])
@pytest.mark.parametrize("cols", [(0, 1, 2, 0), (6, 5, 4, 1), (2, 1, 0, 1)])
@pytest.mark.parametrize("w", [0, 7, 100, -20])
def test_frame_draw_row(sprites, cols, w):
    def setup(e):
        return (frame(e, sprites), 15, 25, w) + cols
    run(emu, 0x4227A0, "MenuFrame_DrawRow", setup, check_eax=True)


@pytest.mark.parametrize("index,right", [(7, 0), (3, 1), (0, 1)])
@pytest.mark.parametrize("h", [0, 5, 61, -9])
@pytest.mark.parametrize("sprites", [0xFF, 0x00])
def test_frame_draw_column(index, right, h, sprites):
    run(emu, 0x4229A0, "MenuFrame_DrawColumn", lambda e: (frame(e, sprites), 15, 25, h, index, right))


@pytest.mark.parametrize("selectable", [0, 1])
@pytest.mark.parametrize("hover,was", [(0, 0), (1, 0), (1, 1)])
@pytest.mark.parametrize("n", [0, 3])
def test_frame_run(selectable, hover, was, n):
    def setup(e):
        f = with_children(e, frame(e), n)
        e.write(f + 0x48, bytes([0, 0, 0, 0, selectable, 1]))
        u32(e, f + 0x50, -7)
        point = (100, 100) if hover else (5, 5)
        input_state(e, f, entered=was, x=point[0], y=point[1])
        return (f,)
    run(emu, 0x422A50, "MenuFrame_Run", setup)


# --- ModelViewBox -------------------------------------------------------------

def viewbox(e, on=0):
    v = window(e, 0xC8)
    u32(e, e.u32(v + 0x44) + 0x14, 0x5150)
    e.write(v + 0x54, bytes([on]))
    u32(e, v + 0x58, 0x1110, 0x2220)
    u32(e, v + 0xC0, 0x3330, 0x4440)
    return v


def test_viewbox_create():
    def setup(e):
        u32(e, 0xA5EA60, 42)
        return viewbox(e), 0x1234
    run(emu, 0x422C90, "ModelViewBox_Create", setup)


def test_viewbox_delete():
    run(emu, 0x422DF0, "ModelViewBox_Delete", lambda e: (viewbox(e),))


def test_viewbox_changed():
    run(emu, 0x422E40, "ModelViewBox_Changed", lambda e: (viewbox(e), 0x4321))


def test_viewbox_run():
    run(emu, 0x422F10, "ModelViewBox_Run", lambda e: (viewbox(e),))


@pytest.mark.parametrize("fade", [0, 8, 9, 13, 16])
def test_viewbox_draw(fade):
    def setup(e):
        v = viewbox(e)
        u32(e, e.u32(v + 0x44) + 0xD0, fade)
        return (v,)
    run(emu, 0x422F30, "ModelViewBox_Draw", setup)


@pytest.mark.parametrize("orig,ours", [(0x422FC0, "ModelViewBox_Activate"), (0x423000, "ModelViewBox_Deactivate")])
@pytest.mark.parametrize("on", [0, 1])
def test_viewbox_activate(orig, ours, on):
    run(emu, orig, ours, lambda e: (viewbox(e, on),))


# --- Cursor -------------------------------------------------------------------

def test_cursor_create():
    def setup(e):
        c = task(e, 0x40)
        e.write(c + 0x20, b"\xAA" * 0x20)
        return c, 0x5678
    run(emu, 0x424C10, "Cursor_CreateHandler", setup)


@pytest.mark.parametrize("sprite", [0, 0x777])
def test_cursor_draw(sprite):
    def setup(e):
        c = task(e, 0x40)
        u32(e, c + 0x24, 300, 200, -2, 3, sprite)
        return (c,)
    run(emu, 0x424C40, "Cursor_Draw", setup)


# --- type registration ----------------------------------------------------------

init, init_setup = init_emu()


def init_with_window(e):
    init_setup(e)
    e.call(0x423FC0)  # MenuWindow


@pytest.mark.parametrize("orig,ours", [(0x4223D0, "MenuFrame_Init"), (0x422B40, "ModelViewBox_Init"),
                                       (0x4230A0, "ControlsMenu_Init")])
def test_init(orig, ours):
    init_compare(init, orig, ours, init_with_window)


def test_cursor_open():
    logfake(init, 0x48F490, 1)  # AppMain_AddWinProcCallback
    init.baseline = init.snapshot()
    init_compare(init, 0x424B90, "Cursor_Open", init_with_window)
