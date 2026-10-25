import struct

import pytest

from emu import Emu, ErrorShown
from test_win_util import LOG, api, check, check_raises, cstr, pyfake, trap, u32, logged

emu = Emu()
APP = 0x5C8BC0
COUNTRY = APP + 0x60

api(emu, "SetWindowTextA", 2, 1)


def locale(name):
    def before(e):
        e.write(e.arg(2), name + b"\0")
    return before


@pytest.mark.parametrize("country,preset", [(b"Germany", b""), (b"germany", b""), (b"France", b""), (b"", b"USA")])
def test_appcontext_open(country, preset):
    def setup(e):
        api(e, "GetLocaleInfoA", 4, 8, before=locale(country))
        e.write(COUNTRY, preset + b"\0")
        u32(e, APP + 0x58, 0x1234)
    check(emu, 0x48F020, "AppContext_Open", setup=setup, check_eax=False)


def test_appcontext_close():
    check(emu, 0x48F070, "AppContext_Close", setup=lambda e: u32(e, APP + 0x58, 0x1234), check_eax=False)


GETTERS = [
    (0x48F080, "AppContext_GetCmdLine", 0x08), (0x48F090, "AppContext_GethInstance", 0x00),
    (0x48F0A0, "AppContext_GethWnd", 0x04), (0x48F0B0, "AppContext_GetName", 0x14),
    (0x48F130, "AppContext_IsFullscreen", 0x40), (0x48F170, "AppContext_IsActive", 0x3C),
    (0x48F1B0, "AppContext_IsDebugged", 0x38), (0x48F1D0, "AppContext_GetCompileScripts", 0x2C),
    (0x48F1F0, "AppContext_GetDebugText", 0x34), (0x48F220, "AppContext_GetPlayIntro", 0x48),
    (0x48F230, "AppContext_GetLightmapsUsed", 0x24), (0x48F250, "AppContext_GetTerrainLightmapsUsed", 0x28),
    (0x48F270, "AppContext_GetWinInput", 0x20), (0x48F2C0, "AppContext_IsGerman", 0x1C),
    (0x48F340, "AppContext_GetQvmOnly", 0x30),
]


@pytest.mark.parametrize("addr,name,off", GETTERS)
def test_appcontext_getters(addr, name, off):
    check(emu, addr, name, setup=lambda e: u32(e, APP + off, 0x11223344 + off))


SETTERS = [
    0x48F0C0, 0x48F0D0, 0x48F0E0, 0x48F0F0, 0x48F120, 0x48F1A0, 0x48F1C0, 0x48F1E0, 0x48F200,
    0x48F210, 0x48F240, 0x48F260, 0x48F280, 0x48F290, 0x48F2A0, 0x48F2B0, 0x48F330,
]
SETTER_NAMES = [
    "AppContext_SetCmdLine", "AppContext_SethInstance", "AppContext_SethWnd", "AppContext_SetFileSysCfg",
    "AppContext_SetFullscreen", "AppContext_SetDebugged", "AppContext_SetCompileScripts",
    "AppContext_SetDebugText", "AppContext_SetUnused44", "AppContext_SetPlayIntro", "AppContext_SetLightmapsUsed",
    "AppContext_SetTerrainLightmapsUsed", "AppContext_SetWinInput", "AppContext_SetUnused4C", "AppContext_SetUnused50",
    "AppContext_SetGerman", "AppContext_SetQvmOnly",
]


@pytest.mark.parametrize("addr,name", zip(SETTERS, SETTER_NAMES))
@pytest.mark.parametrize("value", [0, 1, 0x12345680, 0xFF])
def test_appcontext_setters(addr, name, value):
    check(emu, addr, name, args=(value,), check_eax=False)


@pytest.mark.parametrize("hwnd", [0, 0x5555])
def test_appcontext_set_name(hwnd):
    def setup(e):
        u32(e, APP + 4, hwnd)
        return (cstr(e, b"Project IGI"),)
    check(emu, 0x48F100, "AppContext_SetName", setup=setup, check_eax=False)


@pytest.mark.parametrize("cb", [False, True])
@pytest.mark.parametrize("value", [0, 1, 0x1FF])
def test_appcontext_set_active(cb, value):
    def setup(e):
        if cb:
            u32(e, APP + 0x58, trap(e, "active_cb", logged("active_cb", 2, pop=0)))
            u32(e, APP + 0x5C, 0x77)
        return (value,)
    check(emu, 0x48F140, "AppContext_SetActive", setup=setup, check_eax=False)


def test_appcontext_set_active_callback():
    check(emu, 0x48F180, "AppContext_SetActiveCallback", args=(0x401234, 9), check_eax=False)


@pytest.mark.parametrize("country", [b"USA", b"usa", b"Germany", b""])
def test_appcontext_country(country):
    check(emu, 0x48F2D0, "AppContext_IsUSA", setup=lambda e: e.write(COUNTRY, country + b"\0"))
    check(emu, 0x48F2F0, "AppContext_GetCountry")
    check(emu, 0x48F300, "AppContext_SetCountry", setup=lambda e: (cstr(e, country),), check_eax=False)
