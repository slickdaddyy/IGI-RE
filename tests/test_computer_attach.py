"""HumanAttach and DeathZone (0x465A10 - 0x466230)."""
import random
import struct

import pytest

from emu import ErrorShown, compare
from test_computer_util import (RET, check, d64, f32, floatfake, handler, isa_fake, logfake, make_emu, mat, obj,
                                run, set_handler, setup_open, task, task_array, u16, u32, with_cw)

PHYS, ZONE, VEH, TRAIN, HUMAN = 0x60, 0x61, 0x62, 0x63, 0x64
WORLD = 0x9999
HAI = 0x6000  # HumanAI list
DZ_EVENT = 0x2A

init = make_emu()

emu = make_emu()
isa_fake(emu)
logfake(emu, 0x504AD0, 2)
logfake(emu, 0x4C1780, 1)
logfake(emu, 0x4015F0, 1)  # Task_Delete
logfake(emu, 0x4F0EE0, 0, ret=0x0EE0)
logfake(emu, 0x43EF10, 5, derefs={1: 24, 2: 40, 3: 16, 4: 12},
        effect=lambda e, a: RET.get("phys", 0))  # GenericPhysicsObj_New
logfake(emu, 0x4CEC10, 1, strings=(0,), effect=lambda e, a: RET.get("model", 0))  # VirModel_Find
logfake(emu, 0x4C5710, 3)  # RigidDynCubeObj_Init
floatfake(emu, 0x4D0950, 1)  # VirModel_GetRadius
logfake(emu, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
logfake(emu, 0x4C6940, 4, derefs={2: 24})  # DynCube_Move
logfake(emu, 0x416920, 0, ret=WORLD)  # Game_GetWorld
logfake(emu, 0x4FC7B0, 1, effect=lambda e, a: RET.get("rest", 0))  # PhysicsObj_IsResting
logfake(emu, 0x460C60, 0, effect=lambda e, a: RET.get("hai", 0))
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
for slot, n, d in ((0x30, 2, None), (DZ_EVENT, 2, {1: 8})):
    for t in (PHYS, HUMAN, HUMAN + 1):
        set_handler(emu, slot, t, handler(emu, "%x_%x" % (slot, t), n, derefs=d))
emu.baseline = emu.snapshot()


@pytest.mark.parametrize("orig,ours", [(0x465A10, "HumanAttach_Open"), (0x465BC0, "DeathZone_Open")])
def test_open(orig, ours):
    compare(init, orig, ours, setup=setup_open, check_eax=False, normalized=True)


# --- HumanAttach ---------------------------------------------------------------

def attach(e, detached=0):
    t = obj(e, 0x140, 0x50)
    e.write(t + 0x128, b"part_01\0")
    e.write(t + 0x138, bytes([detached]))
    return t


def test_create():
    run(emu, 0x465AC0, "HumanAttach_Create", lambda e: (attach(e, 1), 0x1234))


@pytest.mark.parametrize("detached", [0, 1, 0x80])
def test_run(detached):
    run(emu, 0x465AE0, "HumanAttach_Run", lambda e: (attach(e, detached),))


@pytest.mark.parametrize("has", [0, 1])
def test_detach(has):
    def setup(e):
        RET["phys"] = task(e, 0x40, PHYS if has else 0x70)
        return attach(e), 0x5555
    run(emu, 0x465B00, "HumanAttach_Detach", setup)


@pytest.mark.parametrize("a,b", [(0, 0), (1, 0), (2, 0), (0, 1), (0, 2), (3, 3), (1, 2)])
def test_detect(a, b):
    def setup(e):
        t = attach(e)
        parent = task(e, 0x40)
        u32(e, t + 0x14, parent)
        q = e.alloc(0x10)
        u32(e, q, 0xCCCCCCCC, 0)
        pick = [0x777, t, parent, 0x888]
        u32(e, q + 8, pick[a], pick[b])
        return t, q
    run(emu, 0x465B80, "HumanAttach_Detect", setup)


def test_close():
    run(emu, 0x465BB0, "HumanAttach_Close", lambda e: u16(e, 0x5BDC48, 0x47))


# --- DeathZone -----------------------------------------------------------------

def zone_arg(e, rng, vehicle=0):
    a = e.alloc(0x90)
    u32(e, a, e.alloc(16), WORLD)
    e.write(e.u32(a), b"dzone\0")
    d64(e, a + 8, *[rng.uniform(-5000, 5000) for _ in range(6)])
    mat(e, a + 0x38, rng, 5)
    mat(e, a + 0x60, rng, 9)
    u32(e, a + 0x88, vehicle)
    return a


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("scale", [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 1.0, 4.0), (2.0, 2.0, 2.0)])
def test_dz_create(scale, cw):
    def setup(e):
        RET["model"] = 0x4D4D
        RET[0x4D0950] = 37.25
        z = obj(e, 0x138, ZONE, scale=scale)
        u32(e, z + 0x6C, 0x4D4D)
        return z, zone_arg(e, random.Random(3))
    run(emu, with_cw(emu, 0x465CD0, cw), with_cw(emu, "DeathZone_Create", cw), setup)


