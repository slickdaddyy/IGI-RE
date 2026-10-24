import math
import struct

import pytest

from emu import Emu, ErrorShown, compare
from test_heli_util import (RET, children, cstr, d64, f32, handler, new_emu, run, set_handler, task, task_system, u16,
                            u32, vfake)


@pytest.fixture(autouse=True, params=[0x37F, 0x7F, 0xC7F, 0x77F], ids=hex)
def cw(request):
    """Every test at the emulator default and at the control words the game uses."""
    Emu.cw = request.param
    yield
    Emu.cw = None


CAR, VEHICLE, WHEEL, CHILD = 0x60, 0x61, 0x62, 0x70


def arg_string(e, a):
    e.write(a[2], b"wheel_%d\0" % a[1])


def arg_real(e, a):
    return [1.5, -0.25, 1e30][a[1] % 3]


emu = new_emu(
    fakes=[(0x4CEC10, 1, dict(strings=(0,))), (0x4C5710, 3), (0x4C7140, 5), (0x4EE140, 1, dict(strings=(0,))),
           (0x4C7560, 2), (0x401CF0, 2, dict(masks={0: 0xFFFF, 1: 0xFFFF})), (0x4015F0, 1), (0x416920, 0),
           (0x4C6940, 4, dict(derefs={2: 24})), (0x401A20, 1, dict(masks={0: 0xFFFF}))],
    floats=[(0x4D0950, 1), (0x4B8A50, 2)])
vfake(emu, 0x4B8A80, 4, effect=lambda e, a: arg_string(e, a))
vfake(emu, 0x4ECF80, 3, show=lambda e, a: (e.normalize(a[1]), e.normalize(a[2])),
      effect=lambda e, a: f32(e, a[0], *RET.get("vel", (3.0, 4.0, 12.0))))
vfake(emu, 0x414ED0, 3, show=lambda e, a: (e.normalize(a[1]), a[2]),
      effect=lambda e, a: f32(e, a[0], 1.0, 2.0, RET.get("pz", 100.0)))
e = emu
u16(e, 0x54E200, VEHICLE)
u16(e, 0x53BDA8, CAR)
u16(e, 0x57BD30, WHEEL)
set_handler(e, 1, CHILD, handler(e, "run", 1))
e.baseline = e.snapshot()

init = new_emu(fakes=[(0x4B8810, 2, dict(strings=(0,)))])


def car(e, type_=CAR, speed=0.5, turn=0.3, brake=0, wheels=((1, 1, 2, 30.0),), driven=2, tank=0, drop=10.0,
        touching=0, side=0, thrust=100.0):
    c = task(e, 0x2ED8, type_)
    f32(e, c + 0x720, speed)
    f32(e, c + 0x72C, turn)
    e.write(c + 0x744, bytes([brake]))
    u32(e, c + 0x758, driven)
    data = e.alloc(0x2C0)
    f32(e, data + 0x1C, thrust)
    u32(e, data + 0x118, len(wheels))
    for i, (steered, drv, joint, steer) in enumerate(wheels):
        p = data + 0x11C + 0x14 * i
        e.write(p, bytes([steered, drv]))
        f32(e, p + 4, steer)
        u32(e, p + 8, joint)
    f32(e, data + 0x25C, drop)
    e.write(data + 0x261, bytes([tank]))
    phys = e.alloc(0x2D8)
    u32(e, phys + 0x2D4, data)
    for j in range(16):
        u32(e, phys + 0x14C + 0x18 * j, 0x40 + j)
    u32(e, c + 0x1B4, phys)
    for j in range(22):
        p = c + 0x1BC + 0x38 * j
        u32(e, p, side)
        f32(e, p + 0xC, 10.0 * j, 20.0, 50.0 + j)
        e.write(p + 0x1A, bytes([touching]))
        f32(e, p + 0x34, 3.0 * j)
    return c


