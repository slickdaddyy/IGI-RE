import random
import struct

import pytest

from emu import Emu, compare
from test_glass_fakes import (Fakes, base, check, check_error, common, cstr, event, f32, f64, i32, new_task, rmatrix,
                              rvec, set_handler, u8, with_children)

GT = 0x55  # Glass type id

e = Emu()
fk = Fakes(e)
common(e, fk, {b"Glass": GT})
LOG1 = fk.handler(1)
VALUE = {}
fk.add(0x4CEC10, 1, ret=lambda em, a: VALUE.get("model", 0), fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4C5710, 3)
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C7560, 2)
fk.add(0x4D0950, 1, ret=lambda em, a: VALUE.get("radius", 100.0), st0="d")
fk.add(0x4C4950, 3)
fk.add(0x4C1790, 2)
fk.add(0x4C17C0, 2)
fk.add(0x416D20, 0, ret=0x2D20)
fk.add(0x4B4770, 1, ret=lambda em, a: VALUE.get("random", 0.5), st0="d")
fk.add(0x4015F0, 1)
fk.add(0x44BD70, 6)
fk.add(0x416C90, 5, ret=lambda em, a: VALUE.get("falloff", 0.0), st0="d", deref={2: 24})
fk.add(0x44A1D0, 5, deref={1: 12})
fk.add(e.symbols["GlassShatter_Spawn"], 5, deref={1: 12}, key=0x44A1D0)
e.baseline = e.snapshot()


def gbase(em):
    VALUE.clear()
    base(em)
    em.write(0x57C220, struct.pack("<H", GT))


def make_arg(em, rng):
    a = em.alloc(0x90)
    i32(em, a, cstr(em, b"glass_model"))
    i32(em, a + 4, 0x4444)
    em.write(a + 8, rvec(rng, 1e5))
    em.write(a + 0x20, bytes(rng.randrange(256) for _ in range(0x18)))
    em.write(a + 0x38, rmatrix(rng))
    em.write(a + 0x60, bytes(rng.randrange(256) for _ in range(0x28)))
    i32(em, a + 0x88, 0x8888)
    i32(em, a + 0x8C, 0x8C8C)
    return a


def model(em, center, radius):
    """A VirModel whose LOD info has the centre at +0x30 and the radius at +0x3C."""
    m, lod, a, info = em.alloc(8), em.alloc(0x20), em.alloc(0x20), em.alloc(0x80)
    i32(em, m, lod)
    i32(em, lod + 0xC, a)
    i32(em, a + 0xC, info)
    em.write(info + 0x30, f32(*center, radius))
    return m


def test_open():
    _, log = check(e, fk, 0x449970, "Glass_Open", gbase)
    assert len(log) == 11


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
def test_create(seed, cw):
    def setup(em):
        gbase(em)
        rng = random.Random(seed)
        g = new_task(em, GT, 0x148)
        em.write(g + 0x98, f32(*[rng.choice([1.0, 2.0, 0.5]) for _ in range(3)]))
        i32(em, g + 0x6C, 0x6C6C)
        em.write(g + 0x13C, b"\x55" * 12)
        VALUE.update(model=0x7700, radius=rng.uniform(10, 1000), random=rng.random())
        return g, make_arg(em, rng)
    check(e, fk, 0x4497F0, "Glass_Create", setup, cw=cw)


def test_create_error():
    def setup(em):
        gbase(em)
        return new_task(em, GT, 0x148), make_arg(em, random.Random(1))
    check_error(e, fk, 0x4497F0, "Glass_Create", setup)


@pytest.mark.parametrize("broken", [0, 1])
@pytest.mark.parametrize("kids", [0, 2])
def test_run_delete(broken, kids):
    def setup(em):
        gbase(em)
        g = new_task(em, GT, 0x148)
        i32(em, g + 0x68, 0x6868)
        u8(em, g + 0x144, broken)
        with_children(em, g, 1, LOG1, kids)
        return (g,)
    check(e, fk, 0x449A80, "Glass_Run", setup)
    check(e, fk, 0x449950, "Glass_Delete", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("damage,amount,strength", [(0.0, 0.3, 1.0), (0.5, 0.5, 1.0), (0.9, 0.2, 1.0),
                                                    (0.1, 0.1, 0.2), (1.2, -0.5, 1.0)])
@pytest.mark.parametrize("broken", [0, 1])
def test_hit(damage, amount, strength, broken, cw):
    def setup(em):
        gbase(em)
        rng = random.Random(int(damage * 10 + amount * 100))
        g = new_task(em, GT, 0x148)
        i32(em, g + 0x6C, 0x6C6C)
        em.write(g + 0x13C, f32(damage, strength))
        u8(em, g + 0x144, broken)
        a = em.alloc(0x14)
        i32(em, a, 0xA0A0)
        em.write(a + 4, f32(amount, *[rng.uniform(-5000, 5000) for _ in range(3)]))
        return g, a
    check(e, fk, 0x449B00, "Glass_Hit", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(10))
def test_blast(seed, cw):
    def setup(em):
        gbase(em)
        rng = random.Random(seed)
        g = new_task(em, GT, 0x148)
        em.write(g + 0x20, rvec(rng, 1e6))
        em.write(g + 0x70, rmatrix(rng))
        i32(em, g + 0x6C, model(em, [rng.uniform(-500, 500) for _ in range(3)], rng.uniform(10, 900)))
        em.write(g + 0x13C, f32(rng.uniform(0, 1), 1.0))
        u8(em, g + 0x144, seed % 5 == 4)
        VALUE["falloff"] = 0.0 if seed % 3 == 0 else rng.uniform(-1, 2)
        a = em.alloc(0x14)
        em.write(a, struct.pack("<IIIIf", 1, 0x7004, 2, 3, rng.uniform(100, 9000)))
        return g, a
    check(e, fk, 0x449BA0, "Glass_Blast", setup, cw=cw)


@pytest.mark.parametrize("broken", [0, 1])
def test_touch(broken):
    def setup(em):
        gbase(em)
        g = new_task(em, GT, 0x148)
        em.write(g + 0x13C, f32(0.25, 1.125))
        u8(em, g + 0x144, broken)
        a = em.alloc(12)
        i32(em, a, 0x7700)
        return g, a
    check(e, fk, 0x449CB0, "Glass_Touch", setup)


def test_small():
    def setup(em):
        gbase(em)
        em.write(0x57C220, struct.pack("<H", 0x66))
    check(e, fk, 0x449D10, "Glass_Close", setup)
    compare(e, 0x449D20, "Glass_GetType", setup=setup, eax_mask=0xFFFF)
