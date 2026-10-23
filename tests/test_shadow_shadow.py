import random
import struct

import pytest

from emu import Emu
from test_shadow_fakes import Fakes, check, f32, f64, i32, new_task, set_handler, u16, with_children

EV2 = 0x24
PT = 0x45  # PolyList type

e = Emu()
fk = Fakes(e)
fk.add(0x401900, 4, ret=0x77, fmt=lambda em, a: (em.read_cstr(a[0]), a[1], a[2] & 0xFFFF, a[3]))
fk.add(0x401400, 3, fmt=lambda em, a: (a[0] & 0xFFFF, a[1] & 0xFF, em.normalize(a[2])))
fk.add(0x401530, 3, fmt=lambda em, a: (a[0] & 0xFFFF, a[1], em.normalize(a[2])))
fk.add(0x401A20, 1, fmt=lambda em, a: (a[0] & 0xFFFF,))
fk.add(0x4018F0, 0, ret=0x12)
fk.add(0x4C56F0, ret=EV2)
fk.add(0x4015F0, 1)
fk.add(0x4012A0, 3, ret=lambda em, a: new_task(em, 0x60, 0x124), fmt=lambda em, a: (a[0], a[1] & 0xFFFF, a[2]))
fk.add(0x49A450, 11, ret=lambda em, a: new_task(em, PT, 0x40), deref={2: 24, 3: 40})
LOG2 = fk.handler(2, deref={0: 0x40})
e.baseline = e.snapshot()


def test_container_open():
    check(e, fk, 0x4E0840, "MagicObjContainer_Open")


@pytest.mark.parametrize("kids", [0, 1, 3])
def test_container_event(kids):
    def setup(em):
        t = new_task(em, 0x60)
        with_children(em, t, EV2, LOG2, kids)
        return t, 0x4444
    check(e, fk, 0x4E0890, "MagicObjContainer_Event", setup=setup)


def test_container_close_new():
    u16(e, 0xA5E364, 0x66)
    e.baseline = e.snapshot()
    check(e, fk, 0x4E0920, "MagicObjContainer_Close")
    check(e, fk, 0x4E0930, "MagicObjContainer_New", args=(0x1234, 0x5678), check_eax=True)


def test_shadow_open_close_delete():
    check(e, fk, 0x4E0950, "Shadow_Open")
    check(e, fk, 0x4E0CB0, "Shadow_Close")
    check(e, fk, 0x4E09C0, "Shadow_Delete", setup=lambda em: (new_task(em, 0x60, 0x124),))


def new_shadow(em):
    i32(em, 0xA44344, 0x9999)
    pos = em.alloc(24)
    em.write(pos, f64(1.5, -2.25, 1e5))
    flags = em.alloc(16)
    em.write(flags, struct.pack("<II", 1, 1) + f32(100.0))
    r = em.call(0x4E0CC0, 0x1000, pos, flags, flags + 4, flags + 8, 0x2222,
                struct.unpack("<I", f32(1843.2))[0], struct.unpack("<I", f32(40.0))[0], 0x3333)[0]
    return r, pos, flags


def test_shadow_new():
    def setup(em):
        i32(em, 0xA44344, 0x9999)
        pos = em.alloc(24)
        em.write(pos, f64(1, 2, 3))
        return (0x1000, pos, 0x4010, 0x4014, 0x4018, 0x2222, struct.unpack("<I", f32(1843.2))[0],
                struct.unpack("<I", f32(40.0))[0], 0x3333)
    check(e, fk, 0x4E0CC0, "Shadow_New", setup=setup, check_eax=True)


DIRS = [(0, 0, 1), (0.3, -0.2, 0.9), (1, 2, 3), (-0.7, 0.1, -0.2), (0, 0, 0)]


@pytest.mark.parametrize("height", [0.0, 100.0, 900.0, 1500.0, 2000.0, -50.0])
@pytest.mark.parametrize("d", DIRS)
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_enter(height, d, cw):
    def setup(em):
        set_handler(em, 4, PT, LOG2)
        sh, pos, flags = new_shadow(em)
        em.write(flags + 8, f32(height))
        dv = em.alloc(12)
        em.write(dv, f32(*d))
        i32(em, sh + 0x34, dv)
        return (sh,)
    check(e, fk, 0x4E09D0, "Shadow_MagicObjEnterHandler", setup=setup, cw=cw)


@pytest.mark.parametrize("vis,en", [(0, 1), (1, 0)])
def test_enter_hidden(vis, en):
    def setup(em):
        sh, pos, flags = new_shadow(em)
        em.write(flags, struct.pack("<II", vis, en))
        return (sh,)
    check(e, fk, 0x4E09D0, "Shadow_MagicObjEnterHandler", setup=setup)
