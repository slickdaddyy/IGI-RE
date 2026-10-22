import random
import struct

import pytest

from emu import Emu, compare
from test_scamera_fakes import (Fakes, add_child, check, check_error, common, cstr, event, f32, f64, i32, new_task,
                                rmatrix, rvec, set_events, set_handler, u8, with_children)

TT, CT, ET = 0x51, 0x52, 0x53  # Terminal, SCameraControl, ExplodeObject type ids
TICK = 0x5000

e = Emu()
fk = Fakes(e)
common(e, fk, {b"Terminal": TT, b"SCameraControl": CT, b"Cabinet": 0x54, b"Drawer": 0x55})
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)
VALUE = {}
fk.add(0x4F1320, 1)
fk.add(0x4F1340, 1)
fk.add(0x4F1400, 2)
fk.add(0x4F1050, 1, ret=lambda em, a: VALUE.get("id", 0))
fk.add(0x4F16C0, 1, ret=lambda em, a: VALUE.get("expr", 0))
fk.add(0x4F16E0, 2, ret=lambda em, a: VALUE.get("eval", 0.0), st0="d")
fk.add(0x4F1270, 2, ret=lambda em, a: em.write(a[0], b"term_7\0"), fmt=lambda em, a: (a[1],))
fk.add(0x4B7EE0, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))
fk.add(0x4B80A0, 1, fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4C1790, 2)
fk.add(0x4C17C0, 2)
fk.add(0x481360, 9)
fk.add(0x414E20, 3, ret=lambda em, a: em.write(a[0], f64(*VALUE["point"])), fmt=lambda em, a: a[1:])
fk.add(0x416AD0, 3, deref={2: 24})
fk.add(0x4871C0, 2)
fk.add(0x4D0950, 1, ret=lambda em, a: VALUE.get("radius", 100.0), st0="d")
fk.add(0x467DF0, 1, deref={0: 0x18})
fk.add(0x467F30, 1)
fk.add(0x4680B0, 1)
e.baseline = e.snapshot()


def base(em):
    VALUE.clear()
    set_events(em)
    em.write(0x53C680, struct.pack("<H", TT))
    em.write(0x54097C, struct.pack("<H", ET))
    info = em.alloc(0x80)
    i32(em, info + 0x68, TICK)
    i32(em, 0xBC2360, info)


@pytest.mark.parametrize("v", [0, 1, 0x80])
def test_alarm_last(v):
    def setup(em):
        p = em.alloc(0x350)
        u8(em, p + 0x2A2, v)
        return (p,)
    compare(e, 0x440C10, "AlarmControl_IsLastAlarm", setup=setup, eax_mask=0xFF)


# --- SCameraControl ------------------------------------------------------------

def test_control_open():
    _, log = check(e, fk, 0x440C20, "SCameraControl_Open", base)
    assert len(log) == 10


