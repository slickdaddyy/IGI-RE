import random
import struct

import pytest

from test_guncasing_util import (CWS, Emu, Fakes, add_child, check, cstr, f32, new_task, rmatrix, rvec, set_handler,
                                 u32)

e = Emu()
fk = Fakes(e)
GT, CT, RT, OT = 0x3E, 0x3F, 0x40, 0x41
LIST, MODEL, RNG, WORLD, OWNER, SHOTS, COLL, TABLE, ENUM = (0x5A000000, 0x5B000000, 0x5C000000, 0x5D000000,
                                                            0x5E000000, 0x5F000000, 0x60000000, 0x61000000,
                                                            0x62000000)
GETTERS = [0x4C48C0, 0x4D9850, 0x4F1A60, 0x4F1A70, 0x416830, 0x416840, 0x416850, 0x416880, 0x4F1FD0, 0x4F1FE0,
           0x4F1FF0, 0x4F2000, 0x4F2010, 0x4F2020, 0x4F2030, 0x4F2040, 0x413B50, 0x413B60, 0x487190, 0x489610,
           0x47B100, 0x460BF0, 0x460C10, 0x4CEA20, 0x4CEA10, 0x47E6F0, 0x4F1E20, 0x4F0FB0]
EV_CHANGED = 0x20 + GETTERS.index(0x4F1A70)
R = {}
s = lambda em, p: em.read_cstr(p)

for i, addr in enumerate(GETTERS):
    fk.add(addr, 0, ret=RT if addr == 0x4C48C0 else 0x20 + i)
fk.add(0x401900, 5, ret=lambda em, a: R.setdefault("types", [GT, CT]).pop(0),
       fmt=lambda em, a: (s(em, a[0]), a[1], a[2] & 0xFFFF, a[3], a[4]))
fk.add(0x4017C0, 1, ret=lambda em, a: R.setdefault("evs", [0x70, 0x71, 0x72, 0x73, 0x74]).pop(0), mask=[0xFFFF])
fk.add(0x401400, 3, mask=[0xFFFF, 0xFF])
fk.add(0x401530, 3, mask=[0xFFFF])
fk.add(0x4012A0, 3, ret=lambda em, a: R["task"], mask=[~0, 0xFFFF])
fk.add(0x4015F0, 1)
fk.add(0x401780, 1)
fk.add(0x401AE0, 1, ret=lambda em, a: a[0] + 0x100)
fk.add(0x401B20, 1)
fk.add(0x413BD0, 1, ret=lambda em, a: R["wt"])
fk.add(0x413DE0, 0, ret=ENUM)
fk.add(0x414E20, 3, fmt=lambda em, a: (a[1], a[2], em.write(a[0], R["point"]))[:2])
fk.add(0x416920, 0, ret=WORLD)
fk.add(0x416D20, 0, ret=RNG)
fk.add(0x47B110, 3, ret=SHOTS)
fk.add(0x47B170, 2)
fk.add(0x47B270, 2, deref={1: 24})
fk.add(0x47CD70, 1, ret=lambda em, a: 0x4400 + a[0])
fk.add(0x4AF810, 2, fmt=lambda em, a: (s(em, a[0]), a[1]))
fk.add(0x4B38E0, 4)
fk.add(0x4B47C0, 2, ret=lambda em, a: R["rand"].pop(0))
fk.add(0x4B8770, 1, ret=lambda em, a: R["id"], fmt=lambda em, a: (s(em, a[0]),))
fk.add(0x4B8810, 2, fmt=lambda em, a: (s(em, a[0]), a[1]))
fk.add(0x4C1790, 2)
fk.add(0x4C17C0, 2)
fk.add(0x4C48D0, 2)
fk.add(0x4C4950, 3)
fk.add(0x4C5710, 3)
fk.add(0x4C6840, 3)
fk.add(0x4C6890, 3)
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4C7560, 2)
fk.add(0x4CEC10, 1, ret=lambda em, a: R["model"], fmt=lambda em, a: (s(em, a[0]),))
fk.add(0x4D0950, 1, ret=lambda em, a: R["radius"], st0="f")
fk.add(0x4D9860, 0, ret=LIST)
fk.add(0x4E0930, 2, ret=COLL)
fk.add(0x4E81F0, 0, ret=lambda em, a: R["view"])
fk.add(0x4F0EE0, 0, ret=OWNER)
fk.add(0x4F1950, 1, ret=TABLE)
fk.add(0x4F1A80, 6, fmt=lambda em, a: (a[0], a[2] - a[1], a[3], a[4], a[5]))
DRAW = fk.handler(2)
RUN = fk.handler(1)
CHANGED = fk.handler(2)
HIDE = fk.handler(2, fmt=lambda em, a: (a[0], e.u32(a[1]), em.write(a[1], u32(R["vis"])))[:2])
e.baseline = e.snapshot()


