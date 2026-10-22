import random
import struct

import pytest

from emu import Emu, compare
from test_glass_fakes import (TICK, Fakes, base, check, common, cstr, event, f32, f64, i32, new_task, rmatrix, rvec,
                              set_handler, u8)

TT = 0x51  # GenericTBA type id
ET = event(0x481340)  # ExplodeObject

e = Emu()
fk = Fakes(e)
common(e, fk, {b"GenericTBA": TT})
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)
VALUE = {}
fk.add(0x4F1320, 1)
fk.add(0x4F1340, 1)
fk.add(0x4F1400, 2)
fk.add(0x4F1050, 1, ret=lambda em, a: VALUE.get("id", 0))
fk.add(0x4F16C0, 1, ret=lambda em, a: VALUE.get("expr", 0))
fk.add(0x4F16E0, 2, ret=lambda em, a: VALUE.get("eval", 0.0), st0="d")
fk.add(0x4F1270, 2, ret=lambda em, a: em.write(a[0], b"tba_3\0"), fmt=lambda em, a: (a[1],))
fk.add(0x4B7EE0, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))
fk.add(0x4B80A0, 1, fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4C1790, 2)
fk.add(0x4C17C0, 2)
fk.add(0x481360, 9)
fk.add(0x4B0D10, 1)
fk.add(0x4871C0, 2)
fk.add(0x414E20, 3, ret=lambda em, a: em.write(a[0], f64(*VALUE["point"])), fmt=lambda em, a: a[1:])
NAMES = [cstr(e, b"icon_%d" % i) for i in range(10)]
e.baseline = e.snapshot()


def tbase(em):
    VALUE.clear()
    base(em)
    em.write(0x53C7DC, struct.pack("<H", TT))
    for i, n in enumerate(NAMES):
        i32(em, 0x5C8A4C + 4 * i, n)


@pytest.mark.parametrize("v", [0, 1, 0x80, 0xFF])
def test_door_toggle(v):
    def setup(em):
        p = em.alloc(0x2A0)
        u8(em, p + 0x290, v)
        return (p,)
    compare(e, 0x447DA0, "Door_Toggle", setup=setup)


def test_open():
    _, log = check(e, fk, 0x447DB0, "GenericTBA_Open", tbase)
    assert len(log) == 19


def test_create():
    def setup(em):
        tbase(em)
        set_handler(em, 0, ET, LOG2)
        t = new_task(em, TT, 0x2A8)
        em.write(t + 0x238, b"\x55" * 0x70)
        return t, 0x1234
    check(e, fk, 0x447FF0, "GenericTBA_Create", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("unk,exploded,expr,value", [(1, 1, 1, 1.0), (0, 0, 0, 1.0), (1, 0, 1, 0.5), (0, 0, 1, 1.5),
                                                     (0, 0, 1, 257.0), (0, 0, 1, -2.5)])
@pytest.mark.parametrize("use,fin", [(TICK, TICK - 3), (TICK - 2, TICK - 2), (TICK - 3, TICK + 5)])
def test_run(unk, exploded, expr, value, use, fin, cw):
    def setup(em):
        tbase(em)
        set_handler(em, 1, ET, LOG1)
        t = new_task(em, TT, 0x2A8)
        u8(em, t + 0x190, exploded)
        em.write(t + 0x238, bytes([unk, 1, 0, 0]) + struct.pack("<i", use) + b"\x07\x01\x01\x00" +
                 struct.pack("<i", fin))
        VALUE.update(expr=expr, eval=value)
        return (t,)
    check(e, fk, 0x4480B0, "GenericTBA_Run", setup, cw=cw)


@pytest.mark.parametrize("icon", [0, 1])
@pytest.mark.parametrize("id_,handler", [(-1, True), (0, True), (3, False)])
def test_delete(icon, id_, handler):
    def setup(em):
        tbase(em)
        set_handler(em, 2, ET, LOG1)
        set_handler(em, event(0x4F1220), TT, LOG1 if handler else 0)
        VALUE.update(id=id_)
        t = new_task(em, TT, 0x2A8)
        i32(em, t + 0x250, 0x2500 if icon else 0)
        return (t,)
    check(e, fk, 0x448160, "GenericTBA_Delete", setup)


@pytest.mark.parametrize("name", [None, b"icon_0", b"icon_4", b"icon_9", b"icon_", b"icon_10", b"", b"other"])
def test_params(name):
    def setup(em):
        tbase(em)
        i32(em, 0x57C1FC, 0xABCD)
        return 0, em.alloc(4)
    check(e, fk, 0x4481F0, "GenericTBA_GetParamsHandler", setup)

    def setup2(em):
        tbase(em)
        set_handler(em, event(0x4F1A70), ET, LOG2)
        t = new_task(em, TT, 0x2A8)
        i32(em, t + 0x254, 0x7777)
        if name is not None:
            i32(em, t + 0x250, cstr(em, name))
        return t, 0x99
    check(e, fk, 0x448200, "GenericTBA_ParamsChanged", setup2)


@pytest.mark.parametrize("registered", [0, 1])
def test_symbols(registered):
    def setup(em):
        tbase(em)
        set_handler(em, event(0x4F1210), ET, LOG1)
        set_handler(em, event(0x4F1220), ET, LOG1)
        t = new_task(em, TT, 0x2A8)
        u8(em, t + 0x248, registered)
        return (t,)
    check(e, fk, 0x4482D0, "GenericTBA_AddSymbols", setup)
    check(e, fk, 0x4483F0, "GenericTBA_RemoveSymbols", setup)


def test_small():
    def setup(em):
        tbase(em)
        t = new_task(em, TT, 0x2A8)
        i32(em, t + 0x254, 0x2540)
        em.write(t + 0x24A, struct.pack("<hf", -1234, 3.75))
        p = em.alloc(4)
        i32(em, p, 0x7000)
        return t, p
    check(e, fk, 0x4484F0, "GenericTBA_Use", setup)
    check(e, fk, 0x448510, "GenericTBA_DrawIcon", setup)
    compare(e, 0x4485C0, "GenericTBA_GetUseTime", setup=setup, check_eax=False, check_st0=True)
    compare(e, 0x4485D0, "GenericTBA_GetAnim", setup=setup, eax_mask=0xFFFF)

    def setup2(em):
        tbase(em)
        i32(em, 0x57C1FC, 0xABCD)
        em.write(0x53C7DC, struct.pack("<H", 0x66))
    check(e, fk, 0x448530, "GenericTBA_Close", setup2)
    compare(e, 0x448550, "GenericTBA_GetType", setup=setup2, eax_mask=0xFFFF)
    compare(e, 0x448560, "GenericTBA_GetParams", setup=setup2)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
def test_use_pos(seed, cw):
    def setup(em):
        tbase(em)
        rng = random.Random(seed)
        t = new_task(em, TT, 0x2A8)
        em.write(t + 0x20, rvec(rng, 1e6))
        em.write(t + 0x70, rmatrix(rng))
        VALUE["point"] = [rng.uniform(-300, 300) for _ in range(3)]
        return t, em.alloc(24), em.alloc(40)
    check(e, fk, 0x448570, "GenericTBA_GetUsePos", setup, cw=cw)
