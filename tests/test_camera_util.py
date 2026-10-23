"""Helpers shared by the test_camera tests (Explosion, ExplodeObject, HumanCamera, 0x480F00 - 0x4849B0)."""
import struct

from emu import Emu
from test_menu_util import LOG, RET, add_child, cstr, handler, logfake, pyfake, set_handler, task, u16, u32  # noqa: F401
from test_vehicle_util import d64, f32, floatfake, run as _run  # noqa: F401

WORLD, RNG, TASKLIST, PROFILE_TYPE = 0x9999, 0x5000, 0x7171, 0x55
EVENTS = {0x416840: 0x29, 0x416850: 0x2A, 0x416860: 0x2B, 0x47E6F0: 0x2C, 0x4C5800: 0x2D, 0x4CEA10: 0x2E,
          0x4CEA20: 0x2F, 0x4D0D80: 0x30, 0x4F1210: 0x31, 0x4F1220: 0x32, 0x4F1A60: 0x33, 0x4F1A70: 0x34,
          0x4017C0: 0x35}
RETS = {0x4018F0: 0x11, 0x4E0830: 0x12, 0x413A70: PROFILE_TYPE, 0x4E75A0: 0x75A0, 0x4F0EE0: 0xEE0,
        0x4F0F50: 0xF50, 0x4F0F70: 0xF70, 0x4F0FA0: 0xFA0, 0x4F0FC0: 0xFC0, 0x4F0FD0: 0xFD0, 0x4F1010: 0x1010,
        0x4F1950: 0x1950, 0x416920: WORLD, 0x416D20: RNG, 0x4168D0: TASKLIST, 0x4C3440: 0x3440}


def rd(e, p, n):
    return e.read(p, n) if p else None


def fake_task_new(e, a):
    t = task(e, 0x200, a[1] & 0xFFFF)
    return t


def coll_lines(e):
    """Coll_Lines: logs the lines and fills the hits from RET['coll'], a list of calls, each a list
    of (found, unk54, material value or None, pos, normal) per line."""
    world, hits, starts, ends, count, ev, filt, flags = (e.arg(i) for i in range(8))
    if LOG:
        LOG[-1].append(("coll", world, e.read(starts, 24 * count), e.read(ends, 24 * count), count, ev & 0xFF,
                        e.u32(filt + 4), flags))
    calls = RET.get("coll", [])
    lines = calls.pop(0) if calls else []
    for i in range(count):
        h = bytearray(0x70)
        if i < len(lines):
            found, unk54, mat, pos, normal = lines[i]
            struct.pack_into("<3d", h, 8, *pos)
            struct.pack_into("<3f", h, 0x38, *normal)
            if mat is not None:
                m = e.alloc(4)
                f32(e, m, mat)
                struct.pack_into("<I", h, 0x4C, m)
            struct.pack_into("<I", h, 0x54, unk54)
            struct.pack_into("<I", h, 0x58, found)
        e.write(hits + 0x70 * i, bytes(h))
    e.ret(0)


def param_add(e):
    table, base, field, type_, flags, extra = (e.arg(i) for i in range(6))
    if LOG:
        LOG[-1].append(("param", table, field - base, type_, flags, extra))
    e.ret(0)


def get_name(e):
    e.write(e.arg(0), b"Obj_12\0")
    if LOG:
        LOG[-1].append(("name", e.normalize(e.arg(1))))
    e.ret(0)


def broadcast(e):
    n = e.arg(0)
    if LOG:
        LOG[-1].append(("noise", e.read(n, 16), e.read(e.u32(n + 16), 24), e.read(n + 20, 8)))
    e.ret(0)


def random_float(e, a):
    seq = RET.get("rf")
    return seq.pop(0) if seq else 0.25