def weapon(em, rate=600, rounds=30, category=0):
    wt = em.alloc(0x6E8)
    em.write(wt + 0x618, u32(category) + b"rifle01\0")
    em.write(wt + 0x63C, u32(7, 1, rate, rounds, 3))
    em.write(wt + 0x6D8, struct.pack("<H", GT))
    return wt


def gun(em, rng=None):
    rng = rng or random.Random(0)
    owner = new_task(em, OT, 0x40)
    g = new_task(em, GT, 0x1B0)
    add_child(em, owner, g)
    em.write(g + 0x20, rvec(rng))
    em.write(g + 0x70, rmatrix(rng))
    em.write(g + 0x98, f32(*[rng.choice([1.0, rng.uniform(0.5, 2)]) for _ in range(3)]))
    coll = new_task(em, 0x42, 0x40)
    em.write(coll + 0x20, u32(g))
    em.write(g + 0xE8, u32(coll))
    shots = em.alloc(4)
    em.write(shots, u32(SHOTS))
    em.write(g + 0x130, u32(shots))
    return g


def test_getters():
    def setup(em):
        em.write(0x5BE3C6, struct.pack("<H", GT))
        em.write(0x5407B8, bytes([0x70, 0x71, 0x72, 0x73, 0x74]))
    check(e, fk, 0x477BF0, "Gun_GetType", setup, eax_mask=0xFFFF)
    for i in range(5):
        check(e, fk, 0x477C00 + 0x10 * i, "Gun_GetEvent%d" % i, setup, eax_mask=0xFF)


@pytest.mark.parametrize("known,handler", [(1, 1), (0, 1), (1, 0)])
def test_new(known, handler):
    def setup(em):
        em.write(0x5BE3C6, struct.pack("<H", 0x43))
        R.update(wt=weapon(em) if known else 0, task=new_task(em, GT if known else 0x43, 0x1B0))
        if handler:
            set_handler(em, EV_CHANGED, GT if known else 0x43, CHANGED)
        return (0x7000, 0xFFFF8005, 1234 if known else -17, 25, 0x301)
    check(e, fk, 0x477C50, "Gun_New", setup)


@CWS
def test_casing_new(cw):
    def setup(em):
        rng = random.Random(5)
        R.update(task=new_task(em, CT, 0x128), model=MODEL, rand=[1, 4, 3])
        em.write(0x5BE3C4, struct.pack("<H", CT))
        pos, vel, mat = em.alloc(24), em.alloc(12), em.alloc(40)
        em.write(pos, rvec(rng))
        em.write(vel, f32(1, 2, 3))
        em.write(mat, rmatrix(rng))
        return (0x7000, pos, vel, mat, cstr(em, b"shell"))
    check(e, fk, 0x477D10, "GunCasing_New", setup, cw=cw)


def test_open():
    def setup(em):
        R["types"] = [GT, CT]
        R["evs"] = [0x70, 0x71, 0x72, 0x73, 0x74]
    check(e, fk, 0x477E30, "Gun_Open", setup, check_eax=False)


def casing(em, rng, life):
    c = new_task(em, CT, 0x128)
    em.write(c + 0x20, rvec(rng))
    em.write(c + 0x70, rmatrix(rng))
    em.write(c + 0xF0, u32(life) + rmatrix(rng) + f32(*[rng.uniform(-300, 300) for _ in range(3)]))
    return c


def test_casing_handlers():
    def setup(em):
        R["view"] = em.alloc(0x60)
        em.write(R["view"] + 0x5C, u32(R["on"]))
        set_handler(em, 4, RT, DRAW)
        return (casing(em, random.Random(1), 3),)
    for on in (0, 1):
        R["on"] = on
        check(e, fk, 0x478510, "GunCasing_DrawHandler", setup, check_eax=False)
    check(e, fk, 0x478290, "GunCasing_CreateHandler", setup, check_eax=False)
    check(e, fk, 0x4784F0, "GunCasing_DeleteHandler", setup, check_eax=False)


