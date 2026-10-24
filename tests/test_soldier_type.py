import math
import struct

import pytest

from test_soldier_util import (LOG, NAN, RET, d64, f32, fake, float_fake, new_emu, pyfake, run, soldier, trap, u32,
                               u8)
from test_menu_util import logger

HUMAN_TYPE = 0x44
HANDLERS = 0xA96AE0

e = new_emu()


def slots_get(em, a):
    return em.u32(a[0] + 4 * a[1])


def param_add(em):
    a = [em.arg(i) for i in range(6)]
    LOG[-1].append((0x4F1A80, a[0], a[2] - a[1], a[3], a[4], a[5]))
    em.ret(0)


def printf(em):
    a = [em.arg(0), em.arg(1)]
    fmt = em.read_cstr(a[1])
    out, i = [0x460C80, a[0], fmt], 2
    for conv in fmt.split(b"%")[1:]:
        if conv[:1] in (b".", b"f"):
            out.append(struct.pack("<II", em.arg(i), em.arg(i + 1)))
            i += 2
        else:
            out.append(em.arg(i))
            i += 1
    LOG[-1].append(tuple(out))
    em.ret(0)


def ai_message(em):
    ai, msg = em.arg(0), em.arg(1)
    w = struct.unpack("<7I", em.read(msg, 28))
    LOG[-1].append((0x450D50, ai, w[0], w[1], w[2], w[5], w[6]))
    em.ret(0)


def find_weapon(em, a):
    return RET.get(("weapon", a[1]), 0)


def delta(em, a):
    f32(em, a[1], *RET.get("delta", (1.0, 2.0, 3.0)))


def normalize(em, a):
    v = struct.unpack("<3d", em.read(a[0], 24))
    n = math.sqrt(sum(x * x for x in v))
    d64(em, a[0], *[x / n for x in v])


for addr, n, kw in (
        (0x401900, 4, dict(strings=(0,), masks={2: 0xFFFF}, ret=0x55)), (0x401FA0, 2, dict(masks={0: 0xFFFF})),
        (0x401400, 3, dict(masks={0: 0xFFFF, 1: 0xFF})), (0x401530, 3, dict(masks={0: 0xFFFF})),
        (0x401A20, 1, dict(masks={0: 0xFFFF})), (0x4C1830, 1, {}),
        (0x460BE0, 0, dict(ret=0x12340000 | HUMAN_TYPE)), (0x4F1A60, 0, dict(ret=0x7700A1)),
        (0x4F1A70, 0, dict(ret=0x7700A2)), (0x4F40D0, 0, dict(ret=0x7700A3)), (0x416840, 0, dict(ret=0x7700A4)),
        (0x416860, 0, dict(ret=0x7700A5)), (0x460C40, 0, dict(ret=0x7700A6)), (0x47E990, 0, dict(ret=0x7700A7)),
        (0x460C50, 0, dict(ret=0xC50)), (0x4F1980, 2, dict(ret=0x1980)), (0x4F0FA0, 0, dict(ret=0xFA0)),
        (0x4EC190, 6, {}), (0x489C30, 2, {}),
        (0x463110, 1, {}), (0x459D50, 1, {}), (0x413CC0, 2, dict(effect=find_weapon)), (0x4F2070, 3, {}),
        (0x4F1050, 1, dict(ret=0x1050)), (0x4AF810, 3, dict(strings=(0,))),
        (0x45D360, 1, dict(also="Soldier_EnsureWeapon")), (0x460CF0, 1, {}), (0x4EC1E0, 2, {}), (0x4610C0, 1, {}),
        (0x464800, 2, {}), (0x4D2FB0, 3, dict(effect=delta)), (0x4613D0, 1, {}), (0x4614B0, 1, {}),
        (0x462CF0, 1, {}), (0x464BD0, 2, {}), (0x462E50, 1, {}), (0x463080, 1, {}), (0x45A7F0, 1, {}),
        (0x463680, 1, dict(ret=0x3680)), (0x4F2060, 2, dict(effect=slots_get)), (0x413BC0, 0, {}),
        (0x489C60, 2, {}), (0x489E10, 2, {}), (0x463AD0, 3, {}), (0x4637C0, 3, dict(strings=(1,))),
        (0x4E6B00, 2, dict(strings=(1,))),
        (0x4B3320, 1, dict(derefs={0: 24})), (0x4B3100, 1, dict(derefs={0: 24}, effect=normalize))):
    fake(e, addr, n, **kw)
