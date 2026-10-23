from emu import Emu
from test_statgun_fakes import Fakes, check, common, event, new_task, set_events, set_handler

e = Emu()
fk = Fakes(e)
common(e, fk)
BASE = fk.handler(2)
fk.add(0x481350, 0, ret=0x481350)
fk.add(0x4C1800, 1, ret=0x4C1800)
fk.add(0x4168B0, 0, ret=0x4168B0)
fk.add(0x476030, 0, ret=0x476030)
fk.add(0x4168F0, 0, ret=0x4168F0)
fk.add(0x4C1790, 2)
fk.add(0x481360, 9)


def test_open():
    check(e, fk, 0x475AC0, "Mine_Open", setup=set_events)


def test_create():
    def setup(em):
        set_events(em)
        set_handler(em, 0, event(0x481340), BASE)
        m = new_task(em, 0x60, 0x250)
        em.write(m + 0x238, b"\x55" * 0x18)
        return m, 0x4321
    check(e, fk, 0x475C10, "Mine_Create", setup=setup)
