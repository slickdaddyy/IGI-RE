"""Helpers shared by the test_widget tests (MenuWindow and the windows built on it)."""
import struct

from emu import Emu, compare
from test_menu_util import LOG, RET, add_child, check, pyfake, cstr, f32, handler, logfake, set_handler, strip_logger, task, trap, u16, u32  # noqa: F401

WINDOW, BOX, CHILD, OTHER, MENU = 0x40, 0x43, 0x42, 0x50, 0x45
EV914, EV918, EV91C, DRAW, DA8, DAC, EV910, PLACE, SIZE = 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28
PARAMS, CHANGED = 0x30, 0x31
SLOTS = (0, 1, 2, DRAW, DAC, DA8, CHANGED, EV914, EV918, EV91C, EV910, PLACE, SIZE)
PARAM_TYPES = (0xA7186C, 0xA75898, 0xA70C7C, 0xA70C90, 0xA70C9C, 0xA71888, 0xA70C78)


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


def alloc_effect(size):
    return lambda e, a: e.alloc(size)


def get_string(e, a):
    p = e.alloc(0x40)
    e.write(p, b"S:" + e.read_cstr(a[1])[:0x3C] + b"\0")
    return p


def task_get_name(e):
    """Task_GetName, logging only the task (the output buffer is on the stack)."""
    if LOG:
        LOG[-1].append((0x4F1270, e.normalize(e.arg(1))))
    e.write(e.arg(0), b"Win%x\0" % (e.arg(1) & 0xFFF))
    e.ret(0)


def make_emu():
    """An Emu with the menu event slots set and logging handlers for every slot."""
    e = Emu()
    for slot in SLOTS:
        for t in (WINDOW, BOX, CHILD, OTHER):
            nargs = 2 if slot in (0, CHANGED, PLACE, SIZE) else 1
            set_handler(e, slot, t, handler(e, "%x_%x" % (slot, t), nargs))
    u16(e, 0x53B90C, WINDOW)
    u32(e, 0x53B910, EV910, EV914, EV918, EV91C)
    e.write(0x53B920, bytes([PLACE, SIZE]))
    u32(e, 0x5488C8, DRAW)
    u32(e, 0x54DDA8, DA8, DAC)
    e.write(0x54E0E4, bytes([PARAMS, CHANGED]))
    u16(e, 0x539824, MENU)  # MenuManager type
    u32(e, 0xC28B44, 800, 600)  # display size
    strip_logger(e)
    logfake(e, 0x4F16C0, 1)  # VarString_HasExpr
    floatfake(e, 0x4F16E0, 2)  # VarString_EvalReal
    pyfake(e, 0x4F1270, task_get_name)
    for addr, n in ((0x4B7FA0, 2), (0x4B7EE0, 2), (0x4B80A0, 1), (0x4B8920, 1), (0x4B8810, 2), (0x4B8890, 4)):
        logfake(e, addr, n, strings=(0,))  # script symbols
    logfake(e, 0x4B4E00, 9)  # Draw_Sprite
    logfake(e, 0x4B4A00, 10)  # Draw_RectA
    logfake(e, 0x4B6CE0, 1, effect=lambda e, a: a[0] and e.u32(a[0]))  # Sprite_Width
    logfake(e, 0x4B6D00, 1, effect=lambda e, a: a[0] and e.u32(a[0] + 4) or 3)  # Sprite_Height
    logfake(e, 0x4B6E90, 2, strings=(1,))  # Text_SetString
    logfake(e, 0x4B6EC0, 4, strings=(3,), effect=alloc_effect(0x28))  # Text_New
    logfake(e, 0x4B6F30, 1)  # Text_Free
    logfake(e, 0x4B6F50, 1, derefs={0: 0x28})  # Text_DrawAlt
    logfake(e, 0x418E40, 2, strings=(1,))  # MenuManager_FindFont
    logfake(e, 0x418DE0, 2, strings=(1,))  # MenuManager_FindSprite
    logfake(e, 0x418EA0, 2, strings=(1,), effect=get_string)  # MenuManager_GetString
    logfake(e, 0x418FE0, 2)  # MenuManager_FocusItem
    logfake(e, 0x419090, 1)  # MenuManager_RefreshScreen
    logfake(e, 0x4E7180, 3, strings=(0,))  # Sound_PlayName
    logfake(e, 0x4C1830, 1)  # QTaskList_Free
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    logfake(e, 0x401CF0, 2, masks={0: 0xFFFF, 1: 0xFFFF},
            effect=lambda e, a: (a[0] & 0xFFFF) != OTHER)  # TaskType_IsA
    e.baseline = e.snapshot()
    return e


def run(e, orig, ours, setup, **kw):
    RET.clear()
    kw.setdefault("check_eax", False)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def window(e, size, type_=BOX, x=40, y=30, w=300, h=200, fade=16):
    """A window task with a MenuManager."""
    t = task(e, size, type_)
    u32(e, t + 0x20, x, y, w, h)
    mm = task(e, 0x2898, MENU)
    u32(e, mm + 0xD0, fade)
    u32(e, t + 0x44, mm)
    return t


def input_state(e, w, button=0, pressed=0, released=0, entered=0, back=0, x=0, y=0):
    mm = e.u32(w + 0x44)
    e.write(mm + 0x27CC, bytes([button, pressed, released, entered, back, 0, 0, 0]))
    u32(e, mm + 0x27D8, x, y)


def shutdown_test(e, orig, ours, type_addr, params_addr=None):
    def setup(e):
        if params_addr:
            u32(e, params_addr, 0x5151)
        e.write(type_addr, b"\x44\x00")
    run(e, orig, ours, setup)


def init_emu():
    """An Emu for the task type registrations, with the original task system."""
    from test_menu_util import task_system
    e = Emu()

    def setup(e):
        task_system(e)
        for i, a in enumerate(PARAM_TYPES):
            u32(e, a, 0x100 + i)
        u32(e, 0x5488C8, 0x60)  # draw event
        u32(e, 0x54DDA8, 0x61, 0x62)  # activate, deactivate events
    return e, setup


def init_compare(e, orig, ours, setup):
    compare(e, orig, ours, setup=setup, check_eax=False, normalized=True)
