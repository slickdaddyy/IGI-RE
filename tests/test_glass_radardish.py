import random
import struct

import pytest

from emu import Emu, compare
from test_glass_fakes import (Fakes, base, check, check_error, common, cstr, event, f32, f64, i32, new_task, rmatrix,
                              rvec, set_handler, with_children)

RT = 0x53  # RadarDish type id

e = Emu()
fk = Fakes(e)
common(e, fk, {b"RadarDish": RT})
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)
VALUE = {}
fk.add(0x4CEC10, 1, ret=lambda em, a: VALUE.get("model", 0), fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4C5710, 3)
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C7560, 2)
fk.add(0x4D0950, 1, ret=lambda em, a: VALUE.get("radius", 100.0), st0="d")
fk.add(0x4C4950, 3)
fk.add(0x4E6AB0, 1, ret=lambda em, a: VALUE.get("params", 0))
fk.add(0x4E6B00, 2, ret=lambda em, a: VALUE.get("sound", 0), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x401AE0, 1, ret=0xAE0)
fk.add(0x401B20, 1)
fk.add(0x4B8A50, 2, ret=lambda em, a: 1.5 + a[1] * 10.25, st0="d")
fk.add(0x4B8A80, 4, ret=lambda em, a: em.write(a[2], b"dish_loop\0"), fmt=lambda em, a: (a[0], a[1], a[3]))
e.baseline = e.snapshot()


def rbase(em):
    VALUE.clear()
    base(em)
    em.write(0x57C20C, struct.pack("<H", RT))


def make_arg(em, rng, max_speed=90.0, accel=30.0):
    a = em.alloc(0x90)
    i32(em, a, cstr(em, b"radar_model"))
    i32(em, a + 4, 0x4444)
    em.write(a + 8, rvec(rng, 1e5))
    em.write(a + 0x20, bytes(rng.randrange(256) for _ in range(0x18)))
    em.write(a + 0x38, rmatrix(rng))
    em.write(a + 0x60, bytes(rng.randrange(256) for _ in range(0x28)))
    i32(em, a + 0x88, 0x8888)
    d = em.alloc(0x28)
    em.write(d, b"dish_loop\0")
    em.write(d + 0x20, f32(max_speed, accel))
    i32(em, a + 0x8C, d)
    return a


def test_open():
    _, log = check(e, fk, 0x448D30, "RadarDish_Open", rbase)
    assert len(log) == 10


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed,sound", [(0, 0), (1, 0x6B00), (2, 0x6B00)])
def test_create(seed, sound, cw):
    def setup(em):
        rbase(em)
        rng = random.Random(seed)
        r = new_task(em, RT, 0x180)
        em.write(r + 0x98, f32(*[rng.choice([1.0, 2.0, 0.5]) for _ in range(3)]))
        i32(em, r + 0x6C, 0x6C6C)
        VALUE.update(model=0x7700, radius=rng.uniform(10, 1000), params=0x6AB0, sound=sound)
        return r, make_arg(em, rng)
    check(e, fk, 0x448B70, "RadarDish_Create", setup, cw=cw)


def test_create_error():
    def setup(em):
        rbase(em)
        return new_task(em, RT, 0x180), make_arg(em, random.Random(1))
    check_error(e, fk, 0x448B70, "RadarDish_Create", setup)


@pytest.mark.parametrize("sound,params", [(0, 0), (0xAE0, 0), (0, 0x6AB0), (0xAE0, 0x6AB0)])
def test_delete(sound, params):
    def setup(em):
        rbase(em)
        r = new_task(em, RT, 0x180)
        i32(em, r + 0x68, 0x6868)
        i32(em, r + 0x174, sound)
        i32(em, r + 0x178, params)
        return (r,)
    check(e, fk, 0x448CF0, "RadarDish_Delete", setup)


SPEEDS = [(0.0, 0.0), (10.0, 90.0), (89.5, 90.0), (90.0, 0.0), (0.5, 0.0), (45.0, 44.9), (-5.0, 0.0),
          (120.0, 90.0), (30.0, 30.0)]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("speed,target", SPEEDS)
@pytest.mark.parametrize("angle", [0.0, 123.456, 359.9, 719.0, 1080.5])
@pytest.mark.parametrize("params", [0, 1])
def test_run(speed, target, angle, params, cw):
    def setup(em):
        rbase(em)
        rng = random.Random(int(speed * 7 + angle))
        r = new_task(em, RT, 0x180)
        with_children(em, r, 1, LOG1, 2)
        d = em.alloc(0x28)
        em.write(d + 0x20, f32(rng.choice([90.0, 45.0, 7.5]), rng.choice([30.0, 12.5, 1000.0])))
        i32(em, r + 0x138, d)
        em.write(r + 0x13C, f32(target, speed, angle))
        em.write(r + 0x148, rmatrix(rng))
        if params:
            p = em.alloc(8)
            em.write(p, f32(5.0, 5.0))
            i32(em, r + 0x178, p)
        return (r,)
    check(e, fk, 0x448E30, "RadarDish_Run", setup, cw=cw)


@pytest.mark.parametrize("arg", [None, b"", b"x"])
@pytest.mark.parametrize("kids", [0, 2])
def test_params_changed(arg, kids):
    def setup(em):
        rbase(em)
        r = new_task(em, RT, 0x180)
        em.write(r + 0x70, rmatrix(random.Random(3)))
        with_children(em, r, event(0x4F1A70), LOG2, kids)
        return r, 0 if arg is None else cstr(em, arg)
    check(e, fk, 0x449180, "RadarDish_ParamsChanged", setup)


def test_small():
    def setup(em):
        rbase(em)
        d = em.alloc(0x28)
        a = em.alloc(12)
        e.write(a, struct.pack("<III", d, 0xA4A4, 4))
        return 0, a
    check(e, fk, 0x449220, "RadarDish_ParseDef", setup)

    def setup2(em):
        rbase(em)
        r = new_task(em, RT, 0x180)
        em.write(r + 0x13C, f32(5.0))
        i32(em, r + 0x178, 0x1780)
        return r, em.alloc(12)
    check(e, fk, 0x449260, "RadarDish_Stop", setup2)
    check(e, fk, 0x449270, "RadarDish_GetSoundSource", setup2)
    check(e, fk, 0x449290, "RadarDish_Close", setup2)
