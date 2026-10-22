"""Helpers shared by the test_props tests."""
import itertools
import random
import struct

from test_vehicle_util import (LOG, RET, add_child, cstr, d64, f32, floatfake, handler, init_emu, logfake,  # noqa: F401
                               pyfake, run, set_events, set_handler, setup_init, task, u16, u32, vfake)

EV_MOVE = 0x34  # RigidDynCubeObj event 4 (0x547F40)
TINY = 2.0 ** -70


def make_emu():
    e = init_emu()
    set_events(e)
    e.write(0x547F40, bytes([EV_MOVE]))
    logfake(e, 0x416920, 0, ret=0x9999)  # Game_GetWorld
    floatfake(e, 0x4D0950, 1)  # VirModel_GetRadius
    vfake(e, 0x4C6940, 4, show=lambda e, a: (a[0], e.normalize(a[1]), e.read(a[2], 24), a[3]))  # DynCube_Move
    logfake(e, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
    logfake(e, 0x4C7560, 2)  # DynCube_Remove
    logfake(e, 0x4C5710, 3)  # RigidDynCubeObj_Init
    logfake(e, 0x4C48D0, 2)  # RigidDynCubeObj_SetModel
    logfake(e, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
    logfake(e, 0x4F1320, 1)  # VarString_Init
    logfake(e, 0x4F1340, 1)  # VarString_Free
    logfake(e, 0x4F1400, 2)  # VarString_Bind
    logfake(e, 0x4F16C0, 1)  # VarString_HasExpr
    floatfake(e, 0x4F16E0, 2)  # VarString_EvalReal
    logfake(e, 0x4F1050, 1)  # Task_GetId
    logfake(e, 0x4B7E80, 2, strings=(0,))  # Script_RegisterInt
    logfake(e, 0x4B7EE0, 2, strings=(0,))  # Script_RegisterBool8
    logfake(e, 0x4B7FC0, 2, strings=(0,))  # Script_RegisterReal
    logfake(e, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    logfake(e, 0x4C1830, 1)  # QTaskList_Free
    logfake(e, 0x4C1790, 2)  # QTaskList_Add
    logfake(e, 0x4C17C0, 2)  # QTaskList_Remove
    logfake(e, 0x4168A0, 0, ret=0x1680)  # Game_GetTaskList0
    logfake(e, 0x4168B0, 0, ret=0x1681)  # Game_GetTaskList1
    logfake(e, 0x4015F0, 1)  # Task_Delete
    logfake(e, 0x4ECF50, 2, derefs={1: 24})  # Body_AddForce
    logfake(e, 0x4ECDB0, 1)  # Body_Update

    def get_name(e):
        e.write(e.arg(0), b"Prop_12\0")
        if LOG:
            LOG[-1].append(("name", e.normalize(e.arg(1))))
        e.ret(0)
    pyfake(e, 0x4F1270, get_name)
    e.baseline = e.snapshot()
    return e


def mat(e, p, vals=None, unk=3, seed=0):
    rng = random.Random(p * 31 + seed)
    f32(e, p, *(vals or [rng.uniform(-1, 1) for _ in range(9)]))
    u32(e, p + 0x24, unk)


def tricky():
    """Matrices whose products lose a term when summed in the wrong order:
    rows (or columns) are a permutation of 1, TINY and -1."""
    out = []
    for perm in itertools.permutations((1.0, TINY, -1.0)):
        out.append(("rows", [perm[i // 3] for i in range(9)]))
        out.append(("cols", [perm[i % 3] for i in range(9)]))
    return out


def rigid(e, size, type_, scale=(1.0, 2.0, 1.5)):
    t = task(e, size, type_)
    d64(e, t + 0x20, 1000.0, -2000.0, 30.5)
    u32(e, t + 0x68, 0x6868, 0x6C6C)
    mat(e, t + 0x70)
    f32(e, t + 0x98, *scale)
    return t


SCALES = [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 1.0, 1.0), (0.5, 0.25, 2.5), (float("nan"), 1.0, 2.0),
          (1.0, float("nan"), 2.0)]
