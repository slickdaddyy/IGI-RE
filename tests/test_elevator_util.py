"""Helpers shared by the test_elevator tests."""
import random
import struct

from test_vehicle_util import (LOG, RET, add_child, cstr, d64, f32, floatfake, handler, init_emu, logfake,  # noqa: F401
                               pyfake, run, set_events, set_handler, setup_init, task, u16, u32, vfake)

# event getters not covered by set_events: global, value, width
MORE_EVENTS = [(0x547F40, 0x34, 1), (0x54836C, 0x35, 4), (0x53954E, 0x36, 1), (0x540904, 0x37, 1),
               (0x53F582, 0x38, 1)]
WORLD = 0x9999
TASKLIST = 0x7171
SOUND_OWNER = 0x5050

# slots with logging handlers on the test types: slot -> (nargs, derefs)
SLOTS = {0: (2, None), 1: (1, None), 2: (1, None), 0x24: (1, None), 0x25: (1, None), 0x31: (2, None),
         0x34: (2, {1: 8}), 0x35: (1, None)}


def all_events(e):
    set_events(e)
    for a, v, n in MORE_EVENTS:
        e.write(a, v.to_bytes(n, "little"))


def show_explosion(e, a):
    return (e.read(a[0], 24),) + tuple(a[1:10]) + (e.read_cstr(a[10]), e.read_cstr(a[11])) + tuple(a[12:])


def make_emu(types=()):
    """An Emu with every outside callee of the range replaced by a logger."""
    e = init_emu()
    all_events(e)
    for slot, (n, derefs) in SLOTS.items():
        for t in types:
            set_handler(e, slot, t, handler(e, "%x_%x" % (slot, t), n, derefs=derefs))
    logfake(e, 0x4C5710, 3)  # RigidDynCubeObj_Init
    floatfake(e, 0x4D0950, 1)  # VirModel_GetRadius
    logfake(e, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
    logfake(e, 0x4C6940, 4, derefs={2: 24})  # DynCube_Move
    logfake(e, 0x4C7560, 2)  # DynCube_Remove
    logfake(e, 0x4DCCB0, 3)
    logfake(e, 0x4DCDD0, 1)
    logfake(e, 0x4DCF00, 3)
    logfake(e, 0x4C3440, 1, ret=0x3440)  # DynCubeObj_GetPosition
    logfake(e, 0x4F1320, 1)  # VarString_Init
    logfake(e, 0x4F1340, 1)  # VarString_Free
    logfake(e, 0x4F1400, 2)  # VarString_Bind
    logfake(e, 0x4F16C0, 1, effect=lambda e, a: RET.get(("has", a[0]), RET.get(0x4F16C0, 0)))
    floatfake(e, 0x4F16E0, 2, effect=lambda e, a: RET.get(("val", a[0]), RET.get(0x4F16E0, 0.0)))
    logfake(e, 0x4168D0, 0, ret=TASKLIST)  # Game_GetTaskList3
    logfake(e, 0x4C1790, 2)  # QTaskList_Add
    logfake(e, 0x4C17C0, 2)  # QTaskList_Remove
    logfake(e, 0x4E6B00, 2, strings=(1,))  # Sound_Play
    logfake(e, 0x4E6C00, 1)  # Sound_Stop
    logfake(e, 0x4E6C30, 2, derefs={1: 24})  # Sound_SetPos
    logfake(e, 0x4015F0, 1)  # Task_Delete
    logfake(e, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
    logfake(e, 0x4C48D0, 2)  # RigidDynCubeObj_SetModel
    logfake(e, 0x4F1050, 1)  # Task_GetId
    logfake(e, 0x4E0930, 2)
    logfake(e, 0x4C4950, 3)
    vfake(e, 0x480FE0, 16, show=show_explosion)  # Explosion_Spawn
    floatfake(e, 0x416C90, 5, derefs={2: 24})  # Game_BlastFalloff
    logfake(e, 0x4B7EE0, 2, strings=(0,))  # Script_RegisterBool8
    logfake(e, 0x4B7E80, 2, strings=(0,))  # Script_RegisterInt
    logfake(e, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    logfake(e, 0x4C1830, 1)  # QTaskList_Free
    logfake(e, 0x4F1030, 1)  # Task_FindById
    logfake(e, 0x4149B0, 2, derefs={1: 0x54})  # Game_DropToGround
    logfake(e, 0x401AE0, 1, ret=0x7000)  # TaskRef_Add
    logfake(e, 0x401B20, 1)  # TaskRef_Remove
    logfake(e, 0x416D20, 0, ret=0x5000)  # Game_GetRandom
    logfake(e, 0x4B47C0, 2, effect=lambda e, a: RET.get((0x4B47C0, a[1]), 0))  # Random_Int
    logfake(e, 0x416920, 0, ret=WORLD)  # Game_GetWorld
    logfake(e, 0x503C00, 9, strings=(1, 2), derefs={7: 12})
    logfake(e, 0x503CC0, 5, derefs={2: 24, 3: 40, 4: 12}, masks={1: 0xFFFF})
    logfake(e, 0x502130, 1)
    floatfake(e, 0x502180, 2, effect=lambda e, a: RET.get((0x502180, a[1]), 0.0))
    floatfake(e, 0x5020C0, 1)
    logfake(e, 0x4028B0, 0)  # Timer_GetTicks
    logfake(e, 0x43D960, 1)
    logfake(e, 0x4453E0, 1)

    def get_name(e):
        e.write(e.arg(0), b"Obj_12\0")
        if LOG:
            LOG[-1].append(("name", e.normalize(e.arg(1))))
        e.ret(0)
    pyfake(e, 0x4F1270, get_name)
    u32(e, 0xA758AC, SOUND_OWNER)
    u32(e, 0xA44344, 0x4444)
    e.baseline = e.snapshot()
    return e


def setup_types(e):
    """setup_init plus the parent types and the parent param table of the range."""
    setup_init(e)
    all_events(e)
    root = e.call(0x4018F0)[0] & 0xFFFF
    for g, name, size in ((0x547F42, b"RigidDynCubeObj", 0xF0), (0x54E634, b"Position", 0x38),
                          (0x54097C, b"SoundObj", 0x238), (0x548698, b"Light", 0xF0)):
        u16(e, g, e.call(0x401900, cstr(e, name), size, root, 0)[0] & 0xFFFF)
    u32(e, 0x5C1184, e.call(0x4F1950, 3)[0])


def mat(e, p, rng, unk=3):
    f32(e, p, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, p + 0x24, unk)


def obj(e, size, type_, scale=(1.0, 2.0, 1.5), seed=1):
    """A RigidDynCubeObj style task with position, model, matrix and scale set."""
    rng = random.Random(seed)
    t = task(e, size, type_)
    d64(e, t + 0x20, 1000.0, -2000.0, 30.5)
    u32(e, t + 0x68, 0x6868, 0x6C6C)
    mat(e, t + 0x70, rng)
    f32(e, t + 0x98, *scale)
    return t


def strz(e, p, s):
    e.write(p, s + b"\0")
