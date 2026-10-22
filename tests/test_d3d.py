import struct

import pytest

from emu import compare
from test_d3d_util import (CLIPPER, D3D, DDRAW, DEVICE, LOG, SURFACE, VBUF, Code, D3DEmu, api, check, check_raises,
                           com, cstr, out, pyfake, u32)

emu = D3DEmu()

HAL = 0x534A00
TNL = 0x5349E0
RGB = 0x534A10
REF = 0x5349F0

G_DDRAW, G_D3D, G_DEVICE = 0x5CA10C, 0x5CA110, 0x5CA114
G_PRIMARY, G_BACK, G_ZBUF = 0x5CA0FC, 0x5CA100, 0x5CA104
G_TNL, G_FOUNDHAL = 0xBCAD74, 0xBCACF4
APP_HWND, APP_ACTIVE, APP_FULLSCREEN = 0x5C8BC4, 0x5C8BFC, 0x5C8C00
CUR_MODE = 0xC28B40
DD_FAIL = 0x80004005
WASSTILLDRAWING = 0x8876021C


def noise(n, seed=0):
    return bytes((i * 151 + seed * 13 + 7) & 0xFF for i in range(n))


def guid(e, addr):
    return e.read(addr, 16)


def devdesc(e, g, raster=0, devcaps=0, tex=0, depths=0):
    d = bytearray(noise(0xEC, 1))
    d[0:4] = struct.pack("<I", devcaps)
    d[0x44:0x48] = struct.pack("<I", raster)
    d[0x5C:0x60] = struct.pack("<I", tex)
    d[0x74:0x78] = struct.pack("<I", depths)
    d[0xC4:0xD4] = guid(e, g) if isinstance(g, int) else g
    p = e.alloc(0xEC)
    e.write(p, bytes(d))
    return p


# --- enumeration callbacks ----------------------------------------------------

@pytest.mark.parametrize("g", [HAL, TNL, RGB])
@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("tnl", [0, 1])
def test_find_device_cb(g, found, tnl):
    def setup(e):
        u32(e, G_FOUNDHAL, found)
        u32(e, G_TNL, tnl)
        return cstr(e, b"desc"), cstr(e, b"name"), devdesc(e, g, raster=0x1FF if found else 0x100), e.alloc(0xEC)
    compare(emu, 0x492230, "Direct3D_FindDeviceCB", setup=setup)


@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("want", [None, b"HAL", b"Ramp"])
def test_list_device_cb(mode, want):
    def setup(e):
        ctx = e.alloc(0x51C)
        e.write(ctx + 0x24, noise(0x4F8, 2))
        devices = e.alloc(0x668 * 4)
        count = e.alloc(4)
        u32(e, count, 2)
        e.write(ctx, struct.pack("<7I", mode, 0, cstr(e, want) if want else 0, 0, 0, devices, count))
        return cstr(e, b"Direct3D HAL device"), cstr(e, b"name"), devdesc(e, HAL, devcaps=0x80000), ctx
    compare(emu, 0x4922B0, "Direct3D_ListDeviceCB", setup=setup)


def test_guid_to_string():
    def setup(e):
        return (e.alloc(64),) + struct.unpack("<4I", guid(e, TNL))
    compare(emu, 0x492370, "Guid_ToString", setup=setup, check_eax=False)


def mode_desc(e, w, h, bits, rgb=True):
    sd = bytearray(0x7C)
    struct.pack_into("<III", sd, 4, 0x1006, h, w)
    struct.pack_into("<II", sd, 0x48, 0x20, 0x40 if rgb else 0x20)
    struct.pack_into("<I", sd, 0x54, bits)
    p = e.alloc(0x7C)
    e.write(p, bytes(sd))
    return p


def driver(e, name=b"display", desc=b"Primary Display Driver", depths=0xD00):
    p = e.alloc(0x4F8)
    e.write(p, noise(0x4F8, 2))
    e.write(p, name + b"\0")
    e.write(p + 0x80, desc + b"\0")
    u32(e, p + 0x480, depths)
    return p


@pytest.mark.parametrize("bits", [8, 15, 16, 24, 32])
@pytest.mark.parametrize("depths", [0, 0x800, 0x400, 0x200, 0x100, 0xF00])
def test_enum_mode_cb(bits, depths):
    def setup(e):
        u32(e, 0xC10384, 3)
        u32(e, 0x5CA048, 5)
        return mode_desc(e, 800, 600, bits), driver(e, depths=depths)
    compare(emu, 0x492670, "Direct3D_EnumModeCB", setup=setup)


@pytest.mark.parametrize("count,rgb", [(1000, True), (0, False)])
def test_enum_mode_cb_skip(count, rgb):
    def setup(e):
        u32(e, 0xC10384, count)
        return mode_desc(e, 640, 480, 16, rgb), driver(e)
    compare(emu, 0x492670, "Direct3D_EnumModeCB", setup=setup)


