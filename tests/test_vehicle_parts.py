import random
import struct

import pytest

from emu import ErrorShown, compare
from test_vehicle_util import (RET, cstr, d64, f32, floatfake, init_emu, logfake, run, set_events, setup_init, task,
                               u32, vfake)

PART, OWNER, TARGET = 0x70, 0x71, 0x72

ARGS = {3: 90.0, 4: -45.5, 5: 0.3}


def make_emu(rot=None):
    """rot: matrix written by Matrix_SetRotX/Y/Z instead of running them."""
    emu = init_emu()
    set_events(emu)
    logfake(emu, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
    logfake(emu, 0x4C5710, 3)  # RigidDynCubeObj_Init
    floatfake(emu, 0x4D0950, 1)  # VirModel_GetRadius
    logfake(emu, 0x4C7140, 5, derefs={2: 24})
    logfake(emu, 0x4C7560, 2)
    logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
    vfake(emu, 0x4C6940, 4, show=lambda e, a: (a[0], e.normalize(a[1]), e.read(a[2], 24), a[3]))
    logfake(emu, 0x416D20, 0, ret=0x5000)  # Game_GetRandom
    logfake(emu, 0x4B47C0, 2, ret=17)  # Random_Int
    logfake(emu, 0x4B8A20, 2)  # Script_GetArgInt
    floatfake(emu, 0x4B8A50, 2, effect=lambda e, a: ARGS.get(a[1], 1.0))  # Script_GetArgReal
    vfake(emu, 0x4CE410, 8, show=lambda e, a: (a[0], e.normalize(a[1]), e.read(a[2], 24), a[3] & 0xFF,
                                               e.read(a[4] + 4, 12), a[5], a[6]))  # Coll_LineOfSight
    vfake(emu, 0x429A30, 10, show=lambda e, a: (a[0], e.normalize(a[1]), e.normalize(a[2]), e.read(a[3], 24),
                                                e.normalize(a[4]), a[5], e.read(a[6], 24), a[7], e.read_cstr(a[8]), a[9]))
    logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    u32(emu, 0xA758AC, 0x7777)
    if rot:
        def set_rot(e, a):
            f32(e, a[0], *rot)
            u32(e, a[0] + 0x24, 6)
        for a in (0x4B3B60, 0x4B3BA0, 0x4B3BE0):
            vfake(emu, a, 2, show=lambda e, a: (a[1],), effect=set_rot)
    emu.baseline = emu.snapshot()
    return emu


emu = make_emu()

init = init_emu()
SEED = [7]


def mat(e, p, vals=None, unk=3):
    rng = random.Random(p * 31 + SEED[0])
    f32(e, p, *(vals or [rng.uniform(-1, 1) for _ in range(9)]))
    u32(e, p + 0x24, unk)


def part(e, axis=0, angle=0.25, open_=0, speed=0.05, target=0.75, scale=(1.0, 2.0, 1.5), cooldown=None):
    p = task(e, 0x148, PART)
    d64(e, p + 0x20, 1000.0, -2000.0, 30.5)
    u32(e, p + 0x68, 0x68, 0x6C6C)
    mat(e, p + 0x70)
    f32(e, p + 0x98, *scale)
    d64(e, p + 0xF0, 120.5, -33.25, 7.125)
    mat(e, p + 0x108, unk=5)
    params = e.alloc(12)
    u32(e, params, axis)
    f32(e, params + 4, speed, target)
    u32(e, p + 0x138, params)
    if cooldown is None:
        f32(e, p + 0x13C, angle)
    else:
        u32(e, p + 0x13C, cooldown)
    e.write(p + 0x140, bytes([open_]))
    return p


def create_arg(e):
    a = e.alloc(0x90)
    u32(e, a, cstr(e, b"door_l"), 0x44)
    d64(e, a + 8, 5.0, 6.0, 7.0, 15.0, 25.0, -35.0)
    mat(e, a + 0x38, unk=1)
    mat(e, a + 0x60, unk=2)
    u32(e, a + 0x88, 0x8888)
    params = e.alloc(12)
    u32(e, params, 45)
    u32(e, a + 0x8C, params)
    return a


def move_arg(e):
    a = e.alloc(8)
    pos = e.alloc(24)
    d64(e, pos, -500.0, 250.0, 12.0)
    m = e.alloc(0x28)
    mat(e, m, unk=9)
    u32(e, a, pos, m)
    return a


INITS = [(0x426630, "CarDoor_Init"), (0x426D90, "HeliWheel_Init"), (0x4272A0, "HeliDoor_Init"),
         (0x427830, "RocketPod_Init"), (0x427D90, "HitZone_Init")]


@pytest.mark.parametrize("orig,ours", INITS)
def test_init(orig, ours):
    compare(init, orig, ours, setup=setup_init, check_eax=False, normalized=True)


CREATES = [(0x426740, "CarDoor_Create"), (0x426E90, "HeliWheel_Create"), (0x427940, "RocketPod_Create")]


@pytest.mark.parametrize("orig,ours", CREATES)
@pytest.mark.parametrize("scale", [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 2.0, 2.5), (1.0, 2.0, 2.0), (2.0, 2.0, 2.0)])
def test_create(orig, ours, scale):
    def setup(e):
        RET[0x4CEC10] = 0x3333
        RET[0x4D0950] = 12.3456789
        return part(e, open_=1, scale=scale), create_arg(e)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("orig,ours", CREATES)
