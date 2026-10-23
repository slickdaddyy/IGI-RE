"""AmmoType, ammo lists and the BulletHole registration (0x47CA40 - 0x47D0B0)."""
import struct

import pytest

from emu import Emu, ErrorShown
from test_menu_util import LOG, RET, check, cstr, handler, logfake, set_handler, u16, u32
from test_vehicle_util import floatfake

TYPES, COUNT, EVENT, ENUM = 0x5BE3E0, 0x5C10E4, 0x5C10E8, 0x5C10F0
SIZE = 0x168

emu = Emu()


def arg_string(e, a):
    s = b"str%d_" % a[1] + b"x" * (a[1] * 3)
    e.write(a[2], s[:a[3] - 1] + b"\0")
    return a[2]


logfake(emu, 0x4B8A20, 2, effect=lambda e, a: 1000 + a[1] * 7)  # Script_GetArgInt
floatfake(emu, 0x4B8A50, 2, effect=lambda e, a: 2.7182818 * a[1])  # Script_GetArgReal
logfake(emu, 0x4B8A80, 4, effect=arg_string, masks={2: 0})  # Script_GetArgString, buffer checked by memory
logfake(emu, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
logfake(emu, 0x4B8890, 4, strings=(0,))  # Script_RegisterFunction
logfake(emu, 0x4B1420, 3, strings=(0,))  # File_ScanDir
logfake(emu, 0x4B80B0, 1, strings=(0,), effect=lambda e, a: RET.get("load", 0x5C0000))  # Script_Load
logfake(emu, 0x4B85B0, 1)  # Script_Run
logfake(emu, 0x4B83D0, 1)  # Script_Free
logfake(emu, 0x4B0D60, 1)  # Mem_PushMode
logfake(emu, 0x4B0D90, 0)  # Mem_PopMode
logfake(emu, 0x4B8770, 1, strings=(0,), ret=0x1234)  # Script_EvalExpr
logfake(emu, 0x4F1CD0, 1)  # ParamEnum_Init
logfake(emu, 0x4F1D80, 1)  # ParamEnum_Free
logfake(emu, 0x4F1DB0, 2, strings=(1,))  # ParamEnum_AddConstants
logfake(emu, 0x4017C0, 1, ret=0x2A)  # TaskEvent_Alloc
logfake(emu, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
logfake(emu, 0x4018F0, 0, ret=0x11)  # Task_GetRootType
logfake(emu, 0x401900, 4, strings=(0,), masks={2: 0xFFFF}, ret=0x77)  # TaskType_New
logfake(emu, 0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})  # TaskType_SetHandler
logfake(emu, 0x401530, 3, masks={0: 0xFFFF})  # TaskType_SetHandler2
logfake(emu, 0x4B0B00, 4)  # Pool_Init
logfake(emu, 0x4C48C0, 0, ret=0x21)
logfake(emu, 0x4E5350, 0, ret=0x22)
logfake(emu, 0x4C56F0, 0, ret=0x31)
logfake(emu, 0x4D0D80, 0, ret=0x32)
logfake(emu, 0x4D97E0, 0, ret=0x33)
FWD = handler(emu, "fwd", 2)

OUT = emu.alloc(24)


def run(orig, ours, setup=None, **kw):
    RET.clear()
    kw.setdefault("check_eax", False)
    return check(emu, orig, ours, setup=setup, normalized=True, **kw)


def add_types(e, rows):
    for i, (aid, cal) in enumerate(rows):
        u32(e, TYPES + i * SIZE, aid, cal)
    u32(e, COUNT, len(rows))


def ammo_list(e, rows):
    p = e.alloc(4 + 12 * 32)
    e.write(p + 4, b"\xAA" * 12 * 32)
    u32(e, p, len(rows))
    for i, r in enumerate(rows):
        u32(e, p + 4 + 12 * i, *r)
    return p


@pytest.mark.parametrize("isdir", [0, 1])
def test_open(isdir):
    def setup(e):
        ent = e.alloc(12)
        u32(e, ent, cstr(e, b"LOCAL:ammo/556"), 0)
        e.write(ent + 8, bytes([isdir]))
        return (ent,)
    run(0x47CA40, "AmmoType_Open", setup)


def test_open_fails():
    for fn in (0x47CA40, "AmmoType_Open"):
        emu.reset()
        RET["load"] = 0
        ent = emu.alloc(12)
        u32(emu, ent, cstr(emu, b"x"), 0)
        emu.write(ent + 8, b"\x01")
        with pytest.raises(ErrorShown, match="Couldn.t load script: %s"):
            emu.call(fn, ent)
    RET.clear()


def test_register():
    run(0x47CAB0, "AmmoTypeOpen", lambda e: u32(e, COUNT, 5))


@pytest.mark.parametrize("count", [0, 3, 31])
def test_define(count):
    def setup(e):
        e.write(TYPES, b"\x55" * SIZE * 32)
        u32(e, COUNT, count)
        return OUT, 0, 0x5A5A0
    ro, rn = run(0x47CAD0, "DefineAmmoType", setup, ignore=[(OUT + 4, OUT + 8), (OUT + 20, OUT + 24)],
                 check_eax=True)
    assert ro[0] == OUT


def test_close_and_load():
    run(0x47CCE0, "AmmoType_Close", lambda e: u32(e, COUNT, 9))
    run(0x47CCF0, "AmmoType_LoadAll", lambda e: u32(e, COUNT, 9))


@pytest.mark.parametrize("i", [0, 7, 31])
def test_get(i):
    run(0x47CD10, "AmmoType_Get", args=(i,), check_eax=True)


def test_count():
    run(0x47CD30, "AmmoType_GetCount", lambda e: u32(e, COUNT, 13), check_eax=True)


@pytest.mark.parametrize("key", [50, 51, 52, 99, 556, 762])
@pytest.mark.parametrize("addr,name", [(0x47CD40, "AmmoType_Find"), (0x47CD70, "AmmoType_FindByCaliber")])
def test_find(addr, name, key):
    def setup(e):
        add_types(e, [(50, 556), (51, 762), (52, 556)])
        return (key,)
    run(addr, name, setup, check_eax=True)


def test_find_empty():
    run(0x47CD40, "AmmoType_Find", lambda e: (add_types(e, []), (50,))[1], check_eax=True)
    run(0x47CD70, "AmmoType_FindByCaliber", lambda e: (add_types(e, []), (50,))[1], check_eax=True)


def test_eval_const():
    run(0x47CDA0, "Script_EvalConst", lambda e: (cstr(e, b"AMMO_ID_556"),), check_eax=True)


def test_event():
    run(0x47CDC0, "AmmoList_GetEvent", lambda e: e.write(EVENT, b"\x9C"), check_eax=True, eax_mask=0xFF)


def test_list_init():
    run(0x47CDD0, "AmmoList_Init", lambda e: (ammo_list(e, [(1, 2, 3)]),))


ROWS = [(50, 10, -1), (51, 5, 20), (52, 0, 8), (53, -3, -1)]


@pytest.mark.parametrize("aid,n", [(50, 7), (50, -20), (51, 30), (51, -9), (51, 2), (52, 8), (52, 9), (53, -1),
                                   (60, 4), (60, -4)])
def test_add(aid, n):
    run(0x47CE00, "AmmoList_Add", lambda e: (ammo_list(e, ROWS), aid, n))


@pytest.mark.parametrize("n", [9, -2, 3])
def test_add_new_with_limit(n):
    # a new entry takes the limit already stored in the free slot
    def setup(e):
        p = ammo_list(e, ROWS)
        u32(e, p + 4 + 12 * 4 + 8, 6)
        return p, 70, n
    run(0x47CE00, "AmmoList_Add", setup)


@pytest.mark.parametrize("aid,lim", [(50, 5), (50, 20), (50, -1), (51, 3), (53, 4), (53, -1), (60, 4), (60, -1)])
def test_set_limit(aid, lim):
    run(0x47CE70, "AmmoList_SetLimit", lambda e: (ammo_list(e, ROWS), aid, lim))


@pytest.mark.parametrize("c", [-16, 2, 9])
def test_set_limit_new_slot_count(c):
    # a new entry keeps the count already stored in the free slot
    def setup(e):
        p = ammo_list(e, ROWS)
        u32(e, p + 4 + 12 * 4 + 4, c)
        return p, 70, 5
    run(0x47CE70, "AmmoList_SetLimit", setup)


@pytest.mark.parametrize("aid", [50, 52, 53, 54])
def test_list_find(aid):
    run(0x47CEE0, "AmmoList_Find", lambda e: (ammo_list(e, ROWS), aid), check_eax=True)
    run(0x47CEE0, "AmmoList_Find", lambda e: (ammo_list(e, []), aid), check_eax=True)


def test_enum():
    run(0x47CF10, "AmmoType_GetEnum", check_eax=True)


def test_init_close():
    run(0x47CF20, "Ammo_Init")
    run(0x47CF50, "Ammo_Close", lambda e: e.write(EVENT, b"\x44"))


def test_bullethole_init():
    run(0x47CF70, "BulletHole_Init")


@pytest.mark.parametrize("child,fn", [(0, 0), (1, 0), (1, 1)])
def test_bullethole_forward(child, fn):
    def setup(e):
        t = e.alloc(0xE148)
        if child:
            c = e.alloc(0x20)
            u16(e, c + 0x1C, 0x45)
            u32(e, t + 0xE130, c)
        set_handler(e, 0x33, 0x45, FWD if fn else 0)
        return t, 0xBEEF
    run(0x47D060, "BulletHole_Forward", setup)
