import struct

import pytest

from test_props_util import (LOG, RET, SCALES, add_child, d64, f32, handler, init_emu, logfake, make_emu, mat, rigid,
                             run, set_handler, setup_init, task, tricky, u16, u32, vfake)

OBJ, CHILD = 0x52, 0x53
NAN = float("nan")

emu = make_emu()
logfake(emu, 0x4FC7B0, 1)  # PhysicsObj_IsResting
vfake(emu, 0x4FC0E0, 7, show=lambda e, a: (e.normalize(a[0]), a[1], a[2], a[3], a[4], a[5],
                                           e.normalize(e.u32(a[6] + 4))))  # PhysicsObj_Collide
logfake(emu, 0x4FB980, 1)  # PhysicsObj_Free
logfake(emu, 0x4FB5F0, 1)  # PhysicsObj_Init
logfake(emu, 0x4FB700, 2)  # PhysicsObj_SetPhysType
logfake(emu, 0x4EE140, 1, strings=(0,), ret=0x7700)  # PhysType_Find
for t in (OBJ, CHILD):
    for slot, n in ((1, 1), (0x31, 2)):
        set_handler(emu, slot, t, handler(emu, "%x_%x" % (slot, t), n))
emu.baseline = emu.snapshot()


def new_task(e, a):
    return RET["new"]


vfake(emu, 0x4012A0, 3, effect=new_task)  # Task_New

init = init_emu()


def obj(e, scale=(1.0, 2.0, 1.5), name=b"crate_01", mass=2.5, m=None):
    t = rigid(e, 0x768, OBJ, scale)
    if m:
        mat(e, t + 0x70, m)
    e.write(t + 0x154, name + b"\0")
    d64(e, t + 0x168, 5.0, 6.0, 7.0)
    f32(e, t + 0x180, 0.5, -1.5, 2.25)
    mat(e, t + 0x18C, unk=8, seed=3)
    phys = e.alloc(16)
    f32(e, phys + 8, mass)
    u32(e, phys + 12, 0x1111)
    u32(e, t + 0x1B4, phys)
    return t


def test_magic_open():
    compare_init(0x43E4F0, "GenericPhysicsMagicObj_Open")


def test_open():
    compare_init(0x43E8D0, "GenericPhysicsObj_Open")


def compare_init(addr, name):
    from emu import compare
    compare(init, addr, name, setup=setup_init, check_eax=False, normalized=True)


@pytest.mark.parametrize("addr,name", [(0x43E580, "GenericPhysicsMagicObj_Run"), (0x43EBA0, "GenericPhysicsObj_Run")])
@pytest.mark.parametrize("resting", [0, 1])
@pytest.mark.parametrize("children", [0, 1, 3])
def test_run(addr, name, resting, children):
    def setup(e):
        RET[0x4FC7B0] = resting
        t = obj(e)
        for i in range(children):
            add_child(e, t, task(e, 0x40, CHILD if i % 2 else OBJ))
        return (t,)
    run(emu, addr, name, setup)


@pytest.mark.parametrize("addr,name", [(0x43E600, "GenericPhysicsMagicObj_Collide"),
                                       (0x43EC30, "GenericPhysicsObj_Collide")])
@pytest.mark.parametrize("resting", [0, 1])
def test_collide(addr, name, resting):
    def setup(e):
        RET[0x4FC7B0] = resting
        return (obj(e),)
    run(emu, addr, name, setup)


def test_delete():
    run(emu, 0x43E650, "GenericPhysicsObj_Delete", lambda e: (obj(e),))


DIRS = [(1.0, 1.0, 1.0), (1.0, 0.0, 0.0), (0.3, -0.7, 0.2), (123.5, 1e-3, -77.0), (0.0, 0.0, 0.0)]
MATS = [None] + [m[1] for m in tricky()]


