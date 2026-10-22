import struct

import pytest

from emu import Emu
from test_win_util import api, check, check_raises, com, u32

emu = Emu()

APP = 0x5C8BC0
WINPROCS = 0x5C8D28
WINPROC_LIST = 0x5C8DF0
MOUSE = 0xC28F80
MOUSE_WHEEL, MOUSE_RIGHT, MOUSE_LEFT, MOUSE_DI, MOUSE_MIDDLE, MOUSE_DEV = (
    0x5C8E08, 0x5C8E0C, 0x5C8E10, 0x5C8E14, 0x5C8E18, 0x5C8E1C)
KEY_DEV, KEY_DI, KEY_ACQUIRED = 0x5C8E24, 0x5C8E28, 0x5C8E30
KEY_BLOCKED, KEYS, KEYS_PREV, WIN_KEYS = 0xC28C60, 0xC28C62, 0xC28D62, 0xC28E80
FAIL = 0x80070057


def winprocs(e, fn=None):
    e.write(WINPROCS, b"\0" * 12 * 16)
    if fn:
        e.write(WINPROCS, struct.pack("<3I", WINPROC_LIST + 4, WINPROC_LIST, fn))
        e.write(WINPROC_LIST, struct.pack("<3I", WINPROCS, 0, WINPROCS))
    else:
        e.write(WINPROC_LIST, struct.pack("<3I", WINPROC_LIST + 4, 0, WINPROC_LIST))


class Parity:
    """compare() runs setup once for the original, then once for ours."""
    def __init__(self):
        self.n = 0

    def pick(self, orig, ours):
        self.n += 1
        return ours if self.n % 2 == 0 else orig


def seq(*values):
    """A stub return that gives the values in turn, then repeats the last."""
    left = list(values)

    def before(e):
        return left.pop(0) if len(left) > 1 else left[0]
    return before


CODES = [0x80004005, 0x80004002, 0x80004001, 0x80040110, 0x80040154, 0x80040200, 0x80040201, 0x80040202,
         0x80040203, 0x80040204, 0x80040205, 0x80040206, 0x80040207, 0x80040208, 0x80040209, 0x8004020A,
         0x80070002, 0x80070005, 0x8007000C, 0x8007000E, 0x80070015, 0x8007001E, 0x80070057, 0x80070077,
         0x800700AA, 0x8007047E, 0x80070481, 0x800704DF, 0, 1, 0x80070001, 0x8004020B]


@pytest.mark.parametrize("code", CODES)
def test_error_string(code):
    ro, rn = check(emu, 0x48FA80, "DInput_GetErrorString", args=(code,), check_eax=False)
    assert emu.read_cstr(ro[0]) == emu.read_cstr(rn[0])


def mouse_state(e):
    p = e.arg(2)
    e.write(p, struct.pack("<3i", STATE_XYZ[0], STATE_XYZ[1], STATE_XYZ[2]) + bytes([0x80, 0, 0x80, 0, 0, 0, 0, 0x80]))


STATE_XYZ = [0, 0, 0]


@pytest.mark.parametrize("results", [(0,), (FAIL, FAIL), (FAIL, 0, 0)])
@pytest.mark.parametrize("win_input", [0, 1])
@pytest.mark.parametrize("xyz,wheel", [((10, -300, 120), 2), ((0, 255, -120), -1), ((7, 7, 0), 0)])
def test_mouse_update(results, win_input, xyz, wheel):
    STATE_XYZ[:] = xyz

    def setup(e):
        states = [r for i, r in enumerate(results) if i != 1]
        acq = results[1] if len(results) > 1 else 0
        get = seq(*states)
        u32(e, MOUSE_DEV, com(e, "mdev", {9: (3, 0, lambda e: (mouse_state(e), get(e))[1]), 7: (1, acq)}))
        u32(e, APP + 0x20, win_input)
        u32(e, MOUSE_LEFT, 1)
        u32(e, MOUSE_RIGHT, 0)
        u32(e, MOUSE_MIDDLE, 1)
        u32(e, MOUSE_WHEEL, wheel)
    check(emu, 0x48FC20, "Mouse_Update", setup=setup, check_eax=False)


def test_mouse_update_error():
    def setup(e):
        get = seq(FAIL, FAIL)
        u32(e, MOUSE_DEV, com(e, "mdev", {9: (3, 0, get), 7: (1, 0)}))
    check_raises(emu, 0x48FC20, "Mouse_Update", setup=setup)


def caps(e):
    u32(e, e.arg(1) + 0x10, 5)


def test_mouse_get_caps():
    check(emu, 0x48FD90, "Mouse_GetCaps",
          setup=lambda e: u32(e, MOUSE_DEV, com(e, "mdev", {3: (2, 0, caps)})), check_eax=False)
    check_raises(emu, 0x48FD90, "Mouse_GetCaps",
                 setup=lambda e: u32(e, MOUSE_DEV, com(e, "mdev", {3: (2, FAIL)})))


def out_obj(i, obj, hr=0):
    def before(e):
        u32(e, e.arg(i), obj)
        return hr
    return before


