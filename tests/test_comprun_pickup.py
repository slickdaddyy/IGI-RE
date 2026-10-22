import pytest

from emu import Emu
from test_comprun_util import (RET, add_child, check_init, d64, f32, floatfake, handler, init_emu, logfake, obj, run,
                               set_handler, task, u16, u32, vfake)

PICK = 0x51
DAC = 0x37
SHOW, HIDE = 0x39, 0x3A
GUN_EV, AMMO_EV = 0x40, 0x41

emu = Emu()
for a, n in ((0x4C1790, 2), (0x4C17C0, 2), (0x4C5710, 3), (0x4C7560, 2), (0x4DCCB0, 3), (0x4DCF00, 3),
             (0x4DCDD0, 1), (0x4C48D0, 2), (0x4015F0, 1), (0x4C1830, 1), (0x4B38E0, 4), (0x47CE00, 3)):
    logfake(emu, a, n)
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
logfake(emu, 0x4168C0, 0, ret=0x9A00)  # Game_GetTaskList2
logfake(emu, 0x4C3440, 1, ret=0x9B00)  # DynCubeObj_GetPosition
logfake(emu, 0x4F0EE0, 0, ret=0x9C00)
logfake(emu, 0x416D20, 0, ret=0x9D00)  # Game_GetRandom
floatfake(emu, 0x4D0950, 1)  # VirModel_GetRadius
floatfake(emu, 0x4B4770, 1)  # Random_Float
logfake(emu, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
logfake(emu, 0x4C6940, 4, derefs={2: 24})  # DynCube_Move
logfake(emu, 0x4E7200, 3, strings=(0,))  # Sound_Play2D
logfake(emu, 0x4E6B00, 2, strings=(1,))  # Sound_Play
logfake(emu, 0x4E6C30, 2, derefs={1: 24})  # Sound_SetPos
logfake(emu, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
logfake(emu, 0x4F1050, 1)  # Task_GetId
logfake(emu, 0x4F1220, 0, ret=DAC)
logfake(emu, 0x4CEA10, 0, ret=SHOW)
logfake(emu, 0x4CEA20, 0, ret=HIDE)
logfake(emu, 0x4F1A70, 0, ret=0x35)  # Param_GetChangedEvent
logfake(emu, 0x416850, 0, ret=0x1231)  # Game_GetEvent8, junk above the byte
logfake(emu, 0x4B7EE0, 2, strings=(0,))  # Script_RegisterBool8
logfake(emu, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
logfake(emu, 0x4B8770, 1, strings=(0,))  # Script_EvalExpr
logfake(emu, 0x413BD0, 1)  # WeaponType_Find
logfake(emu, 0x47CD40, 1)  # AmmoType_Find
logfake(emu, 0x485970, 4, strings=(1, 2, 3))  # StatusMessageShow
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
logfake(emu, 0x401CF0, 2, masks={0: 0xFFFF, 1: 0xFFFF})  # TaskType_IsA
logfake(emu, 0x460BE0, 0, ret=0x77)  # Human_GetType
logfake(emu, 0x4012A0, 3, masks={1: 0xFFFF}, effect=lambda e, a: RET["task"])  # Task_New


def get_name(e, a):
    e.write(a[0], b"pickup_7\0")


vfake(emu, 0x4F1270, 2, show=lambda e, a: (e.normalize(a[1]),), effect=get_name)  # Task_GetName


def euler(e, a):
    f32(e, a[1], RET["euler"][0])
    f32(e, a[2], RET["euler"][1])
    f32(e, a[3], RET["euler"][2])


vfake(emu, 0x4B3C50, 4, show=lambda e, a: (e.normalize(a[0]),), effect=euler)  # Matrix_GetEuler


def coll(e, a):
    hit = RET["hit"]
    u32(e, a[1] + 0x58, hit[0])
    if hit[0]:
        d64(e, a[1] + 8, *hit[1:])


vfake(emu, 0x4CC2A0, 8, effect=coll,
      show=lambda e, a: (a[0], e.read(a[2], 24), e.read(a[3], 24), a[4], a[5] & 0xFF, e.normalize(e.u32(a[6] + 4)),
                         a[7]))  # Coll_Lines

HANDLER = handler(emu, "child", 2)
TOUCHED = handler(emu, "touched", 2, derefs={1: 8})
DACH = handler(emu, "dac", 1)
CHANGED = handler(emu, "changed", 2)
emu.baseline = emu.snapshot()

init = init_emu()


def pickup(e, size, scale=(1.0, 2.0, 1.5)):
    return obj(e, size, PICK, scale=scale)


def toucher(e, dx, dy, dz, base):
    who = task(e, 0x40, 0x52)
    by = e.alloc(0x20)
    u32(e, by, who, 0x1234)
    d64(e, by + 8, base[0] - dx, base[1] - dy, base[2] - dz)
    return by


SCALES = [(1.0, 2.0, 1.5), (3.0, 2.0, 1.0), (1.0, 1.0, 4.0), (float("nan"), 1.0, 2.0)]


@pytest.mark.parametrize("addr,name", [(0x46C0C0, "GenericPickup_Init"), (0x46CB70, "GunPickup_Init"),
                                       (0x46D420, "AmmoPickup_Init"), (0x46DB40, "AddAmmo_Init")])
def test_init(addr, name):
    check_init(init, addr, name)


@pytest.mark.parametrize("addr,name,globs", [
    (0x46C950, "GenericPickup_Close", (0x5BE2D4, 0x5BE2D0)),
    (0x46D3E0, "GunPickup_Close", (0x5BE2F8, 0x53FDE0, 0x5BE2E0)),
    (0x46DB10, "AmmoPickup_Close", (0x5BE304, 0x53FE28, 0x5BE300)),
    (0x46DCE0, "AddAmmo_Close", (0x5BE310, 0x5BE30C))])
def test_close(addr, name, globs):
    def setup(e):
        for i, g in enumerate(globs):
            u32(e, g, 0xABCD0010 + i)
    run(emu, addr, name, setup)


@pytest.mark.parametrize("addr,name,glob", [(0x46C480, "GenericPickup_GetParams", 0x5BE2D4),
                                            (0x46D040, "GunPickup_GetParams", 0x5BE2F8),
                                            (0x46D7C0, "AmmoPickup_GetParams", 0x5BE304),
                                            (0x46DCA0, "AddAmmo_GetParams", 0x5BE310)])
def test_get_params(addr, name, glob):
    def setup(e):
        u32(e, glob, 0x6543)
        return 0, e.alloc(4)
    run(emu, addr, name, setup)


@pytest.mark.parametrize("addr,name", [(0x46C970, "GunPickup_GetEvent"), (0x46D410, "AmmoPickup_GetEvent")])
def test_get_event(addr, name):
    def setup(e):
        u32(e, 0x53FDE0, 0x11223344)
        u32(e, 0x53FE28, 0x55667788)
    run(emu, addr, name, setup, eax_mask=0xFF, check_eax=True)


# GenericPickup

@pytest.mark.parametrize("scale", SCALES)
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_generic_create(scale, cw):
    def setup(e):
        RET[0x4D0950] = 300.5
        return (pickup(e, 0x120, scale),)
    run(emu, 0x46C2B0, "GenericPickup_Create", setup, cw=cw)


@pytest.mark.parametrize("picked", [0, 1])
@pytest.mark.parametrize("inworld", [0, 1])
def test_generic_run(picked, inworld):
    def setup(e):
        t = pickup(e, 0x120)
        e.write(t + 0x119, bytes([picked, inworld]))
        return (t,)
    run(emu, 0x46C3C0, "GenericPickup_Run", setup)


@pytest.mark.parametrize("addr,name,size,flag", [(0x46C410, "GenericPickup_Destroy", 0x120, 0x11A),
                                                 (0x46D760, "Pickup_Destroy", 0x158, None)])
@pytest.mark.parametrize("inworld", [0, 1])
@pytest.mark.parametrize("tid", [-1, 0, 5])
@pytest.mark.parametrize("has", [0, 1])
def test_destroy(addr, name, size, flag, inworld, tid, has):
    def setup(e):
        RET[0x4F1050] = tid
        t = pickup(e, size)
        if flag:
            e.write(t + flag, bytes([inworld]))
        set_handler(e, DAC, PICK, DACH if has else 0)
        return (t,)
    run(emu, addr, name, setup)


@pytest.mark.parametrize("model", [0, 0x7800])
@pytest.mark.parametrize("inworld", [0, 1])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_generic_changed(model, inworld, cw):
    def setup(e):
        RET[0x4CEC10] = model
        RET[0x4D0950] = 1234.25
        t = pickup(e, 0x120)
        e.write(t + 0x108, b"gun_box\0")
        e.write(t + 0x11A, bytes([inworld]))
        d64(e, t + 0xF0, 1.5, 2.5, -3.5)
        return (t,)
    run(emu, 0x46C490, "GenericPickup_Changed", setup, cw=cw)


@pytest.mark.parametrize("addr,name,size,reg,flag", [
    (0x46C570, "GenericPickup_Register", 0x120, 0x118, 0x119), (0x46C5D0, "GenericPickup_Unregister", 0x120, 0x118, 0),
    (0x46D170, "GunPickup_Register", 0x158, 0x128, 0x129), (0x46D1D0, "GunPickup_Unregister", 0x158, 0x128, 0),
    (0x46D8D0, "AmmoPickup_Register", 0x138, 0x108, 0x109), (0x46D930, "AmmoPickup_Unregister", 0x138, 0x108, 0)])
@pytest.mark.parametrize("registered", [0, 1])
def test_register(addr, name, size, reg, flag, registered):
    def setup(e):
        t = pickup(e, size)
        e.write(t + reg, bytes([registered]))
        return (t,)
    run(emu, addr, name, setup)


TOUCH = [(0.0, 0.0, 0.0), (2000.0, 2000.0, 4095.0), (2000.0, 2100.0, 0.0), (0.0, 0.0, -4096.0), (0.0, 0.0, -4095.9),
         (100.0, -50.0, float("nan")), (float("nan"), 0.0, 0.0), (5000.0, 0.0, 10.0)]


@pytest.mark.parametrize("d", TOUCH)
@pytest.mark.parametrize("r", [100.0, 4000.0, float("nan")])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_generic_touch(d, r, cw):
    def setup(e):
        RET[0x4D0950] = r
        t = pickup(e, 0x120)
        return t, toucher(e, *d, (100.5, -200.25, 300.125))
    run(emu, 0x46C630, "GenericPickup_Touch", setup, cw=cw)


@pytest.mark.parametrize("addr,name,ev", [(0x46C750, "GenericPickup_Show", SHOW),
                                         (0x46C8A0, "GenericPickup_Hide", HIDE)])
@pytest.mark.parametrize("inworld", [0, 1])
@pytest.mark.parametrize("picked", [0, 1])
@pytest.mark.parametrize("kids", [0, 1, 3])
def test_generic_show_hide(addr, name, ev, inworld, picked, kids):
    def setup(e):
        RET[0x4D0950] = 50.0
        t = pickup(e, 0x120)
        e.write(t + 0x119, bytes([picked, inworld]))
        for i in range(kids):
            c = task(e, 0x20, 0x60 + i)
            add_child(e, t, c)
            set_handler(e, ev, 0x60 + i, HANDLER if i != 1 else 0)
        return t, 0x4321
    run(emu, addr, name, setup)


# GunPickup

@pytest.mark.parametrize("euler", [(0.0, 0.5, 1.0), (-91.0, -0.5, 2.0), (90.0, 0.0, 3.0), (90.5, -0.0, 0.0),
                                   (float("nan"), float("nan"), 1.0), (-90.0, 1.0, 1.0)])
@pytest.mark.parametrize("hit", [0, 1])
@pytest.mark.parametrize("changed", [0, 1])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_gun_new(euler, hit, changed, cw):
    def setup(e):
        RET["euler"] = euler
        RET["hit"] = (hit, 11.5, -22.25, 333.125)
        t = pickup(e, 0x158)
        u16(e, t + 0x1C, 0x61)
        RET["task"] = t
        set_handler(e, 0x35, 0x61, CHANGED if changed else 0)
        pos = e.alloc(24)
        d64(e, pos, 1000.25, -2000.5, 3000.75)
        u32(e, 0x5BE2E0, 0x12340061)
        return 0x6000, pos, 0x6100, 17
    run(emu, 0x46C980, "GunPickup_New", setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("scale", SCALES[:2])
@pytest.mark.parametrize("addr,name,size", [(0x46CD20, "GunPickup_Create", 0x158),
                                           (0x46D5F0, "AmmoPickup_Create", 0x138)])
def test_pickup_create(scale, addr, name, size):
    def setup(e):
        RET[0x4D0950] = 77.5
        t = pickup(e, size, scale)
        e.write(t + 0xF0, b"\x55" * (size - 0xF0))
        return (t,)
    run(emu, addr, name, setup)


RUNS = [(0.0, 0.0, 0.0), (10.0, 20.0, 5.0), (20.0, 10.0, 5.0), (20.0, 10.0, -500.0), (10.0, 10.0, 409.6),
        (10.0, 10.0, -409.5), (10.0, 10.0, 409.7), (float("nan"), 1.0, 1.0), (1.0, 1.0, float("nan")),
        (5.0, 4.0, float("nan")), (12345.678, 12345.5, 1234.5)]


@pytest.mark.parametrize("zgf", RUNS)
@pytest.mark.parametrize("rnd", [0.5, 0.75])
@pytest.mark.parametrize("sound", [0, 0x7900])
@pytest.mark.parametrize("picked", [0, 1])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_gun_run(zgf, rnd, sound, picked, cw):
    def setup(e):
        RET[0x4B4770] = rnd
        RET[0x4E6B00] = sound
        RET[0x4D0950] = 64.0
        t = pickup(e, 0x158)
        d64(e, t + 0xF0, 1.0, 2.0, zgf[0])
        d64(e, t + 0x108, 3.0, 4.0, zgf[1])
        d64(e, t + 0x120, zgf[2])
        e.write(t + 0x129, bytes([picked]))
        return (t,)
    run(emu, 0x46CEA0, "GunPickup_Run", setup, cw=cw)


@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("model", [0, 0x7800])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_gun_changed(found, model, cw):
    def setup(e):
        w = 0
        if found:
            w = e.alloc(0x700)
            e.write(w + 0x61C, b"m_ak47\0")
            u32(e, w + 0x648, 90)
        RET[0x413BD0] = w
        RET[0x4CEC10] = model
        RET[0x4B8770] = 3
        RET[0x4D0950] = 99.0
        t = pickup(e, 0x158)
        d64(e, t + 0xF0, 5.0, 6.0, 7.0)
        e.write(t + 0x130, b"WEAPON_ID_AK47\0")
        return (t,)
    run(emu, 0x46D050, "GunPickup_Changed", setup, cw=cw)


@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("model", [0, 0x7800])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_ammo_changed(found, model, cw):
    def setup(e):
        a = 0
        if found:
            a = e.alloc(0x200)
            e.write(a + 0x148, b"m_ammo\0")
        RET[0x47CD40] = a
        RET[0x4CEC10] = model
        RET[0x4B8770] = 4
        RET[0x4D0950] = 12.0
        t = pickup(e, 0x138)
        d64(e, t + 0xF0, 5.0, 6.0, 7.0)
        e.write(t + 0x110, b"AMMO_ID_9MM\0")
        return (t,)
    run(emu, 0x46D7D0, "AmmoPickup_Changed", setup, cw=cw)


@pytest.mark.parametrize("d", TOUCH[:4] + TOUCH[5:7])
@pytest.mark.parametrize("r", [100.0, 4000.0])
@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("addr,name,size", [(0x46D230, "GunPickup_Touch", 0x158),
                                           (0x46D990, "AmmoPickup_Touch", 0x138)])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_touch(d, r, has, addr, name, size, cw):
    def setup(e):
        RET[0x4D0950] = r
        w = e.alloc(0x200)
        e.write(w + 0x110, b"AK47\0")
        RET[0x413BD0] = w
        u32(e, 0x53FDE0, GUN_EV)
        u32(e, 0x53FE28, AMMO_EV)
        t = pickup(e, size)
        u32(e, t + 0x12C, 7, 0, 0, 0, 0, 0, 0, 0, 0, 30)
        u32(e, t + 0x10C, 3)
        u16(e, t + 0x130, 0xFFF0)
        set_handler(e, GUN_EV, 0x52, TOUCHED if has else 0)
        set_handler(e, AMMO_EV, 0x52, TOUCHED if has else 0)
        return t, toucher(e, *d, (100.5, -200.25, 300.125))
    run(emu, addr, name, setup, cw=cw)


@pytest.mark.parametrize("picked", [0, 1])
def test_ammo_run(picked):
    def setup(e):
        t = pickup(e, 0x138)
        e.write(t + 0x109, bytes([picked]))
        return (t,)
    run(emu, 0x46D740, "AmmoPickup_Run", setup)


# AddAmmo

def test_addammo_create():
    def setup(e):
        t = task(e, 0x48, 0x70)
        e.write(t + 0x20, b"\x66" * 0x28)
        return (t,)
    run(emu, 0x46DC20, "AddAmmo_Create", setup)


@pytest.mark.parametrize("human", [0, 1])
@pytest.mark.parametrize("found", [0, 0x7A00])
@pytest.mark.parametrize("count", [5, 0xFFFE])
def test_addammo_run(human, found, count):
    def setup(e):
        RET[0x401CF0] = human
        RET[0x47CD40] = found
        h = task(e, 0x400, 0x71)
        t = task(e, 0x48, 0x70, h)
        u32(e, t + 0x20, 9)
        u16(e, t + 0x44, count)
        return (t,)
    run(emu, 0x46DC40, "AddAmmo_Run", setup)


def test_addammo_changed():
    def setup(e):
        RET[0x4B8770] = 6
        t = task(e, 0x48, 0x70)
        e.write(t + 0x24, b"AMMO_ID_12G\0")
        return (t,)
    run(emu, 0x46DCB0, "AddAmmo_Changed", setup)