@pytest.mark.parametrize("d", DIRS)
@pytest.mark.parametrize("m", MATS)
@pytest.mark.parametrize("addr,name", [(0x43E670, "GenericPhysicsMagicObj_Hit"), (0x43EDF0, "GenericPhysicsObj_Hit")])
def test_hit(d, m, addr, name):
    def setup(e):
        src = rigid(e, 0xF0, OBJ)
        d64(e, src + 0x20, 1.25, -3.5, 1e10)
        msg = e.alloc(20)
        u32(e, msg, src, 0x4444)
        f32(e, msg + 8, *d)
        return obj(e, m=m), msg
    run(emu, addr, name, setup)


CENTERS = [(999.0, -2001.0, 29.5), (1000.0, -2000.0, 30.5), (1000.0 + 1e-9, -2000.0 + 3e-12, 30.5 - 7e-13),
           (-5e6, 3e7, 1.0)]


@pytest.mark.parametrize("c", CENTERS)
@pytest.mark.parametrize("m", MATS)
def test_explode(c, m):
    def setup(e):
        center = e.alloc(24)
        d64(e, center, *c)
        msg = e.alloc(8)
        u32(e, msg, 0x5555, center)
        return obj(e, m=m), msg
    run(emu, 0x43E7A0, "GenericPhysicsObj_Explode", setup)


def test_explode_precision():
    """x differences are kept in 80 bits, y and z rounded to double."""
    def setup(e):
        t = obj(e, m=[1.0] * 9)
        d64(e, t + 0x20, 1.0 + 2 ** -52, 1.0 + 2 ** -52, 1.0 + 2 ** -52)
        center = e.alloc(24)
        d64(e, center, -1e-30, -1e-30, -1e-30)
        msg = e.alloc(8)
        u32(e, msg, 0x5555, center)
        return t, msg
    run(emu, 0x43E7A0, "GenericPhysicsObj_Explode", setup)


def test_magic_close():
    def setup(e):
        u32(e, 0x53C46C, 0x12340044)
    run(emu, 0x43E8C0, "GenericPhysicsMagicObj_Close", setup)


def test_create():
    run(emu, 0x43EAA0, "GenericPhysicsObj_Create", lambda e: (obj(e),))


@pytest.mark.parametrize("scale", SCALES)
def test_init(scale):
    def setup(e):
        RET[0x4D0950] = 1.75
        return (obj(e, scale=scale),)
    run(emu, 0x43EAB0, "GenericPhysicsObj_Init", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57C13C, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x43EC20, "GenericPhysicsObj_GetParams", setup)


@pytest.mark.parametrize("model", [0, 0x3300])
@pytest.mark.parametrize("name", [b"crate_01", b"", b"x*y", b"abcdefghijklmn"])
def test_params_changed(model, name):
    def setup(e):
        RET[0x4CEC10] = model
        RET[0x4D0950] = 1.5
        return (obj(e, name=name),)
    run(emu, 0x43EC80, "GenericPhysicsObj_ParamsChanged", setup)


def test_close():
    def setup(e):
        u32(e, 0x57C13C, 0x5151)
        u32(e, 0x53C4A8, 0x12340044)
    run(emu, 0x43EEF0, "GenericPhysicsObj_Close", setup)


@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("handled", [0, 1])
def test_new(mode, handled):
    def setup(e):
        t = obj(e, name=b"")
        RET["new"] = t
        u16(e, 0x53C4A8, OBJ)
        e.write(0x9422FD, bytes([mode]))
        if not handled:
            set_handler(e, 0x31, OBJ, 0)
        pos = e.alloc(24)
        d64(e, pos, 1.0, 2.0, 3.0)
        m = e.alloc(40)
        mat(e, m, unk=11)
        name = e.alloc(16)
        e.write(name, b"barrel_02\0")
        vel = e.alloc(12)
        f32(e, vel, 7.0, 8.0, 9.0)
        return 0x4321, pos, m, name, vel
    run(emu, 0x43EF10, "GenericPhysicsObj_New", setup, check_eax=True, eax_mask=0xFFFFFFFF)