def test_create_no_model(orig, ours):
    def setup(e):
        RET[0x4CEC10] = 0
        return part(e), create_arg(e)
    with pytest.raises(ErrorShown):
        run(emu, orig, ours, setup)


def test_delete():
    run(emu, 0x426860, "VehiclePart_Delete", lambda e: (part(e),))


MOVES = [(0x426880, "CarDoor_Move"), (0x426FA0, "HeliWheel_Move"), (0x4274E0, "HeliDoor_Move")]


@pytest.mark.parametrize("orig,ours", MOVES)
@pytest.mark.parametrize("axis", [0, 1, 2, 3])
@pytest.mark.parametrize("angle", [0.0, 0.7, -1.3])
@pytest.mark.parametrize("seed", range(3))
def test_move(orig, ours, axis, angle, seed):
    if axis == 3 and orig == 0x426880:
        pytest.skip("CarDoor_Move uses an unset matrix for other axes")

    def setup(e):
        SEED[0] = seed
        RET[0x4D0950] = 3.75
        return part(e, axis=axis, angle=angle), move_arg(e)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("orig,ours,n", [(0x426CD0, "CarDoor_ParseArgs", 3), (0x427240, "HeliWheel_ParseArgs", 2),
                                         (0x4277C0, "HeliDoor_ParseArgs", 3), (0x427D40, "RocketPod_ParseArgs", 1),
                                         (0x427F50, "HitZone_ParseArgs", 2)])
@pytest.mark.parametrize("index", [3, 4])
def test_parse_args(orig, ours, n, index):
    def setup(e):
        RET[0x4B8A20] = 7
        a = e.alloc(12)
        u32(e, a, e.alloc(16), 0x4545, index)
        return 0, a
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("item", ["self", "target", "child", "other"])
def test_coll_filter(item):
    def setup(e):
        me, tgt = task(e, 0x148, PART), task(e, 0x148, TARGET)
        t = {"self": me, "target": tgt}.get(item) or task(e, 0x148, PART)
        if item == "child":
            u32(e, t + 0x130, me)
        flt = e.alloc(16)
        u32(e, flt, 0x55, 0x1234, me, tgt)
        return t, flt
    run(emu, 0x426D30, "VehiclePart_CollFilter", setup)


@pytest.mark.parametrize("orig,ours", [(0x426D60, "CarDoor_Close"), (0x427280, "HeliWheel_Close"),
                                       (0x427810, "HeliDoor_Close"), (0x427D80, "RocketPod_Close")])
def test_close(orig, ours):
    def setup(e):
        for i, a in enumerate((0x53BABC, 0x57BC7C, 0x53BB18, 0x53BB3C)):
            u32(e, a, 0x44440000 + i)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("orig,ours,addr", [(0x426D70, "CarDoor_GetType", 0x53BABC),
                                            (0x427290, "HeliWheel_GetType", 0x57BC7C),
                                            (0x427820, "HeliDoor_GetType", 0x53BB18)])
def test_get_type(orig, ours, addr):
    compare(emu, orig, ours, setup=lambda e: u32(e, addr, 0x12340055), eax_mask=0xFFFF)


@pytest.mark.parametrize("open_", [0, 1, 0x80])
def test_set_open(open_):
    run(emu, 0x426D80, "CarDoor_SetOpen", lambda e: (part(e), 0x1200 | open_))


DOORS = [(o, a, s, t) for o in (0, 1) for a in (0.0, 0.5, 0.75, 0.74, 0.76, 1.0, -0.2, -0.04, 0.04, 0.05)
         for s in (0.05, 0.3) for t in (0.75, -0.5)]


@pytest.mark.parametrize("open_,angle,speed,target", DOORS)
def test_door_run(open_, angle, speed, target):
    run(emu, 0x4273B0, "HeliDoor_Run", lambda e: (part(e, open_=open_, angle=angle, speed=speed, target=target),))