pyfake(e, 0x4F1A80, param_add)
pyfake(e, 0x460C80, printf)
pyfake(e, 0x450D50, ai_message)
float_fake(e, 0x414950, 6)
RUN1 = trap(e, "human_run", logger(e, "human_run", 1))  # called with the task only
CREATE = trap(e, "human_create", logger(e, "human_create", 2))
e.baseline = e.snapshot()


def handlers(em):
    u32(em, HANDLERS + 4 * HUMAN_TYPE, CREATE)
    u32(em, HANDLERS + 4 * (0x180 + HUMAN_TYPE), RUN1)


def test_open():
    run(e, 0x45CAB0, "Soldier_Open", lambda em: None)


def test_create():
    def setup(em):
        handlers(em)
        h, sm = soldier(em)
        return h, 0xA4A4
    run(e, 0x45CC50, "Soldier_CreateHandler", setup)


INPUTS = [
    {}, {2: 1.0, 5: -1.0, 9: 3.0}, {3: NAN, 4: 2.0, 6: 1.0, 7: 1.0, 8: 1.0},
    {21: 1.0, 22: 1.0, 23: 2.5, 25: 1.0, 26: 1.0, 27: 1.0, 29: 1.0, 30: 1.0},
    {23: 3.5, 24: 7.5}, {24: -2.5}, {28: 4.5}, {28: 9.0, 24: 1.0}, {20: 1.0, 31: 1.0, 10: 1.0, 19: 1.0},
]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("inputs", INPUTS)
@pytest.mark.parametrize("flags,run_,old,kind", [(0, 0, 0, 0x24), (0x10100, 1, 0xFFFFFFFF, 3), (0x100, 1, 0x40000, 0)])
def test_update_input(inputs, flags, run_, old, kind, cw):
    def setup(em):
        h, sm = soldier(em, old)
        motion = em.alloc(8)
        u32(em, motion, 0, flags)
        u32(em, h + 0x70, motion)
        for i in range(40):
            f32(em, h + 0x1B0 + 4 * i, inputs.get(i, 0.0))
        u8(em, h + 0xD00, run_)
        RET[0x463110] = kind
        RET[("weapon", 4)] = 0x4444
        RET[("weapon", 9)] = 0x9999
        return sm, h
    run(e, 0x45CCF0, "Soldier_UpdateInput", setup, cw=cw)


@pytest.mark.parametrize("kind", [0, 4, 5, 6, 7, 0x16, 0x17, 0x1A, 0x1C, 0x1D, -3])
def test_run_handler(kind):
    def setup(em):
        handlers(em)
        h, sm = soldier(em)
        ai = em.alloc(0x3E00)
        u32(em, ai + 0x3D38, kind)
        u32(em, h + 0x800, ai)
        return (h,)
    run(e, 0x45CF90, "Soldier_RunHandler", setup)


def test_get_params():
    def setup(em):
        out = em.alloc(4)
        u32(em, 0x5BDAB4, 0xABCD)
        return 0, out
    run(e, 0x45D030, "Soldier_GetParams", setup)


def update_setup(em, attach=0, vz=-3.0, ground=4, d=(1.0, 2.0, 3.0), m=None, inp=0.0):
    h, sm = soldier(em, ground)
    f32(em, h + 0x78, *(m or (0.9, -0.1, 0.2, 0.1, 0.8, 0.3, -0.2, 0.4, 0.7)))
    f32(em, h + 0x66C, vz)
    u32(em, h + 0x76C, attach)
    f32(em, h + 0x200, inp)
    d64(em, h + 0x710, 12345.678)
    f32(em, h + 0x718, 0.75)
    RET["delta"] = d
    return h


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("attach,vz,ground,d,inp", [
    (0, -3.0, 4, (1.0, 2.0, 3.0), 0.0), (0x7777, 2.5, 0, (0.1234, -77.7, 1e-3), 1.0),
    (0, NAN, 0x20004, (3e5, 1.5, -2.25), NAN), (0, -0.0, 4, (0.3, 0.3, 0.3), 0.0)])
def test_update(attach, vz, ground, d, inp, cw):
    run(e, 0x45D080, "Soldier_Update", lambda em: (update_setup(em, attach, vz, ground, d, inp=inp),), cw=cw)


@pytest.mark.parametrize("locked", [0, 1])
def test_input_handler(locked):
    def setup(em):
        h = update_setup(em)
        u8(em, h + 0x250, locked)
        inp = em.alloc(160)
        f32(em, inp, *[i * 0.5 for i in range(40)])
        msg = em.alloc(8)
        u32(em, msg, 0, inp)
        return h, msg
    run(e, 0x45D040, "Soldier_InputHandler", setup)