def make_emu():
    """An Emu with every outside callee of the range replaced by a logger."""
    e = Emu()
    for addr, v in EVENTS.items():
        logfake(e, addr, 0, ret=v)
    for addr, v in RETS.items():
        logfake(e, addr, 0, ret=v)
    logfake(e, 0x4012A0, 3, masks={1: 0xFFFF}, effect=fake_task_new)  # Task_New
    logfake(e, 0x4015F0, 1)  # Task_Delete
    logfake(e, 0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})  # TaskType_SetHandler
    logfake(e, 0x401530, 3, masks={0: 0xFFFF})  # TaskType_SetHandler2
    logfake(e, 0x401900, 5, strings=(0,), masks={2: 0xFFFF}, ret=0x77)  # TaskType_New
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    logfake(e, 0x401CF0, 2, masks={0: 0xFFFF, 1: 0xFFFF}, effect=lambda e, a: RET.get(0x401CF0, 0))  # TaskType_IsA
    logfake(e, 0x406220, 0, effect=lambda e, a: RET.get(0x406220, 0))  # Profile_GetActive
    logfake(e, 0x414E20, 3, derefs={0: 24}, effect=lambda e, a: (d64(e, a[0], 11.5, -22.25, 33.125), 0)[1])
    logfake(e, 0x414FD0, 1, effect=lambda e, a: RET.get(("down", a[0]), 0))  # Game_CheatKeyDown
    logfake(e, 0x415020, 1, effect=lambda e, a: RET.get(("pressed", a[0]), 0))  # Game_CheatKeyPressed
    floatfake(e, 0x416C90, 5, derefs={2: 24})  # Game_BlastFalloff
    logfake(e, 0x4453E0, 1)
    pyfake(e, 0x450CE0, broadcast)  # HumanAI_Broadcast
    logfake(e, 0x47F200, 1)
    floatfake(e, 0x489470, 0)
    floatfake(e, 0x4B4770, 1, effect=random_float)  # Random_Float
    logfake(e, 0x4B47C0, 2, effect=lambda e, a: RET.get(0x4B47C0, 0))  # Random_Int
    logfake(e, 0x4B7EE0, 2, strings=(0,))  # Script_RegisterBool8
    logfake(e, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
    logfake(e, 0x4C1790, 2)  # QTaskList_Add
    logfake(e, 0x4C17C0, 2)  # QTaskList_Remove
    logfake(e, 0x4C1830, 1)  # QTaskList_Free
    logfake(e, 0x4C48D0, 2)  # RigidDynCubeObj_SetModel
    logfake(e, 0x4C5710, 3)  # RigidDynCubeObj_Init
    logfake(e, 0x4C6940, 4, derefs={2: 24})  # DynCube_Move
    logfake(e, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
    logfake(e, 0x4C7560, 2)  # DynCube_Remove
    pyfake(e, 0x4CC2A0, coll_lines)
    logfake(e, 0x4CEC10, 1, strings=(0,), effect=lambda e, a: RET.get(("model", e.read_cstr(a[0])), 0))
    floatfake(e, 0x4D0950, 1)  # VirModel_GetRadius
    logfake(e, 0x4D0D30, 1)
    logfake(e, 0x4849B0, 1)
    logfake(e, 0x4D9870, 10, derefs={1: 24, 2: 36})  # Camera_Set
    logfake(e, 0x4DCCB0, 3)
    logfake(e, 0x4DCDD0, 1)
    logfake(e, 0x4DCF00, 3)
    logfake(e, 0x4F1050, 1, effect=lambda e, a: RET.get(0x4F1050, 0))  # Task_GetId
    pyfake(e, 0x4F1270, get_name)
    logfake(e, 0x4F1320, 1)  # VarString_Init
    logfake(e, 0x4F1340, 1)  # VarString_Free
    logfake(e, 0x4F1400, 2)  # VarString_Bind
    logfake(e, 0x4F16C0, 1, effect=lambda e, a: RET.get(0x4F16C0, 0))  # VarString_HasExpr
    floatfake(e, 0x4F16E0, 2)  # VarString_EvalReal
    pyfake(e, 0x4F1A80, param_add)
    logfake(e, 0x503C00, 9, strings=(1, 2), derefs={7: 12})  # SimpointObjGroup_New
    e.baseline = e.snapshot()
    return e


def with_cw(e, target, cw):
    """An entry point that loads x87 control word cw, then jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    cache = e.__dict__.setdefault("cw_tramps", {})
    if (addr, cw) not in cache:
        p = e.fake_top
        e.fake_top += 32
        e.write(p, struct.pack("<H", cw))
        e.write(p + 4, b"\xd9\x2d" + struct.pack("<I", p) + b"\xe9" + struct.pack("<i", addr - (p + 15)))
        cache[(addr, cw)] = p + 4
    return cache[(addr, cw)]


def run(e, orig, ours, setup=None, cw=None, **kw):
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return _run(e, orig, ours, setup or (lambda e: None), **kw)
