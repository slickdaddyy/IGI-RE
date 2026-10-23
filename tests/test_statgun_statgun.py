import random
import struct

import pytest

from emu import Emu
from test_statgun_fakes import (CWS, Fakes, add_child, check, common, event, f32, f64, i32, masked, new_task,
                                rfloats, rmatrix, rvec, set_events, set_handler)

e = Emu()
fk = Fakes(e)
common(e, fk)
fk.add(0x4168A0, 0, ret=0x4168A0)
fk.add(0x4C1790, 2)
fk.add(0x4C17C0, 2)
fk.add(0x416920, 0, ret=0x416920)
fk.add(0x4C5710, 3)
fk.add(0x4D0950, 1, st0="f", ret=1.75)
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C7560, 2)
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4CEC10, 1, ret=0x4CEC10, fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4C48D0, 2)
TICKS = fk.add(0x4028B0, 0, ret=100)
SLOT = fk.add(0x4F2060, 2, ret=0)
fk.add(0x4F2050, 1)
fk.add(0x4F2290, 1)
fk.add(0x4F2140, 3)


def touch(em, a):
    em.write(a[2] + 4, f32(0.125))


fk.add(0x4F2310, 3, deref={1: 24, 2: 0x28}, effect=touch)


def rnd_matrix(em, a):
    em.write(a[0], rmatrix(random.Random(str(a[1:]))))


fk.add(0x4B38E0, 4, effect=rnd_matrix, fmt=lambda em, a: tuple(a[1:]))
fk.add(0x4B3BE0, 2, effect=rnd_matrix, fmt=lambda em, a: (a[1],))
fk.add(0x4658D0, 3, deref={1: 24, 2: 0x28})
fk.add(0x465910, 1)
fk.add(0x4B0D60, 1)
fk.add(0x4B0D90, 0)
fk.add(0x4B8770, 1, ret=0x77, fmt=lambda em, a: (em.read_cstr(a[0]),))
FIND = fk.add(0x413BD0, 1, ret=0)
fk.add(0x477C50, 5)
fk.add(0x416AD0, 3, deref={2: 24})
fk.add(0x4871C0, 2)
fk.add(0x414E20, 3, effect=lambda em, a: em.write(a[0], rvec(random.Random(a[2]), 50)), fmt=lambda em, a: (a[1], a[2]))
ANCESTOR = fk.add(0x401CC0, 2, ret=0, fmt=masked(~0, 0xFFFF))
USE = fk.add(fk.code(b"\xc3"), 2, fmt=lambda em, a: (a[0], em.read(a[1], 2), em.read(a[1] + 4, 16),
                                                        em.read(a[1] + 0x20, 16)),
             effect=lambda em, a: em.write(a[1] + 0x20, rfloats(random.Random(a[0]), 2)))
CHILD = fk.handler(1)
CHILD_MOVE = fk.add(fk.code(b"\xc3"), 2, fmt=lambda em, a: (a[0], em.read(a[1], 8)))


def statgun(em, rng, user=None, model=True):
    o = new_task(em, 0x50, 0x220)
    em.write(o + 0x20, rvec(rng))
    em.write(o + 0x70, rmatrix(rng))
    em.write(o + 0x98, f32(*[rng.choice([0.5, 1.0, rng.uniform(0.2, 3)]) for _ in range(3)]))
    i32(em, o + 0x68, 0x6868)
    if model:
        mdl = em.alloc(0x100)
        i32(em, mdl, mdl + 0x10)
        i32(em, mdl + 0x10 + 0xC, mdl + 0x40)
        i32(em, mdl + 0x40 + 0xC, mdl + 0x80)
        em.write(mdl + 0x80 + 0x7C, struct.pack("<h", rng.choice([0, 1, 2, -1])))
        i32(em, o + 0x6C, mdl)
    em.write(o + 0xF0, rvec(rng) + rmatrix(rng) + rvec(rng) + rmatrix(rng))
    em.write(o + 0x1B0, b"model_x\0")
    em.write(o + 0x1C0, b"WEAPON_ID_M2HB\0")
    em.write(o + 0x1E0, f32(rng.uniform(5, 40), rng.uniform(-40, -5), rng.choice([90.0, 360.0, rng.uniform(10, 400)])))
    i32(em, o + 0x1EC, 300)
    em.write(o + 0x1F0, f32(*[rng.uniform(-0.3, 0.3) for _ in range(2)] + [rng.uniform(-1.2, 1.2) for _ in range(2)]
                            + [rng.uniform(-1, 1) for _ in range(2)]))
    if user:
        i32(em, o + 0x208, user)
    i32(em, o + 0x218, 99)
    return o


