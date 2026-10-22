"""Helpers shared by the test_menuscreen tests (front end widgets)."""
import struct

from emu import Emu
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, pyfake, set_handler,  # noqa: F401
                            strip_logger, task, trap, u16, u32)

WINDOW, BOX, CHILD, BUTTON = 0x40, 0x43, 0x42, 0x44
EV914, EV918, EV91C, DRAW, DA8, DAC, PLACE, PARAMS, CHANGED = 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x30, 0x31
SLOTS = (0, 1, 2, DRAW, DAC, DA8, CHANGED, EV914, EV918, EV91C, PLACE)


def floatfake(e, addr, nargs):
    """Replace addr with a logger that returns RET[addr] (a float) in st0."""
    slot = e.fake_top
    code = e.fake_top + 16
    e.fake_top += 48

    def fn(e):
        args = [e.arg(i + 1) for i in range(nargs)]
        if LOG:
            LOG[-1].append((addr,) + tuple(e.normalize(a) for a in args))
        e.write(slot, struct.pack("<f", RET.get(addr, 0.0)))
        e.ret(0)
    t = trap(e, "floatfake_%x" % addr, fn)
    e.write(code, b"\xe8" + struct.pack("<i", t - (code + 5)) + b"\xd9\x05" + struct.pack("<I", slot) + b"\xc3")
    e.write(addr, b"\xe9" + struct.pack("<i", code - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)
    e.baseline = e.snapshot()


def text_new(e, args):
    t = e.alloc(0x28)
    grid = e.alloc(0x10)
    u32(e, grid + 8, 0x777)
    u32(e, t, grid)
    u32(e, t + 0x20, 0x5A5A)
    u32(e, t + 0x24, 0x100)
    return t


def task_new(e, args):
    t = task(e, 0x300, args[1] & 0xFFFF, args[0])
    u32(e, t + 0x28, 0x10 + RET.get("bw", 0), 0x20 + RET.get("bh", 0))
    return t


def hit_test(e, args):
    if args[5]:
        u32(e, args[5], RET.get("mx", 0))
    if args[6]:
        u32(e, args[6], RET.get("my", 0))
    return 0


def get_name(e, args):
    e.write(args[0], b"Widget7\0")
    return 0


def make_emu():
    """An Emu with the menu event slots set and logging handlers for every slot."""
    e = Emu()
    for slot in SLOTS:
        for t in (WINDOW, BOX, CHILD, BUTTON):
            nargs = 2 if slot in (0, CHANGED, PLACE) else 1
            set_handler(e, slot, t, handler(e, "%x_%x" % (slot, t), nargs))
    u16(e, 0x53B90C, WINDOW)
    u32(e, 0x53B914, EV914, EV918, EV91C)
    e.write(0x53B920, bytes([PLACE]))
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
    # Text and sprites
    logfake(e, 0x4B6E90, 2, strings=(1,))  # Text_SetString
    logfake(e, 0x4B6EC0, 4, strings=(3,), effect=text_new)  # Text_New
    logfake(e, 0x4B6F30, 1)  # Text_Free
    logfake(e, 0x4B6F50, 1, derefs={0: 0x28})  # Text_DrawAlt
    logfake(e, 0x4B6E60, 1, derefs={0: 0x28})  # Text_Draw
    logfake(e, 0x4B7DF0, 1, ret=37)  # Text_GetWidth
    logfake(e, 0x4B7E00, 1, ret=13)  # Text_GetHeight
    logfake(e, 0x4B6E70, 1, ret=301)  # Text_Width
    logfake(e, 0x4B6E80, 1, ret=77)  # Text_Height
    logfake(e, 0x4B6CE0, 1, ret=9)  # Sprite_Width
    logfake(e, 0x4B6D00, 1, ret=11)  # Sprite_Height
    logfake(e, 0x4B7050, 2, strings=(1,), effect=lambda e, a: 7 * len(e.read_cstr(a[1])))  # Sprite_StringWidth
    logfake(e, 0x4B50B0, 11)  # QVertex_DrawFrame
    logfake(e, 0x4B51F0, 15)  # QVertex_DrawQuad
    logfake(e, 0x4B4AF0, 11)  # Draw_RectAZ
    # menu
    logfake(e, 0x418E40, 2, strings=(1,))  # MenuManager_FindFont
    logfake(e, 0x418DE0, 2, strings=(1,))  # MenuManager_FindSprite
    logfake(e, 0x418D80, 2, strings=(1,))  # MenuManager_FindPic
    logfake(e, 0x418EA0, 2, strings=(1,), effect=lambda e, a: a[1] + 0x100000)  # MenuManager_GetString
    logfake(e, 0x418D70, 1)  # MenuManager_Free
    logfake(e, 0x418FE0, 2)  # MenuManager_FocusItem
    logfake(e, 0x4190D0, 1)  # MenuManager_MarkBusy
    logfake(e, 0x419040, 1)  # MenuManager_GetBackgroundFX
    logfake(e, 0x424980, 4)  # MenuWindow_DrawEnd
    logfake(e, 0x4248A0, 7, effect=hit_test, masks={5: 0, 6: 0})  # MenuWindow_HitTest
    logfake(e, 0x4E7180, 3, strings=(0,))  # Sound_Play
    logfake(e, 0x4012A0, 3, masks={1: 0xFFFF}, effect=task_new)  # Task_New
    logfake(e, 0x4F1270, 2, effect=get_name, masks={0: 0})  # Task_GetName
    logfake(e, 0x4F1050, 1)  # Window_IndexOf
    logfake(e, 0x48F2D0, 0)  # AppContext_IsUSA
    for addr in (0x4B7E80, 0x4B7FA0, 0x4B7FC0):  # Script_RegisterInt / Int2 / Real
        logfake(e, addr, 2, strings=(0,))
    logfake(e, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
    logfake(e, 0x4C1830, 1)  # QTaskList_Free
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    e.baseline = e.snapshot()
    return e


def run(e, orig, ours, setup, **kw):
    RET.clear()
    kw.setdefault("check_eax", False)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def window(e, size, type_=BOX, x=40, y=30, w=300, h=200, fade=16, menu=True):
    """A widget task with a MenuManager."""
    t = task(e, size, type_)
    u32(e, t + 0x20, x, y, w, h)
    if menu:
        mm = e.alloc(0x2898)
        u32(e, mm + 0xD0, fade)
        u32(e, t + 0x44, mm)
    return t


def button(e, w=16, h=12, menu=0):
    b = task(e, 0x98, BUTTON)
    u32(e, b + 0x28, w, h)
    u32(e, b + 0x44, menu)
    return b


def shutdown_test(e, orig, ours, type_addr, params_addr):
    def setup(e):
        u32(e, params_addr, 0x5151)
        e.write(type_addr, b"\x44\x00")
    run(e, orig, ours, setup)


def get_params_test(e, orig, ours, params_addr):
    def setup(e):
        u32(e, params_addr, 0x9876)
        return 0, e.alloc(4)
    run(e, orig, ours, setup)


def register_test(e, orig, ours, flag_off, size, on):
    def setup(e):
        t = window(e, size)
        e.write(t + flag_off, bytes([on]))
        return (t,)
    run(e, orig, ours, setup)
