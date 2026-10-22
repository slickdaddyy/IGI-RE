import math
import random
import struct

import pytest

from test_heli_util import (RET, add_child, cstr, d64, f32, handler, new_emu, run, set_handler, task, task_system, u16,
                            u32, vfake)

HELI, VEHICLE, CHILD, DOOR, HWHEEL = 0x60, 0x61, 0x70, 0x71, 0x72
SLOT_CHANGED, SLOT_MOVE, SLOT_EV4 = 0x30, 0x31, 0x32


def bits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def children_of(e, t):
    out, c = [], e.u32(t + 8)
    while c and e.u32(c):
        out.append(c)
        c = e.u32(c)
    return out


def find_child(e, a):
    if not a[0]:
        return 0
    for c in children_of(e, a[0]):
        if e.read(c + 0x1C, 2) == struct.pack("<H", a[1] & 0xFFFF):
            return c
    return 0


def find_next(e, a):
    c = e.u32(a[0])
    while c and e.u32(c):
        if e.read(c + 0x1C, 2) == struct.pack("<H", a[1] & 0xFFFF):
            return c
        c = e.u32(c)
    return 0


def eval_real(e, a):
    return RET.get(("eval", a[0] - e.u32(0x30000000 + 0x7F000)), 0.0)


def has_expr(e, a):
    return RET.get(("has", a[0] - e.u32(0x30000000 + 0x7F000)), 0)


def coll_lines(e, a):
    hits = RET.get("hits", [])
    for i in range(a[4]):
        found, dist = hits[i] if i < len(hits) else (0, 0.0)
        p = a[1] + 0x70 * i
        u32(e, p + 0x58, found)
        d64(e, p + 0x60, dist)


emu = new_emu(
    fakes=[(0x4C5710, 3), (0x416920, 0), (0x4C7140, 5, dict(derefs={2: 24})), (0x4E6AB0, 1), (0x4F4680, 1),
           (0x4F1320, 1), (0x4168D0, 0), (0x4168E0, 0), (0x4C1790, 2), (0x4E6B00, 2, dict(strings=(1,))),
           (0x401AE0, 1), (0x4E6C00, 1), (0x401B20, 1), (0x401A60, 0), (0x416D20, 0), (0x4B47C0, 2),
           (0x480FE0, 16, dict(strings=(10, 11), derefs={0: 24})), (0x4015F0, 1), (0x426D80, 2), (0x489750, 4),
           (0x489B90, 1), (0x464250, 2), (0x4896C0, 4), (0x4F4640, 1), (0x4C7560, 2), (0x4C17C0, 2),
           (0x4C1830, 1), (0x4F1340, 1), (0x4C6940, 4, dict(derefs={2: 24})), (0x4FB700, 2), (0x4F4600, 1),
           (0x4F4820, 1), (0x4F4800, 1), (0x4ECDB0, 1), (0x4F4660, 1), (0x4ECF50, 2, dict(derefs={1: 24})),
           (0x4FC7B0, 1), (0x4EC2C0, 6, dict(derefs={5: 12})), (0x4CEC10, 1, dict(strings=(0,))), (0x4C48D0, 2),
           (0x4EE140, 1, dict(strings=(0,))), (0x4F1400, 2)],
    floats=[(0x4D0950, 1), (0x4F16E0, 2)])
e = emu
vfake(e, 0x4F16C0, 1, effect=has_expr)
vfake(e, 0x414E20, 3, show=lambda e, a: (e.normalize(a[1]), a[2]),
      effect=lambda e, a: d64(e, a[0], 100.0 * a[2], -50.0, 25.5))
vfake(e, 0x401BE0, 2, show=lambda e, a: (e.normalize(a[0]), a[1] & 0xFFFF), effect=find_child)
vfake(e, 0x401C40, 2, show=lambda e, a: (e.normalize(a[0]), a[1] & 0xFFFF), effect=find_next)
vfake(e, 0x4FC0E0, 7, show=lambda e, a: (e.normalize(a[0]), a[1], a[2], a[3], a[4], a[5], e.normalize(e.u32(a[6] + 4))))
vfake(e, 0x4CC2A0, 8, show=lambda e, a: (a[0], e.read(a[2], 24 * a[4]), e.read(a[3], 24 * a[4]), a[4], a[5] & 0xFF,
                                         e.normalize(e.u32(a[6] + 4)), a[7]), effect=coll_lines)
