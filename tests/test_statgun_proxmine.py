import random
import struct

import pytest

from emu import Emu
from test_statgun_fakes import (CWS, Fakes, check, common, event, f32, f64, i32, masked, new_task, rmatrix, rvec,
                                set_events, set_handler)

e = Emu()
fk = Fakes(e)
common(e, fk)
BASE = fk.handler(2)
BASE1 = fk.handler(1)
RADIUS = fk.add(0x4D0950, 1, st0="f", ret=1.75)
fk.add(0x416920, 0, ret=0x416920)
fk.add(0x4C6940, 4, deref={2: 24})
HIT = {}


def coll(em, a):
    h = HIT
    if not h:
        return
    p = a[1]
    i32(em, p, h["task"])
    em.write(p + 8, f64(*h["pos"]))
    em.write(p + 0x38, f32(*h["normal"]))
    i32(em, p + 0x58, h["found"])


fk.add(0x4CC2A0, 8, effect=coll, fmt=lambda em, a: (a[0], em.read(a[2], 24), em.read(a[3], 24), a[4], a[5] & 0xFF,
                                                     em.read(a[6], 20), a[7]))
fk.add(0x401CF0, 2, ret=lambda em, a: int((a[0] & 0xFFFF) == (a[1] & 0xFFFF)), fmt=masked(0xFFFF, 0xFFFF))
fk.add(0x414A60, 2, effect=lambda em, a: em.write(a[0], rmatrix(random.Random(em.read(a[1], 12)))),
       fmt=lambda em, a: (em.read(a[1], 12),))
fk.add(0x4F0EE0, 0, ret=0x4F0EE0)
fk.add(0x476040, 9, deref={1: 24, 2: 0x28}, fmt=lambda em, a: (a[0], em.read(a[1], 24), em.read(a[2], 0x24),
                                                               em.read_cstr(a[3]), *a[4:8], a[8] & 0xFF))
REALS = [1.25, -3.5, 1e-3, 77.0]
fk.add(0x4B8A50, 2, st0="d", ret=lambda em, a: REALS[a[1] % 4])
VARS = {}
fk.add(0x4FE730, 2, ret=lambda em, a: VARS.get(em.read_cstr(a[1]), 0), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))


def add_real(em, a):
    v = em.alloc(0x150)
    i32(em, v + 0x124, a[2])
    return v


fk.add(0x4FE650, 3, ret=add_real, fmt=lambda em, a: (a[0], em.read_cstr(a[1]), a[2]))
fk.add(0x4B0D10, 1)
fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(a[0]))


def test_open_close():
    check(e, fk, 0x475380, "ProxMine_Open", setup=set_events)

    def setup(em):
        em.write(0x5BE394, struct.pack("<H", 0x55))
    check(e, fk, 0x475AB0, "ProxMine_Close", setup=setup)


def test_create_delete():
    def setup(em):
        set_events(em)
        set_handler(em, 0, event(0x477BF0), BASE)
        set_handler(em, 2, event(0x477BF0), BASE1)
        return new_task(em, 0x60, 0x1B0), 0x4321
    check(e, fk, 0x475460, "ProxMine_Create", setup=setup)
    check(e, fk, 0x4754A0, "Gun_Delete", setup=setup)


@CWS
@pytest.mark.parametrize("dyn", [0, 1])
@pytest.mark.parametrize("seed", range(10))
def test_move(dyn, seed, cw):
    def setup(em):
        rng = random.Random(seed)
        g = new_task(em, 0x60, 0x1B0)
        em.write(g + 0xF0, bytes([dyn]))
        i32(em, g + 0x6C, 0x6C6C)
        sc = [rng.choice([0.5, 1.0, 2.0, rng.uniform(0.1, 3)]) for _ in range(3)]
        em.write(g + 0x98, f32(*sc))
        msg = em.alloc(0x80)
        em.write(msg + 8, rvec(rng) + rmatrix(rng))
        em.write(msg, struct.pack("<II", msg + 8, msg + 0x20))
        return g, msg
    check(e, fk, 0x4754C0, "ProxMine_Move", setup=setup, cw=cw)


