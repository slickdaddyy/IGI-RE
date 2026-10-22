import struct

import pytest

from emu import Emu, compare
from test_profile_util import LOG, RET, check, cstr, f32, logfake, pyfake, task_system, u32

TASK_NEW, ROOT_TYPE = 0x4012A0, 0x4018F0
INIT, LOAD = 0x406450, 0x406DF0
ITEM_TYPE, CONFIG = 0x5383DA, 0x5690D0
PARAM_TYPES = (0xA75898, 0xA71870, 0xA70C7C, 0xA71884, 0xA7186C, 0xA70C90, 0x54E160, 0x54E150)

emu = Emu()
CHILDREN = []


def eval_expr(e):
    s = e.read_cstr(e.arg(0))
    LOG[-1].append(("eval", s))
    e.ret(int(s) if s.isdigit() else 0)


def run_script(e):
    """Stands in for loading settings.qsc: adds the items in CHILDREN to the parent."""
    parent = e.arg(0)
    LOG[-1].append(("script", e.read_cstr(e.arg(1)), e.arg(2)))
    if not CHILDREN:
        e.ret(0)
        return
    for ai, diff in CHILDREN:
        t = e.alloc(0x2A8)
        u32(e, t + 8, t + 0xC, 0, t + 8)
        u32(e, t + 0x14, parent)
        e.write(t + 0x1C, e.read(ITEM_TYPE, 2))
        e.write(t + 0x28, b"%d\0" % diff)
        e.write(t + 0x128, b"%d\0" % ai)
        last = e.u32(parent + 0x10)
        u32(e, t, parent + 0xC, last)
        u32(e, last, t)
        u32(e, parent + 0x10, t)
    e.ret(1)


pyfake(emu, 0x4B8770, eval_expr)
pyfake(emu, 0x4F0CF0, run_script)
logfake(emu, 0x4AF810, 3, strings=(0,))

bad = Emu()
logfake(bad, TASK_NEW, 3)
logfake(bad, 0x4AF810, 1, strings=(0,))


def setup_init(e):
    task_system(e)
    for i, a in enumerate(PARAM_TYPES):
        u32(e, a, 0x100 + i)


def setup_types(e):
    setup_init(e)
    e.call(INIT)


def new_root(e):
    return e.call(TASK_NEW, 0, e.call(ROOT_TYPE)[0], 0)[0]


def test_init():
    compare(emu, INIT, "HumanAIConfig_Init", setup=setup_init, check_eax=False, normalized=True)


def test_shutdown():
    compare(emu, 0x406D90, "HumanAIConfig_Shutdown", setup=setup_types, check_eax=False)


@pytest.mark.parametrize("orig,ours,table", [(0x406B60, "HumanAIConfig_GetParams", 0x5690B0),
                                             (0x406D00, "HumanAIConfigItem_GetParams", 0x5690C8)])
def test_get_params(orig, ours, table):
    def setup(e):
        u32(e, table, 0x12345678)
        return 0, e.alloc(4)
    compare(emu, orig, ours, setup=setup, check_eax=False)


@pytest.mark.parametrize("orig,ours,var", [(0x406DD0, "HumanAIConfig_GetType", 0x5383D8),
                                           (0x406DE0, "HumanAIConfigItem_GetType", 0x5383DA)])
def test_get_type(orig, ours, var):
    compare(emu, orig, ours, setup=lambda e: u32(e, var, 0x4567), eax_mask=0xFFFF)


@pytest.mark.parametrize("named", [False, True])
def test_item_create(named):
    def setup(e):
        item = e.alloc(0x2A8)
        e.write(item, b"\x55" * 0x2A8)
        names = 0
        if named:
            names = e.alloc(8)
            u32(e, names, cstr(e, b"GD_2"), cstr(e, b"AITYPE_RIFLE_EASY" * 3))
        return item, names
    compare(emu, 0x406B70, "HumanAIConfigItem_Create", setup=setup, check_eax=False)


@pytest.mark.parametrize("ai,diff", [(b"5", b"2"), (b"28", b"0"), (b"x", b"1")])
def test_item_update(ai, diff):
    def setup(e):
        item = e.alloc(0x2A8)
        e.write(item + 0x28, diff + b"\0")
        e.write(item + 0x128, ai + b"\0")
        return (item,)
    check(emu, 0x406D10, "HumanAIConfigItem_Update", setup=setup, check_eax=False)


def test_load_fails():
    for fn in (LOAD, "HumanAIConfig_Load"):
        bad.reset()
        LOG.append([])
        bad.call(fn, 0x1234)
    assert LOG[-1] == LOG[-2]


@pytest.mark.parametrize("children", [[], [(0, 0)], [(3, 1), (28, 2), (0, 0), (3, 1)],
                                      [(a, d) for a in range(29) for d in range(3)]])
def test_load(children):
    def setup(e):
        CHILDREN[:] = children
        setup_types(e)
        return (new_root(e),)
    check(emu, LOAD, "HumanAIConfig_Load", setup=setup, check_eax=False, normalized=True)


def human(e, ai, owner_type):
    owner = e.alloc(0xD10)
    e.write(owner + 0x1C, struct.pack("<H", owner_type))
    h = e.alloc(0x3DD0)
    u32(e, h + 0x14, owner)
    u32(e, h + 0x3D38, ai)
    return h


@pytest.mark.parametrize("case", ["ok", "other_owner", "no_config", "no_item", "odd_values"])
def test_apply(case):
    def setup(e):
        CHILDREN[:] = []
        setup_types(e)
        e.call(LOAD, new_root(e))
        u32(e, 0xBC2380, 2, 1)
        u32(e, 0xBC2388 + 0x564, 2)
        e.write(0x5BDABC, struct.pack("<H", 0x77))
        hc = e.u32(CONFIG)
        if case == "no_config":
            u32(e, CONFIG, 0)
        if case == "no_item":
            u32(e, hc + 0x24 + (7 * 3 + 2) * 4, 0)
        if case == "odd_values":
            item = e.u32(hc + 0x24 + (7 * 3 + 2) * 4)
            f32(e, item + 0x22C, 0.05, 2.5 / 30, -0.05)
            f32(e, item + 0x268, 1e9, 1.1, -3.3, 0.7)
            u32(e, item + 0x234, 0x7FA00000)
        return (human(e, 7, 0x76 if case == "other_owner" else 0x77),)
    check(emu, 0x406F70, "HumanAIConfig_Apply", setup=setup, check_eax=False)


@pytest.mark.parametrize("ai,diff", [(0, 0), (28, 2), (13, 1)])
def test_item_update_name(ai, diff):
    def setup(e):
        item = e.alloc(0x2A8)
        u32(e, item + 0x20, diff, ai)
        return (item,)
    compare(emu, 0x406D40, "HumanAIConfigItem_UpdateName", setup=setup, check_eax=False)
