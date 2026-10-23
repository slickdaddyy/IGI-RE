"""Helpers for the test_define tests: an Emu with fakes for everything the sound
definitions call, script argument fakes and x87 control word trampolines."""
import struct

from emu import Emu
from test_menu_util import (LOG, RET, add_child, check, handler, logfake, logger, pyfake, set_handler, task,  # noqa: F401
                            trap, u16, u32)

# event numbers the fake getters hand out
PLAY, LIST, INFO, FIND, NAME, SETPOS, SETVOL, SETPITCH, SETGLOBAL, RANGE, DURATION, STOP = range(0x30, 0x3C)
NEARCLIP, WEV0, WEV1, DRAW = 0x3C, 0x3D, 0x3E, 0x3F
GETTERS = {0x4EBDD0: PLAY, 0x4E65F0: LIST, 0x4E6610: INFO, 0x4E6630: FIND, 0x4E6650: NAME, 0x4E6670: SETPOS,
           0x4E6690: SETVOL, 0x4E66B0: SETPITCH, 0x4E66D0: SETGLOBAL, 0x4E66F0: RANGE, 0x4E6710: DURATION,
           0x4E6730: STOP, 0x4D14D0: NEARCLIP, 0x4CEA10: WEV0, 0x4CEA20: WEV1, 0x4E7530: DRAW}
GROUP = 0x48  # the SoundGroup type (0x54DB4A)
RATE = 0xA5E5FC

ARGS = {}       # script argument index -> value
TEMPLATE = []   # what Task_New logs of a definition: (offset, size) or ("s", offset)
REAL = {}       # address -> python float returned in st0


def stfake(e, addr, nargs, value, strings=()):
    """Replace addr with a logger that returns value(e, args) as a double in st0."""
    slot = e.fake_top
    code = slot + 16
    e.fake_top += 48

    def fn(e):
        args = [e.arg(i + 1) for i in range(nargs)]
        if LOG:
            LOG[-1].append((addr,) + tuple(e.read_cstr(a) if i in strings else a for i, a in enumerate(args)))
        e.write(slot, struct.pack("<d", value(e, args)))
        e.ret(0)
    t = trap(e, "stfake_%x" % addr, fn)
    e.write(code, b"\xe8" + struct.pack("<i", t - (code + 5)) + b"\xdd\x05" + struct.pack("<I", slot) + b"\xc3")
    e.write(addr, b"\xe9" + struct.pack("<i", code - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)
    e.baseline = e.snapshot()


def _task_new(e):
    parent, type_, arg = e.arg(0), e.arg(1) & 0xFFFF, e.arg(2)
    shown = 0
    if arg:
        d = e.u32(arg)
        shown = [e.read(arg + 4, 1)]
        for f in TEMPLATE:
            shown.append(e.read_cstr(d + f[1]) if f[0] == "s" else e.read(d + f[0], f[1]))
        shown = tuple(shown)
    if LOG:
        LOG[-1].append(("Task_New", parent, type_, shown))
    e.ret(RET[0x4012A0] if 0x4012A0 in RET else task(e, 0x200, type_, parent))


def _arg_string(e, args):
    s = ARGS[args[1]]
    e.write(args[2], s + b"\0")
    return 0


def make_emu():
    e = Emu()
    for addr, ev in GETTERS.items():
        logfake(e, addr, 0, effect=lambda e, a, ev=ev: ev)
    pyfake(e, 0x4012A0, _task_new)
    logfake(e, 0x4015F0, 1)  # Task_Delete
    logfake(e, 0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})  # TaskType_SetHandler
    logfake(e, 0x401530, 3, masks={0: 0xFFFF})  # TaskType_SetHandler2
    logfake(e, 0x401900, 4, strings=(0,), masks={2: 0xFFFF}, ret=0x77)  # TaskType_New
    logfake(e, 0x4018F0, 0, ret=0x12)  # Task_GetRootType
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    logfake(e, 0x4E6D20, 1)  # Sound_AddTask
    logfake(e, 0x4E6D40, 1)  # Sound_RemoveTask
    logfake(e, 0x4E6D60, 1, derefs={0: 0x68})  # SoundHandle_Play
    logfake(e, 0x4E6FA0, 1, derefs={0: 0x68})  # SoundHandle_IsPlaying
    logfake(e, 0x4EAEB0, 1)  # SoundDefGraph_Update
    logfake(e, 0x512BA0, 0)  # glow drawing
    logfake(e, 0x4B4820, 9)  # Draw_Rect
    logfake(e, 0x497570, 2)  # Scene_Init
    logfake(e, 0x4975F0, 1)  # Scene_Free
    logfake(e, 0x497620, 2)  # Scene_Select
    logfake(e, 0x497680, 2, derefs={1: 16})  # Scene_SetRect
    logfake(e, 0x4B8A80, 4, masks={2: 0}, effect=_arg_string)  # Script_GetArgString
    logfake(e, 0x4B8A20, 2, effect=lambda e, a: ARGS[a[1]])  # Script_GetArgInt
    stfake(e, 0x4B8A50, 2, lambda e, a: ARGS[a[1]])  # Script_GetArgReal
    stfake(e, 0x496DB0, 2, lambda e, a: REAL[0x496DB0])  # Sound_GetDuration
    u16(e, 0x54DB4A, GROUP)
    e.call(0x4A537B)  # the C runtime's float formatting hooks, or %f fails
    e.out = e.alloc(24)  # a script value at a fixed place
    e.baseline = e.snapshot()
    return e


def with_cw(e, target, cw):
    """An entry point that loads x87 control word cw and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    e.cw_tramps = getattr(e, "cw_tramps", {})
    if (addr, cw) not in e.cw_tramps:
        p = e.fake_top
        e.fake_top += 32
        code = b"\xd9\x2d" + struct.pack("<I", p + 16)
        code += b"\xe9" + struct.pack("<i", addr - (p + len(code) + 5))
        e.write(p, code.ljust(16, b"\x90") + struct.pack("<H", cw))
        e.cw_tramps[(addr, cw)] = p
    return e.cw_tramps[(addr, cw)]


def run(e, orig, ours, setup=None, cw=None, **kw):
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p
