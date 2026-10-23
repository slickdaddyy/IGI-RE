"""Helpers shared by the test_mission tests (0x4849B0 - 0x487920, no tests in here)."""
import struct

from emu import Emu
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, pyfake, set_handler,  # noqa: F401
                            strip_logger, task, u16, u32)
from test_vehicle_util import d64, floatfake  # noqa: F401

WORLD = 0x9999
TICK_INFO = 0xBC2360
EVENTS = {0x416850: 0x2A, 0x467D90: 0x2B, 0x4C56F0: 0x2C, 0x4C5800: 0x2D, 0x4CEA10: 0x2E, 0x4CEA20: 0x2F,
          0x4E7530: 0x30, 0x4F1210: 0x31, 0x4F1220: 0x32, 0x4F1A60: 0x33, 0x4F1A70: 0x34}
RETS = {0x4018F0: 0x11, 0x5034D0: 0x12, 0x505D40: 0x13, 0x4F0F50: 0xF50, 0x4F0F70: 0xF70, 0x4F0FC0: 0xFC0,
        0x4F0FD0: 0xFD0, 0x4F0FF0: 0xFF0, 0x4F1000: 0x1000, 0x4F1010: 0x1010, 0x4F1950: 0x1950,
        0x416920: WORLD}


def get(addr, default=0):
    return lambda e, a: RET.get(addr, default)


def task_new(e, a):
    return task(e, RET.get("task_size", 0x200), a[1] & 0xFFFF)


def ref_add(e, a):
    p = e.alloc(4)
    u32(e, p, a[0])
    return p


def text_new(e, a):
    t = e.alloc(0x28)
    u32(e, t + 4, a[1], a[2])
    return t


def resource_load(e, a):
    name = e.read_cstr(a[0])
    return cstr(e, RET.get(("res", name), b"res text of " + name))


def arg_string(e, a):
    s = b"str%d_" % a[1] + b"x" * (a[1] * 3)
    e.write(a[2], s[:a[3] - 1] + b"\0")
    return a[2]


def get_name(e):
    e.write(e.arg(0), b"Obj_12\0")
    if LOG:
        LOG[-1].append(("name", e.normalize(e.arg(1))))
    e.ret(0)


def param_add(e):
    table, base, field, type_, flags, extra = (e.arg(i) for i in range(6))
    if LOG:
        LOG[-1].append(("param", table, field - base, type_, flags, e.normalize(extra)))
    e.ret(0)


def coll_lines(e):
    """Coll_Lines: logs the line and fills the hit from RET['coll'] = (found, pos)."""
    world, hits, starts, ends, count, ev, filt, flags = (e.arg(i) for i in range(8))
    if LOG:
        LOG[-1].append(("coll", world, e.read(starts, 24 * count), e.read(ends, 24 * count), count, ev & 0xFF,
                        e.u32(filt + 4), flags))
    found, pos = RET.get("coll", (0, (0.0, 0.0, 0.0)))
    h = bytearray(0x70)
    struct.pack_into("<3d", h, 8, *pos)
    struct.pack_into("<I", h, 0x58, found)
    e.write(hits, bytes(h))
    e.ret(0)


