import random
import struct

import pytest

from test_door_util import (EV_CHANGED, EV_DAC, NAN, SCALES, check, children, f32, f64, make_emu, new_task, obj,
                            rvec, set_handler, tick, u32)

e, fk = make_emu()
CAB = 0x60


def cabinet(e, rng=None, scale=SCALES[0], e8=0, moving=0, use_tick=100, registered=0):
    rng = rng or random.Random(1)
    c = obj(e, CAB, 0x138, rng, scale)
    e.write(c + 0xF0, rvec(rng))
    e.write(c + 0x108, f32(0.75))
    e.write(c + 0x10C, b"cabinet_01\0")
    e.write(c + 0x11C, bytes([1, 0, 0, 1, 0]))
    u32(e, c + 0x124, use_tick)
    e.write(c + 0x128, bytes([0, registered]))
    e.write(c + 0x12C, f32(4.0))
    e.write(c + 0x130, bytes([moving]))
    u32(e, c + 0xE8, e8)
    return c


@pytest.mark.parametrize("kids", [0, 1, 3])
@pytest.mark.parametrize("e8", [0, 1])
@pytest.mark.parametrize("moving", [0, 1])
@pytest.mark.parametrize("now", [99, 102, 103])
def test_run(kids, e8, moving, now):
    def setup(e):
        sub = 0
        if e8:
            sub = new_task(e, 0x50, 0x40)
            set_handler(e, 1, 0x50, fk.handler(1))
        c = cabinet(e, e8=sub, moving=moving)
        children(e, fk, c, 1, kids, nargs=1)
        tick(e, now)
        return (c,)
    check(e, fk, 0x444590, "Cabinet_Run", setup)


@pytest.mark.parametrize("id_", [-1, 0, 5])
@pytest.mark.parametrize("handled", [0, 1])
@pytest.mark.parametrize("e8", [0, 0x4E8])
def test_delete(id_, handled, e8):
    def setup(e):
        fk.rets[0x4F1050] = id_
        set_handler(e, EV_DAC, CAB, fk.handler(1) if handled else 0)
        return (cabinet(e, e8=e8),)
    check(e, fk, 0x444660, "Cabinet_Delete", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57C1A8, 0x1234)
        return 0, e.alloc(4)
    check(e, fk, 0x4446D0, "Cabinet_GetParams", setup)


@pytest.mark.parametrize("arg", [None, b"", b"x"])
@pytest.mark.parametrize("e8", [0, 0x4E8])
@pytest.mark.parametrize("kids", [0, 2])
@pytest.mark.parametrize("scale", SCALES)
def test_params_changed(arg, e8, kids, scale):
    def setup(e):
        sub = new_task(e, 0x51, 0x40)
        children(e, fk, sub, EV_CHANGED, kids)
        fk.rets[0x4E0930] = sub
        a = 0
        if arg is not None:
            a = e.alloc(4)
            e.write(a, arg + b"\0")
        return cabinet(e, scale=scale, e8=e8), a
    check(e, fk, 0x4446E0, "Cabinet_ParamsChanged", setup)


def use_query(e, c, rng, spread):
    q = e.alloc(0x88)
    u32(e, q, 0x4444)
    base = struct.unpack("<3d", e.read(c + 0x20, 24))
    pt = struct.unpack("<3d", fk.point)
    e.write(q + 8, f64(*[b + p + rng.uniform(-spread, spread) for b, p in zip(base, pt)]))
    return q


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(40))
def test_check_use(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        c = cabinet(e, rng)
        fk.point = rvec(rng, 100.0)
        q = use_query(e, c, rng, 4000.0 if seed % 4 else 1e-3)
        if seed == 3:
            e.write(q + 8, f64(NAN))
        return c, q
    check(e, fk, 0x444860, "Cabinet_CheckUse", setup, cw=cw)


@pytest.mark.parametrize("kind", [0, 1, 2])
def test_use(kind):
    def setup(e):
        tick(e, 777)
        a = e.alloc(0x28)
        u32(e, a + 0x24, kind)
        return cabinet(e), a
    check(e, fk, 0x444940, "Cabinet_Use", setup)


def test_sub_4449a0():
    def setup(e):
        h = e.alloc(4)
        u32(e, h, 0x5000)
        return 0, h
    check(e, fk, 0x4449A0, "Cabinet_DrawIcon", setup)


@pytest.mark.parametrize("registered", [0, 1])
@pytest.mark.parametrize("addr,name", [(0x4449C0, "Cabinet_AddSymbols"), (0x444A70, "Cabinet_RemoveSymbols")])
def test_symbols(registered, addr, name):
    check(e, fk, addr, name, lambda e: (cabinet(e, registered=registered),))


def test_close():
    def setup(e):
        u32(e, 0x57C1A8, 0x1A8)
        u32(e, 0x57C1A4, 0x99990044)
        u32(e, 0x57C1AC, 0x88880055)
    check(e, fk, 0x444B00, "Cabinet_Close", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
def test_get_use_point(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        fk.point = rvec(rng, 300.0)
        return cabinet(e, rng), e.alloc(24), e.alloc(40)
    check(e, fk, 0x444B30, "Obj_GetUsePoint", setup, cw=cw)


def test_get_search_time():
    check(e, fk, 0x444B80, "Cabinet_GetSearchTime", lambda e: (cabinet(e),), check_st0=True)


@pytest.mark.parametrize("addr,name", [(0x444B90, "Cabinet_SetSearched"), (0x444BA0, "Cabinet_Reset")])
def test_setters(addr, name):
    check(e, fk, addr, name, lambda e: (cabinet(e),))