def wheel(e, owner, index=0, lift=0.0, spin=1.0, spin_speed=2.0, dead=0, radius=500.0, travel=0.5, kids=1):
    w = task(e, 0x158, WHEEL)
    f32(e, w + 0x98, 1.0, 1.5, 0.5)
    u32(e, w + 0x6C, 0x4444)
    u32(e, w + 0x68, 0x1111)
    d64(e, w + 0xF0, 10.0, -20.0, 30.0)
    f32(e, w + 0x108, 1.0, 0.0, 0.0, 0.0, 0.8, 0.6, 0.0, -0.6, 0.8)
    u32(e, w + 0x108 + 36, 3)
    u32(e, w + 0x130, owner)
    f32(e, w + 0x138, spin_speed, lift, spin, 0.0)
    u32(e, w + 0x148, index)
    data = e.alloc(0x2F0)
    f32(e, data + 0x2E8, radius, travel)
    phys = e.alloc(0x2D8)
    u32(e, phys + 0x2D4, data)
    u32(e, w + 0x14C, phys)
    e.write(w + 0x150, bytes([dead]))
    children(e, w, kids, CHILD)
    return w


def test_register_def_handlers():
    def setup(e):
        task_system(e)
        t = e.call(0x401900, cstr(e, b"Wheel"), 0x158, e.call(0x4018F0)[0] & 0xFFFF, 0)[0]
        u16(e, 0x57BD30, t)
        e.write(0x54DC30, b"\x40\x41")
    compare(init, 0x42FE40, "Wheel_RegisterDefHandlers", setup=setup, normalized=True, check_eax=False)


def test_parse_def():
    def setup(e):
        RET.clear()
        RET[0x4B8A50] = lambda e, a: arg_real(e, a)
        out = e.alloc(0x2F0)
        a = e.alloc(12)
        u32(e, a, out, 0x5555, 4)
        return 0, a
    run(e, 0x42FE80, "Wheel_ParseDef", setup)


def test_get_def_size():
    run(e, 0x42FEF0, "Wheel_GetDefSize", lambda e: (0, e.alloc(4)))


def test_init():
    def setup(e):
        task_system(e)
        t = e.call(0x401900, cstr(e, b"MagicObj"), 0xF0, e.call(0x4018F0)[0] & 0xFFFF, 0)[0]
        u16(e, 0x548648, t)
        e.write(0x54864A, b"\x30")
        e.write(0x539549, b"\x31")
        e.write(0x53954B, b"\x32")
        e.write(0x54DC32, b"\x33")
        e.write(0x54127C, b"\x34")
    run(init, 0x42FF00, "Wheel_Init", setup)


def create_arg(e, owner):
    a = e.alloc(0x90)
    u32(e, a, cstr(e, b"wheel_1"), 0x77)
    d64(e, a + 8, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0)
    f32(e, a + 0x38, *[0.1 * i for i in range(9)])
    u32(e, a + 0x38 + 36, 5)
    f32(e, a + 0x60, *[0.2 * i for i in range(9)])
    u32(e, a + 0x60 + 36, 6)
    u32(e, a + 0x88, owner, 0x99)
    return a


@pytest.mark.parametrize("type_", [CAR, VEHICLE])
def test_create(type_):
    def setup(e):
        RET.clear()
        RET[0x4CEC10] = 0x5000
        RET[0x4EE140] = 0x6000
        RET[0x4D0950] = 2.5
        c = car(e, type_)
        u32(e, c + 0x750, 3)
        w = task(e, 0x158, WHEEL)
        f32(e, w + 0x98, 1.0, 1.5, 0.5)
        f32(e, w + 0x138, 1.0, 2.0, 3.0, 4.0)
        e.write(w + 0x150, b"\1")
        return w, create_arg(e, c)
    run(e, 0x430000, "Wheel_Create", setup)


def test_create_no_model():
    def setup(e):
        RET.clear()
        return task(e, 0x158, WHEEL), create_arg(e, car(e))
    with pytest.raises(ErrorShown):
        run(e, 0x430000, "Wheel_Create", setup)


@pytest.mark.parametrize("type_", [CAR, VEHICLE])
def test_delete(type_):
    def setup(e):
        c = car(e, type_)
        u32(e, c + 0x750, 3)
        e.write(c + 0x75C + 0x1C * 2, b"\1")
        return (wheel(e, c, index=2),)
    run(e, 0x430170, "Wheel_Delete", setup)