def human(em, rng, fire=0):
    h = new_task(em, 0x40, 0x500)
    em.write(h + 0x1B0, f32(rng.uniform(-1, 1), rng.uniform(-1, 1)))
    em.write(h + 0x1B0 + 24, f32(fire))
    em.write(h + 0x4D4, f32(rng.uniform(0, 2)))
    i32(em, h + 0x4EC, em.alloc(0x200))
    return h


def weapon(em):
    w = new_task(em, 0x58, 0x1B0)
    set_events(em)
    set_handler(em, event(0x460BF0), 0x58, USE)
    fk.rets[SLOT] = w
    return w


def test_create():
    for seed in range(4):
        def setup(em):
            o = statgun(em, random.Random(seed))
            return (o,)
        check(e, fk, 0x473D30, "StationaryGun_CreateHandler", setup=setup)


def test_open_close():
    check(e, fk, 0x473ED0, "StationaryGun_Open", setup=set_events)

    def setup(em):
        em.write(0x5BE388, struct.pack("<HxxI", 0x55, 0x3434))
    check(e, fk, 0x4752E0, "StationaryGun_Close", setup=setup)
    check(e, fk, 0x475300, "StationaryGun_GetType", setup=setup, check_eax=True, eax_mask=0xFFFF)


@CWS
@pytest.mark.parametrize("has_user", [0, 1])
@pytest.mark.parametrize("fire", [0, 1])
@pytest.mark.parametrize("has_w", [0, 1])
@pytest.mark.parametrize("seed", range(6))
def test_run(has_user, fire, has_w, seed, cw):
    fk.rets[TICKS] = 100

    def setup(em):
        rng = random.Random(seed)
        fk.rets[SLOT] = 0
        if has_w:
            weapon(em)
        u = human(em, rng, fire) if has_user else None
        o = statgun(em, rng, u)
        if seed == 1:
            em.write(o + 0x1F8, f32(-25 * 0.017453292))
        for i in range(seed % 3):
            c = new_task(em, 0x51 + i, 0x40)
            add_child(em, o, c)
            set_handler(em, 1, 0x51 + i, CHILD if i != 1 else 0)
        return (o,)
    _, log = check(e, fk, 0x474190, "StationaryGun_RunHandler", setup=setup, cw=cw)
    assert any(k == USE for k, _ in log) == bool(has_w)
    assert sum(k == CHILD for k, _ in log) == (seed % 3 > 0)


def test_run_same_tick():
    fk.rets[TICKS] = 99

    def setup(em):
        return (statgun(em, random.Random(1)),)
    check(e, fk, 0x474190, "StationaryGun_RunHandler", setup=setup)
    fk.rets[TICKS] = 100


@pytest.mark.parametrize("has_user", [0, 1])
@pytest.mark.parametrize("fire", [0, 1, 0xFF])
def test_fire(has_user, fire):
    def setup(em):
        rng = random.Random(fire)
        weapon(em)
        o = statgun(em, rng, human(em, rng) if has_user else None)
        return o, fire
    check(e, fk, 0x474360, "StationaryGun_Fire", setup=setup)


@CWS
@pytest.mark.parametrize("has_user", [0, 1])
@pytest.mark.parametrize("model", [0, 1])
@pytest.mark.parametrize("seed", range(6))
def test_aim(has_user, model, seed, cw):
    def setup(em):
        rng = random.Random(seed)
        weapon(em)
        o = statgun(em, rng, human(em, rng) if has_user else None, model)
        return o, o + 0x70
    _, log = check(e, fk, 0x474420, "StationaryGun_Aim", setup=setup, cw=cw)
    assert any(k == 0x4658D0 for k, _ in log) == bool(has_user)


def test_aim_no_weapon():
    def setup(em):
        fk.rets[SLOT] = 0
        o = statgun(em, random.Random(3))
        return o, o + 0x70
    check(e, fk, 0x474420, "StationaryGun_Aim", setup=setup)


