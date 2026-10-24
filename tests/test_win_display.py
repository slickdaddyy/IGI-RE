import struct

import pytest

from emu import Emu
from test_win_util import LOG, api, check, check_raises, com, cstr, logged, pyfake, trap, u32

emu = Emu()

APP = 0x5C8BC0
DISPLAY_OPEN = 0x5CA02C
MODE_SET = 0x5CA02D
DEVICE = 0x5C9FA8
MODES = 0xBCAE20
MODE_COUNT = 0xC10384
MODE = 0xC28B40
POOL = 0xC28A40
SETMODE_CBS = 0xBCADEC
FRAME_CBS = 0xBCAE00
ENDFRAME_CBS = 0xBCADFC
METHODS = 0x542310
RENDER_FUNCS = 0xA94E84
FOUND = 0xBCACF4

for name, addr, n in [("RenderMode_ForEach", 0x4B0EE0, 2), ("RenderMode_Call", 0x4B1000, 3),
                      ("RenderMode_Select", 0x4B0F30, 1), ("Direct3D_Init", 0x493580, 0),
                      ("Offscreen_Init", 0x498080, 0), ("Direct3D_Shutdown", 0x495060, 0), ("Offscreen_Shutdown", 0x498350, 0),
                      ("App_MessageBox", 0x4988D0, 1), ("Frame_Reset", 0x4977F0, 0), ("Scene_DrawAll", 0x497700, 0),
                      ("Display_RemoveCallback", 0x495220, 2), ("Guid_ToString", 0x492370, 5)]:
    before = None
    if name == "RenderMode_ForEach":
        before = lambda e: (0, (e.u32(e.arg(1)), e.u32(e.arg(1) + 4)))
    elif name == "App_MessageBox":
        before = lambda e: (0, e.read_cstr(e.arg(0)))
    pyfake(emu, addr, name, n, before=before)


def metrics(e):
    return {0: 1024, 1: 768}.get(e.arg(0), 5)


def adjust(e):
    p = e.arg(0)
    l, t, r, b = struct.unpack("<4i", e.read(p, 16))
    e.write(p, struct.pack("<4i", l - 3, t - 20, r + 3, b + 3))
    return (1, (l, t, r, b))


api(emu, "GetSystemMetrics", 1, before=metrics)
api(emu, "GetWindowLongA", 2, 0x14CF0000)
api(emu, "AdjustWindowRect", 3, before=adjust)
api(emu, "SetWindowPos", 7, 1)
api(emu, "UpdateWindow", 1, 1)
api(emu, "SetFocus", 1, 0)


def render_funcs(e, enum=True):
    """Method ids 1..10 and a logged function for each slot of the current render mode."""
    for i in range(10):
        u32(e, METHODS + 4 * i, i + 1)
    u32(e, METHODS + 0x28, 12)
    u32(e, METHODS + 0x2C, 13)
    u32(e, METHODS + 0x30, 14)

    def prim(e):
        p = e.arg(0)
        return (0, [e.u32(p + 4), e.u32(p + 12)] if p >= 0x100000 else p)
    for i in range(16):
        u32(e, RENDER_FUNCS + 4 * i, trap(e, "rf%d" % i, logged("rf%d" % i, 1, pop=0, before=prim)))
    if not enum:
        u32(e, RENDER_FUNCS + 4 * 10, 0)


def mode(e, p, w=640, h=480, depth=16, rm=2, unk16=1, device=b"dev"):
    e.write(p, b"\0" * 0x11C)
    e.write(p, struct.pack("<5i", 0, w, h, 1, depth) + bytes([0, 0, unk16, 0]) + struct.pack("<i", rm))
    e.write(p + 0x1C, device + b"\0")
    e.write(p + 0x9C, b"desc\0")


def test_set_preferred_device():
    check(emu, 0x491610, "Display_SetPreferredDevice", args=(0x601000, 0x602000), check_eax=False)


@pytest.mark.parametrize("used", [0, 1, 5, 31])
@pytest.mark.parametrize("addr,name", [(0x491630, "Display_AddSetModeCallback"),
                                       (0x4916C0, "Display_AddFrameCallback")])