def test_control_create():
    def setup(em):
        base(em)
        set_handler(em, 0, TT, LOG2)
        c = new_task(em, CT, 0x2F8)
        em.write(c + 0x2A0, b"\x55\x66")
        return c, 0x1234
    check(e, fk, 0x440D30, "SCameraControl_Create", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("exploded,expr,value", [(1, 1, 1.0), (0, 0, 1.0), (0, 1, 0.4), (0, 1, 1.6), (0, 1, 2.5),
                                                 (0, 1, -1.0), (0, 1, 300.0)])
def test_control_run(exploded, expr, value, cw):
    def setup(em):
        base(em)
        set_handler(em, 1, TT, LOG1)
        c = new_task(em, CT, 0x2F8)
        u8(em, c + 0x190, exploded)
        u8(em, c + 0x2A0, 0x77)
        VALUE.update(expr=expr, eval=value)
        return (c,)
    check(e, fk, 0x440D70, "SCameraControl_Run", setup, cw=cw)


@pytest.mark.parametrize("id_,handler", [(-1, True), (0, True), (3, False)])
def test_control_delete(id_, handler):
    def setup(em):
        base(em)
        set_handler(em, 2, TT, LOG1)
        set_handler(em, event(0x4F1220), CT, LOG1 if handler else 0)
        c = new_task(em, CT, 0x2F8)
        VALUE.update(id=id_)
        return (c,)
    check(e, fk, 0x440DE0, "SCameraControl_Delete", setup)


def test_control_params():
    def setup(em):
        base(em)
        i32(em, 0x57C170, 0xABCD)
        return 0, em.alloc(4)
    check(e, fk, 0x440E40, "SCameraControl_GetParams", setup)

    def setup2(em):
        base(em)
        set_handler(em, event(0x4F1A70), TT, LOG2)
        return new_task(em, CT, 0x2F8), 0x99
    check(e, fk, 0x440E50, "SCameraControl_ParamsChanged", setup2)


@pytest.mark.parametrize("registered", [0, 1])
def test_control_symbols(registered):
    def setup(em):
        base(em)
        set_handler(em, event(0x4F1210), TT, LOG1)
        set_handler(em, event(0x4F1220), TT, LOG1)
        c = new_task(em, CT, 0x2F8)
        u8(em, c + 0x2A1, registered)
        return (c,)
    check(e, fk, 0x440EA0, "SCameraControl_AddSymbols", setup)
    check(e, fk, 0x440F30, "SCameraControl_RemoveSymbols", setup)


def test_control_close():
    def setup(em):
        base(em)
        i32(em, 0x57C170, 0xABCD)
        em.write(0x57C174, struct.pack("<H", 0x66))
    check(e, fk, 0x440FB0, "SCameraControl_Close", setup)


# --- Terminal --------------------------------------------------------------------

def test_terminal_open():
    _, log = check(e, fk, 0x443680, "Terminal_Open", base)
    assert len(log) == 16


def test_terminal_create():
    def setup(em):
        base(em)
        set_handler(em, 0, ET, LOG2)
        t = new_task(em, TT, 0x2A0)
        em.write(t + 0x238, b"\x55" * 0x18)
        return t, 0x1234
    check(e, fk, 0x443840, "Terminal_Create", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("exploded,expr,value", [(1, 1, 1.0), (0, 0, 1.0), (0, 1, 0.5), (0, 1, 1.5), (0, 1, 257.0)])
@pytest.mark.parametrize("use,hack", [(TICK, TICK - 3), (TICK - 2, TICK - 2), (TICK - 3, TICK + 5)])
def test_terminal_run(exploded, expr, value, use, hack, cw):
    def setup(em):
        base(em)
        set_handler(em, 1, ET, LOG1)
        t = new_task(em, TT, 0x2A0)
        u8(em, t + 0x190, exploded)
        em.write(t + 0x238, b"\x01\x00\x00\x00" + struct.pack("<i", use) + b"\x07\x01\x01\x00" + struct.pack("<i", hack))
        VALUE.update(expr=expr, eval=value)
        return (t,)
    check(e, fk, 0x4438E0, "Terminal_Run", setup, cw=cw)


@pytest.mark.parametrize("id_,handler", [(-1, True), (0, True), (3, False)])
def test_terminal_delete(id_, handler):
    def setup(em):
        base(em)
        set_handler(em, 2, ET, LOG1)
        set_handler(em, event(0x4F1220), TT, LOG1 if handler else 0)
        VALUE.update(id=id_)
        return (new_task(em, TT, 0x2A0),)
    check(e, fk, 0x443990, "Terminal_Delete", setup)


def test_terminal_params():
    def setup(em):
        base(em)
        i32(em, 0x57C1A0, 0xABCD)
        return 0, em.alloc(4)
    check(e, fk, 0x443A00, "Terminal_GetParamsHandler", setup)

    def setup2(em):
        base(em)
        set_handler(em, event(0x4F1A70), ET, LOG2)
        return new_task(em, TT, 0x2A0), 0x99
    check(e, fk, 0x443A10, "Terminal_ParamsChanged", setup2)


@pytest.mark.parametrize("registered", [0, 1])
def test_terminal_symbols(registered):
    def setup(em):
        base(em)
        set_handler(em, event(0x4F1210), ET, LOG1)
        set_handler(em, event(0x4F1220), ET, LOG1)
        t = new_task(em, TT, 0x2A0)
        u8(em, t + 0x248, registered)
        return (t,)
    check(e, fk, 0x443A60, "Terminal_AddSymbols", setup)
    check(e, fk, 0x443B80, "Terminal_RemoveSymbols", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(40))
def test_terminal_check_use(seed, cw):
    def setup(em):
        base(em)
        rng = random.Random(seed)
        t = new_task(em, TT, 0x2A0)
        u8(em, t + 0x240, seed % 7 != 0)
        em.write(t + 0x20, rvec(rng, 1e5))
        em.write(t + 0x70, rmatrix(rng))
        VALUE["point"] = [rng.uniform(-300, 300) for _ in range(3)]
        user = em.alloc(0x40)
        pos = struct.unpack("<3d", em.read(t + 0x20, 24))
        em.write(user + 8, f64(*[p + q + rng.uniform(-6000, 6000) for p, q in zip(pos, VALUE["point"])]))
        return t, user
    check(e, fk, 0x443C80, "Terminal_CheckUse", setup, cw=cw)


def test_terminal_small():
    def setup(em):
        base(em)
        return new_task(em, TT, 0x2A0), em.alloc(4)
    check(e, fk, 0x443D70, "Terminal_GetUseKind", setup)
    check(e, fk, 0x443D80, "Terminal_Use", setup)
    check(e, fk, 0x443EA0, "Terminal_SetHacked", setup)

    def setup2(em):
        base(em)
        p = em.alloc(4)
        i32(em, p, 0x7000)
        return 0, p
    check(e, fk, 0x443DA0, "sub_443DA0", setup2)

    def setup3(em):
        base(em)
        i32(em, 0x57C1A0, 0xABCD)
        em.write(0x53C680, struct.pack("<H", 0x66))
    check(e, fk, 0x443E60, "Terminal_Close", setup3)
    compare(e, 0x443E80, "Terminal_GetType", setup=setup3, eax_mask=0xFFFF)
    compare(e, 0x443E90, "Terminal_GetParams", setup=setup3)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("scale", [(1, 2, 3), (3, 2, 1), (2, 3, 1), (1, 1, 1), (2, 1, 2), (1.5, 2.5, 2.5)])
def test_terminal_map(scale, cw):
    def setup(em):
        base(em)
        t = new_task(em, TT, 0x2A0)
        em.write(t + 0x98, f32(*scale))
        VALUE["radius"] = 1234.567
        return t, em.alloc(0x40)
    check(e, fk, 0x443DC0, "Terminal_MapHandler", setup, cw=cw)


# --- Cabinet and Drawer ----------------------------------------------------------

fk.add(0x4CEC10, 1, ret=lambda em, a: VALUE.get("model", 0), fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4C5710, 3)
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4C3440, 1, ret=0x3440)
fk.add(0x4DCCB0, 3)
e.baseline = e.snapshot()
GAME = e.alloc(0x40)
e.baseline = e.snapshot()


def cab_base(em):
    base(em)
    i32(em, 0x57BABC, GAME)
    i32(em, GAME + 0x20, 0x3333)
    i32(em, 0xA44344, 0x4444)


def test_cabinet_open():
    _, log = check(e, fk, 0x443ED0, "Cabinet_Open", base)
    assert len(log) == 24


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("scale", [(1, 2, 3), (3, 2, 1), (2, 3, 1)])
def test_cabinet_create(scale, cw):
    def setup(em):
        cab_base(em)
        c = new_task(em, 0x54, 0x138)
        em.write(c + 0x98, f32(*scale))
        i32(em, c + 0x68, 0x6868)
        i32(em, c + 0x6C, 0x6C6C)
        em.write(c + 0xF0, b"\x55" * 0x48)
        VALUE["radius"] = 777.25
        return (c,)
    check(e, fk, 0x444420, "Cabinet_Create", setup, cw=cw)


def drawer_arg(em, rng):
    a = em.alloc(0x90)
    i32(em, a, cstr(em, b"drawer_model"))
    i32(em, a + 4, 0x4444)
    em.write(a + 8, rvec(rng, 1e5))
    em.write(a + 0x38, rmatrix(rng))
    i32(em, a + 0x88, 0x8888)
    return a


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
def test_drawer_create(seed, cw):
    def setup(em):
        base(em)
        rng = random.Random(seed)
        d = new_task(em, 0x55, 0x160)
        em.write(d + 0x98, f32(*[rng.choice([1.0, 2.0, 0.5]) for _ in range(3)]))
        em.write(d + 0x150, f32(1, 2, 3))
        i32(em, d + 0x6C, 0x6C6C)
        VALUE.update(model=0x7700, radius=rng.uniform(10, 1000))
        return d, drawer_arg(em, rng)
    check(e, fk, 0x444150, "Drawer_Create", setup, cw=cw)


def test_drawer_create_error():
    def setup(em):
        base(em)
        return new_task(em, 0x55, 0x160), drawer_arg(em, random.Random(1))
    check_error(e, fk, 0x444150, "Drawer_Create", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("flags", [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 1, 1), (1, 1, 0)])
@pytest.mark.parametrize("y", [0.0, 50.0, 1700.0, 1843.2, 1800.0, 2000.0])
def test_drawer_run(flags, y, cw):
    def setup(em):
        cab_base(em)
        rng = random.Random(int(y) + sum(flags))
        d = new_task(em, 0x55, 0x160)
        c = new_task(em, 0x54, 0x138)
        i32(em, d + 0x130, c)
        em.write(c + 0x11C, bytes(flags))
        u8(em, c + 0x130, 1)
        em.write(d + 0x70, rmatrix(rng))
        em.write(d + 0x98, f32(*rng.choice([(1, 1.5, 1), (2, 1, 1), (1, 1, 2), (1, 2, 3)])))
        em.write(d + 0x138, rvec(rng, 1e5))
        em.write(d + 0x150, f32(rng.uniform(-100, 100), y, rng.uniform(-100, 100)))
        VALUE["radius"] = 512.5
        return (d,)
    check(e, fk, 0x444260, "Drawer_Run", setup, cw=cw)
