"""Helpers shared by the test_ui tests (menu widgets)."""
import struct

from emu import Emu
from test_menu_util import LOG, RET, add_child, check, cstr, f32, handler, logfake, set_handler, task, trap, u16, u32  # noqa: F401

WINDOW, BOX, CHILD = 0x40, 0x43, 0x42
EV914, EV918, EV91C, DRAW, DA8, DAC, PARAMS, CHANGED = 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x30, 0x31
SLOTS = (0, 1, 2, DRAW, DAC, DA8, CHANGED, EV914, EV918, EV91C)


def floatfake(e, addr, nargs, effect=None):
    """Replace addr with a logger that returns RET[addr] (a float) in st0."""
    slot = e.fake_top
    code = e.fake_top + 16
    e.fake_top += 48

    def fn(e):
        args = [e.arg(i + 1) for i in range(nargs)]
        if LOG:
            LOG[-1].append((addr,) + tuple(e.normalize(a) for a in args))
        if effect:
            effect(e, args)
        e.write(slot, struct.pack("<f", RET.get(addr, 0.0)))
        e.ret(0)
    t = trap(e, "floatfake_%x" % addr, fn)
    e.write(code, b"\xe8" + struct.pack("<i", t - (code + 5)) + b"\xd9\x05" + struct.pack("<I", slot) + b"\xc3")
    e.write(addr, b"\xe9" + struct.pack("<i", code - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)
    e.baseline = e.snapshot()


def text_new(e, args):
    t = e.alloc(0x28)
    u32(e, t + 0x20, 0x5A5A)
    u32(e, t + 0x24, 0x100)
    return t


def make_emu():
    """An Emu with the menu event slots set and logging handlers for every slot."""
    e = Emu()
    for slot in SLOTS:
        for t in (WINDOW, BOX, CHILD):
            nargs = 2 if slot in (0, CHANGED) else 1
            set_handler(e, slot, t, handler(e, "%x_%x" % (slot, t), nargs))
    u16(e, 0x53B90C, WINDOW)
    u32(e, 0x53B914, EV914, EV918, EV91C)
    u32(e, 0x5488C8, DRAW)
    u32(e, 0x54DDA8, DA8, DAC)
    e.write(0x54E0E4, bytes([PARAMS, CHANGED]))
    u32(e, 0xC28B44, 800, 600)  # display size
    # VarString
    logfake(e, 0x4F1320, 1)
    logfake(e, 0x4F1340, 1)
    logfake(e, 0x4F1400, 2)
    logfake(e, 0x4F16C0, 1)
    floatfake(e, 0x4F16E0, 2)
    logfake(e, 0x419070, 2)
    # Text
    logfake(e, 0x4B6E90, 2, strings=(1,))  # Text_SetString
    logfake(e, 0x4B6EC0, 4, strings=(3,), effect=text_new)  # Text_New
    logfake(e, 0x4B6F30, 1)  # Text_Free
    logfake(e, 0x4B6F50, 1, derefs={0: 0x28})  # Text_DrawAlt
    logfake(e, 0x4B7DF0, 1, ret=37)  # Text_GetWidth
    logfake(e, 0x4B7E00, 1, ret=13)  # Text_GetHeight
    # menu
    logfake(e, 0x418E40, 2, strings=(1,))  # MenuManager_FindFont
    logfake(e, 0x418DE0, 2, strings=(1,))  # MenuManager_FindSprite
    logfake(e, 0x418EA0, 2, strings=(1,), effect=lambda e, a: cstr(e, b"RES:" + e.read_cstr(a[1])))  # MenuManager_GetString
    logfake(e, 0x424980, 4)  # MenuWindow_DrawEnd
    logfake(e, 0x4E7180, 3, strings=(0,))  # Sound_PlayName
    logfake(e, 0x4C1830, 1)  # QTaskList_Free
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    e.baseline = e.snapshot()
    return e


def run(e, orig, ours, setup, **kw):
    RET.clear()
    kw.setdefault("check_eax", False)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def window(e, size, type_=BOX, x=40, y=30, w=300, h=200, fade=16):
    """A widget task with a MenuManager."""
    t = task(e, size, type_)
    u32(e, t + 0x20, x, y, w, h)
    mm = e.alloc(0x2898)
    u32(e, mm + 0xD0, fade)
    u32(e, t + 0x44, mm)
    return t


def shutdown_test(e, orig, ours, type_addr, params_addr):
    def setup(e):
        u32(e, params_addr, 0x5151)
        e.write(type_addr, b"\x44\x00")
    run(e, orig, ours, setup)
