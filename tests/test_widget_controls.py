import struct

import pytest

from emu import compare
from test_menu_util import logfake
from test_widget_util import BOX, CHILD, OTHER, RET, add_child, f32, input_state, make_emu, run, task, u32, window

ACTIONS = 27
KEYS = 0x60000100
KEY_COUNT = 30

emu = make_emu()
logfake(emu, 0x4061C0, 0, ret=KEYS)  # Profile_GetKeys
logfake(emu, 0x4061E0, 0, ret=KEY_COUNT)  # Profile_GetKeyCount
logfake(emu, 0x48A8C0, 0)  # Keys_Clear
logfake(emu, 0x4B8A20, 2)  # Script_GetArgInt
emu.uc.mem_map(0x60000000, 0x1000)
emu.writable.append((0x60000000, 0x1000))
emu.baseline = emu.snapshot()


def config_actions(e):
    return [e.u32(0x536558 + 0x24 * i + 0x20) for i in range(ACTIONS)]


def profile_keys(e, missing=()):
    """Profile bindings in reverse action order, each to keyboard key 2 + i (key 0x80 + i for joystick ones)."""
    actions = config_actions(e)
    for j in range(KEY_COUNT):
        i = ACTIONS - 1 - j
        if i < 0 or i in missing:
            u32(e, KEYS + 12 * j, 0x7FFF, 1, 2)
        else:
            u32(e, KEYS + 12 * j, actions[i], 3 if i % 5 == 0 else 1, 2 + i)


def menu(e, texts=True, offset=0, selected=-1, waiting=0, reset=0, bindings=None):
    cm = window(e, 0x364, x=150, y=100, w=360, h=200)
    if texts:
        for i in range(ACTIONS):
            for base in (0x54, 0x154):
                t = e.alloc(0x28)
                u32(e, t + 0x24, 0x100 + i)
                u32(e, cm + base + 4 * i, t)
    for i, b in enumerate(bindings or range(ACTIONS)):
        u32(e, cm + 0x254 + 4 * i, b)
    u32(e, cm + 0x354, offset)
    e.write(cm + 0x358, bytes([0, waiting, reset, 0]))
    u32(e, cm + 0x35C, selected)
    return cm


def test_create():
    def setup(e):
        cm = menu(e)
        e.write(cm + 0x54, b"\x55" * 0x300)
        return cm, 0x1234
    run(emu, 0x4232B0, "ControlsMenu_Create", setup)


@pytest.mark.parametrize("texts", [0, 1])
def test_delete(texts):
    run(emu, 0x423350, "ControlsMenu_Delete", lambda e: (menu(e, texts),))


@pytest.mark.parametrize("texts", [0, 1])
@pytest.mark.parametrize("has_menu,font", [(1, 0x5555), (1, 0), (0, 0)])
def test_changed(texts, has_menu, font):
    def setup(e):
        RET[0x418E40] = font
        cm = menu(e, texts)
        if not has_menu:
            u32(e, cm + 0x44, 0)
        return cm, 0x4321
    run(emu, 0x4233D0, "ControlsMenu_Changed", setup)


@pytest.mark.parametrize("texts", [0, 1])
@pytest.mark.parametrize("key", [5, -1, 126])
def test_set_binding(texts, key):
    run(emu, 0x423770, "ControlsMenu_SetBinding", lambda e: (menu(e, texts), 3, key))


@pytest.mark.parametrize("value", [0, 5, 13, 126, -1])
def test_get_key_value(value):
    run(emu, 0x423080, "ControlsMenu_GetKeyValue", lambda e: (value,), check_eax=True)


@pytest.mark.parametrize("action", [0, 3, 26])
@pytest.mark.parametrize("missing", [False, True])
def test_find_binding(action, missing):
    def setup(e):
        profile_keys(e, missing=(action,) if missing else ())
        return (action,)
    run(emu, 0x423810, "ControlsMenu_FindBinding", setup, check_eax=True)


@pytest.mark.parametrize("device,key", [(1, 2), (1, 0x1E), (3, 0), (3, 7), (2, 1), (1, 0x999), (0, 0)])
def test_find_key(device, key):
    run(emu, 0x423860, "ControlsMenu_FindKey", lambda e: (device, key), check_eax=True)


def test_load_defaults():
    def setup(e):
        profile_keys(e)
        return (menu(e),)
    run(emu, 0x4237C0, "ControlsMenu_LoadDefaults", setup)


@pytest.mark.parametrize("n", [0, 3])
def test_load(n):
    def setup(e):
        profile_keys(e, missing=(4,))
        cm = menu(e)
        for i in range(n):
            add_child(e, cm, task(e, 0x54, (CHILD, OTHER, BOX)[i % 3]))
        return (cm,)
    run(emu, 0x423DB0, "ControlsMenu_Load", setup)


