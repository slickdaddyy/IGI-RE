import random
import struct

import pytest

from emu import Emu
from test_haze_util import CWS, Fakes, check, f32, f64, new_task, task_system

e = Emu()
ef = Fakes(e)

fe = Emu()
fk = Fakes(fe)
fk.add(0x4012A0, 3, ret=lambda em, a: em.alloc(0x90), fmt=lambda em, a: [a[0], a[1] & 0xFFFF, a[2]])
fk.add(0x4C7350, 6, deref={2: 24, 3: 24, 4: 24})
fk.add(0x4C6E70, 5, deref={2: 24, 3: 24, 4: 24})
fk.add(0x4C7560, 2)
fk.add(0x4015F0, 1)
fk.add(0x4B2A10, 1, ret=lambda em, a: (sum(x * x for x in struct.unpack("<3f", em.read(a[0], 12)))) ** 0.5,
       st0="f", deref={0: 12})

HAZE_TYPE, VISIBLE, VIS_COUNT, FRAME, FRAME_USED = 0xA5E304, 0xA54694, 0xA5E2FC, 0xA546BC, 0xA5E300


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def dwords(v):
    return struct.unpack("<II", struct.pack("<d", v))


def box(em, lo, hi):
    a, b = em.alloc(24), em.alloc(24)
    em.write(a, f64(*lo))
    em.write(b, f64(*hi))
    return a, b


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("color", [True, False])
def test_new(color, cw):
    def setup(em):
        em.write(HAZE_TYPE, struct.pack("<H", 0x77))
        em.write(0xA44344, struct.pack("<I", 0x4321))
        a, b = box(em, (-100.5, 20.25, 3.0), (300.0, 400.75, 9.125))
        c = em.alloc(12)
        em.write(c, f32(0.2, 0.4, 0.6))
        return (a, b, c if color else 0, fbits(0.03)) + dwords(-12.5) + dwords(40.0)
    check(fe, fk, 0x4D9FC0, "HazeLayer_New", setup=setup, check_eax=True, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("color", [True, False])
def test_set(color, cw):
    def setup(em):
        t = new_task(em, 0x77, 0x90)
        a, b = box(em, (1.1, 2.2, 3.3), (7.7, -8.8, 99.9))
        c = em.alloc(12)
        em.write(c, f32(0.9, 0.8, 0.7))
        return (t, a, b, c if color else 0, fbits(1.5)) + dwords(5.0) + dwords(15.0)
    check(fe, fk, 0x4DA0A0, "HazeLayer_Set", setup=setup, check_eax=False, cw=cw)


def test_delete():
    check(fe, fk, 0x4DA160, "HazeLayer_Delete", setup=lambda em: (new_task(em, 0x77, 0x90),), check_eax=False)


@pytest.mark.parametrize("count,used", [(0, 0), (3, 0), (2, 5)])
def test_get_visible(count, used):
    def setup(em):
        em.write(VISIBLE, struct.pack("<8I", *range(0x100, 0x108)))
        em.write(VIS_COUNT, struct.pack("<I", count))
        em.write(FRAME_USED, struct.pack("<I", used))
        return em.alloc(4), em.alloc(4)
    check(fe, fk, 0x4DA180, "Haze_GetVisible", setup=setup, check_eax=False)


def test_open_close():
    def setup(em):
        task_system(em)
        em.call(0x4D9730)
    check(e, ef, 0x4DA1D0, "HazeLayer_Open", setup=setup, check_eax=False)

    def setup2(em):
        setup(em)
        em.call(0x4DA1D0)
    check(e, ef, 0x4DA2E0, "HazeLayer_Close", setup=setup2, check_eax=False)


@pytest.mark.parametrize("fn,name", [(0x4DA240, "HazeLayer_Push"), (0x4DA270, "HazeLayer_Enter"),
                                     (0x4DA2A0, "HazeLayer_Pop"), (0x4DA2C0, "HazeLayer_Leave")])
@pytest.mark.parametrize("visible", [0, 1])
def test_visibility_handlers(fn, name, visible):
    def setup(em):
        t = new_task(em, 0x77, 0x90)
        em.write(t + 0x8C, struct.pack("<I", visible))
        em.write(VIS_COUNT, struct.pack("<I", 3))
        return (t,)
    check(fe, fk, fn, name, setup=setup, check_eax=False)


def test_reset_frame():
    check(fe, fk, 0x4DA2F0, "Haze_ResetFrame", setup=lambda em: em.write(FRAME_USED, b"\x05\0\0\0"),
          check_eax=False)


def layer(em, rng, bottom, top):
    t = new_task(em, 0x77, 0x90)
    em.write(t + 0x6C, f32(rng.uniform(0.001, 0.2)) + f64(bottom, top)
             + f32(*[rng.random() for _ in range(3)]))
    return t


Z = [-50.0, 0.0, 2.0, 5.0, 10.0, 12.0, 20.0, 22.0, 35.0, 60.0]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(120))
def test_get_fog(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        n = rng.choice([0, 1, 2, 3, 4])
        layers = em.alloc(4 * max(n, 1))
        for i in range(n):
            b = rng.choice(Z[:-1] + [rng.uniform(-60, 40)])
            top = b + rng.choice([5.0, 15.0, rng.uniform(1, 40)])
            if rng.random() < 0.1:
                b, top = top, b
            em.write(layers + 4 * i, struct.pack("<I", layer(em, rng, b, top)))
        fz = rng.choice(Z + [rng.uniform(-70, 70)])
        tz = rng.choice(Z + [rng.uniform(-70, 70), fz])
        xy = (0.0, 0.0) if rng.random() < 0.1 else (rng.uniform(-90, 90), rng.uniform(-90, 90))
        a, b = box(em, (rng.uniform(-90, 90), rng.uniform(-90, 90), fz), (xy[0], xy[1], tz))
        if rng.random() < 0.1:
            em.write(b, em.read(a, 24))
        out = em.alloc(16)
        return out, out + 12, a, b, layers, n
    check(fe, fk, 0x4DA300, "Haze_GetFog", setup=setup, check_eax=False, cw=cw)


# (from z, to z) against layers [0, 20] and [5, 40]; fades need the line to leave through a face
CASES = [(-10.0, 30.0), (30.0, -10.0), (-10.0, 50.0), (17.0, 30.0), (3.0, -10.0), (7.0, 12.0), (-5.0, 8.0),
         (45.0, 18.0), (50.0, 60.0), (-9.0, -1.0), (10.0, 10.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("fz,tz", CASES)
@pytest.mark.parametrize("n", [1, 2])
def test_get_fog_cases(fz, tz, n, cw):
    def setup(em):
        rng = random.Random(int(fz * 7 + tz))
        layers = em.alloc(8)
        em.write(layers, struct.pack("<II", layer(em, rng, 0.0, 20.0), layer(em, rng, 5.0, 40.0)))
        a, b = box(em, (3.0, 4.0, fz), (-6.0, 11.0, tz))
        out = em.alloc(16)
        return out, out + 12, a, b, layers, n
    check(fe, fk, 0x4DA300, "Haze_GetFog", setup=setup, check_eax=False, cw=cw)
