"""EditRigidObjAttachedToBone, MenuItemTask and MenuItem_Open (0x504AD0 - 0x505630)."""
import pytest

from test_menu_util import trap
from test_editobj_util import (FISTP_CWS, LOG, RET, add_child, f32, floatfake, handler, init_emu, logfake, obj, rnd,
                               run, set_events, set_handler, setup_types, strz, task, u16, u32)

BONE, PANEL, ITEM, OTHER = 0x6A, 0x6B, 0x6C, 0x6D
EV_508F60, EV_508F70, EV_POINTER, EV_LIST, EV_COMMIT, EV_SELECT = 0x3A, 0x3B, 0x3D, 0x3E, 0x3F, 0x40

emu = init_emu()
set_events(emu)
logfake(emu, 0x4C5710, 3)  # RigidDynCubeObj_Init
floatfake(emu, 0x4D0950, 2)  # VirModel_GetRadius
logfake(emu, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
logfake(emu, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
logfake(emu, 0x4C1780, 1)  # Debug_Log
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
logfake(emu, 0x4C1830, 1)  # QTaskList_Free
logfake(emu, 0x505A30, 1)  # MenuItem_FindSelected
logfake(emu, 0x4012A0, 3, masks={1: 0xFFFF}, effect=lambda e, a: RET.get(0x4012A0, 0))  # Task_New
for t in (PANEL, ITEM, OTHER):
    set_handler(emu, EV_COMMIT, t, handler(emu, "commit_%x" % t, 2))
    set_handler(emu, 1, t, handler(emu, "run_%x" % t, 1))
    set_handler(emu, EV_SELECT, t, handler(emu, "select_%x" % t, 2, derefs={1: 4}))
    for ev in (EV_508F60, EV_508F70, EV_POINTER):
        set_handler(emu, ev, t, handler(emu, "%x_%x" % (ev, t), 2))


def add_item(e):
    """The list handler of the items: adds the node at +0x54 to the panel's list."""
    i, holder = e.arg(0), e.arg(1)
    lst, node = e.u32(holder), i + 0x54
    LOG[-1].append(("list", e.normalize(i)))
    last = e.u32(lst + 8)
    u32(e, node, lst + 4, last)
    u32(e, last, node)
    u32(e, lst + 8, node)
    e.ret(0)


set_handler(emu, EV_LIST, ITEM, trap(emu, "add_item", add_item))
set_handler(emu, EV_LIST, OTHER, handler(emu, "list_other", 2, derefs={1: 4}))
emu.baseline = emu.snapshot()


def bone(e, rng):
    o = obj(e, 0x138, BONE, rng)
    f32(e, o + 0xF0, *[rng.uniform(-9, 9) for _ in range(9 + 3)])
    u32(e, o + 0x120, 7, 3)
    return o


def test_bone_create():
    def setup(e):
        RET[0x4D0950] = 6.5
        o = bone(e, rnd(1))
        e.write(o + 0x128, b"abcdefgh")
        return (o,)
    run(emu, 0x504AD0, "EditRigidObjAttachedToBone_Create", setup)


@pytest.mark.parametrize("name", [b"*crate", b"", b"crate", b"fourteen_chars"])
@pytest.mark.parametrize("found", [0, 0x3333])
def test_bone_changed(name, found):
    def setup(e):
        RET[0x4CEC10] = found
        o = bone(e, rnd(2))
        strz(e, o + 0x128, name)
        return (o,)
    run(emu, 0x504BB0, "EditRigidObjAttachedToBone_Changed", setup)


def test_bone_get_params():
    def setup(e):
        u32(e, 0xBA8080, 0xBA8080)
        return 0, e.alloc(4)
    run(emu, 0x504C40, "EditRigidObjAttachedToBone_GetParams", setup)


def test_bone_close():
    def setup(e):
        u32(e, 0xBA8080, 0xBA8080)
        u16(e, 0xBA8084, 0x84)
    run(emu, 0x504D90, "EditRigidObjAttachedToBone_Close", setup)


@pytest.mark.parametrize("orig,ours", [(0x504DB0, "EditRigidObjAttachedToBone_GetType"),
                                       (0x504DF0, "MenuItemTask_GetListEvent"),
                                       (0x504E00, "MenuItemTask_GetPointerEvent")])
def test_getters(orig, ours):
    run(emu, orig, ours, lambda e: u16(e, 0xBA8084, 0x1284), check_eax=True, eax_mask=0xFF)


def test_add_to_list():
    def setup(e):
        lst = e.alloc(12)
        u32(e, lst, lst + 4, 0, lst)
        old = e.alloc(8)
        u32(e, old, lst + 4, lst)
        u32(e, lst, old, 0, old)
        holder = e.alloc(4)
        u32(e, holder, lst)
        node = e.alloc(8)
        u32(e, node, 0x1111, 0x2222)
        return holder, node
    run(emu, 0x504DC0, "MenuItemTask_AddToList", setup)


def panel(e, cur=(0, 0, 0), old=(0, 0, 0)):
    p = task(e, 0x44, PANEL)
    u32(e, p + 0x20, 0x2020, 0x2424, 0x2828)
    u32(e, p + 0x2C, *(cur + old))
    return p


def item(e, p, id_, rect):
    i = task(e, 0x80, ITEM)
    u32(e, i + 0x38, id_)
    f32(e, i + 0x5C, *rect)
    u32(e, i + 0x74, i)
    add_child(e, p, i)
    return i


RECTS = [(10.0, 20.0, 30.0, 40.0), (100.0, 100.0, 50.5, 20.0), (35.5, 59.5, 4.5, 0.5), (39.5, 59.5, 1.0, 1.0)]


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("cur,old", [((40, 60, 0), (40, 60, 0)), ((40, 60, 0x10), (40, 60, 0)),
                                     ((40, 60, 0x20), (40, 60, 0)), ((40, 60, 0), (41, 60, 0)),
                                     ((10, 20, 0), (0, 0, 0)), ((5, 5, 0), (6, 5, 0)), ((40, 60, 0), (40, 59, 0)),
                                     ((125, 110, 0x40), (0, 0, 0)), ((36, 60, 0), (0, 0, 0)),
                                     ((39, 60, 0), (0, 0, 0))])
@pytest.mark.parametrize("selected", [False, True])
@pytest.mark.parametrize("other", [False, True])
def test_run(cur, old, selected, other, cw):
    def setup(e):
        p = panel(e, cur, old)
        items = [item(e, p, 11 + i, r) for i, r in enumerate(RECTS)]
        if other:
            add_child(e, p, task(e, 0x40, OTHER))
        RET[0x505A30] = items[1] if selected else 0
        return (p,)
    run(emu, 0x504EF0, "MenuItemTask_Run", setup, cw=cw)


def test_run_empty():
    def setup(e):
        p = panel(e, (3, 4, 0), (0, 0, 0))
        return (p,)
    run(emu, 0x504EF0, "MenuItemTask_Run", setup)


def test_set_pointer():
    def setup(e):
        p = panel(e, (1, 2, 3), (4, 5, 6))
        a = e.alloc(12)
        u32(e, a, 7, 8, 9)
        return p, a
    run(emu, 0x5053B0, "MenuItemTask_SetPointer", setup)


@pytest.mark.parametrize("orig,ours", [(0x5053F0, "MenuItemTask_Forward508F70"), (0x505480, "MenuItemTask_Forward508F60"),
                                       (0x505510, "MenuItemTask_ForwardList")])
@pytest.mark.parametrize("n", [0, 3])
def test_forward(orig, ours, n):
    def setup(e):
        p = panel(e)
        for i in range(n):
            add_child(e, p, task(e, 0x40, (OTHER, PANEL, 0x70)[i]))
        a = e.alloc(4)
        u32(e, a, 0xA5A5)
        return p, a
    run(emu, orig, ours, setup)


def test_close():
    run(emu, 0x5055A0, "MenuItemTask_Close", lambda e: u16(e, 0x54E708, 0x77))


def test_send_pointer():
    def setup(e):
        p = panel(e)
        return p, 0xB0B0
    run(emu, 0x5055D0, "MenuItemTask_SendPointer", setup)


def test_new():
    def setup(e):
        t = e.alloc(0x44)
        e.write(t, b"\x99" * 0x44)
        RET[0x4012A0] = t
        return (0x3030,)
    run(emu, 0x505600, "MenuItemTask_New", setup, check_eax=True)


init = init_emu()


@pytest.mark.parametrize("orig,ours", [(0x504C50, "EditRigidObjAttachedToBone_Open"), (0x504E10, "MenuItemTask_Open"),
                                       (0x505630, "MenuItem_Open")])
def test_open(orig, ours):
    run(init, orig, ours, setup_types)
