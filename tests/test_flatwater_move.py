"""SmoothQTask and MoveRigidObj handlers (0x51F140 - 0x51FBA0)."""
import pytest

from test_flatwater_util import (CWS, RET, children, d64, f32, fake_emu, handler, run, set_handler, task, u16,
                                 u32, with_cw)

COMMON = {
    0x4F1320: 1,  # VarString_Init
    0x4F1340: 1,  # VarString_Free
    0x4F1400: 2,  # VarString_Bind
    0x4F16C0: 1,  # VarString_HasExpr
    0x4F1050: 1,  # Task_GetId
    0x4F1270: (2, {"derefs": {0: 4}, "effect": lambda e, a: e.write(a[0], b"Obj_7\0")}),  # Task_GetName
    0x4B7FC0: (2, {"strings": (0,)}),  # Script_RegisterReal
    0x4B80A0: (1, {"strings": (0,)}),  # Script_RemoveSymbol
    0x4E0930: 2,  # MagicObjContainer_New
    0x4C5710: 3,  # RigidDynCubeObj_Init
    0x4C7140: (5, {"derefs": {2: 24}}),  # DynCube_Add
    0x4C6940: (4, {"derefs": {2: 24}}),  # DynCube_Move
    0x4C7560: 2,  # DynCube_Remove
    0x4015F0: 1,  # Task_Delete
    0x4B38E0: 4,  # Matrix_SetEuler
}
emu = fake_emu(COMMON, floats={0x4F16E0: 2, 0x4D0950: 1})
emu2 = fake_emu(COMMON, floats={0x4F16E0: 2, 0x4D0950: 2})
H_RUN = handler(emu, "run", 1)
H_CHG = handler(emu, "chg", 2)
H_OFF = handler(emu, "off", 1)
SMOOTH, MOVE, KID = 0x40, 0x41, 0x42
NAN = float("nan")


def smooth(e, ideal=0.0, value=0.0, const=0.0, start=0.0, up=0.1, down=0.1, lo=-10.0, hi=10.0, step=0.0):
    t = task(e, 0x100, SMOOTH)
    e.write(t + 0x20, bytes(range(0x40)))
    d64(e, t + 0x60, ideal, value, const, start, up, down, lo, hi, step)
    return t


def test_smooth_create():
    def setup(e):
        t = task(e, 0x100)
        e.write(t + 0x20, bytes(range(0xE0)))
        return (t,)
    run(emu, 0x51F140, "SmoothQTask_Create", setup)


@pytest.mark.parametrize("registered", [0, 1])
def test_smooth_activate(registered):
    def setup(e):
        t = smooth(e)
        e.write(t + 0xF8, bytes([registered]))
        return (t,)
    run(emu, 0x51F1F0, "SmoothQTask_Activate", setup)


@pytest.mark.parametrize("registered", [0, 1])
def test_smooth_deactivate(registered):
    def setup(e):
        t = smooth(e)
        e.write(t + 0xF8, bytes([registered]))
        return (t,)
    run(emu, 0x51F270, "SmoothQTask_Deactivate", setup)


@pytest.mark.parametrize("id_,has", [(-1, True), (0, True), (5, False)])
def test_smooth_delete(id_, has):
    def setup(e):
        set_handler(e, 0x3B, SMOOTH, H_OFF if has else 0)
        return (smooth(e),)
    run(emu, 0x51F2E0, "SmoothQTask_Delete", setup, rets={0x4F1050: id_})


SMOOTH_CASES = [
    # has expr, expr value, value, const, up, down, min, max, step
    (0, 0.0, 0.0, 1.0, 0.1, 0.1, -10.0, 10.0, 0.0),
    (0, 0.0, 0.95, 1.0, 0.1, 0.1, -10.0, 10.0, 0.0),
    (0, 0.0, 1.0, 1.0, 0.1, 0.1, -10.0, 10.0, 0.0),
    (0, 0.0, 3.0, 1.0, 0.1, 0.7, -10.0, 10.0, 0.0),
    (0, 0.0, 1.05, 1.0, 0.1, 0.1, -10.0, 10.0, 0.0),
    (1, 2.5, 0.3, 9.0, 0.37, 0.1, -10.0, 10.0, 0.25),
    (1, -2.5, 0.3, 9.0, 0.37, 0.33, -10.0, 10.0, 0.3),
    (1, 7.0, 6.9, 0.0, 0.33, 0.1, -10.0, 10.0, -0.7),
    (1, 7.0, 6.9, 0.0, 0.33, 0.1, -10.0, 5.0, 0.0),
    (1, -7.0, -6.9, 0.0, 0.33, 0.33, -2.0, 5.0, 0.0),
    (1, 1e-17, 0.0, 0.0, 1e-17, 0.1, -2.0, 5.0, 0.0),
    (1, 1.0, 1.0 - 2.0 ** -53, 0.0, 2.0 ** -60, 0.1, -2.0, 5.0, 0.0),
    (1, NAN, 0.5, 0.0, 0.25, 0.1, -2.0, 5.0, 0.0),
    (0, 0.0, NAN, 1.0, 0.25, 0.1, -2.0, 5.0, 0.0),
    (0, 0.0, 2.5, 1.0, 0.25, 0.1, -2.0, NAN, 0.0),
    (0, 0.0, 2.5, 1.0, 0.25, 0.1, 3.0, 1.0, 0.0),
    (0, 0.0, 2.5, 4.0, 0.25, 0.1, 3.0, 1.0, 0.5),
    (1, 123.456, 120.0, 0.0, 1.111, 0.1, -200.0, 200.0, 0.1),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("has,expr,value,const,up,down,lo,hi,step", SMOOTH_CASES)
@pytest.mark.parametrize("kids", [0, 2])
def test_smooth_run(cw, has, expr, value, const, up, down, lo, hi, step, kids):
    def setup(e):
        t = smooth(e, 0.0, value, const, 0.0, up, down, lo, hi, step)
        if kids:
            children(e, t, kids, type_=KID)
            set_handler(e, 1, KID, H_RUN)
        return (t,)
    run(emu, with_cw(emu, 0x51F340, cw), with_cw(emu, "SmoothQTask_Run", cw), setup,
        rets={0x4F16C0: has, 0x4F16E0: expr})


@pytest.mark.parametrize("arg", [None, b"", b"x"])
def test_smooth_params_changed(arg):
    def setup(e):
        from test_flatwater_util import cstr
        t = smooth(e)
        d64(e, t + 0x78, 4.25)
        children(e, t, 2, type_=KID)
        set_handler(e, 0x39, KID, H_CHG)
        return t, cstr(e, arg) if arg is not None else 0
    run(emu, 0x51F4B0, "SmoothQTask_ParamsChanged", setup)


def move(e, value=0.0, last=-1.0, scale=(1.0, 2.0, 1.5), keys=None):
    t = task(e, 0x208, MOVE)
    e.write(t + 0x20, bytes(range(0xD0)))
    u32(e, t + 0x68, 0x6868, 0x6C6C)
    f32(e, t + 0x98, *scale)
    keys = keys or ((10.0, 20.0, -30.0, 0.5, 1.0, -0.25), (-4.0, 22.5, 33.0, 1.5, -1.0, 0.75))
    for i, k in enumerate(keys):
        d64(e, t + 0xF0 + 24 * i, *k[:3])
        f32(e, t + 0x150 + 4 * i, k[3])
        f32(e, t + 0x158 + 4 * i, k[4])
        f32(e, t + 0x160 + 4 * i, k[5])
    d64(e, t + 0x1A0, value, last)
    return t


SCALES = [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 1.0, 4.0), (2.0, 2.0, 2.0), (NAN, 1.0, 2.0), (1.0, NAN, 2.0),
          (1.0, 2.0, NAN)]


