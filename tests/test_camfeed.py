"""Camera filters: LineFilter, WideScreenFilter, BandFilter, HandiCamFilter (0x4A3530 - 0x4A5000)."""
import struct

import pytest

from emu import Emu
from test_computer_util import LOG, RET, com, f32, handler, logfake, pyfake, run, task, u32, vfake

emu = Emu()
logfake(emu, 0x4B72E0, 3, effect=lambda e, a: RET.get("item", 0))  # SortRender_Add
logfake(emu, 0x4B0DB0, 0, effect=lambda e, a: RET.get("mode", 0))  # Mem_GetMode
logfake(emu, 0x4B0D40, 0)  # Mem_SetDefaultMode
logfake(emu, 0x4B0D50, 0)  # Mem_ClearMode
logfake(emu, 0x4B6EC0, 4, strings=(3,), effect=lambda e, a: RET.get("text", 0))  # Text_New
vfake(emu, 0x4B6F50, 1, show=lambda e, a: (e.read(a[0], 0x28),))  # Text_DrawAlt
logfake(emu, 0x4B6F30, 1)  # Text_Free
logfake(emu, 0x4B4910, 10)  # Draw_RectZ
logfake(emu, 0x497680, 2, derefs={1: 16})  # Scene_SetRect
logfake(emu, 0x497800, 1, effect=lambda e, a: RET.get("frame", 0))  # Frame_Alloc
logfake(emu, 0x4978B0, 2)  # RenderQueue_Add
vfake(emu, 0x4977B0, 2, show=lambda e, a: (e.read(a[0] + 0xC, 16), a[1]))  # Scene_QueueClipWindow
logfake(emu, 0x4978E0, 1)  # RenderTask_Unregister
logfake(emu, 0x4B0EB0, 1)  # RenderMode_DeleteMethod
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x490370, 0, effect=lambda e, a: RET.get("ms", 0))  # Timer_GetMs
logfake(emu, 0x4A2600, 1)
logfake(emu, 0x4A26C0, 1)
logfake(emu, 0x4A2720, 1)
logfake(emu, 0x4B2450, 7)  # Texture_Download
logfake(emu, 0x4B6D00, 1, effect=lambda e, a: RET.get("sh", 0))  # Sprite_Height
logfake(emu, 0x4B6CE0, 1, effect=lambda e, a: RET.get("sw", 1))  # Sprite_Width
logfake(emu, 0x4B53B0, 17)  # QVertex_DrawQuadEx


def sprintf(e):
    fmt = e.read_cstr(e.arg(1)).decode()
    args = tuple(struct.unpack("<i", struct.pack("<I", e.arg(2 + i)))[0] for i in range(fmt.count("%")))
    s = (fmt % args).encode()
    e.write(e.arg(0), s + b"\0")
    if LOG:
        LOG[-1].append(("sprintf", fmt) + args)
    e.ret(len(s))


pyfake(emu, 0x4A53B3, sprintf)
DRAW = handler(emu, "draw", 1)
BUF = emu.alloc(0x4000 * 12)


def lock(e, a):
    n = RET.get("locks", 0)
    RET["locks"] = n + 1
    u32(e, a[2], 0 if n == RET.get("null") else BUF + 0x4000 * n)
    return 0x88760000 + n if n == RET.get("fail") else 0


def transform(e, a):
    f32(e, a[2], *[0.5 + 0.25 * i for i in range(16)])


VB = com(emu, "vb", {3: 4}, effects={3: lock})
DEV = com(emu, "dev", {12: 3, 20: 3, 31: 6, 35: 3, 37: 4}, effects={12: transform})
CW = {}


