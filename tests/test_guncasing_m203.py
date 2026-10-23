import random
import struct

import pytest

from test_guncasing_util import CWS, Emu, Fakes, check, cstr, f32, f64, new_task, rmatrix, rvec, u32

e = Emu()
fk = Fakes(e)
GT, RT, EV7 = 0x33, 0x34, 0x47
WORLD, OWNER, SPR, L3, RNG, SMOKE, MODEL = (0x5A000000, 0x5B000000, 0x5C000000, 0x5D000000, 0x5E000000,
                                            0x5F000000, 0x60000000)
R = {}
s = lambda em, p: em.read_cstr(p)


def coll_lines(em, a):
    found = R["found"]
    if found:
        em.write(a[1] + 8, rvec(random.Random(found), 3000))
    em.write(a[1] + 0x58, u32(found))
    return (a[0], em.read(a[2], 24), em.read(a[3], 24), a[4], a[5] & 0xFF, em.read(a[6] + 4, 16), a[7])


fk.add(0x401900, 5, ret=RT, fmt=lambda em, a: (s(em, a[0]), a[1], a[2] & 0xFFFF, a[3], a[4]))
fk.add(0x401400, 3, mask=[0xFFFF, 0xFF])
fk.add(0x401530, 3, mask=[0xFFFF])
fk.add(0x401A20, 1, mask=[0xFFFF])
fk.add(0x4012A0, 3, ret=lambda em, a: R["task"], mask=[~0, 0xFFFF])
fk.add(0x4015F0, 1)
fk.add(0x401AE0, 1, ret=lambda em, a: a[0] + 0x100)
fk.add(0x401B20, 1)
fk.add(0x416830, 0, ret=EV7)
fk.add(0x4168F0, 0, ret=L3)
fk.add(0x416920, 0, ret=WORLD)
fk.add(0x4169D0, 1, ret=SPR, fmt=lambda em, a: (s(em, a[0]),))
fk.add(0x416D20, 0, ret=RNG)
fk.add(0x480FE0, 16, fmt=lambda em, a: (em.read(a[0], 24),) + tuple(a[1:9]) + (a[9] - a[0], s(em, a[10]),
                                                                                s(em, a[11])) + tuple(a[12:]))
fk.add(0x4B4770, 1, ret=lambda em, a: R["rf"].pop(0), st0="d")
fk.add(0x4B8810, 2, fmt=lambda em, a: (s(em, a[0]), a[1]))
fk.add(0x4C1790, 2)
fk.add(0x4C17C0, 2)
fk.add(0x4C48C0, 0, ret=GT)
fk.add(0x4C48D0, 2)
fk.add(0x4C5710, 3)
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C7560, 2)
fk.add(0x4CC2A0, 8, fmt=coll_lines)
fk.add(0x4CEC10, 1, ret=MODEL, fmt=lambda em, a: (s(em, a[0]),))
fk.add(0x4D0950, 1, ret=lambda em, a: R["radius"], st0="f")
fk.add(0x4F0EE0, 0, ret=OWNER)
fk.add(0x4FDF00, 13, ret=SMOKE, deref={3: 12, 4: 12, 5: 8})
fk.add(0x4FE120, 9, deref={1: 24, 2: 12})
e.baseline = e.snapshot()


def shell(em, rng):
    g = new_task(em, RT, 0x128)
    em.write(g + 0x98, f32(*[rng.choice([1.0, rng.uniform(0.5, 2)]) for _ in range(3)]))
    em.write(g + 0xF0, rvec(rng) + f32(*[rng.uniform(-300, 300) for _ in range(3)]))
    em.write(g + 0x114, u32(rng.choice([0, ref(em, 0x7100)]), rng.choice([0, ref(em, 0x7200)]), SMOKE))
    em.write(g + 0x120, f32(rng.uniform(1, 5), rng.uniform(0.5, 2)))
    return g


def ref(em, t):
    p = em.alloc(4)
    em.write(p, u32(t))
    return p


def test_open_close():
    check(e, fk, 0x476110, "M203_Open", check_eax=False)
    check(e, fk, 0x476660, "M203_Close", lambda em: em.write(0x5BE3B0, struct.pack("<H", RT)), check_eax=False)


@CWS
@pytest.mark.parametrize("seed", range(6))
def test_create(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        R["radius"] = rng.uniform(100, 900)
        em.write(0x5BE3AC, u32(SPR))
        return (shell(em, rng),)
    check(e, fk, 0x4761B0, "M203_CreateHandler", setup, check_eax=False, cw=cw)


@CWS
@pytest.mark.parametrize("seed", range(30))
def test_run(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        R["radius"] = rng.uniform(100, 900)
        R["found"] = rng.choice([0, 0, seed + 1])
        R["rf"] = [rng.random() for _ in range(3)]
        return (shell(em, rng),)
    check(e, fk, 0x476340, "M203_RunHandler", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("seed", range(4))
def test_delete(seed):
    def setup(em):
        return (shell(em, random.Random(seed)),)
    check(e, fk, 0x476600, "M203_DeleteHandler", setup, check_eax=False)


@pytest.mark.parametrize("owner,other", [(0, 0), (0x7100, 0), (0, 0x7200), (0x7100, 0x7200)])
def test_new(owner, other):
    def setup(em):
        rng = random.Random(owner + other)
        em.write(0x5BE3B0, struct.pack("<H", RT))
        R["task"] = new_task(em, RT, 0x128)
        pos, mat, vel = em.alloc(24), em.alloc(40), em.alloc(12)
        em.write(pos, rvec(rng))
        em.write(mat, rmatrix(rng))
        em.write(vel, f32(1.5, -2.5, 3.5))
        return (0x7000, owner, other, pos, mat, vel, cstr(em, b"m203shell"), struct.unpack("<I", f32(2.5))[0],
                struct.unpack("<I", f32(0.75))[0])
    check(e, fk, 0x476670, "M203_New", setup)
