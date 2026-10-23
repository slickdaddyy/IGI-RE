"""ExplodeObject (0x481340 - 0x482580)."""
import math
import random
import struct

import pytest

from emu import ErrorShown  # noqa: F401
from test_camera_util import RET, add_child, cstr, d64, f32, handler, make_emu, run, set_handler, task, u16, u32

emu = make_emu()
EXP, CHILD = 0x60, 0x61
CWS = [0x37F, 0x7F]
SLOTS = {1: 1, 0x30: 1, 0x2D: 2, 0x2E: 2, 0x2F: 2, 0x32: 1}
HANDLERS = {s: handler(emu, "slot%x" % s, n, derefs={1: 8} if s == 0x2D else None) for s, n in SLOTS.items()}


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def obj(e, children=0, scale=(1.0, 2.0, 1.5), seed=1, exploded=0, in_world=1, timer=-1, triggered=0,
        model=b"crate", dead=b"crate_dead"):
    rng = random.Random(seed)
    for s in SLOTS:
        set_handler(e, s, CHILD, HANDLERS[s])
        set_handler(e, s, EXP, HANDLERS[s])
    t = task(e, 0x238, EXP)
    d64(e, t + 0x20, 1000.5, -2000.25, 30.75)
    u32(e, t + 0x68, 0x6868, 0x6C6C)
    f32(e, t + 0x70, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, t + 0x94, 4)
    f32(e, t + 0x98, *scale)
    u32(e, t + 0xDC, 0xDCDC)
    d64(e, t + 0xF0, *[rng.uniform(-9000, 9000) for _ in range(3)])
    f32(e, t + 0x108, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, t + 0x12C, 7)
    d64(e, t + 0x130, *[rng.uniform(-900, 900) for _ in range(3)])
    f32(e, t + 0x148, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, t + 0x16C, 11)
    e.write(t + 0x170, model + b"\0")
    e.write(t + 0x180, dead + b"\0")
    e.write(t + 0x190, bytes([exploded, 0]))
    e.write(t + 0x1E4, bytes([triggered]))
    f32(e, t + 0x1E8, 10.0, 50.0, 75.0)
    u32(e, t + 0x1F4, timer)
    f32(e, t + 0x1F8, 2.5, 1.25, 0.75, 0.5, 1.7)
    u32(e, t + 0x20C, 4, 6)
    e.write(t + 0x214, b"explo_02_m\0")
    e.write(t + 0x234, bytes([in_world]))
    for i in range(children):
        add_child(e, t, task(e, 0x40, CHILD if i else EXP))
    return t


def test_get_type_params():
    def setup(e):
        u32(e, 0x54097C, 0x1234ABCD)
        u32(e, 0x5C1184, 0x99887766)
    run(emu, 0x481340, "ExplodeObject_GetType", setup, check_eax=True, eax_mask=0xFFFF)
    run(emu, 0x481350, "ExplodeObject_GetParams", setup, check_eax=True)


def test_get_param_table():
    def setup(e):
        u32(e, 0x5C1184, 0x99887766)
        return 0, e.alloc(4)
    run(emu, 0x481C30, "ExplodeObject_GetParamTableHandler", setup)


def test_setup():
    def setup(e):
        return (obj(e), fbits(150.0), fbits(3.5), fbits(1.5), fbits(0.25), fbits(2.5), 7, 9, cstr(e, b"explo_01_m"))
    run(emu, 0x481360, "ExplodeObject_Setup", setup)


def test_open():
    run(emu, 0x4813E0, "ExplodeObject_Open")


def test_close():
    def setup(e):
        u32(e, 0x54097C, 0x5555)
        u32(e, 0x5C1184, 0x4321)
    run(emu, 0x482560, "ExplodeObject_Close", setup)


SCALES = [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 2.0, 2.5), (2.0, 2.0, 2.0), (math.nan, 1.0, 2.0),
          (1.0, math.nan, 2.0), (1.0, 2.0, math.nan)]


@pytest.mark.parametrize("scale", SCALES)
def test_create(scale):
    def setup(e):
        RET[0x4D0950] = 12.3456789
        u32(e, 0xA44344, 0x4444)
        return (obj(e, scale=scale),)
    run(emu, 0x481780, "ExplodeObject_CreateHandler", setup)