def test_add_callback(addr, name, used):
    def setup(e):
        for i in range(used):
            u32(e, POOL + 8 * i + 4, 0x401000 + i)
        u32(e, SETMODE_CBS, 0x1234)
        u32(e, FRAME_CBS, 0x5678)
        return (0x402000,)
    check(emu, addr, name, setup=setup, check_eax=False)
    check(emu, 0x491650, "Display_AddCallback", setup=lambda e: (setup(e)[0], FRAME_CBS), check_eax=False)


def test_add_callback_full():
    def setup(e):
        for i in range(32):
            u32(e, POOL + 8 * i + 4, 0x401000 + i)
        return (0x402000, SETMODE_CBS)
    assert "No more" in check_raises(emu, 0x491650, "Display_AddCallback", setup=setup)


@pytest.mark.parametrize("addr,name", [(0x4916A0, "Display_RemoveSetModeCallback"),
                                       (0x4916E0, "Display_RemoveFrameCallback")])
def test_remove_callback(addr, name):
    check(emu, addr, name, args=(0x402000,), check_eax=False)


@pytest.mark.parametrize("opened", [0, 1])
def test_open(opened):
    def setup(e):
        render_funcs(e)
        e.write(DISPLAY_OPEN, bytes([opened]))
        e.write(MODE_SET, b"\1")
        e.write(DEVICE, b"old\0")
        u32(e, MODE_COUNT, 3)
        for i in range(32):
            u32(e, POOL + 8 * i + 4, 0x401000 + i)
    if opened:
        check_raises(emu, 0x491700, "Display_Open", setup=setup)
    else:
        check(emu, 0x491700, "Display_Open", setup=setup, check_eax=False)


def test_clear_callbacks():
    def setup(e):
        for i in range(32):
            u32(e, POOL + 8 * i, 0x501000 + i)
            u32(e, POOL + 8 * i + 4, 0x401000 + i)
        for p in (SETMODE_CBS, FRAME_CBS, ENDFRAME_CBS):
            u32(e, p, 0x1234)
    check(emu, 0x491860, "Display_ClearCallbacks", setup=setup, check_eax=False)


def set_state(e, opened, mode_set, fullscreen=0):
    render_funcs(e)
    e.write(DISPLAY_OPEN, bytes([opened]))
    e.write(MODE_SET, bytes([mode_set]))
    u32(e, APP + 0x40, fullscreen)
    u32(e, APP + 4, 0x7788)
    mode(e, MODE, 800, 600, 32, 3, 1, b"cur")


@pytest.mark.parametrize("opened,mode_set", [(0, 0), (1, 0), (1, 1)])
def test_error_handlers(opened, mode_set):
    def setup(e):
        set_state(e, opened, mode_set)
        return (cstr(e, b"Something broke"),)
    check(emu, 0x491890, "Display_WarningHandler", setup=setup, check_eax=False)
    check(emu, 0x4918F0, "Display_ErrorHandler", setup=setup, check_eax=False)


def test_call_method_cb():
    def setup(e):
        info = e.alloc(8)
        u32(e, info, 3)
        args = e.alloc(8)
        u32(e, args, 7)
        u32(e, args + 4, 0x99)
        return (info, args)
    check(emu, 0x491910, "Display_CallMethodCB", setup=setup, check_eax=False)


@pytest.mark.parametrize("mode_set", [0, 1])
def test_close(mode_set):
    check(emu, 0x491930, "Display_Close", setup=lambda e: set_state(e, 1, mode_set), check_eax=False)


@pytest.mark.parametrize("flags", list(range(16)) + [0x108, 0xFF00])
@pytest.mark.parametrize("with_mode", [False, True])
@pytest.mark.parametrize("with_fn", [False, True])
def test_find_modes(flags, with_mode, with_fn):
    def setup(e):
        specs = [(640, 480, 16, 1), (640, 480, 32, 1), (800, 600, 16, 2), (640, 600, 16, 1), (640, 480, 16, 2)]
        for i, (w, h, d, rm) in enumerate(specs):
            mode(e, MODES + 0x11C * i, w, h, d, rm)
        u32(e, MODE_COUNT, len(specs))
        m = 0
        if with_mode:
            m = e.alloc(0x11C)
            mode(e, m, 640, 480, 16, 1)
        fn = trap(e, "found", logged("found", 1, pop=0)) if with_fn else 0
        return (m, flags, fn)
    check(emu, 0x491980, "Display_FindModes", setup=setup)