def zformat(e, flags, depth):
    pf = bytearray(32)
    struct.pack_into("<II", pf, 0, 32, flags)
    struct.pack_into("<I", pf, 0xC, depth)
    p = e.alloc(32)
    e.write(p, bytes(pf))
    return p


@pytest.mark.parametrize("flags", [0x400, 0x4400, 0x4000, 0x440])
def test_zbuffer_caps_cb(flags):
    def setup(e):
        buf = e.alloc(256)
        e.write(buf, b"caps:\0")
        return zformat(e, flags, 24), buf
    compare(emu, 0x492B70, "Direct3D_ZBufferCapsCB", setup=setup)


@pytest.mark.parametrize("cur,flags,depth,limit", [
    (0, 0x400, 16, 32), (16, 0x400, 24, 32), (24, 0x400, 16, 32), (16, 0x4400, 24, 32),
    (16, 0x400, 32, 24), (16, 0x400, 24, 24), (0, 0x400, 16, 0xFFFFFFFF)])
def test_find_zbuffer_cb(cur, flags, depth, limit):
    def setup(e):
        best = e.alloc(36)
        e.write(best, noise(36, 3))
        u32(e, best + 0xC, cur)
        u32(e, best + 0x20, limit)
        return zformat(e, flags, depth), best
    compare(emu, 0x494690, "Direct3D_FindZBufferCB", setup=setup)


# --- error strings ------------------------------------------------------------

def test_error_string():
    codes = list(range(0x88760000, 0x88760320)) + [0, 1, 0x80004001, 0x80004002, 0x80004005, 0x8007000E, 0x80070057,
                                                    0xFFFFFFFF]
    for hr in codes:
        a = emu.read_cstr(emu.call(0x492BE0, hr)[0])
        b = emu.read_cstr(emu.call("DDraw_GetErrorString", hr)[0])
        assert a == b, hex(hr)


# --- simple device calls ------------------------------------------------------

def device(e, methods=None):
    dev = com(e, "dev", DEVICE, methods)
    u32(e, G_DEVICE, dev)
    return dev


def ddraw(e, methods=None):
    dd = com(e, "dd", DDRAW, methods)
    u32(e, G_DDRAW, dd)
    return dd


@pytest.mark.parametrize("hr", [0, DD_FAIL])
def test_vsync(hr):
    def setup(e):
        ddraw(e, {22: hr})
    if hr:
        check_raises(emu, 0x492400, "Direct3D_VSync", setup=setup)
    else:
        check(emu, 0x492400, "Direct3D_VSync", setup=setup, check_eax=False)


@pytest.mark.parametrize("hr", [0, DD_FAIL, None])
def test_clear(hr):
    def setup(e):
        e.write(0xBCADF0, struct.pack("<3I", 0x12, 0x34, 0x56))
        if hr is not None:
            device(e, {10: hr})
    if hr:
        check_raises(emu, 0x492450, "Direct3D_Clear", setup=setup)
    else:
        log = check(emu, 0x492450, "Direct3D_Clear", setup=setup, check_eax=False)
        assert len(log) == (hr == 0)


@pytest.mark.parametrize("orig,ours", [(0x493A60, "Direct3D_ClippingOn"), (0x493AC0, "Direct3D_ClippingOff")])
@pytest.mark.parametrize("hr", [0, DD_FAIL])
def test_lighting(orig, ours, hr):
    def setup(e):
        device(e, {20: hr})
    if hr:
        check_raises(emu, orig, ours, setup=setup)
    else:
        check(emu, orig, ours, setup=setup, check_eax=False)


@pytest.mark.parametrize("hr", [0, DD_FAIL])
def test_clip_window(hr):
    def setup(e):
        device(e, {27: (hr, lambda e: (None, e.read(e.arg(1), 32)))})
        r = e.alloc(28)
        e.write(r, struct.pack("<3I4f", 1, 2, 3, 10.5, 20.25, 640.0, 480.0))
        e.write(0xBCADA0, struct.pack("<f", 0.4096))
        e.write(0xBCADC8, struct.pack("<f", 409600.03))
        return (r,)
    if hr:
        check_raises(emu, 0x493CA0, "Direct3D_ClipWindow", setup=setup)
    else:
        check(emu, 0x493CA0, "Direct3D_ClipWindow", setup=setup, check_eax=False)


def test_trivial():
    compare(emu, 0x493B20, "Direct3D_DisplayOpen")
    compare(emu, 0x4950E0, "Direct3D_GetRenderMode", setup=lambda e: u32(e, 0x5CA0E0, 3))


