import struct

import pytest

from emu import Emu, ErrorShown, compare
from test_weapon_stubs import fakes, stub, u32

emu = Emu()

TYPES, COUNT, MAXID = 0x56E258, 0x57B18C, 0x5391B4
EVENTS = 0x5391AC
HANDLERS = 0xA96AE0
WSIZE = 0x6E8

INTS = emu.fake_top + 0x8000
REALS = INTS + 0x200
STRS = INTS + 0x600
emu.fake_top = STRS + 64 * 0x40
ARGS = 0x5A5A0

# Script_GetArgInt / GetArgReal / GetArgString return INTS[i], REALS[i], STRS[i]
stub(emu, 0x4B8A20, log=(1,), body=b"\x8b\x44\x24\x08\x8b\x04\x85" + u32(INTS) + b"\xc3")
stub(emu, 0x4B8A50, log=(1,), body=b"\x8b\x44\x24\x08\xdd\x04\xc5" + u32(REALS) + b"\xc3")
stub(emu, 0x4B8A80, log=(1, 3), body=b"\x56\x57\x8b\x44\x24\x10\xc1\xe0\x06\x8d\xb0" + u32(STRS)
     + b"\x8b\x7c\x24\x14\xac\xaa\x84\xc0\x75\xfa\x8b\x44\x24\x14\x5f\x5e\xc3")
# names passed from a stack buffer are logged by content (15 chars + NUL)
CONST = stub(emu, 0x4B8810, log=((0, 4), 1))
CONST_RAW = stub(emu, None, tag=0x4B8810, log=(0, 1))
MEM = fakes(emu, {0x4B0C60: (2, 0x22220000), 0x4017C0: (1, 0x1234567), 0x4B8890: 4,
                  0x4F1CD0: 1, 0x4F1DB0: 2, 0x4B1420: 3, 0x4B85B0: 1, 0x4B83D0: 1})
LOAD = stub(emu, 0x4B80B0, log=((0, 4),), ret=0x5C0000)
SIZE = stub(emu, None, tag=0x77000001, log=(0,), body=b"\x8b\x44\x24\x08\xc7\x00" + u32(0x40) + b"\xc3")
SIZE2 = stub(emu, None, tag=0x77000002, log=(0,), body=b"\x8b\x44\x24\x08\xc7\x00" + u32(0x18) + b"\xc3")
# init handlers log what they get and consume 3 and 2 more arguments
INIT = stub(emu, None, tag=0x77000003, log=(0, (1, 3)),
            body=b"\x8b\x44\x24\x08\x83\x40\x08\x03\xc3")
INIT2 = stub(emu, None, tag=0x77000004, log=(0, (1, 3)),
             body=b"\x8b\x44\x24\x08\x83\x40\x08\x02\xc3")
FIRE = stub(emu, None, tag=0x77000005, log=(0, (1, 12)))
# weapon type of a task: the word at task+0x20
GETTYPE = stub(emu, None, tag=0x77000006, log=(0,),
               body=b"\x8b\x44\x24\x04\x8b\x40\x20\x8b\x54\x24\x08\x89\x02\xc3")
emu.baseline = emu.snapshot()

OUT = emu.alloc(24)
IGNORE = [(OUT + 4, OUT + 8), (OUT + 20, OUT + 24)]


def set_events(e, ev=(3, 4, 5, 6, 7, 8, 9)):
    e.write(EVENTS, bytes(ev))


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), u32(fn))


def add_weapons(e, ids, keys=None):
    for i, wid in enumerate(ids):
        b = TYPES + i * WSIZE
        e.write(b, u32(wid))
        e.write(b + 0x61C, ((keys[i] if keys else b"W%d" % wid) + b"\0").ljust(16, b"\0"))
        e.write(b + 0x6D4, u32(0x181) + u32(0x181))
    e.write(COUNT, u32(len(ids)))


