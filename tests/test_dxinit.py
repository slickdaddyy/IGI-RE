import struct

import pytest

from emu import FAKE_BASE, HEAP_BASE, HEAP_SIZE, ErrorShown, compare
from test_d3d_util import DDRAW, SURFACE, LOG, D3DEmu, api, check, check_raises, com, logged, pyfake, trap, u32

emu = D3DEmu()


# --- OffscreenBuffer ------------------------------------------------------------

@pytest.mark.parametrize("w,h", [(0x230, 10), (3, 5), (0, 7)])
def test_offscreen_new(w, h):
    def setup(e):
        b = e.alloc(12)
        e.write(b, b"\x11" * 12)
        return b, w, h
    compare(emu, 0x498380, "OffscreenBuffer_New", setup=setup, check_eax=False)


@pytest.mark.parametrize("same", [0, 1])
def test_offscreen_free(same):
    def setup(e):
        b = e.alloc(12)
        p = e.call(0x4B0C60, 64, 4)[0]
        e.write(b, struct.pack("<iiI", 4, 4, p))
        e.write(0xBCAAA0, struct.pack("<iiI", 9, 9, p if same else p + 4))
        return (b,)
    compare(emu, 0x4983D0, "OffscreenBuffer_Free", setup=setup, check_eax=False)


def test_offscreen_set_target():
    def setup(e):
        b = e.alloc(12)
        e.write(b, struct.pack("<iiI", 640, -480, 0x1234))
        e.write(0xBCAAC0, b"\x55" * 16)
        return (b,)
    compare(emu, 0x498410, "OffscreenBuffer_SetTarget", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F, 0x27F])
def test_fpu_set_truncate(cw):
    cell = HEAP_BASE + HEAP_SIZE - 16
    tramps = []
    for i, target in enumerate((0x498460, emu.symbols["FPU_SetTruncate"])):
        p = FAKE_BASE + 0x40000 + 0x40 * i
        code = b"\xd9\x2d" + struct.pack("<I", cell + 4)
        code += b"\xe8" + struct.pack("<i", target - (p + len(code) + 5))
        code += b"\xd9\x3d" + struct.pack("<I", cell) + b"\xc3"
        emu.write(p, code)
        tramps.append(p)

    def setup(e):
        e.write(cell, struct.pack("<HH", 0, cw))
    compare(emu, tramps[0], tramps[1], setup=setup, check_eax=False)


# --- DirectX_GetVersion -----------------------------------------------------------

FAIL = set()
OS = {"major": 5, "platform": 2}
HDDRAW, HDINPUT = 0x1000, 0x2000


def ok(step, hr=0):
    return 0x80004005 if step in FAIL else hr


def setup_dx(e):
    ge = e

    def version(e):
        if "version" in FAIL:
            return 0
        e.write(e.arg(0) + 4, struct.pack("<I", OS["major"]))
        e.write(e.arg(0) + 0x10, struct.pack("<I", OS["platform"]))
        return (1, e.u32(e.arg(0)))

    def load(e):
        name = e.read_cstr(e.arg(0))
        if name == b"DDRAW.DLL":
            return 0 if "ddraw" in FAIL else HDDRAW
        return 0 if "dinput" in FAIL else HDINPUT

    def proc(e):
        name = e.read_cstr(e.arg(1)).decode()
        return 0 if name in FAIL else procs[name]

    def out_obj(i, obj, step):
        def fn(e):
            if step in FAIL:
                return 0x80004002
            u32(e, e.arg(i), obj)
            return 0
        return fn

    surf_qi = []

    def surface_qi(e):
        step = "qi3" if not surf_qi else "qi4"
        surf_qi.append(1)
        if step in FAIL:
            return 0x80004002
        u32(e, e.arg(2), sub)
        return 0

    sub = com(ge, "sub", SURFACE)
    surf = com(ge, "surf", SURFACE, {0: (0, surface_qi)})
    dd2 = com(ge, "dd2", DDRAW)

    def create_surface(e):
        desc = e.read(e.arg(1), 0x6C)
        if "surface" in FAIL:
            return (0x80004005, desc)
        u32(e, e.arg(2), surf)
        return (0, desc)

    dd = com(ge, "dd", DDRAW, {0: (0, out_obj(2, dd2, "qi2")), 20: (0, lambda e: ok("coop")),
                               6: (0, create_surface)})
    dd7 = com(ge, "dd7", DDRAW)
    music = com(ge, "music", SURFACE)
    procs = {
        "DirectDrawCreate": trap(ge, "DirectDrawCreate", logged("DirectDrawCreate", 3,
                                                                before=out_obj(1, dd, "create"))),
        "DirectInputCreateA": 0x7777,
        "DirectDrawCreateEx": trap(ge, "DirectDrawCreateEx", logged("DirectDrawCreateEx", 4,
                                                                    before=out_obj(1, dd7, "ex"))),
    }
    api(e, "GetVersionExA", 1, before=version)
    api(e, "LoadLibraryA", 1, before=load)
    api(e, "GetProcAddress", 2, before=proc)
    api(e, "FreeLibrary", 1, ret=1)
    api(e, "OutputDebugStringA", 1)
    api(e, "CoInitialize", 1)
    api(e, "CoUninitialize", 0)
    api(e, "CoCreateInstance", 5, before=out_obj(4, music, "music"))
    out = e.alloc(8)
    e.write(out, b"\xAA" * 8)
    return out, out + 4