@pytest.mark.parametrize("held,maxid,have", [
    (0x5000, 5, ()), (0, 5, ()), (0, 5, (3,)), (0, 5, (0, 2, 4)), (0, -1, (0,)), (0, 0, (0,)), (0, 3, (1, 2, 3))])
def test_ensure_weapon(held, maxid, have):
    def setup(em):
        h, sm = soldier(em)
        u32(em, h + 0x4C4, held)
        RET[0x413BC0] = maxid
        for i in have:
            RET[("weapon", i)] = 0x100 + i
        return (h,)
    RET.clear()
    run(e, 0x45D360, "Soldier_EnsureWeapon", setup)


HITS = [
    # ai, invulnerable, headshot, zone, damage, total, max, hitsum, kind
    (0, 0, 0, 3, 10.0, 5.0, 100.0, 0.0, 1),
    (1, 0, 0, 3, 10.0, 5.0, 100.0, 0.3, 3),
    (1, 0, 1, 15, 10.0, 5.0, 100.0, 0.0, 1),
    (1, 0, 1, 16, 10.0, 95.0, 100.0, 0.0, 2),
    (1, 0, 0, 16, 10.0, 95.0, 100.0, 0.0, 3),
    (1, 1, 0, 3, 10.0, 5.0, 100.0, 0.0, 1),
    (0, 0, 0, 5, 0.33, 0.0, 100.0, 0.0, 1),
    (0, 0, 0, 5, 0.1, 0.0, 100.0, 0.23, 1),
    (0, 0, 0, 5, 1e-8, 0.0, NAN, 0.33, 1),
    (0, 0, 0, 5, 0.1, 99.99, 100.0, 0.0, 2),
]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("ai,invul,headshot,zone,damage,total,max_,hitsum,kind", HITS)
def test_hit(ai, invul, headshot, zone, damage, total, max_, hitsum, kind, cw):
    def setup(em):
        h, sm = soldier(em, 0x4)
        a = 0
        if ai:
            a = em.alloc(0x3E00)
            u8(em, a + 0x3ABC, invul, headshot)
        u32(em, h + 0x800, a)
        f32(em, h + 0x254, total, max_, 0.0)
        for i in range(17):
            f32(em, h + 0x260 + 8 * i, 1.0 + i, 0.5 + 0.25 * i)
        f32(em, h + 0xD04, float.fromhex("0x1.51eb86p-2"), hitsum)
        weapon = em.alloc(0x700)
        u32(em, weapon + 0x610, kind)
        hit = em.alloc(28)
        u32(em, hit, 0x77)
        f32(em, hit + 4, damage, 0.25, -0.5, 0.75)
        u32(em, hit + 0x14, 0xA77, weapon)
        RET[0x489C60] = zone
        return h, hit
    run(e, 0x45D3C0, "Soldier_HitHandler", setup, cw=cw)


def test_close():
    def setup(em):
        u32(em, 0x5BDAB4, 0xAB4)
        u32(em, 0x5BDABC, 0xFFFF0042)
    run(e, 0x45D580, "Soldier_Close", setup)


def test_get_type():
    def setup(em):
        u32(em, 0x5BDABC, 0xFFFF0042)
    run(e, 0x45D5A0, "Soldier_GetType", setup, check_eax=True, eax_mask=0xFFFF)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("zero,v,m,yaw", [
    (1, (0, 0, 0), (0, 1, 0, 0, 0, 0, 0, 0, 0), 1.0),
    (0, (1.0, 2.0, 0.5), (0, 0.6, 0, 0, 0.8, 0, 0, 0.1, 0), 1.25),
    (0, (-1.0, -2.0, 0.5), (0, 0.6, 0, 0, 0.8, 0, 0, 0.1, 0), -0.5),
    (0, (1.0, 0.0, 0.0), (0, 0.0, 0, 0, 1.0, 0, 0, 0.0, 0), 3.0),
    (0, (0.3, -0.4, 0.1), (0, 0.4, 0, 0, 0.3, 0, 0, 0.3333333, 0), 2.9),
])
def test_turn_to_hit(zero, v, m, yaw, cw):
    def setup(em):
        h, sm = soldier(em)
        f32(em, h + 0x78, *m)
        d = em.alloc(24)
        d64(em, d, *v)
        RET[0x4B3320] = zero
        RET[0x414950] = yaw
        return h, d
    run(e, 0x45DAA0, "Soldier_TurnToHit", setup, cw=cw, check_eax=True, eax_mask=0xFF)