@pytest.mark.parametrize("fail", [None, "create", "device", "format", "coop", "caps"])
def test_mouse_open(fail):
    def setup(e):
        winprocs(e)
        u32(e, APP, 0x400000)
        u32(e, APP + 4, 0x5151)
        dev = com(e, "mdev", {3: (2, FAIL if fail == "caps" else 0, caps), 11: (2, FAIL if fail == "format" else 0),
                              13: (3, FAIL if fail == "coop" else 0)})
        di = com(e, "mdi", {9: (5, 0, out_obj(3, dev, FAIL if fail == "device" else 0))})
        api(e, "DirectInputCreateEx", 5, before=out_obj(3, di, FAIL if fail == "create" else 0))
    if fail:
        check_raises(emu, 0x48FDE0, "Mouse_Open", setup=setup)
    else:
        check(emu, 0x48FDE0, "Mouse_Open", setup=setup, check_eax=False)


@pytest.mark.parametrize("msg,wparam", [(0x201, 0), (0x202, 0), (0x204, 0), (0x205, 0), (0x207, 0), (0x208, 0),
                                        (0x20A, 0x00780000), (0x20A, 0xFF880000), (0x20A, 0x01680000), (0x200, 0)])
def test_mouse_winproc(msg, wparam):
    def setup(e):
        for p in (MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE):
            u32(e, p, 7)
        u32(e, MOUSE_WHEEL, 1)
        return (0x5151, msg, wparam, 0)
    check(emu, 0x48FED0, "Mouse_WinProcCB", setup=setup)


@pytest.mark.parametrize("di,dev", [(0, 0), (1, 0), (1, 1)])
@pytest.mark.parametrize("which", ["mouse", "keyboard"])
def test_close(di, dev, which):
    parity = Parity()
    addr, name, pdi, pdev, cb = {
        "mouse": (0x48FFA0, "Mouse_Close", MOUSE_DI, MOUSE_DEV, (0x48FED0, "Mouse_WinProcCB")),
        "keyboard": (0x4902F0, "Keyboard_Close", KEY_DI, KEY_DEV, (0x490120, "Keyboard_WinProcCB"))}[which]

    def setup(e):
        winprocs(e, parity.pick(cb[0], e.symbols[cb[1]]))
        u32(e, pdi, com(e, "di", {}) if di else 0)
        u32(e, pdev, com(e, "dev", {}) if dev else 0)
    check(emu, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("fail", [None, "create", "device", "format", "coop", "acquire"])
def test_keyboard_open(fail):
    def setup(e):
        winprocs(e)
        u32(e, APP, 0x400000)
        u32(e, APP + 4, 0x5151)
        e.write(WIN_KEYS, b"\x80" * 256)
        u32(e, KEY_ACQUIRED, 0)
        dev = com(e, "kdev", {11: (2, FAIL if fail == "format" else 0), 13: (3, FAIL if fail == "coop" else 0),
                              7: (1, FAIL if fail == "acquire" else 0)})
        di = com(e, "kdi", {3: (4, 0, out_obj(2, dev, FAIL if fail == "device" else 0))})
        api(e, "DirectInputCreateA", 4, before=out_obj(2, di, FAIL if fail == "create" else 0))
    if fail and fail != "acquire":
        check_raises(emu, 0x48FFF0, "Keyboard_Open", setup=setup)
    else:
        check(emu, 0x48FFF0, "Keyboard_Open", setup=setup, check_eax=False)


VKS = [(0x11, 0x001D0001), (0x11, 0x011D0001), (0x12, 0x01380001), (0x21, 0), (0x22, 0), (0x25, 0), (0x26, 0),
       (0x27, 0), (0x28, 0), (0x41, 0x001E0001), (0x41, 0xC09E0001)]


@pytest.mark.parametrize("vk,lparam", VKS)
def test_vk_to_dik(vk, lparam):
    check(emu, 0x490190, "Keyboard_VKToDIK", args=(vk, lparam))


@pytest.mark.parametrize("msg", [0x100, 0x101, 0x104, 0x105, 0x102])
@pytest.mark.parametrize("vk,lparam", VKS[::3])
def test_keyboard_winproc(msg, vk, lparam):
    def setup(e):
        e.write(WIN_KEYS, bytes(range(256)))
        return (0x5151, msg, vk, lparam)
    check(emu, 0x490120, "Keyboard_WinProcCB", setup=setup)


def key_state(hr):
    def before(e):
        e.write(e.arg(2), bytes((i * 7) & 0xFF for i in range(256)))
        return hr(e)
    return before


@pytest.mark.parametrize("acquired,acq,get", [(1, (0,), (0,)), (0, (FAIL,), (0,)), (0, (0,), (0,)),
                                              (1, (0,), (FAIL, 0)), (1, (FAIL,), (FAIL,)), (1, (0,), (FAIL, FAIL))])
@pytest.mark.parametrize("blocked,win_input", [(0, 0), (1, 0), (0, 1)])
def test_keyboard_update(acquired, acq, get, blocked, win_input):
    def setup(e):
        u32(e, KEY_ACQUIRED, acquired)
        e.write(KEY_BLOCKED, bytes([blocked]))
        u32(e, APP + 0x20, win_input)
        e.write(KEYS, bytes(range(256)))
        e.write(KEYS_PREV, b"\x11" * 256)
        e.write(WIN_KEYS, bytes(255 - i for i in range(256)))
        u32(e, KEY_DEV, com(e, "kdev", {7: (1, 0, seq(*acq)), 9: (3, 0, key_state(seq(*get)))}))
    check(emu, 0x490230, "Keyboard_Update", setup=setup, check_eax=False)


def test_keyboard_flags():
    check(emu, 0x490340, "Keyboard_SetPresent", check_eax=False)
    for v in (0, 1, 0x101):
        check(emu, 0x490350, "Keyboard_SetBlocked", args=(v,), check_eax=False)
