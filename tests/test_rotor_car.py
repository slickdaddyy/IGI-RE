import random
import struct

import pytest

from emu import Emu
from test_rotor import (Fakes, add_child, check, event, f32, f64, new_task, rmatrix, rvec, set_events,
                        set_handler)

e0 = Emu()
fk = Fakes(e0)
WORLD = 0x20F00500
CAR = 0x61

fk.add(0x401400, 3)
fk.add(0x401530, 3)
fk.add(0x401900, 4, ret=CAR, fmt=lambda e, a: (e.read_cstr(a[0]), a[1], a[2] & 0xFFFF, a[3]))
fk.add(0x4B8810, 2)
fk.add(0x4F1950, 1, ret=0x20F00600)
fk.add(0x4F1A80, 6, fmt=lambda e, a: (a[0], a[2] - a[1], a[3], a[4], a[5]))
fk.add(0x4B8A20, 2, ret=lambda e, a: e.ints.get(a[1], a[1] % 7))
fk.add(0x4B8A50, 2, ret=lambda e, a: a[1] * 1.37 - 2.5, st0="d")
fk.add(0x4B8A80, 4)
fk.add(0x416920, 0, ret=WORLD)
fk.add(0x416D20, 0, ret=0x20F00800)
fk.add(0x4C5710, 3)
fk.add(0x4D0950, 1, ret=1.875, st0="f")
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4C7560, 2)
fk.add(0x4E6AB0, 1, ret=0x20F00700)
fk.add(0x4F4680, 1)
fk.add(0x4C1790, 2)
fk.add(0x4C17C0, 2)
fk.add(0x4C1830, 1)
fk.add(0x4F1320, 1)
fk.add(0x4F1340, 1)
fk.add(0x4028B0, 0, ret=lambda e, a: e.tick)
fk.add(0x4F16C0, 1, ret=lambda e, a: e.expr.get(a[0] - e.car, (0, 0))[0])
fk.add(0x4F16E0, 2, ret=lambda e, a: e.expr.get(a[0] - e.car, (0, 0))[1], st0="d")
fk.add(0x4015F0, 1)
fk.add(0x401780, 1)
fk.add(0x4E6B00, 2, ret=lambda e, a: e.sound)
fk.add(0x401BE0, 2, ret=lambda e, a: e.found)
fk.add(0x401C40, 2, ret=0)
fk.add(0x426D80, 2)
fk.add(0x489750, 4)
fk.add(0x489B90, 1)
fk.add(0x4896C0, 6)
fk.add(0x4B47C0, 2, ret=7)
fk.add(0x414E20, 3, fmt=lambda e, a: (a[1], a[2]),
       ret=lambda e, a: e.write(a[0], f64(a[2] * 10.5, -a[2] * 3.25, 99.0)) or 0)
fk.add(0x480FE0, 16, deref={0: 24})
fk.add(0x4F3390, 6, deref={4: 12, 5: 12})
fk.add(0x4F4820, 1)
fk.add(0x42FC80, 2, deref={1: 4})
fk.add(0x4F4640, 1)
fk.add(0x4F1050, 1, ret=lambda e, a: e.id)
fk.add(0x4FB700, 2)
fk.add(0x4F4600, 1)
fk.add(0x4F4660, 1)
fk.add(0x4FC7B0, 1, ret=lambda e, a: e.busy)
fk.add(0x4EC2C0, 6, deref={5: 12})
fk.add(0x4FC0E0, 7, fmt=lambda e, a: tuple(a[:6]) + (e.u32(a[6] + 4),))
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)
MOVE_LOG = fk.handler(2, deref={1: 12})


