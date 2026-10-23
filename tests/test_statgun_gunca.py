import random
import struct

import pytest

from emu import Emu
from test_statgun_fakes import (CWS, Fakes, check, check_error, common, event, i32, new_task, rmatrix,
                                rvec, set_events, set_handler, cstr)

MODEL = 0x20F00000

e = Emu()
fk = Fakes(e)
common(e, fk, {b"GunCABarrel": 0x61, b"GunCAClip": 0x62})
fk.add(0x4CEC10, 1, ret=lambda em, a: em.read_cstr(a[0]) != b"none" and MODEL)
fk.add(0x4C5710, 3)
fk.add(0x4C4950, 3)
fk.add(0x4C48D0, 2)
fk.add(0x477D10, 5, deref={1: 24, 2: 12, 3: 40})


def point(em, a):
    rng = random.Random(a[2])
    em.write(a[0], rvec(rng))


fk.add(0x414E20, 3, fmt=lambda em, a: (a[1], a[2]), effect=point)


def view_point(em, a):
    rng = random.Random(a[0])
    em.write(a[1], rvec(rng) + rmatrix(rng))


VIEW = fk.add(fk.code(b"\xc3"), 2, effect=view_point, fmt=lambda em, a: (a[0],))
SLOT4 = fk.handler(2)


def test_open_close():
    check(e, fk, 0x472670, "GunCA_Open", setup=set_events)

    def setup(em):
        em.write(0x5402FC, struct.pack("<HH", 0x62, 0x61))
    check(e, fk, 0x472A50, "GunCA_Close", setup=setup)


@pytest.mark.parametrize("name", [b"m1", b"none"])
def test_create(name):
    def setup(em):
        rng = random.Random(5)
        o = new_task(em, 0x61, 0x138)
        a = em.alloc(0x90)
        i32(em, a, cstr(em, name))
        i32(em, a + 4, 0x1234)
        em.write(a + 0x20, rvec(rng) + rmatrix(rng) + rmatrix(rng) + struct.pack("<I", 0x4444))
        return o, a
    if name == b"none":
        check_error(e, fk, 0x4727F0, "GunCAPart_Create", setup)
    else:
        check(e, fk, 0x4727F0, "GunCAPart_Create", setup=setup)


@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("has", [0, 1])
def test_follow(on, has):
    def setup(em):
        set_events(em)
        cam = em.alloc(0x60)
        i32(em, cam + 0x5C, on)
        holder = em.alloc(0x60)
        i32(em, holder + 0x50, cam)
        i32(em, 0xA5EC68, holder)
        h = new_task(em, 0x70, 0x100)
        owner = new_task(em, 0x71, 0x100)
        i32(em, owner + 0x14, h)
        gun = new_task(em, 0x72, 0x100)
        i32(em, gun + 0x20, owner)
        o = new_task(em, 0x61, 0x138)
        i32(em, o + 0x14, gun)
        set_handler(em, event(0x477C30), 0x70, VIEW if has else 0)
        set_handler(em, 4, event(0x4C48C0), SLOT4)
        return (o,)
    check(e, fk, 0x472880, "GunCABarrel_Follow", setup=setup)


@CWS
@pytest.mark.parametrize("what", [4, 5, 6])
def test_event(what, cw):
    def setup(em):
        set_events(em)
        rng = random.Random(what)
        o = new_task(em, 0x61, 0x138)
        em.write(o + 0x20, rvec(rng))
        em.write(o + 0x70, rmatrix(rng))
        msg = em.alloc(0x10)
        i32(em, msg + 8, what)
        return o, msg
    check(e, fk, 0x472930, "GunCABarrel_Event", setup=setup, cw=cw)


def test_event_no_model():
    fk.rets[0x4CEC10] = 0
    try:
        def setup(em):
            msg = em.alloc(0x10)
            i32(em, msg + 8, 6)
            return new_task(em, 0x61, 0x138), msg
        check_error(e, fk, 0x472930, "GunCABarrel_Event", setup)
    finally:
        fk.rets[0x4CEC10] = lambda em, a: em.read_cstr(a[0]) != b"none" and MODEL
