import math
import random
import struct

import pytest

from emu import Emu
from test_aiaction_util import Fakes, check, f32, f64, set_game, u32

HANDLERS = 0xA96AE0
e = Emu()
fk = Fakes(e)
ST = {}

fk.add(0x4CEBA0, 1, ret=0x4440, fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x5034D0, 0, ret=0x12)
fk.add(0x401900, 4, ret=0x1C3, fmt=lambda em, a: (em.read_cstr(a[0]), a[1], a[2] & 0xFFFF, a[3]))
fk.add(0x401400, 3, fmt=lambda em, a: (a[0] & 0xFFFF, a[1] & 0xFF, em.normalize(a[2])))
fk.add(0x401530, 3, fmt=lambda em, a: (a[0] & 0xFFFF, a[1], em.normalize(a[2])))
for _a, _v in ((0x4F1A60, 0x32), (0x4F1A70, 0x33), (0x4F1950, 0x7A0000), (0x4F1010, 0x41), (0x4F0F70, 0x42),
               (0x4F0F90, 0x43), (0x4F0F80, 0x44), (0x4F0FF0, 0x45)):
    fk.add(_a, 0, ret=_v)
fk.add(0x4F1A80, 6, fmt=lambda em, a: (a[0], a[2] - a[1], a[3], a[4], a[5]))
fk.add(0x4F1320, 1)   # VarString_Init
fk.add(0x4F1340, 1)   # VarString_Free
fk.add(0x4F1400, 2)   # VarString_Bind
fk.add(0x4F16C0, 1, ret=lambda em, a: ST["expr"])
fk.add(0x4F16E0, 2, ret=lambda em, a: ST["eval"], st0="d")
fk.add(0x4E6B00, 2, ret=lambda em, a: ST["play"], fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4E6C00, 1)   # Sound_Stop
fk.add(0x4E6C30, 2)   # Sound_SetPos
fk.add(0x4E6C60, 3)   # Sound_SetVolume


def _ref_add(em, a):
    em.write(ST["ref"], u32(a[0]))
    return ST["ref"]


fk.add(0x401AE0, 1, ret=_ref_add)
fk.add(0x401B20, 1)   # TaskRef_Remove
fk.add(0x4C1830, 1)   # QTaskList_Free
fk.add(0x401A20, 1, fmt=lambda em, a: (a[0] & 0xFFFF,))
fk.add(0x4012A0, 3, ret=0x5550000, fmt=lambda em, a: (a[0], a[1] & 0xFFFF, a[2]))
CHILD_RUN = fk.add(fk.code(b"\xc3"), 1)

A = e.alloc(0x210)
e.baseline = e.snapshot()


def area(em, rng):
    em.write(A, bytes(rng.randrange(256) for _ in range(0x210)))
    em.write(A + 8, u32(A + 0xC) + u32(0) + u32(A + 8))
    return A


def test_init():
    check(e, fk, 0x44C130, "AmbientArea_Init", check_eax=False)


def test_create():
    check(e, fk, 0x44C2C0, "AmbientArea_Create", setup=lambda em: (area(em, random.Random(1)),), check_eax=False)


@pytest.mark.parametrize("ref", ["none", "dead", "live"])
def test_delete(ref):
    def setup(em):
        a = area(em, random.Random(2))
        r = 0
        if ref != "none":
            r = em.alloc(4)
            em.write(r, u32(0 if ref == "dead" else em.alloc(0x40)))
        em.write(a + 0x1B8, u32(r))
        return (a,)
    check(e, fk, 0x44C700, "AmbientArea_Delete", setup=setup, check_eax=False)


def test_get_params():
    out = e.alloc(4)
    e.baseline = e.snapshot()
    check(e, fk, 0x44C730, "AmbientArea_GetParams", setup=lambda em: (0, out), check_eax=False)


def test_close():
    check(e, fk, 0x44C810, "AmbientArea_Close", check_eax=False)


