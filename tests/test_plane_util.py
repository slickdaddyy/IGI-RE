"""Helpers shared by the test_plane tests."""
import struct

from test_vehicle_util import (LOG, RET, add_child, cstr, d64, f32, floatfake, handler, init_emu, logfake,  # noqa: F401
                               pyfake, run, set_events, set_handler, setup_init, task, trap, u16, u32, vfake)
from emu import Emu

WORLD = 0x5000
PLANE, CHILD, GEAR, VEHICLE = 0x70, 0x71, 0x72, 0x73
EV_MOVE, EV_TABLE, EV_CHANGED, EV_E4 = 0x2D, 0x30, 0x31, 0x34
EV_CONTROL, EV_INFO, EV_4110, EV_SAVE, EV_LOAD, EV_KEYS, EV_SOUND = 0x33, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A
MORE_EVENTS = {0x547F40: EV_E4, 0x54E1DC: EV_INFO, 0x54E1DD: EV_4110, 0x54E1DE: EV_SAVE, 0x54E1DF: EV_LOAD,
               0x54E1E0: EV_KEYS, 0x548813: EV_SOUND}


def all_events(e):
    set_events(e)
    for a, v in MORE_EVENTS.items():
        e.write(a, bytes([v]))
    u16(e, 0x54E200, VEHICLE)
    u16(e, 0x57BD64, GEAR)


def stack(e, a, n):
    return e.read(a, n) if a else None


def sprintf(e, a):
    fmt = e.read_cstr(a[1])
    out = fmt.replace(b"%s", e.read_cstr(a[2]))
    e.write(a[0], out + b"\0")


def model_point(e, a):
    """Model_GetPoint(out, obj, i): a point that depends on the task."""
    obj = a[1]
    k = (obj >> 4) & 0xFF
    d64(e, a[0], 100.0 + k, -250.5 * k, -3000.25 + 7 * k)


def coll_lines(e, a):
    """Coll_Lines(world, hits, starts, ends, n, ...): hits from RET["hits"], a list of (found, dist)."""
    hits, n = a[1], a[4]
    LOG[-1].append(("coll", a[0], [e.read(a[2] + 24 * i, 24) for i in range(n)],
                    [e.read(a[3] + 24 * i, 24) for i in range(n)], n, a[5], e.u32(a[6] + 4), a[7]))
    for i, (found, dist) in enumerate(RET.get("hits", [])[:n]):
        u32(e, hits + 0x70 * i + 0x58, found)
        d64(e, hits + 0x70 * i + 0x60, dist)


def plane_emu():
    """An Emu with the subsystems the plane code calls replaced by loggers."""
    e = Emu()
    all_events(e)
    logfake(e, 0x416920, 0, ret=WORLD)  # Game_GetWorld
    logfake(e, 0x4C5710, 3)  # RigidDynCubeObj_Init
    vfake(e, 0x4C7140, 5, show=lambda e, a: (a[0], e.normalize(a[1]), e.read(a[2], 24), a[3], a[4]))
    vfake(e, 0x4C6940, 4, show=lambda e, a: (a[0], e.normalize(a[1]), e.read(a[2], 24), a[3]))
    logfake(e, 0x4C7560, 2)  # DynCube_Remove
    floatfake(e, 0x4D0950, 1)  # VirModel_GetRadius
    logfake(e, 0x4E6AB0, 1, ret=0x7100)  # SoundParams_New
    for a in (0x4F4640, 0x4F4660, 0x4F4680, 0x4F4800, 0x4F4820):
        logfake(e, a, 1)
    logfake(e, 0x4F1320, 1)  # VarString_Init
    logfake(e, 0x4F1340, 1)  # VarString_Free
    logfake(e, 0x4F1400, 2)  # VarString_Bind
    logfake(e, 0x4F16C0, 1)  # VarString_HasExpr
    floatfake(e, 0x4F16E0, 2)  # VarString_EvalReal
    logfake(e, 0x4E6B00, 2, strings=(1,))  # Sound_Play
    logfake(e, 0x401AE0, 1, ret=0x7000)  # TaskRef_Add
    logfake(e, 0x401B20, 1)  # TaskRef_Remove
    logfake(e, 0x4E6C00, 1)  # Sound_Stop
    logfake(e, 0x4C1830, 1)  # QTaskList_Free
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    logfake(e, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
    logfake(e, 0x4C48D0, 2)  # RigidDynCubeObj_SetModel
    logfake(e, 0x4EE140, 1, strings=(0,), ret=0x7200)  # PhysType_Find
    logfake(e, 0x4FB700, 2)  # Vehicle_SetPhysType
    logfake(e, 0x4ECDB0, 1)  # Body_Update
    vfake(e, 0x4ECF50, 2, show=lambda e, a: (e.normalize(a[0]), e.read(a[1], 24)))  # Body_AddForce
    vfake(e, 0x4A53B3, 3, show=lambda e, a: (e.read_cstr(a[1]), e.read_cstr(a[2])), effect=sprintf)
    logfake(e, 0x4FC7B0, 1)
    vfake(e, 0x4FC0E0, 7, show=lambda e, a: tuple(a[:6]) + (e.u32(a[6] + 4),))
    vfake(e, 0x4EC2C0, 6, show=lambda e, a: tuple(a[:5]) + (e.read(a[5], 12),))
    vfake(e, 0x414E20, 3, show=lambda e, a: (a[1], a[2]), effect=model_point)
    vfake(e, 0x4CC2A0, 8, show=lambda e, a: (), effect=coll_lines)
    logfake(e, 0x4F4780, 2)  # Vehicle_LoadState
    logfake(e, 0x4F46E0, 2)  # Vehicle_SaveState
    e.baseline = e.snapshot()
    return e


def msg_handler(e, tag, n):
    """A task handler that logs the task and n words behind its argument."""
    def fn(e):
        a = e.arg(1)
        LOG[-1].append((tag, e.normalize(e.arg(0)), [e.normalize(e.u32(a + 4 * i)) for i in range(n)] if a else None))
        e.ret(0)
    return trap(e, "msg_%s" % tag, fn)


def matrix(e, p, m, unk=0):
    f32(e, p, *m)
    u32(e, p + 0x24, unk)
