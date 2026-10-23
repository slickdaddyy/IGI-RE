import struct

import pytest

from emu import Emu
from test_statgun_fakes import Fakes, check, common, event, i32, new_task, set_events, set_handler

e = Emu()
fk = Fakes(e)
common(e, fk)
fk.add(0x489F20, 4)
ANIM_DONE = fk.add(0x489F50, 1, ret=1)
AMMO = {}
fk.add(0x47CEE0, 2, ret=lambda em, a: AMMO.get("p", 0))


def set_list(em, a):
    i32(em, a[1], 0x1234)


AMMO_EV = fk.add(fk.code(b"\xc3"), 2, effect=set_list, fmt=lambda em, a: (a[0],))


def test_open_close():
    check(e, fk, 0x472A70, "Spas12_Open", setup=set_events)

    def setup(em):
        em.write(0x5BE350, struct.pack("<H", 0x55))
    check(e, fk, 0x472CF0, "Spas12_Close", setup=setup)


@pytest.mark.parametrize("state", [0, 1, 2, 3, 4])
@pytest.mark.parametrize("count", [0, 1, 5, None])
@pytest.mark.parametrize("need", [0, 1, 3])
@pytest.mark.parametrize("done", [0, 1])
def test_reload(state, count, need, done):
    if count is None and state == 2 and done:
        return  # the original reads through the missing ammo entry
    fk.rets[ANIM_DONE] = done

    def setup(em):
        set_events(em)
        owner = new_task(em, 0x70, 0x40)
        set_handler(em, event(0x47CDC0), 0x70, AMMO_EV)
        info = em.alloc(0x700)
        i32(em, info + 0x648, 6)
        idp = em.alloc(4)
        i32(em, idp, 9)
        g = new_task(em, 0x60, 0x1B0)
        i32(em, g + 0x14, owner)
        i32(em, g + 0x128, info)
        i32(em, g + 0x12C, idp)
        i32(em, g + 0x144, 6 - need)
        i32(em, g + 0x1AC, state)
        AMMO["p"] = 0
        if count is not None:
            a = em.alloc(8)
            em.write(a, struct.pack("<II", 9, count))
            AMMO["p"] = a
        msg = em.alloc(0x24)
        i32(em, msg, 0x20004000)
        em.write(msg + 0x20, b"\x77")
        return g, msg
    check(e, fk, 0x472AE0, "Spas12_Reload", setup=setup)


def test_create():
    base = fk.handler(2)

    def setup(em):
        set_events(em)
        set_handler(em, 0, event(0x477BF0), base)
        return (new_task(em, 0x60, 0x1B0),)
    check(e, fk, 0x472CA0, "Spas12_Create", setup=setup)