def test_get_arg_size():
    run(emu, 0x427D70, "RocketPod_GetArgSize", lambda e: (0, e.alloc(4)))
    run(emu, 0x427F80, "HitZone_GetArgSize", lambda e: (0, e.alloc(4)))


def test_hitzone_create():
    run(emu, 0x427F30, "HitZone_Create", lambda e: (part(e),))


def test_hitzone_clear_hit():
    def setup(e):
        t = task(e, 0x40, PART)
        hit = e.alloc(0x60)
        e.write(hit, b"\x5a" * 0x60)
        u32(e, t + 0x20, hit)
        a = e.alloc(4)
        u32(e, a, t)
        return 0, a
    run(emu, 0x427F90, "HitZone_ClearHit", setup)


def target(e, pos, active=1):
    t = task(e, 0x100, TARGET)
    d64(e, t + 0x20, *pos)
    n = e.alloc(0x30)
    ref = e.alloc(4)
    u32(e, ref, t)
    u32(e, n + 8, ref)
    e.write(n + 0x29, bytes([active]))
    return n


TARGETS = [(1500.0, 3000.0, 0.0), (1000.0, 9000.0, 500.0), (-5000.0, -2000.0, 0.0), (900.0, -1990.0, 30.0),
           (1000.0, -2000.0, 30.5), (3000.0, -1000.0, 40.0)]


@pytest.mark.parametrize("cooldown", [-1, 0, 5])
@pytest.mark.parametrize("visible", [0, 1])
@pytest.mark.parametrize("seed", range(4))
def test_rocketpod_run(cooldown, visible, seed):
    def setup(e):
        SEED[0] = seed
        RET[0x4CE410] = visible
        p = part(e, cooldown=cooldown)
        mat(e, p + 0x70, [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0] if seed == 0 else None)
        owner = task(e, 0x900, OWNER)
        f32(e, owner + 0xF8, 123.0)
        nodes = [target(e, pos, active=int(i != 1)) for i, pos in enumerate(TARGETS)] + [e.alloc(0x30)]
        nodes.insert(2, target(e, (0.0, 0.0, 0.0)))
        u32(e, e.u32(nodes[2] + 8), 0)
        for a, b in zip(nodes, nodes[1:]):
            u32(e, a, b)
        u32(e, owner + 0x8D8, nodes[0])
        u32(e, p + 0x130, owner)
        return (p,)
    run(emu, 0x427A70, "RocketPod_Run", setup)


T = 2.0 ** -70  # lost when added to 1 in 80 bits, so sums show their order
CANCEL = [1.0, T, -1.0, -1.0, 1.0, T, T, -1.0, 1.0]


@pytest.mark.parametrize("orig,ours", MOVES)
@pytest.mark.parametrize("perm", range(3))
def test_move_sum_order(orig, ours, perm):
    def setup(e):
        RET[0x4D0950] = 1.0
        p = part(e, axis=2, angle=0.0)
        rows = [CANCEL[3 * ((i + perm) % 3):3 * ((i + perm) % 3) + 3] for i in range(3)]
        mat(e, p + 0x108, [v for r in rows for v in r])
        d64(e, p + 0xF0, *[(1.0, -1.0, T)[(i + perm) % 3] for i in range(3)])
        a = move_arg(e)
        mat(e, e.u32(a + 4), [1.0] * 9)
        d64(e, e.u32(a), 0.0, 0.0, 0.0)
        return p, a
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("d", [(1.0, -1.0, 1e-25), (-1.0, 1e-25, 1.0), (1e-25, 1.0, -1.0)])
def test_rocketpod_sum_order(d):
    def setup(e):
        RET[0x4CE410] = 1
        p = part(e, cooldown=-1)
        d64(e, p + 0x20, 0.0, 0.0, 0.0)
        mat(e, p + 0x70, [1.0] * 9)
        owner = task(e, 0x900, OWNER)
        nodes = [target(e, d), e.alloc(0x30)]
        u32(e, nodes[0], nodes[1])
        u32(e, owner + 0x8D8, nodes[0])
        u32(e, p + 0x130, owner)
        return (p,)
    run(emu, 0x427A70, "RocketPod_Run", setup)


def test_cardoor_move_sum_order():
    e = make_emu(rot=CANCEL)

    def setup(e):
        RET[0x4D0950] = 1.0
        p = part(e, axis=1, angle=0.0)
        mat(e, p + 0x108, [1.0] * 9)
        a = move_arg(e)
        mat(e, e.u32(a + 4), [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0])
        return p, a
    run(e, 0x426880, "CarDoor_Move", setup)