def test_new():
    check(e, fk, 0x44C830, "AmbientArea_New", setup=lambda em: (0x1234,))


def test_set():
    def setup(em):
        a = area(em, random.Random(3))
        pos, m, size = em.alloc(24), em.alloc(40), em.alloc(24)
        em.write(pos, f64(1.5, 2.5, 3.5))
        em.write(m, bytes(range(40)))
        em.write(size, f64(4.0, 5.0, 6.0))
        s = em.alloc(32)
        em.write(s, b"wind_1 wind_2\0")
        return a, pos, m, size, s
    check(e, fk, 0x44C860, "AmbientArea_Set", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("text", [b"", b"one", b"a b", b" ,;x,,yy ;zzz; ", b"s1,s2;s3 s4,s5,s6;s7 s8",
                                  b"a_name_that_is_long_enough_to_reach_the_name_slots now"])
def test_changed(text, cw):
    def setup(em):
        a = area(em, random.Random(len(text)))
        set_game(em, len(text))
        em.write(a + 0x94, text + b"\0")
        em.write(a + 0x80, f64(2.0, 3.25))
        return (a,)
    check(e, fk, 0x44C740, "AmbientArea_Changed", setup=setup, cw=cw, check_eax=False)


def rot(rng):
    a, b = rng.uniform(0, 6.3), rng.uniform(0, 6.3)
    ca, sa, cb, sb = math.cos(a), math.sin(a), math.cos(b), math.sin(b)
    return f32(ca, -sa * cb, sa * sb, sa, ca * cb, -ca * sb, 0.0, sb, cb) + u32(0)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(60))
def test_run(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        a = area(em, rng)
        g = set_game(em, seed)
        size = [rng.uniform(0.5, 3.0) for _ in range(3)]
        em.write(a + 0x20, f64(*[rng.uniform(-5000, 5000) for _ in range(3)]))
        em.write(a + 0x38, f64(*size))
        em.write(a + 0x50, rot(rng))
        # listener inside or outside the box
        k = 0.8 if rng.random() < 0.6 else 2.5
        local = [rng.uniform(-k, k) * s * 4096 for s in size]
        m = struct.unpack("<9f", em.read(a + 0x50, 36))
        pos = struct.unpack("<3d", em.read(a + 0x20, 24))
        world = [pos[j] + sum(m[3 * i + j] * local[i] for i in range(3)) for j in range(3)]
        em.write(g + 0x88, f64(*world))
        em.write(a + 0x78, f64(rng.choice([0.0, 0.02, 0.04, 0.5, 0.97, 0.96, 1.0])))
        em.write(a + 0x80, f64(rng.uniform(0, 3), rng.uniform(0, 3)))
        em.write(a + 0x90, u32(rng.choice([0, 0, 3])))
        em.write(a + 0x1B4, u32(rng.randrange(1, 8)))
        ref = 0
        kind = rng.choice(["none", "dead", "live"])
        if kind != "none":
            ref = em.alloc(4)
            em.write(ref, u32(0 if kind == "dead" else em.alloc(0x40)))
        em.write(a + 0x1B8, u32(ref))
        ST.update(expr=rng.choice([0, 0, 1]), eval=rng.choice([0.0, 0.4, 1.0, 3.0]),
                  play=rng.choice([0, em.alloc(0x40)]), ref=em.alloc(4))
        if rng.random() < 0.5:
            c = em.alloc(0x40)
            em.write(c, u32(a + 0xC) + u32(a + 8))
            em.write(c + 0x1C, struct.pack("<H", 0x150))
            em.write(a + 8, u32(c))
            em.write(a + 0x10, u32(c))
            em.write(HANDLERS + 4 * (0x180 + 0x150), u32(CHILD_RUN))
        return (a,)
    check(e, fk, 0x44C370, "AmbientArea_Run", setup=setup, cw=cw, check_eax=False)
