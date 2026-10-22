"""Helpers shared by the test_vehicle tests."""
import struct

from emu import Emu
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, task, task_system, trap, u16, u32)

# event getters and the globals they read
EVENTS = {0x54E0E4: 0x30, 0x54E0E5: 0x31, 0x54DDA8: 0x24, 0x54DDAC: 0x25, 0x548177: 0x26, 0x548176: 0x27,
          0x539549: 0x28, 0x53954B: 0x29, 0x547F3C: 0x2A, 0x547F3D: 0x2B, 0x547F3E: 0x2C, 0x54864A: 0x2D,
          0x54127C: 0x2E, 0x54E203: 0x2F, 0x548440: 0x32, 0x54E1DA: 0x33}
PARAM_TYPES = (0xA7186C, 0xA75898, 0xA70C7C, 0xA70C88, 0xA70C90, 0xA70C9C, 0xA71888)


def d64(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def floatfake(e, addr, nargs, effect=None, derefs=None):
    """Replace addr with a logger that returns RET[addr] (a float, or a double when
    RET[addr] is a tuple) in st0."""
    slot = e.fake_top
    code = e.fake_top + 16
    e.fake_top += 48

    def fn(e):
        args = [e.arg(i + 1) for i in range(nargs)]
        shown = [e.read(a, derefs[i]) if derefs and i in derefs else e.normalize(a) for i, a in enumerate(args)]
        if LOG:
            LOG[-1].append((addr,) + tuple(shown))
        v = effect(e, args) if effect else RET.get(addr, 0.0)
        e.write(slot, struct.pack("<d", v))
        e.ret(0)
    t = trap(e, "floatfake_%x" % addr, fn)
    e.write(code, b"\xe8" + struct.pack("<i", t - (code + 5)) + b"\xdd\x05" + struct.pack("<I", slot) + b"\xc3")
    e.write(addr, b"\xe9" + struct.pack("<i", code - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)
    e.baseline = e.snapshot()


def set_events(e):
    for a, v in EVENTS.items():
        e.write(a, bytes([v]))


def init_emu():
    """An Emu for the task type registration functions, with the original task system."""
    e = Emu()
    logfake(e, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
    e.baseline = e.snapshot()
    return e


def setup_init(e):
    task_system(e)
    set_events(e)
    for i, a in enumerate(PARAM_TYPES):
        u32(e, a, 0x100 + i)
    t = e.call(0x401900, cstr(e, b"RigidDynCubeObj"), 0xF0, e.call(0x4018F0)[0] & 0xFFFF, 0)[0]
    u16(e, 0x548648, t)  # parent of the vehicle parts


def run(e, orig, ours, setup, **kw):
    RET.clear()
    kw.setdefault("check_eax", False)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def vfake(e, addr, nargs, show=None, effect=None, ret=0):
    """Replace addr with fn(e) that logs (addr,) + show(e, args) (default: the
    normalized args) and returns effect(e, args), RET[addr] or ret."""
    def fn(e):
        args = [e.arg(i) for i in range(nargs)]
        if LOG:
            LOG[-1].append((addr,) + tuple(show(e, args) if show else [e.normalize(a) for a in args]))
        r = effect(e, args) if effect else RET.get(addr, ret)
        e.ret(r or 0)
    pyfake(e, addr, fn)