e.write(0x54E0E5, bytes([SLOT_CHANGED]))  # Param_GetChangedEvent
e.write(0x54864A, bytes([SLOT_MOVE]))  # MagicObj_GetMoveEvent
e.write(0x547F40, bytes([SLOT_EV4]))  # RigidDynCubeObj_GetEvent4
u16(e, 0x54E200, VEHICLE)
u16(e, 0x53BB18, DOOR)
u16(e, 0x57BC7C, HWHEEL)
u32(e, 0x57BD40, 0x12345678)
for slot in (1, SLOT_CHANGED, SLOT_MOVE, SLOT_EV4):
    set_handler(e, slot, CHILD, handler(e, "child_%x" % slot, 1 if slot == 1 else 2,
                                        derefs={1: 8} if slot in (SLOT_MOVE, SLOT_EV4) else None))
e.baseline = e.snapshot()


def heli(e, explode_points=(), parts=0, kids=2, coll_kids=2, name=b"heli_1", **f):
    h = task(e, 0x2D00, HELI)
    u32(e, 0x30000000 + 0x7F000, h)
    e.write(h + 0x154, name + b"\0")
    f32(e, h + 0x98, 1.0, 1.5, 0.5)
    u32(e, h + 0x6C, 0x4444)
    d64(e, h + 0x20, 1000.0, -2000.0, 300.0)
    d64(e, h + 0x168, 1.0, 2.0, 3.0)
    f32(e, h + 0x180, 10.0, -20.0, 5.0)
    f32(e, h + 0x18C, *[0.1 * i - 0.3 for i in range(9)])
    u32(e, h + 0x18C + 36, 9)
    m = f.get("matrix", (0.8, -0.6, 0.0, 0.6, 0.8, 0.0, 0.0, 0.0, 1.0))
    f32(e, h + 0x70, *m)
    u32(e, h + 0x94, 3)
    f32(e, h + 0xF0, f.get("mass", 1500.0), 0.0, f.get("f8", 5.0))
    f32(e, h + 0x108, 10.0, -20.0, 30.0)
    f32(e, h + 0x120, 0.5, -0.25, 0.125)
    u32(e, h + 0x1B8, f.get("contacts", 0))
    f32(e, h + 0x720, f.get("speed", 0.5), *f.get("turn", (0.1, -0.2, 0.3)))
    f32(e, h + 0x730, f.get("target", 0.9), *f.get("steer", (0.5, -0.5, 0.25)))
    f32(e, h + 0x740, f.get("start", 1.0))
    sp = e.alloc(12)
    u32(e, sp, 2)
    u32(e, h + 0x748, sp if f.get("sound_params", 1) else 0)
    snd = f.get("rotor_sound")
    if snd is not None:
        ref = e.alloc(4)
        u32(e, ref, snd)
        u32(e, h + 0x74C, ref)
    e.write(h + 0x750, bytes([f.get("exploding", 0), f.get("lights", 0)]))
    u32(e, h + 0x754, f.get("timer", -1))
    f32(e, h + 0x2CF0, 180.0, 360.0, 60.0)
    data = e.alloc(0x2C0)
    f32(e, data + 4, 30.0, 0.5, 45.0, 0.25, 60.0, 0.75)
    f32(e, data + 0x20, 0.05, 0.02)
    u32(e, data + 0x28, parts)
    u32(e, data + 0x150, len(explode_points))
    for i, p in enumerate(explode_points):
        u32(e, data + 0x128 + 4 * i, p)
    phys = e.alloc(0x2D8)
    f32(e, phys + 8, f.get("power", 2.0))
    u32(e, phys + 0x2D4, data)
    u32(e, h + 0x1B4, phys)
    if coll_kids is not None:
        c = task(e, 0x40, 0x6F)
        for _ in range(coll_kids):
            add_child(e, c, task(e, 0x40, CHILD))
        for k in f.get("doors", ()):
            add_child(e, c, task(e, 0x40, DOOR))
        for w in f.get("wheels", ()):
            hw = task(e, 0x140, HWHEEL)
            d64(e, hw + 0x20, *w[0])
            d64(e, hw + 0xF0, *w[1])
            prm = e.alloc(8)
            f32(e, prm, *w[2])
            u32(e, hw + 0x138, prm)
            add_child(e, c, hw)
        u32(e, h + 0xE8, c)
    for _ in range(kids):
        add_child(e, h, task(e, 0x40, CHILD))
    return h


