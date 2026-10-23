"""Helpers shared by the test_nodereg tests."""
import math
import random
import struct

from emu import Emu
from test_dxinit_util import with_cw
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, task, trap, u16, u32)

NODE, PATH = 0x51, 0x52  # spline node and path task types used by the tests

CWS = [0x37F, 0x7F]
FISTP_CWS = [0x37F, 0x7F, 0xC7F, 0x77F]

# getters faked with a fixed return value: address -> value
GETTERS = {
    0x4F1A60: 0x20, 0x4F1A70: 0x21, 0x4F9270: 0x22, 0x4F11F0: 0x23, 0x4F1210: 0x24, 0x4F1220: 0x25,
    0x4EC040: 0x26, 0x4EC050: 0x27, 0x4EC060: 0x28, 0x4C56D0: 0x29, 0x4C56E0: 0x2A, 0x4C56F0: 0x2B,
    0x4D14D0: 0x2C, 0x4E55B0: 0x2D, 0x4E6610: 0x2E,
    0x4018F0: 0x10, 0x5034D0: 0x11, 0x4C48C0: 0x12, 0x4D9FA0: 0x13,
    0x4F0F50: 0x101, 0x4F0F60: 0x102, 0x4F0F70: 0x103, 0x4F0FA0: 0x104, 0x4F0FC0: 0x105, 0x4F0FE0: 0x106,
    0x4F1000: 0x107, 0x4F1010: 0x108,
}


def d64(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def u8(e, p, *v):
    e.write(p, bytes(v))


def rnd(seed):
    return random.Random(seed)


def counter(tag, start):
    """effect for logfake that returns start, start + 1, ... (reset by RET.clear())."""
    def fn(e, a):
        n = RET.get(tag, start)
        RET[tag] = n + 1
        return n
    return fn


def param_add(e):
    """ParamTable_Add logged with the field offset instead of the stack pointers."""
    def fn(e):
        t, base, field, typ, flags, extra = [e.arg(i) for i in range(6)]
        if LOG:
            LOG[-1].append(("ParamTable_Add", t, (field - base) & 0xFFFFFFFF, typ, flags, extra))
        e.ret(0)
    pyfake(e, 0x4F1A80, fn)


def reg_fakes(e):
    """Fakes for everything the task type Open and Close functions call."""
    for a, v in GETTERS.items():
        logfake(e, a, 0, ret=v)
    logfake(e, 0x401900, 5, strings=(0,), masks={2: 0xFFFF}, effect=counter("type", 0x60))  # TaskType_New
    logfake(e, 0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})  # TaskType_SetHandler
    logfake(e, 0x401530, 3, masks={0: 0xFFFF})  # TaskType_SetHandler2
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    logfake(e, 0x4017C0, 1, masks={0: 0xFFFF}, effect=counter("event", 0x40))  # TaskEvent_Alloc
    logfake(e, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
    logfake(e, 0x401F60, 1)  # Task_SetGlobalFlags
    logfake(e, 0x401F80, 1)  # Task_ClearGlobalFlags
    logfake(e, 0x401FA0, 2, masks={0: 0xFFFF})  # TaskType_SetFlags
    logfake(e, 0x4F1950, 1, effect=lambda e, a: 0x7000 + a[0])  # ParamTable_New
    logfake(e, 0x4F19C0, 2, strings=(0,), ret=0x333)  # ParamType_New
    logfake(e, 0x4F1A40, 1)  # ParamType_Free
    logfake(e, 0x4C1800, 1, ret=0x1800)  # QTaskList_New
    logfake(e, 0x4C1830, 1)  # QTaskList_Free
    logfake(e, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
    logfake(e, 0x4CEBA0, 1, strings=(0,), ret=0xCEBA)  # VirModel_Add
    param_add(e)
    e.baseline = e.snapshot()


def run(e, orig, ours, setup=None, cw=None, **kw):
    """check() with normalized memory and call logs, optionally at x87 control word cw."""
    RET.clear()
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)

    def s(e):
        RET.clear()
        return setup(e) if setup else None
    return check(e, orig, ours, setup=s, normalized=True, **kw)


def floatret(e, addr, nargs, value, derefs=None):
    """Replace addr with a logger that returns value(e, args) in st0 (as a double)."""
    slot = e.fake_top
    code = e.fake_top + 16
    e.fake_top += 48

    def fn(e):
        args = [e.arg(i + 1) for i in range(nargs)]
        shown = [e.read(a, derefs[i]) if derefs and i in derefs else e.normalize(a) for i, a in enumerate(args)]
        if LOG:
            LOG[-1].append((addr,) + tuple(shown))
        e.write(slot, struct.pack("<d", value(e, args) if callable(value) else value))
        e.ret(0)
    t = trap(e, "floatret_%x" % addr, fn)
    e.write(code, b"\xe8" + struct.pack("<i", t - (code + 5)) + b"\xdd\x05" + struct.pack("<I", slot) + b"\xc3")
    e.write(addr, b"\xe9" + struct.pack("<i", code - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)
    e.baseline = e.snapshot()


def new_emu():
    e = Emu()
    e.baseline = e.snapshot()
    return e


def quat(rng):
    q = [rng.uniform(-1, 1) for _ in range(4)]
    n = math.sqrt(sum(x * x for x in q))
    return [x / n for x in q]


def node(e, rng, parent=0, pos=None, tangent=None, auto=1, speed=None, typ=NODE):
    n = task(e, 0xC8, typ, parent)
    d64(e, n + 0x20, *(pos or [rng.uniform(-3000, 3000) for _ in range(3)]))
    f32(e, n + 0x38, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, n + 0x5C, rng.randrange(100))
    f32(e, n + 0x60, *quat(rng))
    u32(e, n + 0x70, 0x7070)
    d64(e, n + 0x78, *(tangent if tangent is not None else [rng.uniform(-500, 500) for _ in range(3)]))
    f32(e, n + 0x90, rng.uniform(0, 30) if speed is None else speed, rng.uniform(0, 10), rng.uniform(1, 400))
    f32(e, n + 0x9C, *[rng.uniform(1, 40) for _ in range(10)])
    u8(e, n + 0xC4, auto)
    return n


def path(e, rng, n, autos=None, extra=(), speeds=None):
    """A path with n node children (plus children of other types) and its node list filled."""
    p = task(e, 0x200, PATH)
    nodes = []
    for i in range(n):
        a = autos[i] if autos else 1
        k = node(e, rng, p, auto=a, speed=speeds[i] if speeds else None)
        add_child(e, p, k)
        nodes.append(k)
    for t in extra:
        add_child(e, p, task(e, 0x40, t))
    u32(e, p + 0x68, *nodes)
    u32(e, p + 0x1F8, n)
    return p, nodes