@CWS
@pytest.mark.parametrize("seed", range(20))
def test_casing_run(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        return (casing(em, rng, rng.choice([0, 1, 19])),)
    check(e, fk, 0x4782D0, "GunCasing_RunHandler", setup, check_eax=False, cw=cw)


def test_create_reset():
    def setup(em):
        g = gun(em)
        em.write(g + 0xF0, b"\x55" * 0xC0)
        return (g,)
    check(e, fk, 0x478540, "Gun_CreateHandler", setup, check_eax=False)
    check(e, fk, 0x4785F0, "Gun_Reset", setup, check_eax=False)


@CWS
@pytest.mark.parametrize("seed", range(40))
def test_run(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        g = gun(em, rng)
        em.write(g + 0x128, u32(weapon(em, rng.choice([600, 700, 333, 1, 950]), rng.randrange(40))))
        em.write(g + 0xFC, bytes([rng.random() < 0.1]))
        em.write(g + 0x144, u32(rng.choice([-2, 5])))
        em.write(g + 0x13C, u32(rng.choice([0, 3])))
        em.write(g + 0x198, f32(rng.choice([0.0, -0.0, 1.5, -1.5, 0.001, -0.001, 60.0, -60.0, float("nan")])))
        em.write(g + 0x19C, u32(*[rng.choice([0, 0, 1, 5]) for _ in range(4)]))
        coll = em.u32(g + 0xE8)
        for i in range(rng.randrange(3)):
            c = new_task(em, 0x50 + i, 0x40)
            add_child(em, coll, c)
            set_handler(em, 1, 0x50 + i, RUN)
        return (g,)
    check(e, fk, 0x478640, "Gun_RunHandler", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("seed", range(6))
def test_delete(seed):
    def setup(em):
        rng = random.Random(seed)
        g = gun(em, rng)
        em.write(g + 0xF0, bytes([rng.choice([0, 1])]))
        if rng.random() < 0.5:
            em.write(em.u32(g + 0x130), u32(0))
        sound = rng.choice([0, 1, 2])
        if sound:
            p = em.alloc(4)
            em.write(p, u32(0x7800 if sound == 2 else 0))
            em.write(g + 0x194, u32(p))
        em.write(g + 0x190, u32(rng.choice([0, 0x7900])))
        return (g,)
    check(e, fk, 0x478840, "Gun_DeleteHandler", setup, check_eax=False)


@pytest.mark.parametrize("handler,vis", [(0, 1), (1, 0), (1, 3)])
def test_hide(handler, vis):
    def setup(em):
        R["vis"] = vis
        em.write(0x5407B8, bytes([0x70, 0x71, 0x72, 0x73, 0x74]))
        g = gun(em)
        em.write(g + 0xEC, u32(9))
        if handler:
            set_handler(em, 0x71, OT, HIDE)
        h = em.alloc(4)
        em.write(h, u32(0x66))
        return (g, h)
    check(e, fk, 0x478900, "Gun_HideHandler", setup, check_eax=False)


@pytest.mark.parametrize("rounds,category", [(0, 5), (0, 1), (3, 5)])
def test_draw(rounds, category):
    def setup(em):
        g = gun(em)
        em.write(g + 0x128, u32(weapon(em, category=category)))
        em.write(g + 0x144, u32(rounds))
        set_handler(em, 4, RT, DRAW)
        return (g,)
    check(e, fk, 0x478970, "Gun_DrawHandler", setup, check_eax=False)


def test_param_table_state():
    def setup(em):
        em.write(0x5BE3C0, u32(TABLE))
        return (gun(em), em.alloc(4))
    check(e, fk, 0x4789B0, "Gun_GetParamTableHandler", setup, check_eax=False)
    check(e, fk, 0x478A80, "Gun_GetStateHandler", setup, check_eax=False)


@pytest.mark.parametrize("known,model,coll", [(0, 0, 1), (1, 0, 1), (1, 1, 1), (1, 1, 0)])
def test_param_changed(known, model, coll):
    def setup(em):
        g = gun(em)
        em.write(g + 0x104, b"WEAPON_ID_AK47\0")
        em.write(g + 0x124, struct.pack("<h", -3))
        if not coll:
            em.write(g + 0xE8, u32(0))
        R.update(id=1234, wt=weapon(em) if known else 0, model=MODEL if model else 0)
        return (g,)
    check(e, fk, 0x4789C0, "Gun_ParamChangedHandler", setup, check_eax=False)


@CWS
@pytest.mark.parametrize("seed", range(8))
def test_move(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        g = gun(em, rng)
        em.write(g + 0xF0, bytes([seed & 1]))
        R.update(radius=rng.uniform(100, 900), point=rvec(rng, 100))
        pos, mat, msg = em.alloc(24), em.alloc(40), em.alloc(12)
        em.write(pos, rvec(rng))
        em.write(mat, rmatrix(rng))
        em.write(msg, u32(pos, mat, 0))
        return (g, msg)
    check(e, fk, 0x478A90, "Gun_MoveHandler", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("in_world", [0, 1])
def test_remove_from_world(in_world):
    def setup(em):
        g = gun(em)
        em.write(g + 0xF0, bytes([in_world]))
        return (g,)
    check(e, fk, 0x4788D0, "Gun_RemoveFromWorld", setup, check_eax=False)
