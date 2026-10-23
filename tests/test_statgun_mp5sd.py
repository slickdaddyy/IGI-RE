import random
import struct

import pytest

from emu import Emu
from test_statgun_fakes import CWS, Fakes, check, common, event, f32, i32, new_task, set_events, set_handler

e = Emu()
fk = Fakes(e)
common(e, fk)
BASE = fk.handler(2)
BASE1 = fk.handler(1)
FIND = fk.add(0x4FE730, 2, ret=0, fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4FE650, 3, ret=0x20C00000, fmt=lambda em, a: (a[0], em.read_cstr(a[1]), a[2]))
fk.add(0x4B0D10, 1)
fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(4))


def test_open_close():
    check(e, fk, 0x4739D0, "Mp5sd_Open", setup=set_events)

    def setup(em):
        em.write(0x5BE374, struct.pack("<H", 0x55))
    check(e, fk, 0x473D20, "Mp5sd_Close", setup=setup)


def test_create_unzoom():
    def setup(em):
        set_events(em)
        set_handler(em, 0, event(0x477BF0), BASE)
        set_handler(em, event(0x4F2030), event(0x477BF0), BASE1)
        g = new_task(em, 0x60, 0x1B8)
        i32(em, g + 0x1B0, 7)
        return g, 0x4321
    check(e, fk, 0x473AB0, "Mp5sd_Create", setup=setup)
    check(e, fk, 0x473990, "Mp5sd_Unzoom", setup=setup)


@CWS
@pytest.mark.parametrize("active,zoomed", [(0, 1), (1, 0), (1, 1)])
@pytest.mark.parametrize("i7,p7", [(0, 0), (1, 0), (1, 1)])
@pytest.mark.parametrize("seed", range(6))
def test_use(active, zoomed, i7, p7, seed, cw):
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
        i32(em, g + 0x1B0, zoomed)
        h = em.alloc(0x500)
        em.write(h + 0x110 + 7 * 4, f32(p7))
        em.write(h + 0x1B0 + 7 * 4, f32(i7))
        v = em.alloc(0x200)
        em.write(v + 0x1E4, f32(rng.choice([0.03, 0.3, 0.99, 1.0, 1.5, rng.uniform(0.01, 2)]), rng.uniform(0.01, 2)))
        i32(em, h + 0x4EC, v)
        msg = em.alloc(0x30)
        i32(em, msg + 0x14, active)
        i32(em, msg + 0x2C, h)
        return g, msg
    check(e, fk, 0x473AF0, "Mp5sd_Use", setup=setup, cw=cw)


@CWS
@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("old", [0, 1])
@pytest.mark.parametrize("deg", [5.0, 50.0, 33.3])
def test_config(found, old, deg, cw):
    def setup(em):
        var = 0x20C00000 if not found else em.alloc(0x150)
        em.write(var + 0x124, f32(deg))
        fk.rets[FIND] = var if found else 0
        info = em.alloc(0x700)
        i32(em, info + 0x6E4, 0x9990 if old else 0)
        m = em.alloc(8)
        em.write(m, struct.pack("<II", 0x7000, info))
        return 0, m
    check(e, fk, 0x473C80, "Mp5sd_Config", setup=setup, cw=cw)
