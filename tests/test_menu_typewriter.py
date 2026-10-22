import struct

import pytest

from emu import Emu
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, set_handler, task, u16, u32)

emu = Emu()

WINDOW, TW, CHILD = 0x40, 0x43, 0x42
EV914, EV918, EV91C, DRAW, DA8, DAC, CHANGED = 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x31


def text_new(e, args):
    t = e.alloc(0x28)
    u32(e, t + 0x20, 0x5A5A)
    return t


logfake(emu, 0x4B6E90, 2, strings=(1,))  # Text_SetString
logfake(emu, 0x4B6EC0, 4, effect=text_new)  # Text_New
logfake(emu, 0x4B6F30, 1)  # Text_Free
logfake(emu, 0x4B6F50, 1, derefs={0: 0x28})  # Text_DrawAlt
logfake(emu, 0x4B7DF0, 1, ret=37)  # Text_GetWidth
logfake(emu, 0x4B7E00, 1, ret=13)  # Text_GetHeight
logfake(emu, 0x4B4A00, 10)  # Draw_RectA
logfake(emu, 0x424980, 4)
logfake(emu, 0x4C1780, 1)
logfake(emu, 0x4B64E0, 1, strings=(0,))  # Resource_Find
logfake(emu, 0x4B5F00, 2, strings=(0,), ret=0x5000)  # Resource_Load
LANG = cstr(emu, b"english")
logfake(emu, 0x406320, 1, effect=lambda e, a: LANG)
for slot in (0, 1, 2, DRAW, DAC, CHANGED, EV914, EV91C):
    for t in (WINDOW, TW, CHILD):
        set_handler(emu, slot, t, handler(emu, "%x_%x" % (slot, t), 2 if slot in (0, CHANGED) else 1))
u16(emu, 0x53B90C, WINDOW)
u32(emu, 0x53B914, EV914, EV918, EV91C)
u32(emu, 0x5488C8, DRAW)
u32(emu, 0x54DDA8, DA8, DAC)
emu.write(0x54E0E4, bytes([0x30, CHANGED]))
emu.baseline = emu.snapshot()


def run(orig, ours, setup, **kw):
    RET.clear()
    return check(emu, orig, ours, setup=setup, normalized=True, **kw)


LINES = [
    (0, 4, 1, 2, 0, 1, 0, b"Innerloop"),
    (0, 34, 3, 3, 2, 0, 0, b"Game Concept"),
    (5, 54, 2, 2, 0, 0, 0, b"Andrew Wensley"),
    (-20, 54, 4, 1, 0, 0, 0, b"Project Leader"),
    (-100, -1, 2, 2, -1, 0, 0, b""),
    (0, 4, 0, 0, 0, 1, 0, b"Programming"),
    (12, 40, 1, 3, 7, 0, 0, b"x"),
]


def lines(e, rows=LINES):
    p = e.alloc(0x24 * len(rows))
    for i, (x, y, color, font, pause, page, loc, s) in enumerate(rows):
        ps = cstr(e, s)
        e.write(p + 0x24 * i, struct.pack("<7i2I", x, y, color, font, pause, page, loc, ps, ps))
    return p


def box(e, fade=16, line=0, first=0, chars=0, delay=0, frame=0, fading=0, fonts=(1, 1, 1, 1), rows=LINES):
    tw = task(e, 0x88, TW)
    u32(e, tw + 0x20, 40, 30, 300, 200)
    mm = e.alloc(0x2898)
    u32(e, mm + 0xD0, fade)
    u32(e, tw + 0x44, mm)
    u32(e, tw + 0x58, lines(e, rows), len(rows), line, first, chars, delay, frame, fading)
    for i, on in enumerate(fonts):
        if on:
            t = e.alloc(0x28)
            f32(e, t + 4, 1.0, 2.0, 1.0, 0.5, 9.0, 9.0, 9.0)
            u32(e, t + 0x24, 0x100 | i)
            u32(e, tw + 0x78 + 4 * i, t)
    return tw


def test_create():
    def setup(e):
        tw = box(e)
        return tw, 0x1234
    run(0x41A370, "TypeWriterBox_Create", setup, check_eax=False)


@pytest.mark.parametrize("fonts", [(0, 0, 0, 0), (1, 0, 1, 1)])
def test_delete(fonts):
    run(0x41A3C0, "TypeWriterBox_Delete", lambda e: (box(e, fonts=fonts),), check_eax=False)