def setup_with(f, **ret):
    def setup(e):
        RET.clear()
        RET[0x416920] = 0x7777
        RET[0x4D0950] = 3.5
        RET[0x4E6AB0] = 0x5151
        RET[0x4168D0] = 0x3333
        RET[0x4168E0] = 0x4444
        RET[0x464250] = 0x99
        RET[0x401AE0] = 0x6262
        RET[0x4B47C0] = 7
        RET[0x4F16E0] = eval_real
        RET.update(ret)
        return (heli(e, **f),)
    return setup


def test_create():
    run(e, 0x431540, "Heli_Create", setup_with({}))


def test_reset():
    run(e, 0x431560, "Heli_Reset", setup_with({}))


RUNS = [
    ({}, {}), (dict(rotor_sound=0x5000), {}), (dict(rotor_sound=0x5000), {("has", 0x7F8): 1}),
    (dict(rotor_sound=0x5000), {("has", 0x7F8): 1, ("eval", 0x7F8): 0.4}),
    (dict(rotor_sound=0x5000), {("has", 0x7F8): 1, ("eval", 0x7F8): 2.6}),
    ({}, {0x4E6B00: 0x8000}), (dict(rotor_sound=0), {}),
    (dict(exploding=1), {}), (dict(exploding=1, explode_points=(3, 5, 9)), {}),
    (dict(exploding=1, timer=5), {}), (dict(exploding=1, timer=1), {}),
    ({}, {("has", 0x758): 1, ("eval", 0x758): 1.0}),
    (dict(doors=(0,)), {("has", 0x758): 1, ("eval", 0x758): -3.0}),
    ({}, {("has", 0x7A8): 1, ("eval", 0x7A8): 1.0}),
    (dict(lights=1), {("has", 0x7A8): 1, ("eval", 0x7A8): 0.0}),
    (dict(lights=1), {("has", 0x7A8): 1, ("eval", 0x7A8): 300.0}),
    (dict(lights=1), {("has", 0x7A8): 1, ("eval", 0x7A8): 256.0}),
]


@pytest.mark.parametrize("f,ret", RUNS)
def test_run(f, ret):
    run(e, 0x4316D0, "Heli_Run", _with_ret(setup_with(f), ret))


def _with_ret(base, ret):
    def setup(e):
        r = base(e)
        RET.update(ret)
        return r
    return setup


@pytest.mark.parametrize("sound", [None, 0, 0x5000])
def test_stop_sound(sound):
    run(e, 0x431A80, "Heli_StopSound", setup_with(dict(rotor_sound=sound)))


@pytest.mark.parametrize("doors", [(), (0,)])
def test_find_door(doors):
    run(e, 0x431AC0, "Heli_FindDoor", setup_with(dict(doors=doors)), check_eax=True)


def test_delete():
    run(e, 0x431AE0, "Heli_Delete", setup_with(dict(rotor_sound=0x5000)))


def test_get_param_table():
    run(e, 0x431B60, "Heli_GetParamTable", lambda e: (0, e.alloc(4)))


def control_arg(e, tick, axes):
    a = e.alloc(8)
    ax = e.alloc(20)
    f32(e, ax, *axes)
    u32(e, a, tick, ax)
    return a


