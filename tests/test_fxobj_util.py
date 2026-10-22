"""Helpers shared by the test_fxobj tests."""
import struct

from emu import Emu
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, task, task_system, trap, u16, u32)
from test_vehicle_util import d64, floatfake, vfake  # noqa: F401

# event getters' globals and the slots they give
EVENTS = {0x54E0E4: 0x30, 0x54E0E5: 0x31, 0x54DDA8: 0x24, 0x54DDAC: 0x25, 0x538518: 0x26, 0x538519: 0x27,
          0x53851A: 0x28, 0x539542: 0x29, 0x539543: 0x2A, 0x539549: 0x2B, 0x53954B: 0x2C, 0x547F40: 0x2D,
          0x548813: 0x2E, 0x540904: 0x2F, 0x539548: 0x32}
PARAM_TYPES = (0xA70C84, 0xA71870, 0xA70C7C, 0xA70C9C, 0xA71884, 0xA70C90, 0xA70C88, 0xA70CA4, 0xA71880,
               0xA70C6C, 0xA71874, 0xA7186C, 0xA75898, 0xA71888)
# globals holding the parent types used by the registrations
PARENTS = (0x54E634, 0x53C7DC, 0x548698, 0x548648, 0x547F42)


def set_events(e):
    for a, v in EVENTS.items():
        e.write(a, bytes([v]))
    for a in (0x54DDA8, 0x54DDAC):
        u32(e, a, EVENTS[a])


def init_emu():
    """An Emu for the task type registrations, with the original task system."""
    e = Emu()
    logfake(e, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
    for a, r in ((0x4169D0, 0x6100), (0x416930, 0x6200), (0x4CEBA0, 0x6300)):
        logfake(e, a, 1, strings=(0,), ret=r)  # Game_AddSprite, Game_AddTexture, VirModel_Add
    e.baseline = e.snapshot()
    return e


def setup_init(e):
    task_system(e)
    set_events(e)
    for i, a in enumerate(PARAM_TYPES):
        u32(e, a, 0x100 + i)
    root = e.call(0x4018F0)[0] & 0xFFFF
    for i, a in enumerate(PARENTS):
        t = e.call(0x401900, cstr(e, b"Parent%d" % i), 0xF0, root, 0)[0]
        u16(e, a, t)
    table = e.call(0x4F1950, 3)[0]
    u32(e, 0x57C1FC, table)


def run(e, orig, ours, setup, **kw):
    RET.clear()
    kw.setdefault("check_eax", False)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def d64s(e, p, n):
    return struct.unpack("<%dd" % n, e.read(p, 8 * n))


def f32s(e, p, n):
    return struct.unpack("<%df" % n, e.read(p, 4 * n))
