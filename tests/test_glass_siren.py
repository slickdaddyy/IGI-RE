import struct

import pytest

from emu import Emu
from test_glass_fakes import Fakes, base, check, common, event, i32, new_task, set_handler, u8

ST = 0x54  # Siren type id
ET = event(0x481340)  # ExplodeObject

e = Emu()
fk = Fakes(e)
common(e, fk, {b"Siren": ST})
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)
VALUE = {}
fk.add(0x4F1320, 1)
fk.add(0x4F1340, 1)
fk.add(0x4F1400, 2)
fk.add(0x4F16E0, 2, ret=lambda em, a: VALUE.get("eval", 0.0), st0="d")
fk.add(0x4F1270, 2, ret=lambda em, a: em.write(a[0], b"siren_2\0"), fmt=lambda em, a: (a[1],))
fk.add(0x4B7EE0, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))
fk.add(0x4B80A0, 1, fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x481360, 9)
fk.add(0x4E6B00, 2, ret=lambda em, a: VALUE.get("play", 0), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4E6C00, 1)
e.baseline = e.snapshot()


def sbase(em):
    VALUE.clear()
    base(em)
    em.write(0x57C214, struct.pack("<H", ST))


def siren(em):
    s = new_task(em, ST, 0x2D8)
    em.write(s + 0x238, b"siren_loop\0")
    em.write(s + 0x258, b"siren_broken\0")
    return s


def test_open():
    _, log = check(e, fk, 0x449360, "Siren_Open", sbase)
    assert len(log) == 13


def test_create_delete():
    def setup(em):
        sbase(em)
        set_handler(em, 0, ET, LOG2)
        set_handler(em, 2, ET, LOG1)
        s = new_task(em, ST, 0x2D8)
        em.write(s + 0x238, b"\x55" * 0xA0)
        return s, 0x1234
    check(e, fk, 0x4492A0, "Siren_Create", setup)
    check(e, fk, 0x449330, "Siren_Delete", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("value", [0.0, 0.4, 1.0, 2.6, 256.0])
@pytest.mark.parametrize("exploded", [0, 1])
@pytest.mark.parametrize("sound,broken", [(0, 0), (0x2C80, 0), (0, 0x2CC0), (0x2C80, 0x2CC0)])
@pytest.mark.parametrize("play", [0, 0x6B00])
def test_run(value, exploded, sound, broken, play, cw):
    def setup(em):
        sbase(em)
        set_handler(em, 1, ET, LOG1)
        s = siren(em)
        u8(em, s + 0x190, exploded)
        i32(em, s + 0x2C8, sound)
        i32(em, s + 0x2CC, broken)
        em.write(s + 0x2D0, b"\x01\x00")
        VALUE.update(eval=value, play=play)
        return (s,)
    check(e, fk, 0x4494D0, "Siren_Run", setup, cw=cw)


def test_params():
    def setup(em):
        sbase(em)
        set_handler(em, event(0x4F1A70), ET, LOG2)
        i32(em, 0x57C21C, 0xABCD)
        return siren(em), em.alloc(12)
    check(e, fk, 0x449600, "Siren_GetParamsHandler", setup)
    check(e, fk, 0x449610, "Siren_ParamsChanged", setup)
    check(e, fk, 0x4497B0, "Siren_GetSoundSource", setup)
    check(e, fk, 0x4497D0, "Siren_Close", setup)


@pytest.mark.parametrize("registered", [0, 1])
def test_symbols(registered):
    def setup(em):
        sbase(em)
        set_handler(em, event(0x4F1210), ET, LOG1)
        set_handler(em, event(0x4F1220), ET, LOG1)
        s = siren(em)
        u8(em, s + 0x2D2, registered)
        return (s,)
    check(e, fk, 0x449660, "Siren_AddSymbols", setup)
    check(e, fk, 0x449710, "Siren_RemoveSymbols", setup)