@pytest.mark.parametrize("tick", [0, 1, -1])
@pytest.mark.parametrize("axes", [(0.1, 0.2, 0.3, -1.0, 4.0), (0.5, -0.5, 1.0, 0.95, -2.0)])
@pytest.mark.parametrize("coll", [None, 2])
def test_control(tick, axes, coll):
    base = setup_with(dict(coll_kids=coll))

    def setup(e):
        h = base(e)[0]
        return h, control_arg(e, tick, axes)
    run(e, 0x431B70, "Heli_Control", setup)


MOVES = [
    dict(), dict(speed=0.9, target=0.5), dict(speed=0.5, target=0.5), dict(speed=0.0005, target=0.0),
    dict(speed=0.0005, target=0.0001), dict(speed=0.79, target=1.2), dict(speed=1.2, target=0.79),
    dict(turn=(0.0, 0.0, 0.0), steer=(0.0, 0.0, 0.0)), dict(contacts=2), dict(contacts=2, f8=2.5),
    dict(f8=1.0), dict(matrix=(0.0, 0.3, 0.95, 0.0, 0.95, -0.3, -1.0, 0.0, 0.0)),
    dict(matrix=(0.6, 0.0, -0.8, 0.0, 1.0, 0.0, 0.8, 0.0, 0.6)), dict(matrix=(0.6, 0.0, 0.8, 0.0, 1.0, 0.0, -0.8, 0.0, -0.6)),
    dict(parts=3), dict(fc=1), dict(coll_kids=None), dict(sound_params=0), dict(speed=0.005, target=0.005),
    dict(wheels=[((0.0, 0.0, 100.0), (1.0, 2.0, 3.0), (2048.0, 1024.0))], hits=[(1, 9000.0)]),
]


@pytest.mark.parametrize("c", MOVES)
def test_move(c):
    f = dict(c)
    hits = f.pop("hits", [])
    fc = f.pop("fc", 0)
    base = setup_with(f)

    def setup(e):
        r = base(e)
        RET[0x4FC7B0] = fc
        RET["hits"] = hits
        return r
    run(e, 0x431E30, "Heli_Move", setup)


def random_move(seed):
    r = random.Random(seed)
    u = lambda a: r.uniform(-a, a)  # noqa: E731
    a, b = u(3.0), u(1.5)
    m = (math.cos(a), -math.sin(a) * math.cos(b), math.sin(a) * math.sin(b), math.sin(a), math.cos(a) * math.cos(b),
         -math.cos(a) * math.sin(b), 0.0, math.sin(b), math.cos(b))
    return dict(speed=r.uniform(0, 1.3), target=r.uniform(0, 1.3), turn=(u(1), u(1), u(1)), steer=(u(1), u(1), u(1)),
                matrix=m, power=r.uniform(0.5, 5.0), f8=r.uniform(0, 4.0), contacts=r.randint(0, 1))


@pytest.mark.parametrize("seed", range(30))
def test_move_random(seed):
    run(e, 0x431E30, "Heli_Move", setup_with(random_move(seed)))


W = [((0.0, 0.0, 100.0), (1.0, 2.0, 3.0), (2048.0, 1024.0)), ((500.0, 0.0, 120.0), (-4.0, 5.0, 6.0), (3000.0, 512.0)),
     ((0.0, 700.0, 80.0), (7.0, -8.0, 9.0), (1000.0, 4096.0)), ((9.0, 9.0, 9.0), (1.5, 2.5, 3.5), (100.0, 50.0)),
     ((1.0, 1.0, 1.0), (0.0, 0.0, 0.0), (10.0, 20.0))]
WHEELS = [
    dict(wheels=[]), dict(wheels=W[:1], hits=[(0, 0.0)]), dict(wheels=W[:1], hits=[(1, 9000.0)]),
    dict(wheels=W[:1], hits=[(1, 20000.0)]), dict(wheels=W[:1], hits=[(1, 9216.0)]),
    dict(wheels=W[:1], hits=[(1, -9100.0)]), dict(wheels=W[:3], hits=[(1, 9000.0), (1, 9000.0), (1, 12000.0)]),
    dict(wheels=W[:3], hits=[(1, 12000.0), (1, 8000.0), (1, 9000.0)]), dict(wheels=W, hits=[(1, 8500.0)] * 5),
    dict(wheels=W[:2], hits=[(1, 0.0), (1, 12288.0)]),
]


