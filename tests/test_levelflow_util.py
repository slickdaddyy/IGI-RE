"""Helpers shared by the test_levelflow tests."""
import struct

from emu import Emu
from test_comprun_util import (CW, LOG, RET, add_child, check, check_init, cstr, d64, f32, floatfake,  # noqa: F401
                               handler, init_emu as comprun_init_emu, logfake, logger, pyfake, run, set_handler, task,
                               task_system, trap, u16, u32, vfake, with_cw)

# event slots returned by the faked getters
EVENTS = {0x4F1A60: 0x34, 0x4F1A70: 0x35, 0x4F1220: 0x37, 0x467D90: 0x38, 0x4CEA10: 0x39, 0x4CEA20: 0x3A,
          0x4C5800: 0x3F}
CHILD = 0x60


def get_name(e, a):
    e.write(a[0], b"obj_7\0")


def make_emu():
    """An Emu with the usual callees of the task handlers faked."""
    e = Emu()
    for a, v in EVENTS.items():
        logfake(e, a, 0, ret=v)
    for a, n in ((0x4F1320, 1), (0x4F1340, 1), (0x4F1400, 2), (0x4015F0, 1), (0x4C1830, 1)):
        logfake(e, a, n)
    logfake(e, 0x4F1050, 1, effect=lambda e, a: RET.get("id", 0))  # Task_GetId
    logfake(e, 0x4F16C0, 1, effect=lambda e, a: RET["has"].pop(0))  # VarString_HasExpr
    floatfake(e, 0x4F16E0, 2, effect=lambda e, a: RET["evals"].pop(0))  # VarString_EvalReal
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    for a in (0x4B7E80, 0x4B7EE0, 0x4B7FA0):  # Script_RegisterInt / Bool8 / Int2
        logfake(e, a, 2, strings=(0,))
    logfake(e, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
    logfake(e, 0x4AF810, 1, strings=(0,))  # Warning_Show
    vfake(e, 0x4F1270, 2, show=lambda e, a: (e.normalize(a[1]),), effect=get_name)  # Task_GetName
    e.baseline = e.snapshot()
    return e


def make_init_emu():
    """An Emu for the task type registrations."""
    e = comprun_init_emu()
    snap = e.snapshot()
    task_system(e)
    root = e.call(0x4018F0)[0] & 0xFFFF
    e.restore(snap)
    logfake(e, 0x5034D0, 0, ret=root)  # Position_GetType
    logfake(e, 0x4C5800, 0, ret=0x3F)
    for i, a in enumerate((0x4F0FA0, 0x4F0FF0, 0x4F1000, 0x4F0FC0)):
        logfake(e, a, 0, ret=0x6200 + i)
    logfake(e, 0x4F1CB0, 0, ret=0x6300)  # Param_Range0100
    logfake(e, 0x4CEBA0, 1, strings=(0,), ret=0x6400)  # VirModel_Add
    e.baseline = e.snapshot()
    return e


def kids(e, parent, n, slot, fn, size=0x20):
    """n children of types CHILD.., the second one without a handler for slot."""
    out = []
    for i in range(n):
        c = task(e, size, CHILD + i)
        add_child(e, parent, c)
        set_handler(e, slot, CHILD + i, fn if i != 1 else 0)
        out.append(c)
    return out


def s32(e, p):
    return struct.unpack("<i", e.read(p, 4))[0]