@pytest.mark.parametrize("menu", [0, 1])
@pytest.mark.parametrize("children", [0, 3])
@pytest.mark.parametrize("found", [0, 1])
def test_update_text(menu, children, found):
    def setup(e):
        e.call(0x4B0BA0)
        RET[0x4B64E0] = found
        rows = [r[:6] + (i % 2, r[7]) for i, r in enumerate(LINES)]
        tw = box(e, rows=rows)
        if not menu:
            u32(e, tw + 0x44, 0)
        for i in range(children):
            add_child(e, tw, task(e, 0x54, CHILD if i % 2 else TW))
        return (tw,)
    run(0x41A410, "TypeWriterBox_UpdateText", setup, check_eax=False)


@pytest.mark.parametrize("menu", [0, 1])
@pytest.mark.parametrize("fontnames", [(), (b"FONT1.FNT", b"font3.fnt"), (b"font4.fnt", b"font2.fnt", b"font1.fnt")])
def test_changed(menu, fontnames):
    def setup(e):
        e.call(0x4B0BA0)
        tw = box(e, fonts=(1, 0, 1, 0))
        mm = e.u32(tw + 0x44)
        for i, n in enumerate(fontnames):
            e.write(mm + 0x264 + 0x58 * i, n + b"\0")
        u16(e, mm + 0x2666, len(fontnames))
        if not menu:
            u32(e, tw + 0x44, 0)
        return tw, 0x4321
    run(0x41A4D0, "TypeWriterBox_Changed", setup, check_eax=False)


RUN_CASES = [dict(delay=5), dict(delay=0, line=0, chars=3), dict(delay=0, line=0, chars=9),
             dict(delay=0, line=3, chars=14), dict(delay=0, line=6, chars=1), dict(delay=0, line=1, chars=12),
             dict(delay=0, line=4, chars=0), dict(delay=2, keys=1)]


@pytest.mark.parametrize("c", RUN_CASES)
def test_run(c):
    def setup(e):
        tw = box(e, line=c.get("line", 0), chars=c.get("chars", 0), delay=c["delay"], frame=7)
        for k in (0x1D, 0x2A, 0x18):
            e.write(0xC28C62 + k, bytes([0x80 if c.get("keys") else 0]))
        return (tw,)
    run(0x41A600, "TypeWriterBox_Run", setup, check_eax=False)


DRAW_CASES = [
    dict(), dict(fade=4), dict(fade=8), dict(fade=12), dict(line=3, first=0, chars=5, frame=8),
    dict(line=3, first=0, chars=5, frame=7), dict(line=6, first=2, chars=1, frame=9),
    dict(line=1, first=5, chars=2, frame=8), dict(line=2, first=0, fading=1, delay=10, frame=8, chars=4),
    dict(line=2, first=0, fading=1, delay=20), dict(line=2, first=0, fading=1, delay=40),
    dict(line=5, first=0, fading=1, delay=0), dict(line=4, first=1, fonts=(1, 0, 1, 0), frame=8, chars=0),
    dict(line=3, first=3, chars=2, frame=8), dict(line=0, first=1),
]


@pytest.mark.parametrize("c", DRAW_CASES)
def test_draw(c):
    def setup(e):
        RET[0x4B7DF0] = c.get("width", 37)
        return (box(e, **c),)
    run(0x41A6D0, "TypeWriterBox_DrawHandler", setup, check_eax=False)


def test_get_params():
    def setup(e):
        u32(e, 0x57BB90, 0x9876)
        return 0, e.alloc(4)
    run(0x41A9E0, "TypeWriterBox_GetParams", setup, check_eax=False)


@pytest.mark.parametrize("children", [0, 2])
def test_restart(children):
    def setup(e):
        tw = box(e, line=3, first=2, chars=4, delay=9, frame=99, fading=1)
        for i in range(children):
            add_child(e, tw, task(e, 0x54, CHILD))
        return (tw,)
    run(0x41A9F0, "TypeWriterBox_Restart", setup, check_eax=False)


@pytest.mark.parametrize("orig,ours", [(0x41AA80, "TypeWriterBox_Activate"), (0x41AAA0, "TypeWriterBox_Deactivate")])
@pytest.mark.parametrize("active", [0, 1])
def test_activate(orig, ours, active):
    def setup(e):
        tw = box(e)
        u32(e, tw + 0x54, active)
        return (tw,)
    run(orig, ours, setup, check_eax=False)
