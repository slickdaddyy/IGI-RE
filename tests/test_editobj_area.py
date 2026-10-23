"""PhysicsMagicObj and AreaActivate (0x503D60 - 0x504AC0)."""
import struct

import pytest

from emu import ErrorShown
from test_editobj_util import (CWS, LOG, RET, add_child, cstr, d64, f32, floatfake, handler, init_emu, logfake, mat,
                               obj, pyfake, rnd, run, set_events, set_handler, setup_types, task, u16, u32)
from test_menu_util import trap

PHYS, AREA, OTHER = 0x67, 0x68, 0x69
MOVE_EVENT = 0x2D  # MagicObj move event, set by set_events
DAC_EVENT = 0x25
CRITERIA = 0xA7A640

emu = init_emu()
set_events(emu)
logfake(emu, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
logfake(emu, 0x4C5710, 3)  # RigidDynCubeObj_Init
floatfake(emu, 0x4D0950, 1)  # VirModel_GetRadius (the second arg is not always passed)
logfake(emu, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
logfake(emu, 0x4C6940, 4, derefs={2: 24})  # DynCube_Move
logfake(emu, 0x4FB5F0, 1)  # PhysicsObj_Init
logfake(emu, 0x4EE140, 1, strings=(0,), ret=0x4EE1)  # PhysType_Find
logfake(emu, 0x4FB700, 2)  # Vehicle_SetPhysType
logfake(emu, 0x4C4950, 3)
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
logfake(emu, 0x4C1830, 1)  # QTaskList_Free
logfake(emu, 0x4B8770, 1, strings=(0,))  # Script_EvalExpr
logfake(emu, 0x4B7E80, 2, strings=(0,))  # Script_RegisterInt
logfake(emu, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
logfake(emu, 0x4B8920, 1, strings=(0,))  # Script_UnregisterSymbol
logfake(emu, 0x4F1050, 1)  # Task_GetId
logfake(emu, 0x4B0D10, 1)  # Mem_Free


def get_name(e):
    e.write(e.arg(0), b"Area_7\0")
    LOG[-1].append(("name", e.normalize(e.arg(1))))
    e.ret(0)


pyfake(emu, 0x4F1270, get_name)
for t in (PHYS, AREA, OTHER):
    set_handler(emu, MOVE_EVENT, t, handler(emu, "move_%x" % t, 2, derefs={1: 8}))
    set_handler(emu, 1, t, handler(emu, "run_%x" % t, 1))
    set_handler(emu, DAC_EVENT, t, handler(emu, "dac_%x" % t, 1))


def criteria_fn(i):
    def fn(e):
        a = e.arg(0)
        LOG[-1].append(("crit", i, e.normalize(e.u32(a)), e.u32(a + 4)))
        u32(e, a + 4, i * 3 + 1)
        e.ret(0)
    return trap(emu, "crit_%d" % i, fn)


CRIT_FNS = [criteria_fn(i) for i in range(31)]
u16(emu, 0x54E66C, PHYS)
emu.baseline = emu.snapshot()

SCALES = [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 2.0, 2.5)]


def create_arg(e, rng, name=b"crate"):
    a = e.alloc(0x8C)
    u32(e, a, cstr(e, name), 0x4444)
    d64(e, a + 8, *[rng.uniform(-1000, 1000) for _ in range(6)])
    mat(e, a + 0x38, rng, 3)
    mat(e, a + 0x60, rng, 4)
    u32(e, a + 0x88, 0x8888)
    return a


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("found", [0, 0x3333])
@pytest.mark.parametrize("scale", SCALES)
def test_phys_create(found, scale, cw):
    def setup(e):
        RET[0x4CEC10] = found
        RET[0x4D0950] = 4.5
        rng = rnd(1)
        return obj(e, 0x768, PHYS, rng, scale=scale), create_arg(e, rng)
    if found:
        run(emu, 0x503DF0, "PhysicsMagicObj_Create", setup, cw=cw)
    else:
        with pytest.raises(ErrorShown):
            run(emu, 0x503DF0, "PhysicsMagicObj_Create", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("kids", [(), (PHYS,), (OTHER, PHYS, 0x70)])
@pytest.mark.parametrize("seed", [1, 2])
def test_phys_move(kids, seed, cw):
    def setup(e):
        RET[0x4D0950] = 2.25
        rng = rnd(seed)
        o = obj(e, 0x768, PHYS, rng, scale=SCALES[seed])
        d64(e, o + 0x720, *[rng.uniform(-100, 100) for _ in range(3)])
        mat(e, o + 0x738, rng, 5)
        for k in kids:
            add_child(e, o, task(e, 0x40, k))
        a = e.alloc(8)
        p, m = e.alloc(24), e.alloc(40)
        d64(e, p, *[rng.uniform(-1000, 1000) for _ in range(3)])
        mat(e, m, rng, 6)
        u32(e, a, p, m)
        return o, a
    run(emu, 0x503F20, "PhysicsMagicObj_Move", setup, cw=cw)


@pytest.mark.parametrize("which", ["self", "owner", "other"])
def test_phys_slot12(which):
    def setup(e):
        o = task(e, 0x768, PHYS)
        u32(e, o + 0x130, 0x1300)
        a = e.alloc(8)
        u32(e, a, 0xEEEEEEEE, {"self": o, "owner": 0x1300, "other": 0x5555}[which])
        return o, a
    run(emu, 0x504250, "PhysicsMagicObj_Slot12", setup)


def test_phys_close():
    run(emu, 0x504280, "PhysicsMagicObj_Close", None)


def test_phys_get_type():
    run(emu, 0x5042A0, "PhysicsMagicObj_GetType", None, check_eax=True, eax_mask=0xFFFF)


def area(e, rng, size=(100.0, 50.0, 30.0), mask=0, criteria=None):
    a = task(e, 0xB8, AREA)
    d64(e, a + 0x50, *[rng.uniform(-1000, 1000) for _ in range(3)])
    mat(e, a + 0x68, rng, 2)
    f32(e, a + 0x90, size[0] / 2, size[1] / 2, size[2] / 2)
    f32(e, a + 0x9C, *size)
    u32(e, a + 0xA8, 77, mask, cstr(e, criteria) if criteria else 0)
    return a


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("point", [(0, 0, 0), (49, 0, 0), (51, 0, 0), (0, -24.9, 0), (0, 0, 15.0), (0, 0, -15.5),
                                   (-60, 0, 0), (0, 26, 0), (float("nan"), 0, 0), (20, -20, 10)])
@pytest.mark.parametrize("seed", [1, 2])
def test_contains(point, seed, cw):
    def setup(e):
        rng = rnd(seed)
        a = area(e, rng)
        if seed == 1:
            f32(e, a + 0x68, 1, 0, 0, 0, 1, 0, 0, 0, 1)
        m = struct.unpack("<9f", e.read(a + 0x68, 36))
        base = struct.unpack("<3d", e.read(a + 0x50, 24))
        # world = pos + M * local
        w = [base[r] + sum(m[r * 3 + c] * point[c] for c in range(3)) for r in range(3)]
        p = e.alloc(24)
        d64(e, p, *w)
        return a, p
    run(emu, 0x5042B0, "AreaActivate_Contains", setup, cw=cw, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("registered", [0, 1])
@pytest.mark.parametrize("orig,ours", [(0x504530, "AreaActivate_Activate"), (0x504590, "AreaActivate_Deactivate")])
def test_activate(orig, ours, registered):
    def setup(e):
        a = area(e, rnd(3))
        e.write(a + 0xB5, bytes([registered]))
        return (a,)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("criteria", [None, b"CRITERIA_HUMAN|4"])
@pytest.mark.parametrize("size", [(100.0, 50.0, 30.0), (0.0, 0.0, 0.0), (3.3, 1e-3, 7e5), (float("nan"), 1.0, 2.0)])
@pytest.mark.parametrize("seed", [1, 2])
def test_changed(criteria, size, seed, cw):
    def setup(e):
        RET[0x4B8770] = 0x55
        return (area(e, rnd(seed), size=size, criteria=criteria),)
    run(emu, 0x5045F0, "AreaActivate_Changed", setup, cw=cw)


def test_get_params():
    def setup(e):
        u32(e, 0xA7A7B4, 0x7B47B4)
        return 0, e.alloc(4)
    run(emu, 0x504890, "AreaActivate_GetParams", setup)


@pytest.mark.parametrize("mask", [0, 1, 0x80000005, 0x40000000, 0x7FFFFFFF])
@pytest.mark.parametrize("kids", [(), (AREA, OTHER)])
def test_run(mask, kids):
    def setup(e):
        for i in range(31):
            u32(e, CRITERIA + 12 * i, 1, 0x1000 + i, CRIT_FNS[i])
        a = area(e, rnd(4), mask=mask)
        for k in kids:
            add_child(e, a, task(e, 0x40, k))
        return (a,)
    run(emu, 0x5048A0, "AreaActivate_Run", setup)


@pytest.mark.parametrize("id_", [-1, 0, 9])
@pytest.mark.parametrize("criteria", [None, b"x"])
def test_delete(id_, criteria):
    def setup(e):
        RET[0x4F1050] = id_
        return (area(e, rnd(5), criteria=criteria),)
    run(emu, 0x504970, "AreaActivate_Delete", setup)


def test_create():
    def setup(e):
        a = area(e, rnd(6), mask=5, criteria=b"x")
        e.write(a + 0xB4, b"\x11\x22\x33\x44")
        return (a,)
    run(emu, 0x5049D0, "AreaActivate_Create", setup)


def test_close():
    def setup(e):
        u16(e, 0x54E69C, 0x44)
        e.write(0x54E69E, b"\x12")
        u32(e, 0xA7A7B4, 0x7B47B4)
    run(emu, 0x5049F0, "AreaActivate_Close", setup)


@pytest.mark.parametrize("free", [0, 5, 30, None])
def test_add_criteria(free):
    def setup(e):
        for i in range(31):
            e.write(CRITERIA + 12 * i, bytes([0 if i == free or (free is not None and i > free) else 1]))
        return cstr(e, b"CRITERIA_X"), 0x12345
    if free is None:
        with pytest.raises(ErrorShown):
            run(emu, 0x504A30, "AreaActivate_AddCriteria", setup)
    else:
        run(emu, 0x504A30, "AreaActivate_AddCriteria", setup, check_eax=True)


@pytest.mark.parametrize("bit", [1, 2, 0x40, 0x80000000, 0x30])
def test_remove_criteria(bit):
    def setup(e):
        for i in range(31):
            u32(e, CRITERIA + 12 * i, 1, cstr(e, b"CRIT_%d" % i), 0)
        return (bit,)
    run(emu, 0x504A90, "AreaActivate_RemoveCriteria", setup)


def test_get_mask():
    run(emu, 0x504AC0, "AreaActivate_GetMask", lambda e: (area(e, rnd(7), mask=0x1234),), check_eax=True)


init = init_emu()


@pytest.mark.parametrize("orig,ours", [(0x503D60, "PhysicsMagicObj_Open"), (0x5043B0, "AreaActivate_Open")])
def test_open(orig, ours):
    run(init, orig, ours, setup_types)