def cw_entry(target, cw):
    """Entry that loads an x87 control word and jumps to target."""
    addr = emu.symbols[target] if isinstance(target, str) else target
    if (addr, cw) not in CW:
        emu.reset()
        p = emu.fake_top
        emu.fake_top += 32
        emu.write(p + 16, struct.pack("<H", cw))
        emu.write(p, b"\xd9\x2d" + struct.pack("<I", p + 16) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        emu.baseline = emu.snapshot()
        CW[(addr, cw)] = p
    return CW[(addr, cw)]


def both(orig, ours, setup, cw=0x37F, **kw):
    run(emu, cw_entry(orig, cw), cw_entry(ours, cw), setup, **kw)


def screen(e, x=0.0, y=0.0, w=640.0, h=480.0):
    f32(e, 0xBCABB8, x, y, w, h)


def d3d(e, near=10.0, used=0, size=300):
    u32(e, 0x5CA114, DEV)
    u32(e, 0xBCAD00, VB, size, used)
    f32(e, 0xBCADC0, near)
    f32(e, 0xBCABEC, 0.001)
    f32(e, 0xBCADA0, 0.25)
    f32(e, 0xBCADB4, 0.125)
    f32(e, 0xBCAD8C, 0.875)
    for g in (0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4):
        u32(e, g, e.alloc(8))
    e.write(BUF, bytes(range(256)) * (0x4000 * 12 // 256))


def text(e):
    RET["text"] = e.alloc(0x30)
    e.write(RET["text"], bytes(range(0x30)))


CWS = pytest.mark.parametrize("cw", [0x37F, 0x7F])

# --- LineFilter ----------------------------------------------------------


def line(e, sorted_=1, z=150.0):
    t = task(e, 0x38, 0x40)
    u32(e, t + 0x20, 1, 2, 3)
    f32(e, t + 0x2C, z, 0.5)
    u32(e, t + 0x34, sorted_)
    return t


@pytest.mark.parametrize("fn", [0, 1])
def test_line_draw_prim(fn):
    def setup(e):
        u32(e, 0x936010, 7)
        u32(e, 0xA94E84 + 7 * 4, DRAW if fn else 0)
        d = e.alloc(16)
        u32(e, d, 0, 0, 0, line(e))
        return (d,)
    run(emu, 0x4A3530, "LineFilter_DrawPrim", setup)


@pytest.mark.parametrize("sorted_", [0, 1, 5])
def test_line_draw(sorted_):
    def setup(e):
        RET["item"] = e.alloc(16)
        u32(e, 0x936014, 0x21)
        u32(e, 0x936010, 7)
        u32(e, 0xA94E84 + 7 * 4, DRAW)
        return (line(e, sorted_),)
    run(emu, 0x4A3550, "LineFilter_Draw", setup)


def test_line_set_level():
    run(emu, 0x4A4FF0, "LineFilter_SetLevel", lambda e: (line(e), 0x3F123456))

# --- WideScreenFilter ----------------------------------------------------


def wide(e):
    t = task(e, 0x44, 0x41)
    f32(e, t + 0x20, 0.1, 0.2, 0.3, 4.0, 600.5, 300.25, 10.5, 20.75, 33.3)
    return t


@CWS
def test_wide_draw_prim(cw):
    def setup(e):
        d = e.alloc(16)
        u32(e, d, 0, 0, 0, wide(e))
        return (d,)
    both(0x4A35B0, "WideScreenFilter_DrawPrim", setup, cw)


def test_wide_draw():
    def setup(e):
        RET["frame"] = e.alloc(16)
        e.write(RET["frame"], b"\x77" * 16)
        u32(e, 0x936018, 0x22)
        return (wide(e),)
    run(emu, 0x4A3650, "WideScreenFilter_Draw", setup)

# --- BandFilter ----------------------------------------------------------


def band(e, z=150.0, level=0.3, max_level=0.8, counter=3, period=60, timer=2, interval=10, camera=4,
         color=(0.25, 0.5, 1.0)):
    t = task(e, 0x74, 0x42)
    f32(e, t + 0x20, z, level, max_level)
    u32(e, t + 0x2C, *range(0x100, 0x109))
    u32(e, t + 0x50, counter, period, timer, interval, camera)
    f32(e, t + 0x64, *color)
    u32(e, t + 0x70, 0xF047)
    return t


@pytest.mark.parametrize("fn", [0, 1])
def test_band_draw_prim(fn):
    def setup(e):
        u32(e, 0x935C00, 9)
        u32(e, 0xA94E84 + 9 * 4, DRAW if fn else 0)
        d = e.alloc(16)
        u32(e, d, 0, 0, 0, band(e))
        return (d,)
    run(emu, 0x4A36B0, "BandFilter_DrawPrim", setup)


@CWS
@pytest.mark.parametrize("max_level,mode,z,near", [
    (0.8, 0, 150.0, 10.0), (0.8, 1, 150.0, 10.0), (-0.5, 0, 5.0, 10.0),
    (0.0, 0, 150.0, 10.0), (-0.0, 1, 5.0, 10.0), (0.0, 0, 10.0, 10.0),
])
@pytest.mark.parametrize("view", [(0.0, 0.0, 640.0, 480.0), (10.5, 20.5, 800.4, 600.6), (-3.5, 7.5, 1023.5, 767.5)])
def test_band_draw(max_level, mode, z, near, view, cw):
    def setup(e):
        RET["item"] = e.alloc(16)
        RET["mode"] = mode
        text(e)
        screen(e, *view)
        f32(e, 0xBCADC0, near)
        u32(e, 0x935C0C, 0x23)
        f32(e, 0x93603C, 0.9, 0.2, -50.0, 0.0)
        return (band(e, z=z, max_level=max_level, color=(0.3, 0.6, 0.9)),)
    both(0x4A36D0, "BandFilter_Draw", setup, cw)


@CWS
@pytest.mark.parametrize("level,period,counter,timer,interval,max_level", [
    (0.3, 60, 3, 2, 10, 0.8), (0.01, 60, 60, 2, 10, 0.8), (0.0, 7, 70, 10, 10, 0.8),
    (0.9, 3, 0, 11, 10, 0.37), (0.1, 30, 29, 0, 0, 1.5), (1.0, 1, -5, 3, 1000, 0.2),
])
def test_band_run(level, period, counter, timer, interval, max_level, cw):
    def setup(e):
        e.call(0x4B4720, 0x935BF0)  # Random_Init
        return (band(e, level=level, period=period, counter=counter, timer=timer, interval=interval,
                     max_level=max_level),)
    both(0x4A38A0, "BandFilter_Run", setup, cw)


@CWS
@pytest.mark.parametrize("view", [(0.0, 0.0, 640.0, 480.0), (10.5, 20.5, 800.4, 600.6), (-3.5, 7.4, 1023.5, 767.5),
                                  (0.0, 0.0, 333.0, 257.0)])
@pytest.mark.parametrize("counter,mode", [(3, 0), (46, 1), (45, 0)])
def test_band_render(view, counter, mode, cw):
    def setup(e):
        RET["locks"] = 0
        RET["mode"] = mode
        screen(e, *view)
        d3d(e)
        return (band(e, counter=counter),)
    both(0x4A3940, "BandFilter_Render", setup, cw)


@pytest.mark.parametrize("max_level,z,used,size,fail,null", [
    (0.0, 150.0, 0, 300, None, None),
    (0.5, 5.0, 150, 300, None, None),
    (0.5, 150.0, 200, 300, None, None),
    (0.5, 150.0, 0, 300, 0, None),
    (0.5, 150.0, 0, 300, 1, None),
    (0.5, 150.0, 250, 300, None, 0),
    (0.5, 150.0, 150, 300, None, 1),
])
def test_band_render_paths(max_level, z, used, size, fail, null):
    def setup(e):
        RET["locks"] = 0
        RET["fail"] = fail
        RET["null"] = null
        screen(e)
        d3d(e, used=used, size=size)
        return (band(e, z=z, max_level=max_level),)
    both(0x4A3940, "BandFilter_Render", setup)

# --- HandiCamFilter ------------------------------------------------------


def handicam(e, z=150.0, blink=0.5, last=1000, color=(0.25, 0.5, 1.0)):
    t = task(e, 0x54, 0x43)
    f32(e, t + 0x20, *color, z)
    u32(e, t + 0x30, 0xF047, *range(0x200, 0x206))
    f32(e, t + 0x4C, blink)
    u32(e, t + 0x50, last)
    return t


@pytest.mark.parametrize("fn", [0, 1])
def test_handicam_draw_prim(fn):
    def setup(e):
        u32(e, 0x935BFC, 11)
        u32(e, 0xA94E84 + 11 * 4, DRAW if fn else 0)
        d = e.alloc(16)
        u32(e, d, 0, 0, 0, handicam(e))
        return (d,)
    run(emu, 0x4A4C90, "HandiCamFilter_DrawPrim", setup)


@CWS
@pytest.mark.parametrize("ms,last,blink,timecode", [
    (1000, 1000, 0.5, 0.0), (1016, 1000, 0.5, 100.0), (2500, 1000, 0.2, 7.3),
    (1500, 1000, 1.0, 1234.5), (1000, 3000, 0.1, 50.0), (0x7FFFFFFF, 5, 1.4, 3.0),
])
def test_handicam_run(ms, last, blink, timecode, cw):
    def setup(e):
        RET["ms"] = ms
        f32(e, 0x936038, timecode)
        return (handicam(e, blink=blink, last=last),)
    both(0x4A4CB0, "HandiCamFilter_Run", setup, cw)


@CWS
@pytest.mark.parametrize("timecode,blink,mode,sh,sw", [
    (0.0, 0.5, 0, 16, 32), (12345.6, 0.75, 1, 17, 31), (5000000.0, 1.2, 0, 7, 3), (-777.0, 0.7499, 1, 64, 64),
    (86399.4, 0.0, 0, 1, 100),
])
@pytest.mark.parametrize("view", [(0.0, 0.0, 640.0, 480.0), (10.5, 20.5, 800.4, 600.6)])
def test_handicam_draw(timecode, blink, mode, sh, sw, view, cw):
    def setup(e):
        RET["item"] = e.alloc(16)
        RET["mode"] = mode
        RET["sh"] = sh
        RET["sw"] = sw
        text(e)
        screen(e, *view)
        u32(e, 0x935C08, 0x24)
        u32(e, 0x936028, 4, 2, 0x75, 0x14)
        f32(e, 0x936038, timecode)
        return (handicam(e, blink=blink),)
    both(0x4A4D10, "HandiCamFilter_Draw", setup, cw)


@CWS
@pytest.mark.parametrize("view", [(0.0, 0.0, 640.0, 480.0), (10.5, 20.5, 800.4, 600.6), (-3.5, 7.4, 1023.5, 767.5),
                                  (0.0, 0.0, 333.0, 2.0)])
@pytest.mark.parametrize("color", [(0.25, 0.5, 1.0), (1.5, -0.2, 0.7)])
def test_handicam_render(view, color, cw):
    def setup(e):
        RET["locks"] = 0
        screen(e, *view)
        d3d(e, size=1000)
        return (handicam(e, color=color),)
    both(0x4A4300, "HandiCamFilter_Render", setup, cw)


@pytest.mark.parametrize("z,used,size,fail,null", [
    (5.0, 900, 1000, None, None),
    (150.0, 0, 1000, 0, None),
    (150.0, 0, 1000, 1, None),
    (150.0, 0, 1000, 2, None),
    (150.0, 0, 1000, 3, None),
    (150.0, 0, 300, None, 1),
    (150.0, 0, 300, None, 3),
    (150.0, 900, 1000, None, 0),
])
def test_handicam_render_paths(z, used, size, fail, null):
    def setup(e):
        RET["locks"] = 0
        RET["fail"] = fail
        RET["null"] = null
        screen(e)
        d3d(e, used=used, size=size)
        return (handicam(e, z=z),)
    both(0x4A4300, "HandiCamFilter_Render", setup)

# --- shutdown ------------------------------------------------------------


def test_close():
    def setup(e):
        u32(e, 0x935BFC, 1, 2, 3, 4)
        u32(e, 0x936010, 5, 6, 7)
        e.write(0x543D4C, struct.pack("<4H", 0x40, 0x41, 0x42, 0x77))
        e.write(0x543D58, struct.pack("<H", 0x43))
    run(emu, 0x4A4F60, "CameraFilter_Close", setup)
