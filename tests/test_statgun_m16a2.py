import random
import struct

import pytest

from emu import Emu
from test_statgun_fakes import (CWS, Fakes, check, common, event, f32, i32, new_task, rmatrix, set_events,
                                set_handler)

e = Emu()
fk = Fakes(e)
common(e, fk)
BASE = fk.handler(2)


def set_matrix(em, a):
    em.write(a[0], rmatrix(random.Random(em.read(a[1], 12))))


fk.add(0x414A60, 2, effect=set_matrix, fmt=lambda em, a: (em.read(a[1], 12),))
fk.add(0x4F0EE0, 0, ret=0x4F0EE0)
SOUND = fk.add(0x4E6B00, 2, ret=0x20B00000)
fk.add(0x4E6C30, 2)
fk.add(0x476670, 9, deref={4: 0x24, 5: 12})
fk.add(0x491CF0, 0, ret=0x20D00000)
fk.add(0x47CDA0, 1, ret=0x33)
AMMO = {"p": 0}
fk.add(0x47CEE0, 2, ret=lambda em, a: AMMO["p"])


def set_xy(em, a):
    em.write(a[0], f32(random.Random(a[2]).uniform(100, 600)))
    em.write(a[1], f32(random.Random(a[3]).uniform(100, 600)))


fk.add(0x47AAB0, 4, effect=set_xy, fmt=lambda em, a: (a[2], a[3]))
fk.add(0x4B6E90, 2, fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4B7DF0, 1, ret=37)
fk.add(0x4B6F50, 1, fmt=lambda em, a: (a[0], em.read(a[0], 12)))
fk.add(0x4B6CE0, 1, ret=19)
fk.add(0x4B4E00, 9)
REALS = [1.25, -3.5, 1e-3]
fk.add(0x4B8A50, 2, st0="d", ret=lambda em, a: REALS[a[1] % 3])
VARS = {}
fk.add(0x4FE730, 2, ret=lambda em, a: VARS.get(em.read_cstr(a[1]), 0), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))


def add_var(value):
    def ret(em, a):
        v = em.alloc(0x150)
        if isinstance(value, bytes):
            em.write(v + 0x124, value + b"\0")
        else:
            em.write(v + 0x124, a[2].to_bytes(4, "little"))
        return v
    return ret


fk.add(0x4FE650, 3, ret=add_var(0), fmt=lambda em, a: (a[0], em.read_cstr(a[1]), a[2]))
fk.add(0x4FE6B0, 3, ret=add_var(b"default"), fmt=lambda em, a: (a[0], em.read_cstr(a[1]), em.read_cstr(a[2])))
fk.add(0x4B0D10, 1)
fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(a[0]))


def test_open_close():
    check(e, fk, 0x4733F0, "M16A2_Open", setup=set_events)

    def setup(em):
        em.write(0x5BE36C, struct.pack("<H", 0x55))
    check(e, fk, 0x473980, "M16A2_Close", setup=setup)


def test_create():
    def setup(em):
        set_events(em)
        set_handler(em, 0, event(0x477BF0), BASE)
        return new_task(em, 0x60, 0x1B0), 0x4321
    check(e, fk, 0x4734D0, "M16A2_Create", setup=setup)


@CWS
@pytest.mark.parametrize("alt", [0, 1])
@pytest.mark.parametrize("snd", [0, 0x20B00000])
@pytest.mark.parametrize("seed", range(4))
def test_use(alt, snd, seed, cw):
    fk.rets[SOUND] = snd

    def setup(em):
        rng = random.Random(seed)
        set_events(em)
        set_handler(em, event(0x460BF0), event(0x477BF0), BASE)
        d = em.alloc(0x1C)
        em.write(d, f32(rng.uniform(50, 900), rng.uniform(1, 9), rng.uniform(0.5, 3)) + b"108_05_1\0")
        info = em.alloc(0x700)
        i32(em, info + 0x6E4, d)
        g = new_task(em, 0x60, 0x1B0)
        i32(em, g + 0x14, 0x20A00000)
        i32(em, g + 0x128, info)
        em.write(g + 0x150, struct.pack("<3d", *[rng.uniform(-900, 900) for _ in range(3)]))
        em.write(g + 0x168, f32(*[rng.uniform(-1, 1) for _ in range(3)]))
        msg = em.alloc(0x30)
        i32(em, msg + 0x18, alt)
        return g, msg
    check(e, fk, 0x473510, "M16A2_Use", setup=setup, cw=cw)


@CWS
@pytest.mark.parametrize("has_fn", [0, 1])
@pytest.mark.parametrize("count", [None, 0, 7, 12345])
def test_render(has_fn, count, cw):
    def set_list(em, a):
        i32(em, a[1], 0x1234)
    ammo_ev = fk.add(fk.code(b"\xc3"), 2, effect=set_list, fmt=lambda em, a: (a[0],))

    def setup(em):
        set_events(em)
        set_handler(em, event(0x487190), event(0x477BF0), BASE)
        set_handler(em, event(0x47CDC0), 0x70, ammo_ev if has_fn else 0)
        owner = new_task(em, 0x70, 0x40)
        g = new_task(em, 0x60, 0x1B0)
        i32(em, g + 0x14, owner)
        AMMO["p"] = 0
        if count is not None:
            AMMO["p"] = em.alloc(8)
            em.write(AMMO["p"], struct.pack("<II", 0x33, count))
        hud = em.alloc(0x500)
        text = em.alloc(0x40)
        i32(em, hud + 0x4E0, text)
        msg = em.alloc(8)
        i32(em, msg, hud)
        return g, msg
    check(e, fk, 0x473620, "M16A2_Render", setup=setup, cw=cw)


@CWS
def test_read_args(cw):
    def setup(em):
        a = em.alloc(12)
        out = em.alloc(16)
        em.write(a, struct.pack("<III", out, 0x1234, 5))
        return 0, a
    check(e, fk, 0x473790, "M16A2_ReadArgs", setup=setup, cw=cw)

    def setup2(em):
        return 0, em.alloc(4)
    check(e, fk, 0x4737E0, "M16A2_GetDataSize", setup=setup2)


@pytest.mark.parametrize("found", [0, 1, 2])
@pytest.mark.parametrize("old", [0, 1])
def test_config(found, old):
    def setup(em):
        VARS.clear()
        names = [b"M203 - Muzzle velocity (km/h)", b"M203 - Explosion radius (meters)",
                 b"M203 - Activate radius (meters)", b"M203 - Model"]
        for i, n in enumerate(names):
            if found == 2 or (found == 1 and i % 2):
                v = em.alloc(0x150)
                em.write(v + 0x124, b"m_%d\0" % i if i == 3 else f32(i * 1.5 + 0.25))
                VARS[n] = v
        info = em.alloc(0x700)
        i32(em, info + 0x6E4, 0x9990 if old else 0)
        m = em.alloc(8)
        em.write(m, struct.pack("<II", 0x7000, info))
        return 0, m
    check(e, fk, 0x4737F0, "M16A2_Config", setup=setup)