@pytest.mark.parametrize("scale", SCALES)
def test_move_create(scale):
    def setup(e):
        u32(e, 0xA44344, 0x4344)
        return (move(e, scale=scale),)
    run(emu2, 0x51F810, "MoveRigidObj_Create", setup, rets={0x4E0930: 0xE8E8, 0x4D0950: 2.5})


@pytest.mark.parametrize("registered", [0, 1])
def test_move_activate(registered):
    def setup(e):
        t = move(e)
        e.write(t + 0x204, bytes([registered]))
        return (t,)
    run(emu, 0x51F980, "MoveRigidObj_Activate", setup)


@pytest.mark.parametrize("registered", [0, 1])
def test_move_deactivate(registered):
    def setup(e):
        t = move(e)
        e.write(t + 0x204, bytes([registered]))
        return (t,)
    run(emu, 0x51F9E0, "MoveRigidObj_Deactivate", setup)


@pytest.mark.parametrize("id_,has", [(-1, True), (0, True), (5, False)])
def test_move_delete(id_, has):
    def setup(e):
        set_handler(e, 0x3B, MOVE, H_OFF if has else 0)
        t = move(e)
        u32(e, t + 0xE8, 0xE8E8)
        return (t,)
    run(emu, 0x51FA30, "MoveRigidObj_Delete", setup, rets={0x4F1050: id_})


MOVE_CASES = [
    # has expr, expr, value, last
    (0, 0.0, 0.0, -1.0),
    (0, 0.0, 0.25, -1.0),
    (0, 0.0, 0.25, 0.25),
    (0, 0.0, 1.0, 0.25),
    (0, 0.0, 0.5, 0.5),
    (1, 0.75, 0.5, 0.5),
    (1, 0.3333, 0.5, 0.5),
    (1, 1.2, 0.5, 0.5),
    (1, 4.7, 0.5, 0.5),
    (1, -0.3, 0.5, 0.5),
    (1, -3.9, 0.5, 0.5),
    (1, -2.0, 0.5, 0.5),
    (1, 1.0 + 2.0 ** -40, 0.5, 0.5),
    (1, -(2.0 ** -60), 0.5, 0.5),
    (1, 0.4999999, 0.5, 0.5),
    (1, 0.6666667, 0.5, 0.5),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("has,expr,value,last", MOVE_CASES)
@pytest.mark.parametrize("scale", SCALES[:2] + SCALES[4:5])
def test_move_run(cw, has, expr, value, last, scale):
    def setup(e):
        t = move(e, value, last, scale)
        children(e, t, 2, type_=KID)
        set_handler(e, 1, KID, H_RUN)
        return (t,)
    run(emu, with_cw(emu, 0x51FAA0, cw), with_cw(emu, "MoveRigidObj_Run", cw), setup,
        rets={0x4F16C0: has, 0x4F16E0: expr, 0x4D0950: 3.75})


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("value,last,force", [(0.5, 0.5, 0), (0.5, 0.5, 1), (0.1, 0.5, 0), (NAN, 0.5, 0),
                                              (NAN, NAN, 1), (0.9, -1.0, 0), (1.0, 0.0, 0), (0.0, 1.0, 0),
                                              (0.123456789, 0.0, 0), (0.5, 0.0, 0)])
def test_move_apply_other(cw, value, last, force):
    def setup(e):
        t = move(e, value, last)
        o = move(e, scale=(0.5, 4.0, 1.0))
        u32(e, o + 0x68, 0x9898, 0x6D6D)
        return t, o, force | 0xABCDEF00
    run(emu, with_cw(emu, 0x51FBA0, cw), with_cw(emu, "MoveRigidObj_Apply", cw), setup, rets={0x4D0950: 1.25})