def test_reset_lists():
    def setup(e):
        for a in (0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4):
            p = e.alloc(16)
            e.write(p, noise(16, a & 0xFF))
            u32(e, a, p)
    compare(emu, 0x4946D0, "Direct3D_ResetLists", setup=setup, check_eax=False)


@pytest.mark.parametrize("pos", [True, False])
def test_make_matrix(pos):
    def setup(e):
        m = e.alloc(64)
        e.write(m, noise(64, 4))
        rot = e.alloc(36)
        e.write(rot, struct.pack("<9f", *[i * 0.5 - 2 for i in range(9)]))
        p = 0
        if pos:
            p = e.alloc(12)
            e.write(p, struct.pack("<3f", 1.5, -2.5, 3.25))
        return m, rot, p
    compare(emu, 0x4950F0, "Direct3D_MakeMatrix", setup=setup, check_eax=False)


# --- callbacks ----------------------------------------------------------------

def add_callbacks(e, n, start=0x401000):
    for i in range(n):
        e.call(0x495190, start + 16 * i, [0x5CA118, 0x5CA11C, 0x5CA120][i % 3])


@pytest.mark.parametrize("n", [0, 4, 9])
@pytest.mark.parametrize("orig,ours", [
    (0x495170, "Direct3D_AddSetModeCallback"), (0x4951E0, "Direct3D_AddUnsetModeCallback"),
    (0x495200, "Direct3D_AddRestoreCallback")])
def test_add_callback(n, orig, ours):
    def setup(e):
        add_callbacks(e, n)
        return (0x405000,)
    compare(emu, orig, ours, setup=setup, check_eax=False)


def test_add_callback_full():
    def setup(e):
        add_callbacks(e, 10)
        return 0x405000, 0x5CA118
    check_raises(emu, 0x495190, "Direct3D_AddCallback", setup=setup)


