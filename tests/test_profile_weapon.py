import struct

import pytest

from emu import Emu, compare
from test_profile_util import LOG, RET, check, cstr, logfake, pyfake, task_system, u32

TASK_NEW, ROOT_TYPE, TYPE_NEW, SET_HANDLER = 0x4012A0, 0x4018F0, 0x401900, 0x401400
INIT = 0x407290
PARAM_TYPES = (0xA75898, 0xA71870, 0xA70C7C, 0xA71884, 0xA7186C, 0xA70C90, 0x54E160, 0x54E150)

emu = Emu()
STATE = {}


def run_script(e):
    """Stands in for loading weaponconfig.qsc: a root task with CHILDREN tasks of given types."""
    LOG[-1].append(("script", e.arg(0), e.read_cstr(e.arg(1)), e.arg(2)))
    if STATE["children"] is None:
        e.ret(0)
        return
    root = e.alloc(0x20)
    u32(e, root + 8, root + 0xC, 0, root + 8)
    for type_ in STATE["children"]:
        t = e.alloc(0x20)
        u32(e, t + 8, t + 0xC, 0, t + 8)
        e.write(t + 0x1C, struct.pack("<H", type_))
        last = e.u32(root + 0x10)
        u32(e, t, root + 0xC, last)
        u32(e, last, t)
        u32(e, root + 0x10, t)
    e.ret(root)


def handler(e):
    LOG[-1].append(("handler", e.arg(0), e.arg(1)))
    e.ret(0)


def apply_handler(e):
    pair = e.arg(1)
    LOG[-1].append(("apply", e.arg(0), e.u32(pair), e.u32(pair + 4)))
    e.ret(0)


pyfake(emu, 0x4F0CF0, run_script)
pyfake(emu, 0x401010, handler)
pyfake(emu, 0x401020, apply_handler)
logfake(emu, 0x4B8890, 4, strings=(0,))


def setup_init(e):
    task_system(e)
    for i, a in enumerate(PARAM_TYPES):
        u32(e, a, 0x100 + i)


def setup_types(e):
    setup_init(e)
    e.call(INIT)


def test_init():
    compare(emu, INIT, "WeaponConfig_Init", setup=setup_init, check_eax=False, normalized=True)


def test_shutdown():
    compare(emu, 0x407D00, "WeaponConfig_Shutdown", setup=setup_types, check_eax=False)


def test_get_event():
    compare(emu, 0x4071D0, "WeaponConfig_GetEvent", setup=lambda e: e.write(0x5690D8, b"\x2a"), eax_mask=0xFF)


@pytest.mark.parametrize("orig,ours,table", [(0x407B90, "WeaponConfigContainer_GetParams", 0x569204),
                                             (0x407CF0, "WeaponConfig_GetParams", 0x569208)])
def test_get_params(orig, ours, table):
    def setup(e):
        u32(e, table, 0x12345678)
        return 0, e.alloc(4)
    compare(emu, orig, ours, setup=setup, check_eax=False)


@pytest.mark.parametrize("children", [None, [], [5], [5, 6, 5], [6] * 70])
def test_read(children):
    def setup(e):
        STATE["children"] = children
        task_system(e)
        for t in (5, 6):
            e.call(SET_HANDLER, t, e.call(0x4F1A70)[0] & 0xFF, 0x401010)
        return 0x4444, cstr(e, b"LOCAL:weapons/weaponconfig.qsc")
    check(emu, 0x4071E0, "WeaponConfig_Read", setup=setup)


def test_container_create():
    def setup(e):
        e.write(0x5690F0, b"LOCAL:weapons/weaponconfig.qsc\0")
        t = e.alloc(0x124)
        e.write(t, b"\x77" * 0x124)
        return (t,)
    compare(emu, 0x407B50, "WeaponConfigContainer_Create", setup=setup, check_eax=False)


@pytest.mark.parametrize("orig,ours", [(0x407BA0, "WeaponConfig_Create"), (0x407BB0, "WeaponConfig_Clear")])
def test_create(orig, ours):
    def setup(e):
        t = e.alloc(0x708)
        e.write(t, b"\x77" * 0x708)
        return (t,)
    compare(emu, orig, ours, setup=setup, check_eax=False)


@pytest.mark.parametrize("value", [-1, 0, 5, 0x17F, 0x180, 0x7FFFFFFF])
def test_task_type_is_used(value):
    def setup(e):
        for i in (0, 5, 0x17F):
            e.write(0xAF5F05 + i * 0x30, b"\xff")
    compare(emu, 0x407CD0, "TaskType_IsUsed", setup=setup, args=(value,))


@pytest.mark.parametrize("case", ["missing", "no_handlers", "handlers", "bad_types", "unused_types"])
def test_apply(case):
    def setup(e):
        task_system(e)
        u32(e, 0x5690D8, 9)
        # one weapon type, id 42, found through the original lookup
        u32(e, 0x57B18C, 3)
        for i in range(3):
            e.write(0x56E258 + i * 0x6E8, bytes((i + j) & 0xFF for j in range(0x6E8)))
            u32(e, 0x56E258 + i * 0x6E8, 40 + i)
        cfg = e.alloc(0x708)
        e.write(cfg, bytes((i * 7) & 0xFF for i in range(0x708)))
        u32(e, cfg + 0x20, 43 if case == "missing" else 41)
        t1, t2 = (0x10007, 3) if case != "bad_types" else (0x180, -2)
        u32(e, cfg + 0x6F4, t1, t2)
        if case != "unused_types":
            for t in (7, 3):
                e.write(0xAF5F05 + t * 0x30, b"\x01")
        if case in ("handlers", "unused_types", "bad_types"):
            for t in (7, 3):
                u32(e, 0xA96AE0 + (9 * 0x180 + t) * 4, 0x401020)
        return (cfg,)
    check(emu, 0x407BE0, "WeaponConfig_Apply", setup=setup, check_eax=False)


def test_material_init():
    def setup(e):
        task_system(e)
        for i in range(32):
            e.write(0x569474 + i * 0x268, b"\x01")
    check(emu, 0x407D60, "Material_Init", setup=setup, check_eax=False)