CASES = [
    dict(fire=0),
    dict(fire=1, last=1),
    dict(fire=1, found=0),
    dict(fire=1, task=None),
    dict(fire=1, task=None, normal=(0, 0, 0)),
    dict(fire=1, task="human"),
    dict(fire=1, task="humanpart"),
    dict(fire=1, task="glass"),
    dict(fire=1, task="door"),
    dict(fire=1, task="vehicle"),
    dict(fire=1, task="other", rounds=0),
    dict(fire=1, task="other", eye=0, view=0, alt=1),
]
TYPES = {"human": 0x460BE0, "glass": 0x449D20, "door": 0x447D20, "vehicle": 0x4F45F0}


@CWS
@pytest.mark.parametrize("case", range(len(CASES)))
@pytest.mark.parametrize("seed", range(3))
def test_use(case, seed, cw):
    c = CASES[case]

    def setup(em):
        rng = random.Random(seed * 100 + case)
        set_events(em)
        d = em.alloc(16)
        em.write(d, f32(rng.uniform(0, 5), rng.uniform(0, 1), rng.uniform(0, 3), rng.uniform(0, 3)))
        info = em.alloc(0x700)
        em.write(info + 0x61C, b"WEAPON_KEY\0")
        em.write(info + 0x650, f32(rng.uniform(1, 9)))
        i32(em, info + 0x6E4, d)
        g = new_task(em, 0x60, 0x1B0)
        i32(em, g + 0x14, 0x20A00000)
        i32(em, g + 0x128, info)
        em.write(g + 0x70, rmatrix(rng))
        em.write(g + 0x150, rvec(rng))
        em.write(g + 0x134, bytes([c.get("last", 0)]))
        i32(em, g + 0x144, c.get("rounds", 5))
        msg = em.alloc(0x30)
        em.write(msg, bytes([c["fire"], 0]))
        if c.get("eye", 1):
            eye = em.alloc(24)
            em.write(eye, rvec(rng))
            i32(em, msg + 4, eye)
        if c.get("view", 1):
            view = em.alloc(0x28)
            em.write(view, rmatrix(rng))
            i32(em, msg + 8, view)
        i32(em, msg + 0x18, c.get("alt", 0) * 0x101)
        HIT.clear()
        t = c.get("task")
        tp = 0
        if t:
            tp = new_task(em, 0x99, 0x40)
            if t == "humanpart":
                parent = new_task(em, event(TYPES["human"]), 0x40)
                i32(em, tp + 0x14, parent)
            elif t in TYPES:
                em.write(tp + 0x1C, struct.pack("<H", event(TYPES[t])))
        HIT.update(task=tp, found=c.get("found", 1), pos=[rng.uniform(-99, 99) for _ in range(3)],
                   normal=c.get("normal", [rng.uniform(-2, 2) for _ in range(3)]))
        return g, msg
    _, log = check(e, fk, 0x4755A0, "ProxMine_Use", setup=setup, cw=cw)
    placed = c["fire"] and not c.get("last") and c.get("found", 1) and c.get("task") in (None, "other")
    assert any(k == 0x476040 for k, _ in log) == bool(placed)


@CWS
def test_read_args(cw):
    def setup(em):
        a = em.alloc(12)
        out = em.alloc(16)
        em.write(a, struct.pack("<III", out, 0x1234, 5))
        return 0, a
    check(e, fk, 0x475900, "ProxMine_ReadArgs", setup=setup, cw=cw)


@pytest.mark.parametrize("found", [0, 1, 2])
@pytest.mark.parametrize("old", [0, 1])
def test_config(found, old):
    def setup(em):
        VARS.clear()
        names = [b"Proximitymine - Time before arming (seconds)", b"Proximitymine - Countdown time (seconds)",
                 b"Proximitymine - Activate radius (meters)", b"Proximitymine - Explosion radius (meters)"]
        for i, n in enumerate(names):
            if found == 2 or (found == 1 and i % 2):
                v = em.alloc(0x150)
                em.write(v + 0x124, f32(i * 1.5 + 0.25))
                VARS[n] = v
        info = em.alloc(0x700)
        i32(em, info + 0x6E4, 0x9990 if old else 0)
        m = em.alloc(8)
        em.write(m, struct.pack("<II", 0x7000, info))
        return 0, m
    check(e, fk, 0x475960, "ProxMine_Config", setup=setup)