@pytest.mark.parametrize("addr,name,off", [
    (0x413B00, "WeaponType_GetSizeEvent", 0), (0x413B10, "WeaponType_GetInitEvent", 1),
    (0x413B20, "Weapon_GetFireEvent", 2), (0x413B30, "WeaponType_GetSizeEvent2", 3),
    (0x413B40, "WeaponType_GetInitEvent2", 4), (0x413B50, "Weapon_GetTypeEvent", 5),
    (0x413B60, "Weapon_GetUnusedPointsEvent", 6)])
def test_event_getters(addr, name, off):
    compare(emu, addr, name, setup=lambda e: set_events(e, (0x81, 0x92, 0xA3, 0xB4, 0xC5, 0xD6, 0xE7)),
            eax_mask=0xFF)


@pytest.mark.parametrize("addr,name", [
    (0x413DB0, "WeaponType_GetCategoryEnum"), (0x413DC0, "WeaponType_GetSightEnum"),
    (0x413DD0, "WeaponType_GetAmmoEnum"), (0x413DE0, "WeaponType_GetIdEnum"),
    (0x413BB0, "WeaponType_GetCount"), (0x413BC0, "WeaponType_GetMaxId")])
def test_getters(addr, name):
    def setup(e):
        e.write(COUNT, u32(7))
        e.write(MAXID, u32(321))
    compare(emu, addr, name, setup=setup)


@pytest.mark.parametrize("i", [0, 5, 29])
def test_get(i):
    compare(emu, 0x413B90, "WeaponType_Get", args=(i,))


@pytest.mark.parametrize("wid", [10, 30, 77, 5])
def test_find(wid):
    compare(emu, 0x413BD0, "WeaponType_Find", setup=lambda e: add_weapons(e, [10, 20, 30]) or (wid,))


@pytest.mark.parametrize("key", [b"AK47", b"M16", b"Colt", b"Col", b"Colt1", b""])
def test_find_by_key(key):
    def setup(e):
        add_weapons(e, [1, 2, 3], [b"AK47", b"Colt", b"M16"])
        p = e.alloc(16)
        e.write(p, key + b"\0")
        return (p,)
    compare(emu, 0x413C00, "WeaponType_FindByKey", setup=setup)


@pytest.mark.parametrize("isdir,ok", [(0, 1), (1, 1), (1, 0)])
def test_open(isdir, ok):
    def setup(e):
        LOAD.set_ret(0x5C0000 if ok else 0)
        ent = e.alloc(16)
        name = e.alloc(16)
        e.write(name, b"abcd\0")
        e.write(ent, u32(name) + u32(0) + bytes([isdir]))
        return (ent,)
    try:
        if ok:
            compare(emu, 0x413A90, "WeaponType_Open", setup=setup, check_eax=False, normalized=True)
        else:
            for fn in (0x413A90, "WeaponType_Open"):
                emu.reset()
                with pytest.raises(ErrorShown, match="Couldn.t load script"):
                    emu.call(fn, *setup(emu))
    finally:
        LOAD.set_ret(0x5C0000)


def test_load_all():
    compare(emu, 0x413B70, "WeaponType_LoadAll", setup=lambda e: e.write(COUNT, u32(9)),
            check_eax=False, normalized=True)