RUNS = [
    dict(), dict(isa=0), dict(dead=1), dict(type_=VEHICLE), dict(type_=VEHICLE, brake=1),
    dict(touching=1), dict(touching=1, vel=(1.0, 2.0, 3.0)), dict(touching=1, vel=(40.0, 3.0, 0.5)),
    dict(touching=1, vel=(40.96, 0.0, 0.0)), dict(wheels=((1, 0, 2, 30.0),)), dict(wheels=((0, 1, -12, 30.0),)),
    dict(tank=1, side=1, turn=0.5), dict(tank=1, side=0, turn=-0.5), dict(tank=1, side=1, turn=-0.5),
    dict(tank=1, touching=1, side=0, turn=-0.25), dict(lift=0.5), dict(lift=-0.5, wheels=((1, 1, -3, 30.0),)),
    dict(lift=0.5, brake=1), dict(speed=-0.75, driven=3), dict(lift=float("nan")),
]


@pytest.mark.parametrize("c0", RUNS)
def test_run(c0):
    def setup(e):
        c = dict(c0)
        RET.clear()
        RET[0x401CF0] = c.pop("isa", 1)
        if "vel" in c:
            RET["vel"] = c.pop("vel")
        w = dict(lift=c.pop("lift", 0.0), dead=c.pop("dead", 0))
        return (wheel(e, car(e, **c), **w),)
    run(e, 0x4301C0, "Wheel_Run", setup)


@pytest.mark.parametrize("turn", [0.5, 0.01, 0.0100001, -0.01, -0.0100001, -0.75, float("nan")])
@pytest.mark.parametrize("side", [0, 1, 2])
def test_track_factor(turn, side):
    compare(e, 0x4304B0, "Car_GetTrackFactor", setup=lambda e: (car(e, turn=turn, side=side), 3), check_eax=False,
            check_st0=True)


LIFTS = [(100.0, 1.0, 5.0), (100.0, -1.0, 5.0), (52.0, 3.0, 0.0), (52.0, 0.5, 2.0), (55.0, 4.0, 1.0),
         (40.0, 7.0, 3.0), (60.0, 0.25, 20.0), (30.0, 2.0, 0.001), (52.0, 1.0, 1e-9), (float("nan"), 1.0, 1.0)]


@pytest.mark.parametrize("pz,amount,max_", LIFTS)
@pytest.mark.parametrize("drop", [0.0, 10.0])
def test_lift_joint(pz, amount, max_, drop):
    def setup(e):
        RET.clear()
        RET["pz"] = pz
        return car(e, drop=drop), 2, struct.unpack("<I", struct.pack("<f", amount))[0], \
            struct.unpack("<I", struct.pack("<f", max_))[0]
    run(e, 0x430530, "Car_LiftJoint", setup)


@pytest.mark.parametrize("type_", [CAR, VEHICLE])
@pytest.mark.parametrize("joint", [2, -13])
def test_move(type_, joint):
    def setup(e):
        RET.clear()
        RET[0x4D0950] = 1.25
        RET[0x416920] = 0x7777
        c = car(e, type_, wheels=((1, 1, joint, 30.0),))
        w = wheel(e, c)
        f32(e, w + 0x140, 0.7, -0.3)
        pos = e.alloc(24)
        d64(e, pos, 1000.5, -2000.25, 300.125)
        m = e.alloc(40)
        a, b = math.cos(0.4), math.sin(0.4)
        f32(e, m, a, -b, 0.0, b * 0.9, a * 0.9, 0.1, 0.05, 0.2, 0.97)
        u32(e, m + 36, 4)
        arg = e.alloc(12)
        u32(e, arg, pos, m, c)
        return w, arg
    run(e, 0x430640, "Wheel_Move", setup)


def test_close():
    run(e, 0x430940, "Wheel_Close", lambda e: None)


def test_get_type():
    compare(e, 0x430950, "Wheel_GetType", eax_mask=0xFFFF)