def test_dz_create_error():
    msgs = []
    for target in (0x465CD0, "DeathZone_Create"):
        emu.reset()
        RET.clear()
        z = obj(emu, 0x138, ZONE)
        with pytest.raises(ErrorShown) as err:
            emu.call(target, z, zone_arg(emu, random.Random(3)))
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]


def humans(e, n):
    return task_array(e, [task(e, 0x40, HUMAN + (i & 1)) for i in range(n)])


@pytest.mark.parametrize("veh,isa,kind,parent_train,speed,rest,n", [
    (0, 1, 0, 0, 1.0, 0, 2),
    (1, 0, 0, 0, 1.0, 0, 2),
    (1, 1, 0, 0, 1.0, 0, 2),
    (1, 1, 0, 0, 1.0, 1, 2),
    (1, 1, 0, 0, 1.0, 0, 0),
    (1, 1, 1, 0, 2.5, 0, 3),
    (1, 1, 1, 0, 0.0, 0, 3),
    (1, 1, 1, 0, -0.0, 0, 3),
    (1, 1, 1, 0, float("nan"), 0, 3),
    (1, 1, 1, 1, 2.5, 0, 1),
    (1, 1, 1, 1, 0.0, 0, 1),
])
def test_dz_run(veh, isa, kind, parent_train, speed, rest, n):
    def setup(e):
        RET[0x4F45F0] = 0x55
        RET[0x42BC20] = TRAIN
        RET["rest"] = rest
        RET["hai"] = humans(e, n)
        e.write(0x53F4FE, bytes([DZ_EVENT]))
        z = obj(e, 0x138, ZONE)
        v = 0
        if veh:
            v = obj(e, 0x200, TRAIN if kind else VEH)
            RET[("isa", TRAIN if kind else VEH)] = isa
            p = obj(e, 0x200, TRAIN if parent_train else VEH)
            u32(e, v + 0x14, p)
            f32(e, v + 0x108, speed if not parent_train else 7.0)
            f32(e, p + 0x108, speed if parent_train else 9.0)
        u32(e, z + 0x130, v)
        return (z,)
    run(emu, 0x465DE0, "DeathZone_Run", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("isa", [0, 1])
def test_dz_move(seed, isa, cw):
    def setup(e):
        rng = random.Random(seed)
        RET[0x4D0950] = rng.uniform(1, 100)
        RET[("isa", VEH)] = isa
        z = obj(e, 0x138, ZONE, scale=(rng.uniform(0, 3), rng.uniform(0, 3), rng.uniform(0, 3)), seed=seed)
        d64(e, z + 0xF0, *[rng.uniform(-500, 500) for _ in range(3)])
        mat(e, z + 0x108, rng, rng.randrange(100))
        v = obj(e, 0x200, VEH)
        u32(e, z + 0x130, v)
        a = e.alloc(8)
        pos = e.alloc(24)
        d64(e, pos, *[rng.uniform(-50000, 50000) for _ in range(3)])
        m = e.alloc(40)
        mat(e, m, rng, rng.randrange(100))
        u32(e, a, pos, m)
        return z, a
    run(emu, with_cw(emu, 0x465F50, cw), with_cw(emu, "DeathZone_Move", cw), setup)


def test_dz_close():
    def setup(e):
        u16(e, 0x53F4FC, 0x47)
        e.write(0x53F4FE, bytes([0x2B]))
    run(emu, 0x466210, "DeathZone_Close", setup)


def test_dz_get_event():
    def setup(e):
        e.write(0x53F4FC, struct.pack("<I", 0x12345678))
    compare(emu, 0x466230, "DeathZone_GetEvent", setup=setup, eax_mask=0xFF)