def jump(e, addr, to):
    e.write(addr, b"\xe9" + struct.pack("<i", to - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)


def test_init():
    jump(emu, 0x4B8810, CONST_RAW.code)
    try:
        compare(emu, 0x413DF0, "WeaponType_Init", check_eax=False, normalized=True)
        assert len(emu.call_log()) > 60
    finally:
        jump(emu, 0x4B8810, CONST.code)


def task_with_children(e, values):
    """A task whose children answer the weapon type query with values[i] (0 = no handler)."""
    task = e.alloc(0x24)
    lst = task + 8
    nodes = []
    for v in values:
        n = e.alloc(0x24)
        e.write(n + 0x1C, struct.pack("<H", 0x30 if v else 0x31))
        e.write(n + 0x20, u32(v))
        nodes.append(n)
    chain = nodes + [lst + 4]
    e.write(lst, u32(chain[0]) + u32(0) + u32(nodes[-1] if nodes else lst))
    for i, n in enumerate(nodes):
        e.write(n, u32(chain[i + 1]) + u32(chain[i - 1] if i else lst))
    return task, nodes


def weapon_setup(e):
    set_events(e)
    set_handler(e, 8, 0x30, GETTYPE.code)
    add_weapons(e, [10, 20, 30])


@pytest.mark.parametrize("which", [0, 1])
def test_get_type(which):
    def setup(e):
        weapon_setup(e)
        _, nodes = task_with_children(e, [TYPES + WSIZE, 0])
        return (nodes[which],)
    compare(emu, 0x413C80, "Weapon_GetType", setup=setup, normalized=True)


@pytest.mark.parametrize("wid,count", [(20, 3), (10, 3), (99, 3), (20, 0), (30, 1)])
def test_find_weapon(wid, count):
    def setup(e):
        weapon_setup(e)
        vals = [0, TYPES + 2 * WSIZE, TYPES + WSIZE, TYPES][:count]
        if count == 1:
            vals = [TYPES + 2 * WSIZE]
        task, _ = task_with_children(e, vals)
        return task, wid
    compare(emu, 0x413CC0, "Task_FindWeapon", setup=setup, normalized=True)


@pytest.mark.parametrize("ttype", [0x40, 0x181])
def test_fire(ttype):
    def setup(e):
        set_events(e)
        set_handler(e, 5, 0x40, FIRE.code)
        add_weapons(e, [10])
        e.write(TYPES + 0x6D4, u32(ttype))
        e.write(TYPES + 0x6E0, u32(0xDA7A))
        return (1, 2, 3, 4, 5, 6, TYPES, 8, 9)
    compare(emu, 0x413D10, "Weapon_Fire", setup=setup, check_eax=False, normalized=True)


def set_args(e, wid, t1, t2):
    ints = [wid] + [0x100 + i for i in range(1, 60)]
    ints[0x2F] = t1
    ints[0x30] = t2
    ints[0x33] = t2  # where the second type sits after the first type took 3 arguments
    reals = [0.1 * i - 2.3 for i in range(60)]
    e.write(INTS, b"".join(u32(v) for v in ints))
    e.write(REALS, b"".join(struct.pack("<d", v) for v in reals))
    for i in range(60):
        e.write(STRS + 64 * i, b"WEAPON_ID_%05d" % i + b"\0")


@pytest.mark.parametrize("wid,t1,t2,sized", [
    (40, 0x181, 0x181, (1, 1)), (5, 0x41, 0x181, (1, 1)), (40, 0x41, 0x42, (1, 1)),
    (40, 0x181, 0x42, (1, 1)), (40, 0x41, 0x42, (0, 1)), (40, 0x41, 0x42, (1, 0))])
def test_define(wid, t1, t2, sized):
    def setup(e):
        set_events(e)
        add_weapons(e, [1, 2, 30])
        e.write(MAXID, u32(30))
        if sized[0]:
            set_handler(e, 3, 0x41, SIZE.code)
        set_handler(e, 4, 0x41, INIT.code)
        if sized[1]:
            set_handler(e, 6, 0x42, SIZE2.code)
        set_handler(e, 7, 0x42, INIT2.code)
        set_args(e, wid, t1, t2)
        return OUT, 0, ARGS
    compare(emu, 0x413F90, "DefineWeaponType", setup=setup, ignore=IGNORE, normalized=True)


def test_define_duplicate():
    for fn in (0x413F90, "DefineWeaponType"):
        emu.reset()
        add_weapons(emu, [1, 2, 30])
        set_args(emu, 2, 0x181, 0x181)
        with pytest.raises(ErrorShown, match="not unique"):
            emu.call(fn, OUT, 0, ARGS)