def car(e, rng, wheels=3, explode=2, parts=True):
    c = new_task(e, CAR, 0x2ED8)
    defn = e.alloc(0x2D8)
    d = e.alloc(0x2C0)
    e.write(defn + 8, f32(1.5))
    e.write(defn + 0x2D4, struct.pack("<I", d))
    e.write(c + 0x1B4, struct.pack("<I", defn))
    e.write(c + 0x20, rvec(rng))
    e.write(c + 0x70, rmatrix(rng))
    e.write(c + 0x98, f32(1.0, 2.5, 1.5))
    e.write(c + 0xF0, f32(*[rng.uniform(-900, 900) for _ in range(9)]))
    e.write(c + 0x168, rvec(rng) + f32(1, 2, 3) + rmatrix(rng))
    for j in range(23):
        e.write(c + 0x1BC + j * 0x38 + 0xC, f32(rng.uniform(-500, 500), rng.uniform(-500, 500), rng.uniform(-500, 500)))
    e.write(d + 0x18, f32(rng.uniform(0, 1)))
    e.write(d + 0x20, f32(rng.uniform(0, 1)))
    e.write(d + 0x118, struct.pack("<i", wheels))
    for i in range(wheels):
        e.write(d + 0x11C + i * 0x14, struct.pack("<BBxxfiff", 1, 0, 0.5, (i * 5) % 23 if i != 1 else -1, 400.0, 0.2))
    e.write(d + 0x25C, f32(37.5))
    e.write(d + 0x262, b"engine\0")
    e.write(d + 0x284, struct.pack("<4i", 3, 9, 1, 4))
    e.write(d + 0x2AC, struct.pack("<i", explode))
    e.write(d + 0x2B0, b"wreck\0")
    e.write(c + 0x720, f32(rng.uniform(-1, 1)))
    e.write(c + 0x72C, f32(rng.uniform(-1, 1)))
    e.write(c + 0x730, f32(rng.uniform(-1, 1)))
    e.write(c + 0x73C, f32(rng.uniform(-1, 1)))
    e.write(c + 0x740, f32(0.25))
    e.write(c + 0x74C, struct.pack("<I", e.alloc(16)))
    if parts:
        p = new_task(e, 0x70, 0x40)
        e.write(c + 0xE8, struct.pack("<I", p))
        for i in range(2):
            k = new_task(e, 0x71 + i, 0x40)
            add_child(e, p, k)
            set_handler(e, 1, 0x71 + i, LOG1)
            set_handler(e, event(e, 0x4D9FB0), 0x71 + i, MOVE_LOG)
    k = new_task(e, 0x75, 0x40)
    add_child(e, c, k)
    set_handler(e, 1, 0x75, LOG1)
    e.car = c
    e.ints = {}
    e.tick = 100
    e.expr = {}
    e.sound = 0x20F00900
    e.found = 0
    e.id = 3
    e.busy = 0
    e.hits = [(1, 4100.0, (0.2, 0.3, 0.9)), (1, 3000.0, (0.0, 0.0, 1.0)), (0, 0.0, (0.0, 0.0, 0.0))]
    return c