@pytest.mark.parametrize("c", WHEELS)
def test_update_wheels(c):
    f = dict(c)
    hits = f.pop("hits", [])
    base = setup_with(f)

    def setup(e):
        r = base(e)
        RET["hits"] = hits
        return r
    run(e, 0x432550, "Heli_UpdateWheels", setup)


@pytest.mark.parametrize("model", [0, 1])
@pytest.mark.parametrize("name", [b"heli_1", b"", b"x*"])
def test_params_changed(model, name):
    base = setup_with(dict(name=name, lights=1))

    def setup(e):
        r = base(e)
        RET[0x4CEC10] = 0x5000 if model else 0
        RET[0x4EE140] = 0x6000
        return r
    run(e, 0x432950, "Heli_ParamsChanged", setup)


def test_get_sound_params():
    run(e, 0x432BB0, "Heli_GetSoundParams", lambda e: (heli(e), e.alloc(12)))


def test_get_state_size():
    run(e, 0x432BD0, "Heli_GetStateSize", lambda e: (0, e.alloc(4)))


@pytest.mark.parametrize("i", [-1, 0, 3, 4, 5])
def test_get_control_info(i):
    def setup(e):
        a = e.alloc(8)
        u32(e, a, i, 0x55)
        return 0, a
    run(e, 0x432BE0, "Heli_GetControlInfo", setup)


KEYS = [(), (0x1D, 0xCD, 0xD0), (0x9D, 0xC8), (0xCD, 0xC8), (0xCB, 0xD0, 0x2C), (0x2D,), (0x2C, 0x2D), (0x29,),
        (0x29, 2), (3, 7), (0xB,), (0x31,), (0x32,), (0x31, 0x32), (0x1E,), (0x1E, 0x29), (0x1E, 2, 5), (0x1E, 0xB),
        (0x1E, 0xA, 0x29), (0x1D, 0x31, 4)]


@pytest.mark.parametrize("keys", KEYS)
@pytest.mark.parametrize("prev", [(), (0x31, 0x32, 0x29, 2, 5, 0xB)])
@pytest.mark.parametrize("target", [1.0, 0.805, 1.245, 3.0])
def test_read_keys(keys, prev, target):
    def setup(e):
        for k in keys:
            e.write(0xC28C62 + k, b"\x80")
        for k in prev:
            e.write(0xC28D62 + k, b"\x80")
        h = heli(e, target=target)
        out = e.alloc(20)
        f32(e, out, 9.0, 9.0, 9.0, 9.0, 9.0)
        a = e.alloc(4)
        u32(e, a, out)
        return h, a
    run(e, 0x432C10, "Heli_ReadKeys", setup)


init = new_emu(fakes=[(0x4B8810, 2, dict(strings=(0,))), (0x4F1950, 1, dict(ret=0x4242))])
vfake(init, 0x4F1A80, 6, show=lambda e, a: (a[0], a[2] - a[1], a[3], a[4], a[5]))
init.baseline = init.snapshot()


def test_init():
    def setup(e):
        task_system(e)
        t = e.call(0x401900, cstr(e, b"Vehicle"), 0x748, e.call(0x4018F0)[0] & 0xFFFF, 0)[0]
        u16(e, 0x54E200, t)
        for i, a in enumerate((0x54E0E4, 0x54E0E5, 0x539549, 0x53954B, 0x54E1DA, 0x54E1DC, 0x54E1DD, 0x54E1E0,
                               0x54E1DF, 0x54E1DE, 0x54DC30, 0x54DC31, 0x548177, 0x54127C, 0x53F760, 0x54127E)):
            e.write(a, bytes([0x30 + i]))
        e.write(0x548813, b"\x40")
    run(init, 0x4310F0, "Heli_Init", setup)
