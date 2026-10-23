"""Helpers for the test_vehbase tests (no tests in here)."""
import struct

from emu import Emu
from test_haze_util import (CWS, ROUND_CWS, Fakes, add_child, check, check_error, cstr, f32, f64,  # noqa: F401
                            new_task, rmatrix, set_handler)

# event and type globals read by the getters, with the values the tests give them
EVENTS = {
    0xA76C01: 0x21, 0xA76C03: 0x22, 0xA76C08: 0x23, 0xA76C09: 0x24, 0xA76C10: 0x25,  # weapon
    0x547F40: 0x26,  # RigidDynCubeObj move (event 4)
    0x54E0E4: 0x27, 0x54E0E5: 0x28,  # param table, param changed
    0x548176: 0x29, 0x548177: 0x2A,  # World events 0, 1
    0x54E1DA: 0x2B, 0x54E1DB: 0x2C, 0x54E1DC: 0x2D, 0x54E1DD: 0x2E, 0x54E1DE: 0x2F, 0x54E1DF: 0x30,
    0x54E1E0: 0x31,  # AnimTask / vehicle events
    0x54DC30: 0x32, 0x54DC31: 0x33,  # PhysType args, size
    0x54E202: 0x34, 0x54E203: 0x35,  # Vehicle
    0x54E0E6: 0x36, 0x54E0E7: 0x37, 0x54E0E8: 0x38, 0x54E0E9: 0x39, 0x54E0EA: 0x3A, 0x54E0EB: 0x3B,  # param ops
}
INTS = {0x54DDA8: 0x3C, 0x54DDAC: 0x3D}  # Level events DA8 / DAC
TYPES = {0x547F42: 0x50, 0x548698: 0x51, 0x54E634: 0x52, 0xA774B0: 0x53, 0x54E1AC: 0x54, 0x54E1D8: 0x55,
         0x54E200: 0x56}
RIGID, EDIT, ANIM, VEHICLE = 0x50, 0x54, 0x55, 0x56
WORLD = 0xA44344


def ev(addr):
    return EVENTS[addr]


def set_globals(e):
    for a, v in EVENTS.items():
        e.write(a, bytes([v]))
    for a, v in INTS.items():
        e.write(a, struct.pack("<I", v))
    for a, v in TYPES.items():
        e.write(a, struct.pack("<H", v))
    e.write(WORLD, struct.pack("<I", 0x7777))


def masked(*masks):
    def fmt(em, a):
        return [v & m if m else em.normalize(v) for v, m in zip(a, masks)]
    return fmt


def strings(*kinds):
    """'s' logs a C string, 'n' a normalized word, an int a mask."""
    def fmt(em, a):
        out = []
        for v, k in zip(a, kinds):
            if k == "s":
                out.append(em.read_cstr(v) if v else None)
            elif k == "n":
                out.append(em.normalize(v))
            else:
                out.append(v & k)
        return out
    return fmt


def param_add(em, a):
    table, base, field, type_, flags, range_ = a
    return [table, field - base, em.normalize(type_), flags, em.normalize(range_)]


def get_name(em, a):
    em.write(a[0], b"Anim_7\0")
    return 0


def unlink(em, a):
    """Task_Delete fake: takes a real task off its parent's child list."""
    t = a[0]
    if t >= 0x20000000:
        nxt, prv = em.u32(t), em.u32(t + 4)
        em.write(prv, struct.pack("<I", nxt))
        em.write(nxt + 4, struct.pack("<I", prv))
    return 0