def test_register_def_handlers():
    check(e0, fk, 0x42DAC0, "Car_RegisterDefHandlers", setup=lambda e: set_events(e), check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("wheels,explode", [(3, 4), (0, 0), (2, -1)])
def test_parse_def(wheels, explode, cw):
    def setup(e):
        set_events(e)
        set_handler(e, event(e, 0x4EE0E0), event(e, 0x4F45F0), LOG2)
        d = e.alloc(0x2C0)
        a = e.alloc(12)
        e.write(a, struct.pack("<IIi", d, 0x20F00200, 2))
        e.ints = {2: wheels, 3 + 6 * wheels + 4: explode}
        return 0x20F00300, a
    check(e0, fk, 0x42DB00, "Car_ParseDef", setup=setup, check_eax=False, cw=cw)
    check(e0, fk, 0x42DCE0, "Car_GetDefSize", setup=setup, check_eax=False)


def test_init():
    def setup(e):
        set_events(e)
        e.write(0x53BDA8, struct.pack("<HH", 0x1234, 0x5678))
    check(e0, fk, 0x42DCF0, "Car_Init", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("addr,name", [(0x42E160, "Car_Create"), (0x42E180, "Car_Reset")])
def test_create(addr, name, cw):
    def setup(e):
        c = car(e, random.Random(1))
        return c, 0
    check(e0, fk, addr, name, setup=setup, check_eax=False, cw=cw)


RUN_CASES = [
    # tick, wrecked, expr9c8, sound, has sound task, no def, parts, unk2ecc, found
    dict(),
    dict(tick=0),
    dict(wrecked=1),
    dict(expr=(1, 0.2)),
    dict(expr=(1, 0.2), task=1),
    dict(expr=(1, 3.0), task=1),
    dict(expr=(1, 3.0)),
    dict(sound=0),
    dict(nodef=1),
    dict(parts=False),
    dict(ecc=1),
    dict(found=1),
]


@pytest.mark.parametrize("case", RUN_CASES)
def test_run(case):
    def setup(e):
        set_events(e)
        c = car(e, random.Random(2), parts=case.get("parts", True))
        e.write(c + 0x924, struct.pack("<i", 0))
        e.tick = case.get("tick", 100)
        e.write(c + 0x2ED5, bytes([case.get("wrecked", 0)]))
        if "expr" in case:
            e.expr[0x9C8] = case["expr"]
        if "sound" in case:
            e.sound = case["sound"]
        if case.get("task"):
            e.write(c + 0x748, struct.pack("<I", 0x20F00A00))
        if case.get("nodef"):
            e.write(c + 0x1B4, struct.pack("<I", 0))
        e.write(c + 0x2ECC, bytes([case.get("ecc", 0)]))
        if case.get("found"):
            g = new_task(e, 0x77, 0x40)
            set_handler(e, 1, 0x77, LOG1)
            e.found = g
        return (c,)
    check(e0, fk, 0x42E320, "Car_Run", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("timer,explode,exploding", [(-1, 2, 1), (-1, 0, 1), (-1, -2, 1), (5, 2, 1), (1, 2, 1),
                                                     (0, 2, 1), (-1, 2, 0)])
def test_update_explosion(timer, explode, exploding, cw):
    def setup(e):
        c = car(e, random.Random(3), explode=explode)
        e.write(c + 0x91C, bytes([exploding]))
        e.write(c + 0x920, struct.pack("<i", timer))
        return (c,)
    check(e0, fk, 0x42E5A0, "Car_UpdateExplosion", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("has,value,found", [(0, 0, 1), (1, 0.4, 1), (1, 0.6, 1), (1, 2.0, 0)])
def test_update_doors(has, value, found):
    def setup(e):
        c = car(e, random.Random(4))
        e.expr[0x928] = (has, value)
        e.found = 0x20F00B00 if found else 0
        return (c,)
    check(e0, fk, 0x42E810, "Car_UpdateDoors", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("has,value,was", [(0, 0, 1), (1, 1.0, 0), (1, 1.0, 1), (1, 0.0, 1), (1, 0.0, 0),
                                           (1, 300.0, 0), (1, -1.0, 0)])
def test_update_978(has, value, was, cw):
    def setup(e):
        c = car(e, random.Random(5))
        e.expr[0x978] = (has, value)
        e.write(c + 0x91D, bytes([was]))
        return (c,)
    check(e0, fk, 0x42E870, "sub_42E870", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("wrecked,params,id_,handler", [(0, 1, 3, 1), (1, 0, 3, 1), (0, 1, -1, 1), (0, 1, 0, 0)])
def test_delete(wrecked, params, id_, handler):
    def setup(e):
        set_events(e)
        c = car(e, random.Random(6))
        e.write(c + 0x2ED5, bytes([wrecked]))
        if not params:
            e.write(c + 0x74C, struct.pack("<I", 0))
        e.id = id_
        set_handler(e, event(e, 0x4F1220), CAR, LOG1 if handler else 0)
        return (c,)
    check(e0, fk, 0x42E940, "Car_Delete", setup=setup, check_eax=False)


def test_get_param_table():
    def setup(e):
        e.write(0x57BD28, struct.pack("<I", 0x20F00600))
        return 0, e.alloc(4)
    check(e0, fk, 0x42EA00, "Car_GetParamTable", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("active", [0, 1])
@pytest.mark.parametrize("inp", [(0.5, -1.0, 0.0, -1.0), (-0.3, 0.7, 1.0, 0.0), (0.0, 0.0, 0.0, 2.0)])
def test_control(active, inp, seed, cw):
    def setup(e):
        set_events(e)
        c = car(e, random.Random(10 + seed), parts=seed != 1)
        e.busy = seed & 1
        if seed == 2:
            e.write(c + 0x730, f32(0.0))
            e.write(c + 0x720, f32(0.005))
        if seed == 3:
            e.write(car_data(e, c) + 0x28, struct.pack("<i", 2))
            e.write(c + 0xF8, f32(1500.0))
        a = e.alloc(8)
        i = e.alloc(16)
        e.write(i, f32(*inp))
        e.write(a, struct.pack("<iI", active, i))
        return c, a
    check(e0, fk, 0x42EA10, "Car_Control", setup=setup, check_eax=False, cw=cw)


def car_data(e, c):
    return e.u32(e.u32(c + 0x1B4) + 0x2D4)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("wheels,nodef", [(3, 0), (0, 0), (5, 1)])
def test_apply_wheel_drop(wheels, nodef, cw):
    def setup(e):
        c = car(e, random.Random(7), wheels=wheels)
        if nodef:
            e.write(c + 0x1B4, struct.pack("<I", 0))
        return (c,)
    check(e0, fk, 0x42EC30, "Car_ApplyWheelDrop", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
def test_drive(seed, cw):
    def setup(e):
        set_events(e)
        rng = random.Random(20 + seed)
        c = car(e, rng, parts=seed % 4 != 0)
        e.busy = seed % 3 == 0
        if seed % 2:
            e.write(c + 0x730, f32(0.0))
            e.write(c + 0x720, f32(rng.choice([0.005, -0.009, 0.5, -0.02, 0.0])))
        if seed % 4 == 1:
            e.write(car_data(e, c) + 0x28, struct.pack("<i", 3))
        if seed == 6:
            e.write(c + 0x74C, struct.pack("<I", 0))
        e.write(c + 0xF8, f32(rng.choice([1500.0, 200.0, -50.0])))
        return (c,)
    check(e0, fk, 0x42ECA0, "Car_Drive", setup=setup, check_eax=False, cw=cw)


def coll_lines(e, a):
    n = a[4]
    for k in range(n):
        found, dist, normal = e.hits[k % len(e.hits)]
        h = a[1] + k * 0x70
        e.write(h + 0x38, f32(*normal))
        e.write(h + 0x58, struct.pack("<i", found))
        e.write(h + 0x60, f64(dist))
    return 0


fk.add(0x4CC2A0, 8, ret=coll_lines,
       fmt=lambda e, a: (a[0], e.read(a[2], 24 * a[4]), e.read(a[3], 24 * a[4]), a[4], a[5] & 0xFF,
                         e.u32(a[6] + 4), a[7]))
fk.add(0x430D10, 3)
fk.add(0x4ECF50, 2, deref={1: 24})

HITS = [(1, 3000.0, (0.0, 0.0, 1.0)), (1, 4100.0, (0.2, 0.3, 0.9)), (1, 4600.0, (0.0, 0.0, 0.0)),
        (1, 4096.0, (1.0, 0.0, 0.0)), (0, 100.0, (0.0, 1.0, 0.0)), (1, 3900.5, (0.0, 0.0, 0.0)),
        (1, 4495.0, (0.0, 0.5, 0.0)), (1, -50.0, (3.0, 4.0, 0.0))]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(10))
def test_update_wheels(seed, cw):
    def setup(e):
        rng = random.Random(40 + seed)
        wheels = rng.choice([0, 2, 5, 8])
        c = car(e, rng, wheels=wheels)
        d = car_data(e, c)
        for i in range(wheels):
            e.write(c + 0x75C + i * 0x1C, bytes([rng.random() < 0.8]))
            e.write(d + 0x11C + i * 0x14 + 8, struct.pack("<i", rng.choice([-1, 0, 3, 7, 22, i])))
            e.write(d + 0x11C + i * 0x14 + 0xC, f32(rng.choice([400.0, 512.0, 37.25])))
            e.write(d + 0x11C + i * 0x14 + 0x10, f32(rng.uniform(0.1, 2.0)))
        for j in range(23):
            e.write(c + 0x1BC + j * 0x38 + 0x34, f32(rng.choice([0.0, 10.0, 400.0, 99.9995, 600.0])))
        e.hits = rng.sample(HITS, len(HITS))
        return (c,)
    check(e0, fk, 0x42EF00, "Car_UpdateWheels", setup=setup, check_eax=False, cw=cw)
