import random
import struct

import pytest

from emu import Emu
from test_glass_fakes import (Fakes, base, check, common, event, f32, i32, new_task, rmatrix, rvec, set_handler,
                              with_children)

BT = 0x52  # Building type id
ST = event(0x4DEE80)  # SectorObject

e = Emu()
fk = Fakes(e)
common(e, fk, {b"Building": BT})
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)
VALUE = {}
fk.add(0x416920, 0, ret=0x4444)
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4C7560, 2)
fk.add(0x4D0950, 1, ret=lambda em, a: VALUE.get("radius", 100.0), st0="d")
fk.add(0x4DEE90, 2, fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4DEEC0, 1)
fk.add(0x4E0810, 2, deref={1: 40})
e.baseline = e.snapshot()


def bbase(em):
    VALUE.clear()
    base(em)
    em.write(0x57C200, struct.pack("<H", BT))


def building(em, rng, scale=(1, 1, 1)):
    b = new_task(em, BT, 0x1F0)
    i32(em, b + 0x68, 0x6868)
    i32(em, b + 0x6C, 0x6C6C)
    em.write(b + 0x98, f32(*scale))
    em.write(b + 0x1A0, rmatrix(rng))
    em.write(b + 0x1C8, b"house_01\0")
    em.write(b + 0x1D8, rvec(rng, 1e6))
    VALUE["radius"] = rng.uniform(10, 5000)
    return b


def test_open():
    _, log = check(e, fk, 0x4485E0, "Building_Open", bbase)
    assert len(log) == 14


def test_create():
    def setup(em):
        bbase(em)
        set_handler(em, 0, ST, LOG2)
        b = new_task(em, BT, 0x1F0)
        em.write(b + 0x1A0, b"\x55" * 0x50)
        return b, 0x1234
    check(e, fk, 0x448760, "Building_Create", setup)


def test_small():
    def setup(em):
        bbase(em)
        set_handler(em, 2, ST, LOG1)
        i32(em, 0x57C204, 0xABCD)
        return building(em, random.Random(1)), em.alloc(4)
    check(e, fk, 0x448850, "Building_Delete", setup)
    check(e, fk, 0x448880, "Building_GetParamsHandler", setup)
    check(e, fk, 0x448B50, "Building_Close", setup)


SCALES = [(1, 2, 3), (3, 2, 1), (2, 3, 1), (1, 1, 1), (2, 1, 2), (1.5, 2.5, 2.5)]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("scale", SCALES)
def test_params_changed(scale, cw):
    def setup(em):
        bbase(em)
        return (building(em, random.Random(2), scale),)
    check(e, fk, 0x448890, "Building_ParamsChanged", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("scale", SCALES)
@pytest.mark.parametrize("kids", [0, 1, 3])
def test_world_add(scale, kids, cw):
    def setup(em):
        bbase(em)
        b = building(em, random.Random(3), scale)
        slot = event(0x4CEA10)
        set_handler(em, slot, ST, LOG2)
        with_children(em, b, slot, LOG2, kids)
        arg = em.alloc(8)
        i32(em, arg, 0x1111)
        i32(em, arg + 4, 0x2222)
        return b, arg
    check(e, fk, 0x448950, "Building_WorldAdd", setup, cw=cw)


@pytest.mark.parametrize("kids", [0, 1, 3])
def test_world_remove(kids):
    def setup(em):
        bbase(em)
        b = building(em, random.Random(4))
        slot = event(0x4CEA20)
        set_handler(em, slot, ST, LOG2)
        with_children(em, b, slot, LOG2, kids)
        arg = em.alloc(8)
        i32(em, arg, 0x1111)
        return b, arg
    check(e, fk, 0x448A90, "Building_WorldRemove", setup)