@pytest.mark.parametrize("fn", [0x401000, 0x401010, 0x401020, 0x401030, 0x401040, 0x401050, 0x401080, 0x405000])
@pytest.mark.parametrize("lst", [0x5CA118, 0x5CA11C, 0x5CA120])
def test_remove_callback(fn, lst):
    def setup(e):
        add_callbacks(e, 7)
        return fn, lst
    compare(emu, 0x495220, "Direct3D_RemoveCallback", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("fn", [0x401020, 0x401050, 0x493820])
def test_remove_restore_callback(fn):
    def setup(e):
        add_callbacks(e, 7)
        e.call(0x495200, 0x493820)
        return (fn,)
    compare(emu, 0x495260, "Direct3D_RemoveRestoreCallback", setup=setup, eax_mask=0xFF)


# --- init and shutdown --------------------------------------------------------

def init_setup(e):
    pyfake(e, 0x48F490, "AppMain_AddWinProcCallback", 1)
    pyfake(e, 0x48F4F0, "AppMain_RemoveWinProcCallback", 1)
    e.call(0x4B0BA0)
    e.call(0x4B0DC0)
    for i in range(13):
        u32(e, 0x542310 + 4 * i, 20 + i)


def test_init():
    check(emu, 0x493580, "Direct3D_Init", setup=init_setup, check_eax=False)


def test_shutdown():
    # Init of the same side registers the restore callback that Shutdown removes
    inits = [0x493580, "Direct3D_Init"]

    def setup(e):
        init_setup(e)
        e.call(inits.pop(0))
        e.call(0x495200, 0x401000)
    log = check(emu, 0x495060, "Direct3D_Shutdown", setup=setup, check_eax=False)
    assert not inits, log


# --- display mode enumeration -------------------------------------------------

def counter(results, i=None, extra=None):
    """before() returning results[n] on the n-th call (last one after that); optional out param i."""
    state = {"n": 0}

    def fn(e):
        n = state["n"]
        state["n"] += 1
        r = results[min(n, len(results) - 1)]
        if i is not None:
            v = i[1](e) if callable(i[1]) else i[1]
            e.write(e.arg(i[0]), struct.pack("<I", v))
        return (r, extra(e)) if extra else r
    return fn


def fill_drivers(n, guids):
    def fn(e):
        ctx = e.arg(0)
        drivers, count = e.u32(ctx + 0xC), e.u32(ctx + 0x10)
        for i in range(n):
            d = drivers + 0x4F8 * (e.u32(count) + i)
            e.write(d, noise(0x4F8, i))
            e.write(d, b"display%d\0" % i)
            e.write(d + 0x80, b"Driver number %d\0" % i)
            u32(e, d + 0x100, guids[i])
            u32(e, d + 0x480, 0x500)
        u32(e, count, e.u32(count) + n)
        name = e.u32(ctx + 4)
        return (None, (e.u32(ctx), name and e.read_cstr(name)))
    return fn


@pytest.mark.parametrize("n,start,limit", [(2, 0, 5), (3, 1, 2), (0, 0, 5)])
@pytest.mark.parametrize("fail", [None, "create", "enum"])
def test_enum(n, start, limit, fail):
    def setup(e):
        modes = [mode_desc(e, 640, 480, 16), mode_desc(e, 1024, 768, 32), mode_desc(e, 800, 600, 24)]
        dd = com(e, "dd", DDRAW, {8: Code(modes, 0x10, 0x14, DD_FAIL if fail == "enum" else 0)})
        g = e.alloc(16)
        pyfake(e, 0x491EF0, "Display_EnumDrivers", 1, before=fill_drivers(n, [0, g, 0]))
        api(e, "DirectDrawCreateEx", 4, DD_FAIL if fail == "create" else 0, before=out(1, dd))
        u32(e, 0xC10384, 1)
        a = e.alloc(12)
        cnt = e.alloc(4)
        u32(e, cnt, start)
        e.write(a, struct.pack("<3I", e.alloc(0x100 * 8), cnt, limit))
        return (a,)
    if fail and min(n + start, limit) > 0:
        check_raises(emu, 0x4924D0, "Direct3D_Enum", setup=setup)
    else:
        check(emu, 0x4924D0, "Direct3D_Enum", setup=setup, check_eax=False)


# --- caps report --------------------------------------------------------------

@pytest.mark.parametrize("tnl", [0, 1])
@pytest.mark.parametrize("raster,tex", [(0, 0), (0xFFFFFFFF, 0xFFFFFFFF), (0x2A5A0, 0x102), (0x3D5A00, 0x2)])
def test_gfx_caps(tnl, raster, tex):
    def setup(e):
        e.call(0x4A537B)  # CRT float formatting hooks
        u32(e, G_TNL, tnl)
        u32(e, 0x5CA124, 77)
        u32(e, 0x5CA128, 12345678)
        desc = e.read(devdesc(e, HAL, raster=raster, tex=tex), 0xEC)
        device(e, {3: (0, lambda e: e.write(e.arg(1), desc))})

        def caps(e):
            hal, hel = e.arg(1), e.arg(2)
            sizes = (e.u32(hal), e.u32(hel))
            u32(e, hal + 0x3C, 32 << 20)
            u32(e, hal + 0x40, 5 << 20 | 12345)
            return (None, sizes)
        ddraw(e, {11: (0, caps)})
        zf = [zformat(e, 0x400, 16), zformat(e, 0x4400, 32), zformat(e, 0x20, 8)]
        u32(e, G_D3D, com(e, "d3d", D3D, {6: Code(zf, 0x10, 0xC)}))
        buf = e.alloc(10008)
        return (buf,)
    check(emu, 0x4927B0, "Direct3D_GetGFXCaps", setup=setup, check_eax=False)


@pytest.mark.parametrize("which", [3, 11])
def test_gfx_caps_errors(which):
    def setup(e):
        device(e, {3: DD_FAIL if which == 3 else 0})
        ddraw(e, {11: DD_FAIL if which == 11 else 0})
        return (e.alloc(10008),)
    check_raises(emu, 0x4927B0, "Direct3D_GetGFXCaps", setup=setup)


# --- vertex buffers -----------------------------------------------------------

def vb_setup(fail_at=None, tnl=0):
    def setup(e):
        u32(e, G_TNL, tnl)
        vbs = [com(e, "vb%d" % i, VBUF, {3: (0, out(2, 0x7000 + i))}) for i in range(3)]
        results = [0, 0, 0]
        if fail_at is not None:
            results[fail_at] = DD_FAIL
        state = {"n": 0}

        def create(e):
            n = state["n"]
            state["n"] += 1
            u32(e, e.arg(2), vbs[n])
            return (results[n], e.read(e.arg(1), 16))
        u32(e, G_D3D, com(e, "d3d", D3D, {5: (0, create)}))
        return vbs
    return setup


@pytest.mark.parametrize("tnl", [0, 1])
def test_create_vertex_buffers(tnl):
    check(emu, 0x493830, "Direct3D_CreateVertexBuffers", setup=vb_setup(tnl=tnl), check_eax=False)


@pytest.mark.parametrize("fail_at", [0, 1, 2])
def test_create_vertex_buffers_fail(fail_at):
    check_raises(emu, 0x493830, "Direct3D_CreateVertexBuffers", setup=vb_setup(fail_at))


@pytest.mark.parametrize("present", [(1, 1, 1), (0, 0, 0), (1, 0, 1), (0, 1, 0)])
def test_free_vertex_buffers(present):
    def setup(e):
        for i, (a, on) in enumerate(zip((0xBCAD00, 0xBCAD80, 0xBCAD90), present)):
            u32(e, a, com(e, "vb%d" % i, VBUF) if on else 0)
    check(emu, 0x493A10, "Direct3D_FreeVertexBuffers", setup=setup, check_eax=False)


def test_restore_vertex_buffers():
    def setup(e):
        vb_setup()(e)
        for i, a in enumerate((0xBCAD00, 0xBCAD80, 0xBCAD90)):
            u32(e, a, com(e, "old%d" % i, VBUF))
    check(emu, 0x493820, "Direct3D_RestoreVertexBuffers", setup=setup, check_eax=False)


# --- unset mode ---------------------------------------------------------------

def callback_list(e, head, names):
    """A Direct3D callback list whose functions are logged traps."""
    nxt = 0
    for name in reversed(names):
        node = e.alloc(8)
        fn = e.alloc(8)
        pyfake(e, fn, name, 0)
        e.write(node, struct.pack("<II", nxt, fn))
        nxt = node
    u32(e, head, nxt)


@pytest.mark.parametrize("objs", [(1, 1, 1, 1, 1, 1), (1, 0, 1, 1, 1, 1), (1, 1, 1, 0, 0, 0), (1, 0, 1, 0, 1, 0)])
@pytest.mark.parametrize("busy", [0, 2])
def test_unset_mode(objs, busy):
    def setup(e):
        dev, z, back, prim, d3d, dd = objs
        u32(e, APP_HWND, 0x1234)
        device(e)
        u32(e, G_ZBUF, com(e, "z", SURFACE) if z else 0)
        u32(e, G_BACK, com(e, "back", SURFACE, {13: (0, counter([WASSTILLDRAWING] * busy + [0]))}) if back else 0)
        u32(e, G_PRIMARY, com(e, "prim", SURFACE) if prim else 0)
        u32(e, G_D3D, com(e, "d3d", D3D) if d3d else 0)
        if dd:
            ddraw(e)
        else:
            u32(e, G_DDRAW, 0)
        callback_list(e, 0x5CA11C, ["cb1", "cb2"])
        for i, a in enumerate((0xBCAD00, 0xBCAD80, 0xBCAD90)):
            u32(e, a, com(e, "vb%d" % i, VBUF))
    check(emu, 0x493B30, "Direct3D_UnsetMode", setup=setup, check_eax=False)


def test_unset_mode_coop_fail():
    def setup(e):
        device(e)
        u32(e, G_BACK, com(e, "back", SURFACE))
        ddraw(e, {20: DD_FAIL})
    check_raises(emu, 0x493B30, "Direct3D_UnsetMode", setup=setup)


# --- device creation ----------------------------------------------------------

DDERR_INVALIDPIXELFORMAT = 0x88760091


def new_mode(e, w=800, h=600, depth=32, back=1, device=b""):
    m = e.alloc(0x11C)
    e.write(m, noise(0x11C, 9))
    e.write(m, struct.pack("<5I", 0, w, h, back, depth))
    e.write(m + 0x1C, device + b"\0")
    e.write(m + 0x9C, b"some driver\0")
    return m


def device_env(e, fullscreen, fail=(), modes=(0,), zf=None):
    """Fake DirectDraw/Direct3D objects for Direct3D_CreateDevice. fail names the calls that fail."""
    f = lambda name: DD_FAIL if name in fail else 0
    u32(e, APP_FULLSCREEN, fullscreen)
    u32(e, APP_HWND, 0x1234)
    api(e, "ShowCursor", 1)
    api(e, "CreateICA", 4, 0x777)
    api(e, "GetDeviceCaps", 2, 32)
    api(e, "DeleteDC", 1, 1)
    dev = com(e, "dev", DEVICE, {3: (0, lambda e: e.write(e.arg(1), noise(0xEC, 5))),
                                 13: (0, lambda e: (None, e.read(e.arg(1), 24))),
                                 27: (0, lambda e: (None, e.read(e.arg(1), 32)))})
    z = com(e, "z", SURFACE)
    back = com(e, "back", SURFACE, {3: f("attach")})
    pf = struct.pack("<8I", 32, 0x40, 0, 16, 0xF800, 0x7E0, 0x1F, 0)
    prim = com(e, "prim", SURFACE, {12: (f("attached"), out(2, back, lambda e: e.read(e.arg(1), 16))),
                                    21: (f("pixfmt"), lambda e: (e.write(e.arg(1), pf), e.read(e.arg(1), 8))[1:] and None),
                                    28: f("setclipper")})
    surfaces = [prim] + ([] if fullscreen else [back]) + [z]
    results = [f("prim")] + ([] if fullscreen else [f("back")]) + [f("zsurf")]
    state = {"n": 0}

    def create_surface(e):
        n = state["n"]
        state["n"] += 1
        u32(e, e.arg(2), surfaces[n])
        return (results[n], e.read(e.arg(1), 0x7C))
    clipper = com(e, "clip", CLIPPER, {8: f("sethwnd")})
    if zf is None:
        zf = [zformat(e, 0x400, 16), zformat(e, 0x4400, 32), zformat(e, 0x400, 24), zformat(e, 0x400, 32)]
    vbs = [com(e, "vb%d" % i, VBUF) for i in range(3)]
    vb_state = {"n": 0}

    def create_vb(e):
        u32(e, e.arg(2), vbs[vb_state["n"] % 3])
        vb_state["n"] += 1
        return (None, e.read(e.arg(1), 16))
    d3d = com(e, "d3d", D3D, {4: (f("create"), out(3, dev)), 5: (0, create_vb),
                              6: Code(zf, 0x10, 0xC, f("enumz"))})
    dd = com(e, "dd", DDRAW, {0: (f("qi"), out(2, d3d)), 4: (f("clipper"), out(2, clipper)), 6: (0, create_surface),
                              20: f("coop"), 21: (0, counter(list(modes)))})
    u32(e, G_DDRAW, dd)
    u32(e, G_D3D, d3d)
    for a in (G_DEVICE, G_PRIMARY, G_BACK, G_ZBUF):
        u32(e, a, 0)
    return dd


CD_ARGS = [(-1, -1, -1, -1), (16, 16, 640, 480), (-1, 16, 1024, -1), (24, 24, -1, 768)]


@pytest.mark.parametrize("fullscreen", [0, 1])
@pytest.mark.parametrize("args", CD_ARGS)
@pytest.mark.parametrize("g", [HAL, TNL, RGB, REF])
def test_create_device(fullscreen, args, g):
    def setup(e):
        device_env(e, fullscreen)
        return args + (new_mode(e), g)
    check(emu, 0x4942A0, "Direct3D_CreateDevice", setup=setup)


@pytest.mark.parametrize("fullscreen", [0, 1])
@pytest.mark.parametrize("fail", ["mode", "prim", "attached", "pixfmt", "back", "enumz", "zsurf", "attach", "create"])
def test_create_device_fail(fullscreen, fail):
    def setup(e):
        device_env(e, fullscreen, fail=(fail,), modes=(DD_FAIL if fail == "mode" else 0,))
        return (-1, -1, -1, -1, new_mode(e), HAL)
    check(emu, 0x4942A0, "Direct3D_CreateDevice", setup=setup)


@pytest.mark.parametrize("fullscreen", [0, 1])
def test_create_device_no_zbuffer(fullscreen):
    def setup(e):
        device_env(e, fullscreen, zf=[zformat(e, 0x4400, 16)])
        return (-1, 8, -1, -1, new_mode(e), HAL)
    check_raises(emu, 0x4942A0, "Direct3D_CreateDevice", setup=setup)


# --- set mode -----------------------------------------------------------------

def set_mode_env(e, fullscreen, fail=(), modes=(0,), found=1, have_dd=True, tnl=0, msgs=2):
    dd = device_env(e, fullscreen, fail, modes)
    u32(e, G_TNL, tnl)
    if not have_dd:
        u32(e, G_DDRAW, 0)
    api(e, "DirectDrawCreateEx", 4, DD_FAIL if "ddcreate" in fail else 0, before=out(1, dd))
    g = e.alloc(16)
    pyfake(e, 0x491EF0, "Display_EnumDrivers", 1, before=fill_drivers(found, [g]))
    callback_list(e, 0x5CA118, ["setmode_cb"])
    api(e, "PeekMessageA", 5, before=counter([1] * msgs + [0], extra=lambda e: e.arg(0) and None))
    api(e, "GetMessageA", 4, before=counter([1, 0]))
    api(e, "TranslateMessage", 1)
    api(e, "DispatchMessageA", 1)
    for a in (0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4):
        u32(e, a, e.alloc(16))


@pytest.mark.parametrize("fullscreen", [0, 1])
@pytest.mark.parametrize("name", [b"", b"display0"])
@pytest.mark.parametrize("have_dd,tnl", [(True, 0), (False, 1)])
def test_set_mode(fullscreen, name, have_dd, tnl):
    def setup(e):
        set_mode_env(e, fullscreen, have_dd=have_dd, tnl=tnl)
        return (new_mode(e, device=name),)
    check(emu, 0x493D50, "Direct3D_SetMode", setup=setup, check_eax=False)


@pytest.mark.parametrize("modes", [
    (DDERR_INVALIDPIXELFORMAT, 0), (DD_FAIL, 0), (DDERR_INVALIDPIXELFORMAT, DD_FAIL, 0), (DD_FAIL, DD_FAIL, 0),
    (DD_FAIL, DD_FAIL, DD_FAIL, 0)])
def test_set_mode_fallback(modes):
    def setup(e):
        set_mode_env(e, 1, modes=modes)
        return (new_mode(e),)
    log = check(emu, 0x493D50, "Direct3D_SetMode", setup=setup, check_eax=False)
    assert len([x for x in log if x[0] == "dd.21"]) == len(modes)


@pytest.mark.parametrize("fail", ["found", "ddcreate", "coop", "qi", "device", "clipper", "sethwnd", "setclipper"])
def test_set_mode_fail(fail):
    def setup(e):
        set_mode_env(e, 1, fail=(fail,), have_dd=fail != "ddcreate", found=fail != "found",
                     modes=(DD_FAIL,) if fail == "device" else (0,))
        return (new_mode(e),)
    check_raises(emu, 0x493D50, "Direct3D_SetMode", setup=setup)


# --- swap buffers -------------------------------------------------------------

def window_apis(e):
    def client_rect(e):
        e.write(e.arg(1), struct.pack("<4i", 0, 0, 640, 480))

    def to_screen(e):
        x, y = struct.unpack("<2i", e.read(e.arg(1), 8))
        e.write(e.arg(1), struct.pack("<2i", x + 100, y + 50))
    api(e, "GetClientRect", 2, 1, before=client_rect)
    api(e, "ClientToScreen", 2, 1, before=to_screen)


SWAP_CASES = [
    dict(), dict(fullscreen=1), dict(fullscreen=1, flip=DD_FAIL), dict(blt=(WASSTILLDRAWING, 0)),
    dict(blt=(DD_FAIL,)), dict(blt=(WASSTILLDRAWING, DD_FAIL)), dict(begin=DD_FAIL), dict(nodev=1),
    dict(lost=1), dict(lost=1, evict=DD_FAIL), dict(lost=1, restore=DD_FAIL), dict(lost=1, fullscreen=1)]


@pytest.mark.parametrize("case", SWAP_CASES)
def test_swap_buffers(case):
    def setup(e):
        u32(e, APP_FULLSCREEN, case.get("fullscreen", 0))
        u32(e, APP_HWND, 0x1234)
        window_apis(e)
        if case.get("nodev"):
            u32(e, G_DEVICE, 0)
        else:
            device(e, {6: DD_FAIL if case.get("lost") else 0, 5: case.get("begin", 0)})

        def caps(e):
            u32(e, e.arg(1) + 0x40, 0x123456)
            return (None, (e.u32(e.arg(1)), e.u32(e.arg(2))))
        ddraw(e, {25: case.get("restore", 0), 11: (0, caps)})
        u32(e, G_D3D, com(e, "d3d", D3D, {7: case.get("evict", 0)}))
        blt = case.get("blt", (0,))
        u32(e, G_PRIMARY, com(e, "prim", SURFACE, {11: case.get("flip", 0),
                                                   5: (0, counter(blt, extra=lambda e: e.read(e.arg(1), 16)))}))
        u32(e, G_BACK, com(e, "back", SURFACE))
        callback_list(e, 0x5CA120, ["restore_cb"])
        for a in (0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4):
            u32(e, a, e.alloc(16))
        for a in (0xBCAD08, 0xBCAD88, 0xBCAD98):
            u32(e, a, 33)
    errors = case.get("flip") or case.get("begin") or case.get("evict") or DD_FAIL in case.get("blt", ())
    if errors:
        check_raises(emu, 0x494700, "Direct3D_SwapBuffers", setup=setup)
    else:
        check(emu, 0x494700, "Direct3D_SwapBuffers", setup=setup, check_eax=False)


# --- framebuffer snapshot and blit --------------------------------------------

FORMATS = {
    "8888": (0x40, 32, 0xFF0000, 0xFF00, 0xFF),
    "565": (0x40, 16, 0xF800, 0x7E0, 0x1F),
    "555": (0x40, 16, 0x7C00, 0x3E0, 0x1F),
    "bgr": (0x40, 32, 0xFF, 0xFF00, 0xFF0000),
    "24": (0x40, 24, 0xFF0000, 0xFF00, 0xFF),
    "yuv": (0x10, 16, 0xF800, 0x7E0, 0x1F),
    "zero": (0x40, 16, 0, 0x7E0, 0),
}


def lock_desc(e, fmt, w=5, h=3, pitch_extra=8):
    flags, bits, r, g, b = FORMATS[fmt]
    pitch = w * bits // 8 + pitch_extra
    pixels = e.alloc(pitch * h)
    e.write(pixels, noise(pitch * h, 6))
    sd = bytearray(0x7C)
    struct.pack_into("<IIIIi", sd, 0, 0x7C, 0x100F, h, w, pitch)
    struct.pack_into("<I", sd, 0x24, pixels)
    struct.pack_into("<IIIIIII", sd, 0x48, 32, flags, 0, bits, r, g, b)
    return bytes(sd)


@pytest.mark.parametrize("fmt", list(FORMATS))
@pytest.mark.parametrize("fail", [None, "lock", "unlock"])
def test_snapshot(fmt, fail):
    def setup(e):
        sd = lock_desc(e, fmt)

        def lock(e):
            size = e.u32(e.arg(2))
            e.write(e.arg(2), sd)
            return (DD_FAIL if fail == "lock" else 0, size)
        u32(e, G_BACK, com(e, "back", SURFACE, {25: (0, lock), 32: DD_FAIL if fail == "unlock" else 0}))
        a = e.alloc(16)
        dst = e.alloc(5 * 3 * 3 + 16)
        e.write(a, struct.pack("<4I", 1, 2, 3, dst))
        return (a,)
    if fail or fmt in ("24", "yuv"):
        check_raises(emu, 0x494970, "Direct3D_Snapshot", setup=setup)
    else:
        check(emu, 0x494970, "Direct3D_Snapshot", setup=setup, check_eax=False)


@pytest.mark.parametrize("fmt", ["8888", "565", "555", "yuv"])
@pytest.mark.parametrize("primary", [0, 1])
def test_blit(fmt, primary):
    def setup(e):
        e.call(0x4B0BA0)
        u32(e, APP_HWND, 0x1234)
        window_apis(e)
        flags, bits, r, g, b = FORMATS[fmt]
        pf = struct.pack("<8I", 32, flags, 0, bits, r, g, b, 0)

        def get_pf(e):
            size = e.u32(e.arg(1))
            e.write(e.arg(1), pf)
            return (None, size)
        tmp = com(e, "tmp", SURFACE)

        def create(e):
            sd = e.read(e.arg(1), 0x7C)
            h, w, pitch = struct.unpack_from("<IIi", sd, 8)
            u32(e, e.arg(2), tmp)
            return (None, (sd, e.read(struct.unpack_from("<I", sd, 0x24)[0], h * pitch)))
        ddraw(e, {6: (0, create)})

        def blt(e):
            return (None, (e.read(e.arg(1), 16), e.read(e.arg(3), 16)))
        u32(e, G_PRIMARY, com(e, "prim", SURFACE, {21: (0, get_pf), 5: (0, blt)}))
        u32(e, G_BACK, com(e, "back", SURFACE, {21: (0, get_pf), 5: (0, blt)}))
        a = e.alloc(40)
        px = e.alloc(4 * 12)
        e.write(px, noise(48, 8))
        e.write(a, struct.pack("<10I", 1, 2, 3, 4, 30, 40, 4, 3, px, primary))
        return (a,)
    check(emu, 0x494C50, "Direct3D_Blit", setup=setup, check_eax=False)


@pytest.mark.parametrize("fail", ["pixfmt", "create"])
def test_blit_fail(fail):
    def setup(e):
        e.call(0x4B0BA0)
        ddraw(e, {6: DD_FAIL if fail == "create" else 0})
        u32(e, G_BACK, com(e, "back", SURFACE, {21: DD_FAIL if fail == "pixfmt" else 0}))
        a = e.alloc(40)
        e.write(a, struct.pack("<10I", 1, 2, 3, 4, 30, 40, 4, 3, e.alloc(48), 0))
        return (a,)
    check_raises(emu, 0x494C50, "Direct3D_Blit", setup=setup)


# --- window messages ----------------------------------------------------------

@pytest.mark.parametrize("msg,wparam", [(6, 0), (6, 1), (6, 2), (6, 0x10001), (0x24, 0), (0x10, 0)])
@pytest.mark.parametrize("active", [0, 1])
@pytest.mark.parametrize("state", ["ok", "fullscreen", "othermode", "noddraw"])
def test_winproc_cb(msg, wparam, active, state):
    def setup(e):
        u32(e, 0x5CA0E0, 2)
        u32(e, 0xA94A64, 3 if state == "othermode" else 2)
        u32(e, G_DDRAW, 0 if state == "noddraw" else 0x1000)
        u32(e, APP_ACTIVE, active)
        u32(e, APP_FULLSCREEN, state == "fullscreen")
        u32(e, CUR_MODE + 4, 800)
        u32(e, CUR_MODE + 8, 600)
        api(e, "InvalidateRect", 3, 1)

        def adjust(e):
            r = struct.unpack("<4i", e.read(e.arg(0), 16))
            e.write(e.arg(0), struct.pack("<4i", r[0] - 4, r[1] - 23, r[2] + 4, r[3] + 4))
            return (None, (r, e.arg(1), e.arg(2)))
        api(e, "AdjustWindowRect", 3, 1, before=adjust)
        mmi = e.alloc(40)
        e.write(mmi, noise(40, 10))
        return 0x1234, msg, wparam, mmi
    check(emu, 0x494F50, "Direct3D_WinProcCB", setup=setup)
