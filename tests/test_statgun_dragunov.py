import random
import struct

import pytest

from emu import Emu
from test_statgun_fakes import (CWS, Fakes, check, common, event, f32, i32, new_task, set_events,
                                set_handler)

e = Emu()
fk = Fakes(e)
common(e, fk)
fk.add(0x4169D0, 1, ret=lambda em, a: 0x20E00000 + len(em.read_cstr(a[0])) * 0x100)
fk.add(0x4659D0, 1)
fk.add(0x4659E0, 1)
fk.add(0x4B51F0, 15)
SIZE = {"w": 64, "h": 32}
fk.add(0x4B6CE0, 1, ret=lambda em, a: SIZE["w"])
fk.add(0x4B6D00, 1, ret=lambda em, a: SIZE["h"])
DISPLAY = 0x20D00000
fk.add(0x491CF0, 0, ret=DISPLAY)
FIND = fk.add(0x4FE730, 2, ret=0, fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4FE650, 3, ret=0x20C00000, fmt=lambda em, a: (a[0], em.read_cstr(a[1]), a[2]))
fk.add(0x4B0D10, 1)
fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(4))
ARG = {"v": 0}
fk.add(0x4B8A20, 2, ret=lambda em, a: ARG["v"])
BASE = fk.handler(2)


def test_open_close():
    check(e, fk, 0x472D00, "Dragunov_Open", setup=set_events)

    def setup(em):
        em.write(0x5BE358, struct.pack("<H", 0x55))
    check(e, fk, 0x4733E0, "Dragunov_Close", setup=setup)


def test_create():
    def setup(em):
        set_events(em)
        set_handler(em, 0, event(0x477BF0), BASE)
        g = new_task(em, 0x60, 0x1B8)
        i32(em, g + 0x1B0, 7)
        return g, 0x4321
    check(e, fk, 0x472E10, "Dragunov_Create", setup=setup)


def human(em, rng, i7, p7, i14, i15):
    h = em.alloc(0x500)
    em.write(h + 0x110 + 7 * 4, f32(p7))
    em.write(h + 0x1B0 + 7 * 4, f32(i7))
    em.write(h + 0x1B0 + 14 * 4, f32(i14))
    em.write(h + 0x1B0 + 15 * 4, f32(i15))
    v = em.alloc(0x200)
    em.write(v + 0x1E4, f32(rng.choice([0.05, 0.3, 1.0, 1.5, rng.uniform(0.01, 2)]), rng.uniform(0.01, 2)))
    i32(em, h + 0x4EC, v)
    return h


@CWS
@pytest.mark.parametrize("active,zoomed", [(0, 1), (1, 0), (1, 1)])
@pytest.mark.parametrize("i7,p7", [(0, 0), (1, 0), (1, 1), (0, 1)])
@pytest.mark.parametrize("i14,i15", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_use(active, zoomed, i7, p7, i14, i15, cw):
    for seed in range(4):
        def setup(em):
            rng = random.Random(seed)
            set_events(em)
            set_handler(em, event(0x460BF0), event(0x477BF0), BASE)
            info = em.alloc(0x700)
            mf = em.alloc(4)
            em.write(mf, f32(rng.choice([0.04, 0.5, rng.uniform(0.01, 1)])))
            i32(em, info + 0x6E4, mf)
            g = new_task(em, 0x60, 0x1B8)
            i32(em, g + 0x128, info)
            msg = em.alloc(0x30)
            i32(em, msg + 0x14, active)
            i32(em, msg + 0x1C, zoomed)
            em.write(msg + 0x20, f32(rng.uniform(-3, 3), rng.uniform(-3, 3)))
            i32(em, msg + 0x2C, human(em, rng, i7, p7, i14, i15))
            return g, msg
        check(e, fk, 0x472E50, "Dragunov_Use", setup=setup, cw=cw)


@CWS
@pytest.mark.parametrize("zoomed", [0, 1])
@pytest.mark.parametrize("w,h,sw,sh", [(640, 480, 64, 32), (800, 600, 37, 13), (1023, 767, 255, 3)])
def test_render(zoomed, w, h, sw, sh, cw):
    SIZE.update(w=sw, h=sh)

    def setup(em):
        set_events(em)
        set_handler(em, event(0x487190), event(0x477BF0), BASE)
        em.write(DISPLAY, struct.pack("<III", 0, w, h))
        em.write(0x5BE35C, struct.pack("<III", 0x20E01000, 0x20E02000, 0x20E03000))
        g = new_task(em, 0x60, 0x1B8)
        i32(em, g + 0x1B0, zoomed)
        return g, 0x5555
    check(e, fk, 0x473040, "Dragunov_Render", setup=setup, cw=cw)


@CWS
@pytest.mark.parametrize("v", [0, 1, 2, 5, 15, 60, 91, -7])
def test_zoom_arg(v, cw):
    ARG["v"] = v

    def setup(em):
        a = em.alloc(12)
        out = em.alloc(4)
        em.write(a, struct.pack("<III", out, 0x1234, 3))
        return 0, a
    check(e, fk, 0x4732F0, "Gun_ReadZoomArg", setup=setup, cw=cw)


@CWS
@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("old", [0, 1])
@pytest.mark.parametrize("deg", [5.0, 12.5, 33.3])
def test_config(found, old, deg, cw):
    def setup(em):
        var = 0x20C00000 if not found else em.alloc(0x150)
        em.write(var + 0x124, f32(deg))
        fk.rets[FIND] = var if found else 0
        info = em.alloc(0x700)
        em.write(info + 0x664, f32(1, 2))
        i32(em, info + 0x6E4, 0x9990 if old else 0)
        m = em.alloc(8)
        em.write(m, struct.pack("<II", 0x7000, info))
        return 0, m
    check(e, fk, 0x473330, "Dragunov_Config", setup=setup, cw=cw)
