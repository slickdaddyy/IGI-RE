import random
import struct

import pytest

from test_guncasing_util import (CWS, Emu, Fakes, check, cstr, f32, f64, new_task, rmatrix, rvec, set_getter,
                                 set_handler, u32)

e = Emu()
fk = Fakes(e)
ET, MT, EV = 0x31, 0x32, 0x45
OWNER, SND, RES, L1, L3 = 0x5A000000, 0x5B000000, 0x5C000000, 0x5D000000, 0x5E000000
RUN = fk.handler(1)
DEL = fk.handler(1)
CHG = fk.handler(2)
NEW = {}
fk.add(0x4015F0, 1)
fk.add(0x401A20, 1, mask=[0xFFFF])
fk.add(0x4168B0, 0, ret=L1)
fk.add(0x4168F0, 0, ret=L3)
fk.add(0x4C17C0, 2)
fk.add(0x4C1830, 1)
fk.add(0x4F0EE0, 0, ret=OWNER)
fk.add(0x4E6B00, 2, ret=lambda em, a: NEW.get("snd", SND), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4E6C30, 2)
fk.add(0x4D0950, 1, ret=lambda em, a: NEW["radius"], st0="f")
fk.add(0x46B4D0, 0, ret=RES)
fk.add(0x467DF0, 1, fmt=lambda em, a: (em.read(a[0], 0x2C),))
fk.add(0x4012A0, 3, ret=lambda em, a: NEW["task"], mask=[~0, 0xFFFF])
fk.add(0x481360, 9, fmt=lambda em, a: tuple(a[:8]) + (em.read_cstr(a[8]),))
e.baseline = e.snapshot()


def base(em):
    set_getter(em, 0x481340, ET)
    set_getter(em, 0x4F1A70, EV)
    em.write(0x5BE3A0, struct.pack("<H", MT))
    em.write(0x5BE39C, u32(0x11112222))
    em.write(0x5BE3A8, u32(0x33334444))
    set_handler(em, 1, ET, RUN)
    set_handler(em, 2, ET, DEL)
    set_handler(em, EV, ET, CHG)


def mine(em):
    m = new_task(em, MT, 0x250)
    return m


@pytest.mark.parametrize("arm,trig,count,exploded", [(0, 0, 0, 0), (3, 0, 5, 0), (0, 1, 2, 0), (1, 1, 0, 1),
                                                     (0, 0, 0, 1)])
def test_run(arm, trig, count, exploded):
    def setup(em):
        base(em)
        m = mine(em)
        em.write(m + 0x238, u32(arm, count))
        em.write(m + 0x248, bytes([trig]))
        em.write(m + 0x190, bytes([exploded]))
        return (m,)
    check(e, fk, 0x475CC0, "Mine_RunHandler", setup, check_eax=False)


def test_delete():
    def setup(em):
        base(em)
        return (mine(em),)
    check(e, fk, 0x475D30, "Mine_DeleteHandler", setup, check_eax=False)


def test_param_table_close_list():
    def setup(em):
        base(em)
        out = em.alloc(4)
        return (mine(em), out)
    check(e, fk, 0x475D80, "Mine_GetParamTableHandler", setup, check_eax=False)
    check(e, fk, 0x476000, "Mine_Close", base, check_eax=False)
    check(e, fk, 0x476030, "Mine_GetList", base)


@CWS
@pytest.mark.parametrize("delay", [0.75, 1.3333, 0.016, 2.5, -1.25])
def test_param_changed(delay, cw):
    def setup(em):
        base(em)
        m = mine(em)
        em.write(m + 0x240, f32(delay))
        return (m, 0x1234)
    check(e, fk, 0x475D90, "Mine_ParamChangedHandler", setup, check_eax=False, cw=cw)


@CWS
@pytest.mark.parametrize("seed", range(40))
def test_proximity(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        base(em)
        NEW["snd"] = rng.choice([SND, 0])
        m = mine(em)
        em.write(m + 0x20, rvec(rng, 4000))
        em.write(m + 0x70, rmatrix(rng))
        em.write(m + 0x238, u32(rng.choice([0, 0, 2])))
        em.write(m + 0x244, f32(rng.uniform(0.5, 1.5)))
        em.write(m + 0x248, bytes([rng.choice([0, 0, 1]), rng.choice([0, 0, 0, 1])]))
        p = em.alloc(0x24)
        em.write(p + 8, rvec(rng, 4000) + f32(rng.uniform(0, 2000)))
        return (m, p)
    check(e, fk, 0x475DF0, "Mine_ProximityHandler", setup, check_eax=False, cw=cw)


@CWS
@pytest.mark.parametrize("seed", range(12))
def test_map(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        base(em)
        NEW["radius"] = rng.uniform(100, 2000)
        m = mine(em)
        em.write(m + 0x98, f32(*[rng.choice([1.0, 2.0, rng.uniform(0.5, 3)]) for _ in range(3)]))
        em.write(m + 0x190, bytes([rng.choice([0, 1])]))
        em.write(m + 0x249, bytes([rng.choice([0, 1])]))
        q = em.alloc(0x2C)
        em.write(q, bytes(range(0x2C)))
        return (m, q)
    check(e, fk, 0x475F30, "Mine_MapHandler", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("remote", [0, 1])
def test_new(remote):
    def setup(em):
        rng = random.Random(remote)
        base(em)
        NEW["task"] = mine(em)
        pos = em.alloc(24)
        em.write(pos, rvec(rng))
        mat = em.alloc(40)
        em.write(mat, rmatrix(rng))
        name = cstr(em, b"mine_snd_%d" % remote)
        return (0x7000, pos, mat, name, 7, struct.unpack("<I", f32(0.5))[0], struct.unpack("<I", f32(2.5))[0],
                struct.unpack("<I", f32(3.25))[0], 0x100 | remote)
    check(e, fk, 0x476040, "Mine_New", setup)