def make():
    """An Emu with every outside callee of the vehbase code replaced by a logging fake."""
    e = Emu()
    fk = Fakes(e)
    fk.add(0x4012A0, 3, fmt=masked(0, 0xFFFF, 0))  # Task_New
    fk.add(0x4015F0, 1, ret=unlink)  # Task_Delete
    fk.add(0x401400, 3, fmt=masked(0xFFFF, 0xFF, 0))  # TaskType_SetHandler
    fk.add(0x401530, 3, fmt=masked(0xFFFF, 0, 0))  # TaskType_SetHandler2
    fk.add(0x4017C0, 1, ret=lambda em, a: 0x40 + len(fk.log))  # TaskEvent_Alloc
    fk.add(0x4018C0, 1, fmt=masked(0xFF))  # TaskEvent_Free
    fk.add(0x401900, 4, ret=0x77, fmt=strings("s", "n", 0xFFFF, "n"))  # TaskType_New
    fk.add(0x401A20, 1, fmt=masked(0xFFFF))  # TaskType_Free
    fk.add(0x401B20, 1)  # TaskRef_Remove
    fk.add(0x401CC0, 2, fmt=masked(0, 0xFFFF))  # Task_FindAncestor
    fk.add(0x4B7EE0, 2, fmt=strings("s", "n"))  # Script_RegisterBool8
    fk.add(0x4B7FA0, 2, fmt=strings("s", "n"))  # Script_RegisterInt2
    fk.add(0x4B80A0, 1, fmt=strings("s"))  # Script_RemoveSymbol
    fk.add(0x4B8810, 2, fmt=strings("s", "n"))  # Script_RegisterConstant
    fk.add(0x4B8A20, 2)  # Script_GetArgInt
    fk.add(0x4B8A50, 2, st0="f")  # Script_GetArgReal
    fk.add(0x4C1830, 1)  # QTaskList_Free
    fk.add(0x4C3440, 1, ret=lambda em, a: a[0] + 0x20)  # DynCubeObj_GetPosition
    fk.add(0x4C48D0, 2)  # RigidDynCubeObj_SetModel
    fk.add(0x4C4950, 3)  # sub_4C4950
    fk.add(0x4C5710, 3)  # RigidDynCubeObj_Init
    fk.add(0x4C6940, 4, deref={2: 24})  # DynCube_Move
    fk.add(0x4C7140, 5, deref={2: 24})  # DynCube_Add
    fk.add(0x4C7560, 2)  # DynCube_Remove
    fk.add(0x4CEC10, 1, fmt=strings("s"))  # VirModel_Find
    fk.add(0x4D0950, 1, ret=2.75, st0="f")  # VirModel_GetRadius
    fk.add(0x4DCCB0, 3, deref={2: 24})  # Sector_AddObject
    fk.add(0x4DCDD0, 1)  # Sector_RemoveObject
    fk.add(0x4DCF00, 3, deref={2: 24})  # Sector_MoveObject
    fk.add(0x4E0930, 2)  # MagicObjContainer_New
    fk.add(0x4F1030, 1)  # Task_FindById
    fk.add(0x4F1050, 1)  # Task_GetId
    fk.add(0x4F1270, 2, ret=get_name, fmt=lambda em, a: [em.normalize(a[1])])  # Task_GetName
    fk.add(0x4F1320, 1)  # VarString_Init
    fk.add(0x4F1340, 1)  # VarString_Free
    fk.add(0x4F1400, 2)  # VarString_Bind
    fk.add(0x4F16C0, 1)  # VarString_HasExpr
    fk.add(0x4F16E0, 2, st0="f")  # VarString_EvalReal
    fk.add(0x4F1950, 1, ret=0x5000)  # ParamTable_New
    fk.add(0x4F19C0, 2, ret=0x33, fmt=strings("s", "n"))  # ParamType_New
    fk.add(0x4F1A40, 1)  # ParamType_Free
    fk.add(0x4F1A80, 6, fmt=param_add)  # ParamTable_Add
    fk.add(0x4F1BC0, 3, fmt=masked(0, 0xFF, 0))  # ParamType_SetOp
    fk.add(0x4FB5F0, 1)  # PhysicsObj_Init
    fk.add(0x4FB980, 1)  # PhysicsObj_Free
    fk.add(0x5031F0, 3)  # Vehicle_ParseConfig
    fk.add(0x5042B0, 2)  # Criteria_Contains
    fk.add(0x504A30, 2, ret=0x9876, fmt=strings("s", "n"))  # Criteria_New
    fk.add(0x504A90, 1)  # Criteria_Free
    set_globals(e)
    e.baseline = e.snapshot()
    return e, fk


def child_list(e, parent, slot, handler, types):
    """Children of parent of the given types, all with handler in slot."""
    for ty in types:
        c = new_task(e, ty, 0x40)
        add_child(e, parent, c)
        set_handler(e, slot, ty, handler)