DX_CASES = [
    ((), 5, 2), ((), 4, 1), (("version",), 5, 2), ((), 3, 2), ((), 4, 2), (("dinput",), 4, 2),
    (("DirectInputCreateA",), 4, 2), (("ddraw",), 5, 2), (("DirectDrawCreate",), 5, 2), (("create",), 5, 2),
    (("qi2",), 5, 2), (("dinput",), 5, 2), (("DirectInputCreateA",), 4, 1), (("coop",), 5, 2),
    (("surface",), 5, 2), (("qi3",), 5, 2), (("qi4",), 5, 2), (("music",), 5, 2),
    (("DirectDrawCreateEx",), 5, 2), (("ex",), 5, 2)]


@pytest.mark.parametrize("fail,major,platform", DX_CASES)
def test_dx_get_version(fail, major, platform):
    FAIL.clear()
    FAIL.update(fail)
    OS.update(major=major, platform=platform)
    check(emu, 0x498480, "DirectX_GetVersion", setup=setup_dx, check_eax=False)


# --- error handlers ---------------------------------------------------------------

DEBUGGED = [0]


def setup_handlers(e):
    api(e, "ShowCursor", 1)
    api(e, "MessageBoxA", 4, ret=1)
    api(e, "DebugBreak", 0)


hemu = D3DEmu()
pyfake(hemu, 0x48F0A0, "GethWnd", 0, ret=0x4321)
pyfake(hemu, 0x48F0B0, "GetName", 0, ret=0x541D68)
pyfake(hemu, 0x48F1B0, "IsDebugged", 0, before=lambda e: DEBUGGED[0])


def stop_exit(e):
    if LOG:
        LOG[-1].append(("exit", e.arg(0)))
    e.import_error = ErrorShown("exit")
    e.uc.emu_stop()


_t = trap(hemu, "crt_exit", stop_exit)
hemu.write(0x4A5AD5, b"\xe9" + struct.pack("<i", _t - 0x4A5AD5 - 5))


def test_message_box():
    def setup(e):
        setup_handlers(e)
        p = e.alloc(16)
        e.write(p, b"Fatal\0")
        return (p,)
    calls = check(hemu, 0x4988D0, "App_MessageBox", setup=setup, check_eax=False)
    assert [c[0] for c in calls] == ["ShowCursor", "GetName", "GethWnd", "MessageBoxA", "ShowCursor"]


def test_set_error_handlers():
    compare(hemu, 0x498900, "App_SetErrorHandlers", normalized=True, check_eax=False)


def test_reset_error_handlers():
    def setup(e):
        e.write(0x93625C, b"\x77" * 0x24)
    compare(hemu, 0x498950, "App_ResetErrorHandlers", setup=setup, normalized=True, check_eax=False)


@pytest.mark.parametrize("debugged", [0, 1])
def test_error_exit(debugged):
    DEBUGGED[0] = debugged
    calls = check_raises(hemu, 0x498930, "App_ErrorExit", setup=setup_handlers)
    assert calls == "exit"