@CWS
@pytest.mark.parametrize("seed", range(40))
def test_clamp(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        o = statgun(em, rng)
        if seed % 5 == 0:
            em.write(o + 0x1F8, f32(rng.choice([25, -25]) * 0.017453292))
        return (o,)
    check(e, fk, 0x4748B0, "StationaryGun_Clamp", setup=setup, cw=cw)


def test_delete_misc():
    def setup(em):
        rng = random.Random(4)
        return statgun(em, rng, human(em, rng)), em.alloc(0x50)
    check(e, fk, 0x474960, "StationaryGun_DeleteHandler", setup=setup)
    check(e, fk, 0x474ED0, "StationaryGun_GetUseKind", setup=setup)
    check(e, fk, 0x474F50, "StationaryGun_GetParams", setup=setup)
    check(e, fk, 0x475340, "StationaryGun_Release", setup=setup)

    def setup2(em):
        o, out = setup(em)
        return o, out, out + 0x30
    check(e, fk, 0x475310, "StationaryGun_GetMuzzle", setup=setup2)
    for used in (0, 1, 0x80):
        def setup3(em):
            o, _ = setup(em)
            em.write(o + 0x21C, bytes([used]))
            return (o,)
        check(e, fk, 0x475370, "StationaryGun_IsUsed", setup=setup3, check_eax=True, eax_mask=0xFF)


@CWS
@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("carrier", [0, 1])
@pytest.mark.parametrize("seed", range(4))
def test_changed(found, carrier, seed, cw):
    def setup(em):
        rng = random.Random(seed)
        weapon(em)
        o = statgun(em, rng, human(em, rng))
        fk.rets[FIND] = 0x1234 if found else 0
        c = 0
        if carrier:
            c = new_task(em, 0x52, 0xF0)
            em.write(c + 0x20, rvec(rng))
            em.write(c + 0x70, rmatrix(rng))
        fk.rets[ANCESTOR] = c
        return (o,)
    _, log = check(e, fk, 0x474980, "StationaryGun_Changed", setup=setup, cw=cw)
    assert any(k == 0x477C50 for k, _ in log) == bool(found)


@CWS
@pytest.mark.parametrize("carrier", [0, 1])
@pytest.mark.parametrize("seed", range(8))
def test_update_carrier(carrier, seed, cw):
    def setup(em):
        rng = random.Random(seed)
        o = statgun(em, rng)
        c = 0
        if carrier:
            c = new_task(em, 0x52, 0xF0)
            em.write(c + 0x20, rvec(rng))
            em.write(c + 0x70, rmatrix(rng))
        fk.rets[ANCESTOR] = c
        return (o,)
    check(e, fk, 0x474AE0, "StationaryGun_UpdateCarrier", setup=setup, cw=cw)


@CWS
@pytest.mark.parametrize("used", [0, 1])
def test_use_query(used, cw):
    seen = set()
    for seed in range(10):
        seen.add(use_query(used, seed, cw))
    assert seen == ({False} if used else {False, True})


def use_query(used, seed, cw):
    def setup(em):
        rng = random.Random(seed)
        o = statgun(em, rng)
        em.write(o + 0x21C, bytes([used]))
        q = em.alloc(0x40)
        i32(em, q, 0x4444)
        em.write(q + 8, f64(*[v + rng.uniform(-60, 60) for v in struct.unpack("<3d", em.read(o + 0x20, 24))]))
        return o, q
    _, log = check(e, fk, 0x474E30, "StationaryGun_UseQuery", setup=setup, cw=cw)
    return any(k == 0x416AD0 for k, _ in log)


@pytest.mark.parametrize("unk44", [0, 1])
def test_attach_events(unk44):
    def setup(em):
        rng = random.Random(2)
        h = human(em, rng)
        o = statgun(em, rng)
        m = em.alloc(0x50)
        i32(em, m, h)
        i32(em, m + 0x44, unk44)
        return o, m
    check(e, fk, 0x474EE0, "StationaryGun_Attach", setup=setup)
    check(e, fk, 0x474F10, "StationaryGun_Event6", setup=setup)

    def setup2(em):
        o, m = setup(em)
        set_handler(em, 1, 0x50, CHILD)
        return o, m
    check(e, fk, 0x474F30, "StationaryGun_Event14", setup=setup2)


@CWS
@pytest.mark.parametrize("kids", [0, 1, 3])
@pytest.mark.parametrize("seed", range(4))
def test_move(kids, seed, cw):
    def setup(em):
        rng = random.Random(seed)
        weapon(em)
        o = statgun(em, rng, human(em, rng))
        for i in range(kids):
            c = new_task(em, 0x51 + i, 0x40)
            add_child(em, o, c)
            set_handler(em, event(0x4C5800), 0x51 + i, CHILD_MOVE if i != 1 else 0)
        m = em.alloc(0x50)
        em.write(m + 8, rvec(rng) + rmatrix(rng))
        em.write(m, struct.pack("<II", m + 8, m + 0x20))
        return o, m
    _, log = check(e, fk, 0x474F60, "StationaryGun_Move", setup=setup, cw=cw)
    assert sum(k == CHILD_MOVE for k, _ in log) == (kids > 0) + (kids > 2)


@pytest.mark.parametrize("a,b", [(0, 0), (1, 0), (2, 0), (0, 1), (0, 2), (3, 3)])
def test_detect(a, b):
    def setup(em):
        rng = random.Random(1)
        h = human(em, rng)
        o = statgun(em, rng, h)
        vals = [0x1111, o, h, 0x2222]
        m = em.alloc(0x10)
        em.write(m, b"\x77" * 8 + struct.pack("<II", vals[a], vals[b]))
        return o, m
    check(e, fk, 0x4752B0, "StationaryGun_Detect", setup=setup)