@pytest.mark.parametrize("n", [0, 3])
def test_save(n):
    def setup(e):
        profile_keys(e)
        cm = menu(e, bindings=[(7 * i) % 127 for i in range(ACTIONS)])
        for i in range(n):
            add_child(e, cm, task(e, 0x54, (CHILD, OTHER, BOX)[i % 3]))
        return (cm,)
    run(emu, 0x423EB0, "ControlsMenu_Save", setup)


@pytest.mark.parametrize("orig,ours", [(0x423CB0, "ControlsMenu_Activate"), (0x423D30, "ControlsMenu_Deactivate")])
@pytest.mark.parametrize("registered", [0, 1])
def test_activate(orig, ours, registered):
    def setup(e):
        cm = menu(e)
        e.write(cm + 0x358, bytes([registered]))
        return (cm,)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("fade", [0, 9, 16])
@pytest.mark.parametrize("offset", [0, 5, 17])
def test_draw(fade, offset):
    def setup(e):
        cm = menu(e, offset=offset)
        u32(e, e.u32(cm + 0x44) + 0xD0, fade)
        u32(e, cm + 0x54 + 4 * (offset + 2), 0)
        u32(e, cm + 0x154 + 4 * (offset + 5), 0)
        return (cm,)
    run(emu, 0x4238B0, "ControlsMenu_Draw", setup)


@pytest.mark.parametrize("alpha", [0.0, 0.3, 1.0])
@pytest.mark.parametrize("box", [(150, 100, 360, 200), (-7, 3, 1, 0), (1, 2, 333, 77)])
def test_draw_frame(alpha, box):
    def setup(e):
        cm = menu(e)
        u32(e, cm + 0x20, *box)
        a = e.alloc(4)
        f32(e, a, alpha)
        return cm, e.u32(a)
    run(emu, 0x423930, "ControlsMenu_DrawFrame", setup)


@pytest.mark.parametrize("alpha", [0.0, 0.3, 1.0])
@pytest.mark.parametrize("offset", [0, 3, -1])
def test_draw_list(alpha, offset):
    def setup(e):
        cm = menu(e, offset=offset)
        a = e.alloc(4)
        f32(e, a, alpha)
        return cm, e.u32(a)
    run(emu, 0x423AC0, "ControlsMenu_DrawList", setup)


# --- Run --------------------------------------------------------------------

def set_keys(e, down=(), joy=0):
    for k in down:
        e.write(0xC28C62 + k, b"\x80")
    u32(e, 0xC28F8C, joy)


@pytest.mark.parametrize("clicked", [0, 1])
@pytest.mark.parametrize("texts", [0, 1])
@pytest.mark.parametrize("reset", [0, 1])
def test_run_idle(clicked, texts, reset):
    def setup(e):
        profile_keys(e)
        cm = menu(e, texts, offset=2, reset=reset)
        e.write(cm + 0x48, bytes([0, 0, clicked]))
        input_state(e, cm, released=1, x=200, y=165)
        return (cm,)
    run(emu, 0x4234E0, "ControlsMenu_Run", setup)


# keyboard keys are config key indexes 1.. (value 1 << 30 | code), joystick buttons start at 0x6D
@pytest.mark.parametrize("keys,joy", [((), 0), ((1,), 0), ((0x1E,), 0), ((0x1E, 0x30), 0), ((0xDB,), 0),
                                      ((), 3), ((), 99), ((0x2A,), 5)])
@pytest.mark.parametrize("seed", [0, 1])
def test_run_waiting(keys, joy, seed):
    def setup(e):
        profile_keys(e)
        cm = menu(e, selected=4, waiting=1, bindings=[(5 * i) % 127 for i in range(ACTIONS)])
        set_keys(e, keys, joy)
        u32(e, 0x57BC48, 0x12345 + seed * 0x77777777, 0x9999, 0x31337)
        return (cm,)
    run(emu, 0x4234E0, "ControlsMenu_Run", setup)


# --- script functions ----------------------------------------------------------

OUT = 0x20F00000


@pytest.mark.parametrize("value", [0, 1, 8, 17, -3, 100])
def test_convert_float_from_offset(value):
    def setup(e):
        RET[0x4B8A20] = value
        return OUT, 0x5555, 0x6666
    run(emu, 0x4231E0, "ControlsMenu_ConvertFloatFromOffset", setup, check_eax=True,
        ignore=[(OUT + 4, OUT + 8), (OUT + 20, OUT + 24)])


@pytest.mark.parametrize("value", [0.0, 0.5, 0.123, 1.0, -0.7, 0.33333334])
def test_convert_offset_from_float(value):
    from test_widget_util import floatfake
    e = make_emu()
    floatfake(e, 0x4B8A50, 2)

    def setup(e):
        RET[0x4B8A50] = value
        return OUT, 0x5555, 0x6666
    run(e, 0x423240, "ControlsMenu_ConvertOffsetFromFloat", setup, check_eax=True,
        ignore=[(OUT + 4, OUT + 8), (OUT + 20, OUT + 24)])