@pytest.mark.parametrize("exploded", [0, 1])
@pytest.mark.parametrize("has,value", [(0, 0.0), (1, 0.0), (1, 1.0), (1, 2.6), (1, -1.5), (1, 300.4)])
@pytest.mark.parametrize("timer", [-1, 0, 1, 5])
@pytest.mark.parametrize("dead", [0, 0x3333])
@pytest.mark.parametrize("children", [0, 2])
def test_run(exploded, has, value, timer, dead, children):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        RET[("model", b"crate_dead")] = dead
        return (obj(e, exploded=exploded, timer=timer, children=children, triggered=1 if timer == 5 else 0),)
    run(emu, 0x481970, "ExplodeObject_RunHandler", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("dead", [0, 0x3333])
@pytest.mark.parametrize("children", [0, 2])
def test_explode(cw, dead, children):
    def setup(e):
        RET[("model", b"crate_dead")] = dead
        return (obj(e, children=children),)
    run(emu, 0x481A60, "ExplodeObject_Explode", setup, cw=cw)


@pytest.mark.parametrize("id_", [-1, 0, 3])
@pytest.mark.parametrize("handled", [0, 1])
def test_delete(id_, handled):
    def setup(e):
        RET[0x4F1050] = id_
        t = obj(e)
        if not handled:
            set_handler(e, 0x32, EXP, 0)
        return (t,)
    run(emu, 0x481BC0, "ExplodeObject_DeleteHandler", setup)


@pytest.mark.parametrize("model,found", [(b"crate", 0), (b"crate", 1), (b"", 0), (b"x*y", 0), (b"**", 0)])
@pytest.mark.parametrize("dead,dfound", [(b"crate_dead", 0), (b"crate_dead", 1), (b"", 0), (b"d*", 0)])
@pytest.mark.parametrize("scale", SCALES[:4])
def test_param_changed(model, found, dead, dfound, scale):
    def setup(e):
        RET[0x4D0950] = 3.75
        RET[("model", model)] = 0x1111 * found
        RET[("model", dead)] = 0x2222 * dfound
        return (obj(e, model=model, dead=dead, scale=scale),)
    run(emu, 0x481C40, "ExplodeObject_ParamChangedHandler", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("exploded", [0, 1])
@pytest.mark.parametrize("dmg,have,health", [(5.0, 10.0, 15.0), (5.0, 10.0, 20.0), (0.1, 0.2, 0.3),
                                             (1e-9, 15.0, 15.0), (math.nan, 1.0, 2.0), (1.0, 1.0, math.nan)])
def test_hit(cw, exploded, dmg, have, health):
    def setup(e):
        t = obj(e, exploded=exploded, timer=7)
        f32(e, t + 0x1E8, have, health)
        a = e.alloc(8)
        f32(e, a + 4, dmg)
        return t, a
    run(emu, 0x481E60, "ExplodeObject_HitHandler", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("exploded,in_world", [(0, 1), (1, 1), (0, 0)])
@pytest.mark.parametrize("falloff,have,health", [(5.0, 10.0, 15.0), (5.0, 10.0, 20.0), (0.1, 0.2, 0.3),
                                                 (1e-9, 15.0, 15.0), (math.nan, 1.0, 2.0)])
@pytest.mark.parametrize("delay", [1.7, 0.05, 2.5 / 30])
def test_blast(cw, exploded, in_world, falloff, have, health, delay):
    def setup(e):
        RET[0x4D0950] = 3.75
        RET[0x416C90] = falloff
        t = obj(e, exploded=exploded, in_world=in_world, timer=7)
        f32(e, t + 0x1E8, have, health)
        f32(e, t + 0x208, delay)
        return t, 0xB1A5
    run(emu, 0x481EA0, "ExplodeObject_BlastHandler", setup, cw=cw)


@pytest.mark.parametrize("registered", [0, 1])
def test_register_vars(registered):
    def setup(e):
        t = obj(e)
        e.write(t + 0x191, bytes([registered]))
        return (t,)
    run(emu, 0x481F90, "ExplodeObject_RegisterVarsHandler", setup)
    run(emu, 0x481FF0, "ExplodeObject_UnregisterVarsHandler", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", [1, 2, 3])
@pytest.mark.parametrize("children", [0, 2])
def test_move(cw, seed, children):
    def setup(e):
        RET[0x4D0950] = 3.75
        rng = random.Random(seed * 7)
        t = obj(e, children=children, seed=seed)
        pos, m = e.alloc(24), e.alloc(40)
        d64(e, pos, *[rng.uniform(-90000, 90000) for _ in range(3)])
        f32(e, m, *[rng.uniform(-1, 1) for _ in range(9)])
        u32(e, m + 36, 21)
        a = e.alloc(8)
        u32(e, a, pos, m)
        return t, a
    run(emu, 0x482050, "ExplodeObject_MoveHandler", setup, cw=cw)


@pytest.mark.parametrize("children", [0, 2])
def test_join_leave(children):
    def setup(e):
        RET[0x4D0950] = 3.75
        t = obj(e, children=children, in_world=children == 0)
        a = e.alloc(8)
        u32(e, a, 0x1234, 0x5678)
        return t, a
    run(emu, 0x482390, "ExplodeObject_JoinWorldHandler", setup)
    run(emu, 0x4824C0, "ExplodeObject_LeaveWorldHandler", setup)
