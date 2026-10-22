"""Helpers shared by the test_comprun tests."""
import struct

from emu import Emu, compare
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, task, task_system, trap, u16, u32)
from test_vehicle_util import d64, floatfake, vfake  # noqa: F401

CW = {}

# getters of parent task types, all made to return the root type
PARENT_GETTERS = (0x4C48C0, 0x4E0830, 0x4D9FA0, 0x4F45F0)
# event slot getters and the slot each one returns
EVENT_GETTERS = {0x416830: 0x30, 0x416850: 0x31, 0x416880: 0x32, 0x4167E0: 0x33, 0x4F1A60: 0x34, 0x4F1A70: 0x35,
                 0x4F1210: 0x36, 0x4F1220: 0x37, 0x467D90: 0x38, 0x4CEA10: 0x39, 0x4CEA20: 0x3A, 0x4C56F0: 0x3B,
                 0x413B20: 0x3C, 0x489630: 0x3D, 0x489610: 0x3E}
# parameter type getters and enum getters, each returning a marker
PARAM_GETTERS = (0x4F1010, 0x4F0F70, 0x4F0FD0, 0x4F1E20, 0x4F0FB0, 0x4F0F30, 0x4F0F50, 0x4F0FC0, 0x413DE0,
                 0x47CF10)


def with_cw(e, target, cw):
    """Entry that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    if (e, addr, cw) not in CW:
        p = e.fake_top
        e.fake_top += 32
        e.write(p + 16, struct.pack("<H", cw))
        e.write(p, b"\xd9\x2d" + struct.pack("<I", p + 16) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        e.baseline = e.snapshot()
        CW[(e, addr, cw)] = p
    return CW[(e, addr, cw)]


def run(e, orig, ours, setup, cw=None, **kw):
    """check() with normalized memory; cw runs both at that x87 control word."""
    RET.clear()
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def init_emu():
    """An Emu for the task type registrations: original task system, faked getters."""
    e = Emu()
    snap = e.snapshot()
    task_system(e)
    root = e.call(0x4018F0)[0] & 0xFFFF
    e.restore(snap)
    for a in PARENT_GETTERS:
        logfake(e, a, 0, ret=root)
    for a, v in EVENT_GETTERS.items():
        logfake(e, a, 0, ret=v)
    for i, a in enumerate(PARAM_GETTERS):
        logfake(e, a, 0, ret=0x6100 + i)
    logfake(e, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
    e.baseline = e.snapshot()
    return e


def check_init(e, orig, ours):
    compare(e, orig, ours, setup=lambda em: task_system(em), check_eax=False, normalized=True)


def obj(e, size, type_=0x50, scale=(1.0, 2.0, 1.5), model=0x7600, world=0x9998):
    """A RigidDynCubeObj based task with a position, matrix and scale."""
    t = task(e, size, type_)
    d64(e, t + 0x20, 100.5, -200.25, 300.125)
    u32(e, t + 0x68, world, model)
    f32(e, t + 0x70, *[0.1 * i for i in range(9)])
    u32(e, t + 0x94, 7)
    f32(e, t + 0x98, *scale)
    return t
