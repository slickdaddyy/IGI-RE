import pytest

from emu import Emu
from test_menu_util import RET, add_child, check, handler, logfake, set_handler, strip_logger, task, u16, u32

emu = Emu()
strip_logger(emu)
logfake(emu, 0x4B4AF0, 11)  # Draw_RectAZ

WINDOW, DIALOG, CHILD = 0x40, 0x41, 0x42
EV914, EV918, EV91C, DRAW, DA8, DAC, CHANGED = 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x31
for slot in (0, 1, 2, DRAW, DAC, CHANGED, EV918):
    for t in (WINDOW, DIALOG, CHILD):
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


def dialog(e, x=100, y=80, w=300, h=200, fade=16, horizontal=0):
    d = task(e, 0x5C, DIALOG)
    u32(e, d + 0x20, x, y, w, h)
    mm = e.alloc(0x2898)
    u32(e, mm + 0xD0, fade)
    u32(e, d + 0x44, mm)
    e.write(d + 0x59, bytes([horizontal]))
    return d


def test_create():
    def setup(e):
        d = dialog(e)
        e.write(d + 0x3C, b"\xAA" * 0x20)
        return d, 0x1234
    run(0x419BE0, "DialogWindow_Create", setup, check_eax=False)


@pytest.mark.parametrize("orig,ours", [(0x419C20, "DialogWindow_Delete"), (0x419C50, "DialogWindow_Changed"),
                                       (0x419C90, "DialogWindow_Run")])
def test_handlers(orig, ours):
    run(orig, ours, lambda e: (dialog(e), 0x5678), check_eax=False)


@pytest.mark.parametrize("fade", [0, 1, 7, 8, 9, 16, -3])
def test_draw(fade):
    run(0x419CB0, "DialogWindow_Draw", lambda e: (dialog(e, fade=fade),), check_eax=False)


@pytest.mark.parametrize("alpha", [0.0, 0.125, 0.5, 0.875, 1.0])
@pytest.mark.parametrize("box,screen", [((100, 80, 300, 200), (640, 480)), ((-5, 13, 77, 9), (800, 600)),
                                       ((0, 0, 641, 479), (1024, 768)), ((33, 41, 3, 1), (513, 385))])
def test_draw_frame(alpha, box, screen):
    import struct

    def setup(e):
        u32(e, 0xC28B44, *screen)
        u32(e, 0xA84A50, 0x1111, 0, 0x2222)
        return dialog(e, *box), struct.unpack("<I", struct.pack("<f", alpha))[0]
    run(0x419D30, "DialogWindow_DrawFrame", setup, check_eax=False)


def test_get_params():
    def setup(e):
        u32(e, 0x57BB88, 0x9876)
        return 0, e.alloc(4)
    run(0x41A110, "DialogWindow_GetParams", setup, check_eax=False)


@pytest.mark.parametrize("children", [0, 1, 4])
@pytest.mark.parametrize("handled", [0, 1])
def test_forward_event(children, handled):
    def setup(e):
        d = dialog(e)
        for i in range(children):
            add_child(e, d, task(e, 0x54, CHILD if i % 2 else DIALOG))
        if not handled:
            set_handler(e, EV918, CHILD, 0)
        u32(e, 0xAFA7E0, 3)
        return (d,)
    run(0x41A120, "MenuWindow_ForwardSave", setup, check_eax=False)


@pytest.mark.parametrize("value", [0, 1, 0x7F, 0x80, 0xFF])
def test_is_horizontal(value):
    run(0x41A1D0, "DialogWindow_IsHorizontal", lambda e: (dialog(e, horizontal=value),))