def test_find_modes_none():
    check(emu, 0x491980, "Display_FindModes", setup=lambda e: (u32(e, MODE_COUNT, 0), (0, 0, 0))[1])


@pytest.mark.parametrize("mode_set", [0, 1])
@pytest.mark.parametrize("addr,name", [(0x491A30, "Display_Clear"), (0x491CC0, "Display_SwapBuffers"),
                                       (0x491D00, "Display_NextFrame"), (0x491D20, "Display_EndFrame")])
def test_frame(addr, name, mode_set):
    def setup(e):
        set_state(e, 1, mode_set)
        u32(e, 0xC10380, 41)
        node = e.alloc(16)
        u32(e, node, node + 8)
        u32(e, node + 4, trap(e, "cb1", logged("cb1", 0, pop=0)))
        u32(e, node + 8, 0)
        u32(e, node + 12, trap(e, "cb2", logged("cb2", 0, pop=0)))
        u32(e, FRAME_CBS, node)
        u32(e, ENDFRAME_CBS, node + 8)
    check(emu, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("mode_set", [0, 1])
def test_unset_mode(mode_set):
    check(emu, 0x491A50, "Display_UnsetMode", setup=lambda e: (set_state(e, 1, mode_set), (0x1234,))[1],
          check_eax=False)


@pytest.mark.parametrize("change", [None, "w", "h", "depth", "rm", "unk16", "device", "empty"])
@pytest.mark.parametrize("mode_set,fullscreen", [(0, 0), (0, 1), (1, 0)])
def test_set_mode(change, mode_set, fullscreen):
    def setup(e):
        set_state(e, 1, mode_set, fullscreen)
        e.write(DEVICE, b"cur\0")
        node = e.alloc(8)
        u32(e, node, 0)
        u32(e, node + 4, trap(e, "pre", logged("pre", 0, pop=0)))
        u32(e, SETMODE_CBS, node)
        kw = dict(w=800, h=600, depth=32, rm=3, unk16=1, device=b"cur")
        if change == "w":
            kw["w"] = 1024
        elif change == "h":
            kw["h"] = 768
        elif change == "depth":
            kw["depth"] = 16
        elif change == "rm":
            kw["rm"] = 1
        elif change == "unk16":
            kw["unk16"] = 0
        elif change == "device":
            kw["device"] = b"other"
        elif change == "empty":
            kw["device"] = b""
        m = e.alloc(0x11C)
        mode(e, m, **kw)
        return (m,)
    check(emu, 0x491A90, "Display_SetMode", setup=setup, check_eax=False)


def test_run_callbacks():
    def setup(e):
        nodes = e.alloc(24)
        for i in range(3):
            u32(e, nodes + 8 * i, nodes + 8 * (i + 1) if i < 2 else 0)
            u32(e, nodes + 8 * i + 4, trap(e, "c%d" % i, logged("c%d" % i, 0, pop=0)))
        return (nodes,)
    check(emu, 0x491CA0, "Display_RunCallbacks", setup=setup, check_eax=False)
    check(emu, 0x491CA0, "Display_RunCallbacks", args=(0,), check_eax=False)


def test_get_mode():
    check(emu, 0x491CF0, "Display_GetMode")


@pytest.mark.parametrize("w,h", [(640, 480), (800, 600), (1024, 768), (1280, 1024), (7, 3)])
def test_get_aspect(w, h):
    check(emu, 0x491D40, "Display_GetAspect", setup=lambda e: mode(e, MODE, w, h), check_eax=False,
          check_st0=True)


def test_draw_primitive():
    def setup(e):
        render_funcs(e)
        p = e.alloc(16)
        e.write(p, struct.pack("<4I", 0, 5, 0, 0x4444))
        return (p,)
    check(emu, 0x491D70, "Display_DrawPrimitive", setup=setup, check_eax=False)
    check(emu, 0x491D80, "Display_Snapshot", setup=lambda e: (render_funcs(e), (0x5555,))[1], check_eax=False)


GETTERS = [(0x491DB0, "Display_GetClipWindowPrim"), (0x491DC0, "Display_GetBlitPrim"),
           (0x491DD0, "Display_GetSnapshotPrim"), (0x491DE0, "Display_GetGFXCapsMethod"),
           (0x491DF0, "Display_GetOpenMethod"), (0x491E00, "Display_GetCloseMethod"),
           (0x491E10, "Display_GetSetModeMethod"), (0x491E20, "Display_GetUnsetModeMethod"),
           (0x491E30, "Display_GetVSyncMethod"), (0x491E40, "Display_GetSwapBuffersMethod"),
           (0x491E50, "Display_GetClearMethod"), (0x491E60, "Display_GetEnumMethod")]


@pytest.mark.parametrize("addr,name", GETTERS)
def test_getters(addr, name):
    check(emu, addr, name, setup=render_funcs)


def test_set_clear_color():
    check(emu, 0x491E70, "Display_SetClearColor", args=(0x1FF, 0x80, 0xFFFFFF01), check_eax=False)


@pytest.mark.parametrize("enum", [False, True])
def test_enum_modes(enum):
    def fill(e):
        p = e.arg(0)
        u32(e, e.u32(p + 4), 7)
        return (0, (e.u32(p), e.u32(p + 8)))

    def setup(e):
        render_funcs(e, enum)
        if enum:
            u32(e, RENDER_FUNCS + 40, trap(e, "enum", logged("enum", 1, pop=0, before=fill)))
        return (0x601000, 20)
    check(emu, 0x491EA0, "Display_EnumModes", setup=setup)


def test_enum_drivers():
    api(emu, "DirectDrawEnumerateExA", 3, 1)
    check(emu, 0x491EF0, "Display_EnumDrivers", args=(0x601000,))


def out_ptr(i, value):
    def before(e):
        u32(e, e.arg(i), value(e) if callable(value) else value)
    return before


def setup_driver(e, guid, list_devices, found, driver_name, fail):
    hr = lambda step: 0x88760000 + step if fail == step else 0

    def enum(e):
        u32(e, FOUND, found)
        return hr(4)

    def caps(e):
        return (hr(3), (e.u32(e.arg(1)), e.u32(e.arg(2))))
    d3d = com(e, "d3d", {3: (3, 0, enum)})
    dd = com(e, "dd", {0: (3, 0, lambda e: (out_ptr(2, d3d)(e), hr(2))[1]), 11: (3, 0, caps)})
    api(e, "DirectDrawCreateEx", 4, before=lambda e: (out_ptr(1, dd)(e), hr(1))[1])
    ctx = e.alloc(0x51C)
    count = e.alloc(4)
    u32(e, count, 1)
    drivers = e.alloc(0x4F8 * 3)
    e.write(ctx, struct.pack("<5I", list_devices, cstr(e, driver_name) if driver_name else 0, 0, drivers, count))
    e.write(ctx + 0x24, b"leftover\0")
    e.write(ctx + 0x430, b"\x55" * 0xEC)
    g = 0
    if guid:
        g = e.alloc(16)
        e.write(g, bytes(range(1, 17)))
    return (g, cstr(e, b"Primary Display Driver"), cstr(e, b"display"), ctx, 0)


def name_guid(e):
    e.write(e.arg(0), b"drvname\0")


@pytest.mark.parametrize("guid", [False, True])
@pytest.mark.parametrize("list_devices", [0, 1])
@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("driver_name", [None, b"drvname", b"other"])
def test_enum_driver_cb(guid, list_devices, found, driver_name):
    pyfake(emu, 0x492370, "Guid_ToString", 5, before=name_guid)
    check(emu, 0x491F10, "Display_EnumDriverCB",
          setup=lambda e: setup_driver(e, guid, list_devices, found, driver_name, None))


@pytest.mark.parametrize("fail,list_devices", [(1, 0), (2, 0), (3, 0), (4, 0), (4, 1)])
def test_enum_driver_cb_errors(fail, list_devices):
    pyfake(emu, 0x492370, "Guid_ToString", 5, before=name_guid)
    check_raises(emu, 0x491F10, "Display_EnumDriverCB",
                 setup=lambda e: setup_driver(e, True, list_devices, 1, None, fail))
