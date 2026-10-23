"""Helpers shared by the test_terredit tests (0x52B040 - 0x52F3D0, no tests in here)."""
import struct

from test_menu_util import (LOG, RET, add_child, cstr, handler, logfake, pyfake, set_handler, task, trap,  # noqa: F401
                            u16, u32)
from test_mission_util import make_emu, run, with_cw  # noqa: F401

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]
WORLD = 0xA44344


def u8(e, p, *v):
    e.write(p, bytes(v))


def d64(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def f32(e, p, *v):
    e.write(p, struct.pack("<%df" % len(v), *v))


def cstr_or_null(e, s):
    return 0 if s is None else cstr(e, s)


def bits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def drop_points(e):
    """Coll_DropPointsMat: logs the query; the hit comes from RET['drops'], a list of
    (found, z, dist) used in turn (the last one repeats)."""
    world, hit, pts, count, dz, unk, unk2, flags = (e.arg(i) for i in range(8))
    if LOG:
        LOG[-1].append(("drop", world, e.read(pts, 24 * count), count, e.read(dz, 8), unk, unk2, flags))
    seq = RET.get("drops", [(1, 100.0, 50.0)])
    i = RET.get("drop_i", 0)
    RET["drop_i"] = i + 1
    found, z, dist = seq[min(i, len(seq) - 1)]
    x, y, _ = struct.unpack("<3d", e.read(pts, 24))
    d64(e, hit + 8, x + 0.5, y - 0.25, z)
    u32(e, hit + 0x58, found)
    d64(e, hit + 0x60, dist)
    e.ret(0)


def point_materials(e):
    """Coll_GetPointMaterials: logs the query; the material's float at +0xC comes from
    RET['mats'] in turn (the last one repeats)."""
    world, out, pts, count, unk, query, flags = (e.arg(i) for i in range(7))
    if LOG:
        LOG[-1].append(("mat", world, e.read(pts, 24 * count), count, unk & 0xFF, e.u32(query + 4), flags))
    seq = RET.get("mats", [1.0])
    i = RET.get("mat_i", 0)
    RET["mat_i"] = i + 1
    m = e.alloc(16)
    f32(e, m + 0xC, seq[min(i, len(seq) - 1)])
    u32(e, out, m)
    e.ret(0)


def rigid_init(e, a):
    u32(e, a[1] + 0x68, a[0])
    return 0


def make():
    """make_emu() of the mission tests plus the outside callees of this range."""
    e = make_emu()
    for addr, v in ((0x4C48C0, 0x14), (0x4D9610, 0x15), (0x505C80, 0x16), (0x4E5360, 0x35), (0x4D14D0, 0x36),
                    (0x504DF0, 0x37), (0x4F0F20, 0xF20), (0x4F0F80, 0xF80), (0x4F0FA0, 0xFA0)):
        logfake(e, addr, 0, ret=v)
    logfake(e, 0x4F1950, 1, ret=0x1950)  # ParamTable_New
    logfake(e, 0x4D9FC0, 8, derefs={0: 24, 1: 24}, ret=0x4D9FC0)  # HazeLayer_New
    logfake(e, 0x4DA0A0, 9, derefs={1: 24, 2: 24})  # HazeLayer_Set
    logfake(e, 0x4DA160, 1)  # HazeLayer_Delete
    logfake(e, 0x401780, 1)  # Task_DeleteChildren
    logfake(e, 0x4CEC10, 1, strings=(0,), ret=0xC10)  # VirModel_Find
    logfake(e, 0x4CEBA0, 1, strings=(0,), ret=0xCBA0)  # VirModel_Add
    logfake(e, 0x4C5710, 3, effect=rigid_init)  # RigidDynCubeObj_Init
    logfake(e, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
    logfake(e, 0x4C6E70, 5, derefs={2: 24, 3: 24, 4: 24})  # DynCube_MoveBox
    logfake(e, 0x4C7560, 2)  # DynCube_Remove
    logfake(e, 0x4C6890, 3)  # DynCube_ListAdd
    logfake(e, 0x4C1780, 2)  # Debug_Log
    logfake(e, 0x504DC0, 2)  # MenuItemTask_AddToList
    logfake(e, 0x4B6F90, 2)  # Text_GetRect
    logfake(e, 0x495200, 1)  # Direct3D_AddRestoreCallback
    logfake(e, 0x4B0D40, 0)  # Mem_SetDefaultMode
    logfake(e, 0x4B0D50, 0)  # Mem_ClearMode
    logfake(e, 0x4B0DB0, 0, effect=lambda e, a: RET.get("mode", 1))  # Mem_GetMode
    pyfake(e, 0x4CB9F0, drop_points)
    pyfake(e, 0x4C9750, point_materials)
    e.baseline = e.snapshot()
    return e


def world(e, offset=(0x12345, -0x5000, 0x7000)):
    """A Terrain with an offset, made the current world."""
    w = e.alloc(0xE00)
    u32(e, w + 0xDE0, *offset)
    u32(e, WORLD, w)
    return w