def make_emu():
    """An Emu with every outside callee of the range replaced by a logger."""
    e = Emu()
    for addr, v in EVENTS.items():
        logfake(e, addr, 0, ret=v)
    for addr, v in RETS.items():
        logfake(e, addr, 0, ret=v)
    logfake(e, 0x4012A0, 3, masks={1: 0xFFFF}, effect=task_new)  # Task_New
    logfake(e, 0x4015F0, 1)  # Task_Delete
    logfake(e, 0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})  # TaskType_SetHandler
    logfake(e, 0x401530, 3, masks={0: 0xFFFF})  # TaskType_SetHandler2
    logfake(e, 0x4017C0, 1, ret=0x2A)  # TaskEvent_Alloc
    logfake(e, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
    logfake(e, 0x401900, 5, strings=(0,), masks={2: 0xFFFF}, ret=0x77)  # TaskType_New
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    logfake(e, 0x401AE0, 1, effect=ref_add)  # TaskRef_Add
    logfake(e, 0x401B20, 1)  # TaskRef_Remove
    logfake(e, 0x416D40, 0, effect=get(0x416D40))  # Game_Get
    logfake(e, 0x416D50, 0, effect=get(0x416D50))  # Game_IsPaused
    logfake(e, 0x418E40, 2, strings=(1,), effect=lambda e, a: 0xF000 + len(e.read_cstr(a[1])))  # FindFont
    logfake(e, 0x468DD0, 0)  # ComputerLog_Clear
    logfake(e, 0x468DE0, 1, strings=(0,))  # ComputerLog_Add
    logfake(e, 0x46B4D0, 0, effect=get(0x46B4D0))  # ComputerRes_Get
    logfake(e, 0x46C060, 0, effect=get(0x46C060))  # Computer_Get
    logfake(e, 0x4894B0, 0, effect=lambda e, a: RET["time"].pop(0) if RET.get("time") else 0)
    logfake(e, 0x48F340, 0, effect=get(0x48F340))  # Config_GetQvmOnly
    logfake(e, 0x491CF0, 0, effect=get(0x491CF0))  # Display_Get
    logfake(e, 0x497570, 2)
    logfake(e, 0x4975F0, 1)
    logfake(e, 0x497620, 2)
    logfake(e, 0x497680, 2, derefs={1: 16})
    logfake(e, 0x4B09C0, 6, strings=(3, 4), effect=get(0x4B09C0))  # Wildcard_Find
    logfake(e, 0x4B0C60, 2, effect=lambda e, a: e.alloc(a[0]))  # Mem_Alloc
    logfake(e, 0x4B0D10, 1)  # Mem_Free
    logfake(e, 0x4B0D60, 1, masks={0: 0xFF})  # Mem_PushMode
    logfake(e, 0x4B0D90, 0)  # Mem_PopMode
    logfake(e, 0x4B12F0, 2, strings=(0, 1))  # Alias_Set
    logfake(e, 0x4B13A0, 1, strings=(0,))  # Alias_Remove
    logfake(e, 0x4B1420, 3, strings=(0,))  # File_ScanDir
    logfake(e, 0x4B4AF0, 11)  # Draw_RectAZ
    logfake(e, 0x4B4E00, 9)  # Draw_Sprite
    logfake(e, 0x4B50B0, 11)  # QVertex_DrawFrame
    logfake(e, 0x4B5F00, 2, strings=(0,), effect=resource_load)  # Resource_Load
    logfake(e, 0x4B63D0, 1)  # Resource_Flush
    logfake(e, 0x4B64E0, 1, strings=(0,), effect=get(0x4B64E0))  # Resource_Find
    logfake(e, 0x4B66A0, 1)  # Sprite_Free
    logfake(e, 0x4B6720, 2, strings=(1,))  # Sprite_Load
    logfake(e, 0x4B6CE0, 1, effect=get(0x4B6CE0, 33))  # Sprite_Width
    logfake(e, 0x4B6D00, 1, effect=get(0x4B6D00, 17))  # Sprite_Height
    logfake(e, 0x4B6E90, 2, strings=(1,))  # Text_SetString
    logfake(e, 0x4B6EC0, 4, strings=(3,), effect=text_new)  # Text_New
    logfake(e, 0x4B6F30, 1)  # Text_Free
    logfake(e, 0x4B6F50, 1)  # Text_DrawAlt
    logfake(e, 0x4B7050, 2, strings=(1,), effect=lambda e, a: 9 * len(e.read_cstr(a[1])))  # StringWidth
    logfake(e, 0x4B7E00, 1, effect=get(0x4B7E00, 13))  # Text_GetHeight
    logfake(e, 0x4B7E80, 2, strings=(0,))  # Script_RegisterInt
    logfake(e, 0x4B7EE0, 2, strings=(0,))  # Script_RegisterBool8
    logfake(e, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
    logfake(e, 0x4B80B0, 1, strings=(0,), effect=get(0x4B80B0, 0x5C0000))  # Script_Load
    logfake(e, 0x4B83D0, 1)  # Script_Free
    logfake(e, 0x4B85B0, 1)  # Script_Run
    logfake(e, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
    logfake(e, 0x4B8890, 4, strings=(0,))  # Script_RegisterFunction
    logfake(e, 0x4B8A20, 2, effect=lambda e, a: RET.get(("arg", a[1]), 1000 + a[1] * 7))  # Script_GetArgInt
    logfake(e, 0x4B8A80, 4, effect=arg_string, masks={2: 0})  # Script_GetArgString
    logfake(e, 0x4C1780, 0)
    logfake(e, 0x4C1830, 1)  # QTaskList_Free
    pyfake(e, 0x4CC2A0, coll_lines)
    logfake(e, 0x4E7200, 3, strings=(0,), derefs={2: 12})  # Sound_Play2D
    logfake(e, 0x4E7AC0, 2)
    logfake(e, 0x4E8110, 12, ret=0x8110)  # View_New
    logfake(e, 0x4E8370, 2)  # View_SetWorld
    logfake(e, 0x4E9430, 1)
    logfake(e, 0x4E9460, 1)
    logfake(e, 0x4E9500, 1)
    logfake(e, 0x4F1050, 1, effect=get(0x4F1050))  # Task_GetId
    pyfake(e, 0x4F1270, get_name)  # Task_GetName
    logfake(e, 0x4F1320, 1)  # VarString_Init
    logfake(e, 0x4F1340, 1)  # VarString_Free
    logfake(e, 0x4F1400, 2)  # VarString_Bind
    logfake(e, 0x4F16C0, 1, effect=lambda e, a: RET.get(("has", a[0]), RET.get(0x4F16C0, 0)))
    floatfake(e, 0x4F16E0, 2, effect=lambda e, a: RET.get(("val", a[0]), RET.get(0x4F16E0, 0.0)))
    pyfake(e, 0x4F1A80, param_add)  # ParamTable_Add
    e.baseline = e.snapshot()
    return e


def with_cw(e, target, cw):
    """An entry point that loads x87 control word cw, then jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    cache = e.__dict__.setdefault("cw_tramps", {})
    if (addr, cw) not in cache:
        p = e.fake_top
        e.fake_top += 32
        e.write(p, struct.pack("<H", cw) + b"\0\0")
        code = b"\xd9\x2d" + struct.pack("<I", p) + b"\xe9" + struct.pack("<i", addr - (p + 4 + 11))
        e.write(p + 4, code)
        cache[(addr, cw)] = p + 4
        e.baseline = e.snapshot()
    return cache[(addr, cw)]


def run(e, orig, ours, setup=None, cw=None, **kw):
    """compare() of orig and ours plus their logged calls; cw runs both with that x87 control word."""
    RET.clear()
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def tick_info(e, ticks=1000, timer=0, mm=0x1200):
    p = e.alloc(0x200)
    u32(e, p + 0x68, ticks)
    e.write(p + 0x70, bytes([timer]))
    u32(e, p + 0x120, mm)
    u32(e, TICK_INFO, p)
    return p


def display(e, w=640, h=480):
    p = e.alloc(12)
    u32(e, p, 0, w, h)
    RET[0x491CF0] = p
    return p
