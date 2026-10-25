"""GFXCapsViewer, ShadowTask, AnimSound, EditorMagicObj, LevelTimer and the
small Water helpers of 0x5196D0 - 0x51BF90."""
import math
import struct

import pytest

from emu import Emu
from test_nodereg_util import (CWS, FISTP_CWS, LOG, RET, add_child, cstr, d64, f32, floatret, handler, logfake,
                               pyfake, rnd, run, set_handler, task, trap, u8, u16, u32)

TIMER, CHILD = 0x60, 0x61
EV_DAC, EV_CHANGED = 3, 9
RIGID = 0x12

emu = Emu()
logfake(emu, 0x491DE0, 0, ret=2)  # Display_GetGFXCapsMethod
logfake(emu, 0x4B8A20, 2, effect=lambda e, a: RET.get("int", a[1] * 3 - 7))  # Script_GetArgInt
floatret(emu, 0x4B8A50, 2, lambda e, a: RET.get("real", a[1] * 1.37 - 0.11))  # Script_GetArgReal
logfake(emu, 0x4B8A80, 4)  # Script_GetArgString
logfake(emu, 0x4E6B00, 2, effect=lambda e, a: RET.get("sound", 0x6B00))  # Sound_Play
logfake(emu, 0x4E6C30, 2)  # Sound_SetPos
logfake(emu, 0x4F0EE0, 0, ret=0x0EE0)  # Level_GetDynamic
logfake(emu, 0x4CEC10, 1, strings=(0,), ret=0xEC10)  # VirModel_Find
logfake(emu, 0x4C5710, 3)  # RigidDynCubeObj_Init
floatret(emu, 0x4D0950, 2, lambda e, a: RET.get("radius", 12.375))  # VirModel_GetRadius
logfake(emu, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
logfake(emu, 0x4C4950, 3)  # sub_4C4950
logfake(emu, 0x4C7560, 2)  # DynCube_Remove
logfake(emu, 0x4C48C0, 0, ret=RIGID)  # RigidDynCubeObj_GetType
logfake(emu, 0x4F1320, 1)  # VarString_Init
logfake(emu, 0x4F1340, 1)  # VarString_Free
logfake(emu, 0x4F1400, 2)  # VarString_Bind
logfake(emu, 0x4F16C0, 1, effect=lambda e, a: RET.get(("has", a[0]), 1))  # VarString_HasExpr
floatret(emu, 0x4F16E0, 2, lambda e, a: RET.get(("eval", a[0]), 0.0))  # VarString_EvalReal
logfake(emu, 0x4F1050, 1, effect=lambda e, a: RET.get("id", 5))  # Task_GetId
logfake(emu, 0x4F1220, 0, ret=EV_DAC)  # Level_GetDeactivateEvent
logfake(emu, 0x4F1A70, 0, ret=EV_CHANGED)  # Param_GetChangedEvent
logfake(emu, 0x4B7EE0, 2, strings=(0,))  # Script_RegisterBool8
logfake(emu, 0x4B7E80, 2, strings=(0,))  # Script_RegisterInt
logfake(emu, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol


def get_name(e):
    LOG[-1].append(("GetName", e.normalize(e.arg(1))))
    e.write(e.arg(0), b"Timer_%d\0" % (e.arg(1) & 0xFF))
    e.ret(0)


def sprintf(e):
    fmt = e.read_cstr(e.arg(1)).decode()
    text = (fmt % e.read_cstr(e.arg(2)).decode()).encode()
    LOG[-1].append(("sprintf", fmt))
    e.write(e.arg(0), text + b"\0")
    e.ret(len(text))


def rotz(e):
    """Matrix_SetRotZ done in Python, logging the angle."""
    m, a = e.arg(0), struct.unpack("<f", struct.pack("<I", e.arg(1)))[0]
    LOG[-1].append(("rotz", a))
    c, s = math.cos(a), math.sin(a)
    f32(e, m, c, s, 0, -s, c, 0, 0, 0, 1)
    u32(e, m + 0x24, 4)
    e.ret(0)


pyfake(emu, 0x4F1270, get_name)
pyfake(emu, 0x4A53B3, sprintf)
pyfake(emu, 0x4B3BE0, rotz)
render = trap(emu, "render", lambda e: (LOG[-1].append(("render", e.arg(0))), e.ret(0)))
u32(emu, 0xA94E84 + 8, render)
set_handler(emu, 4, RIGID, handler(emu, "move", 2))
for t in (TIMER, CHILD):
    set_handler(emu, EV_DAC, t, handler(emu, "dac_%x" % t, 1))
    set_handler(emu, EV_CHANGED, t, handler(emu, "changed_%x" % t, 2))
    set_handler(emu, 1, t, handler(emu, "run_%x" % t, 1))
emu.baseline = emu.snapshot()


def test_gfxcaps():
    run(emu, 0x5196F0, "GFXCapsViewer_GetCaps", lambda e: (0, e.alloc(4)))


@pytest.mark.parametrize("orig,ours,glob", [(0x519810, "ShadowTask_GetParamTable", 0xA83744),
                                            (0x519A50, "SplinePathNode_GetParamTable", 0xA8374C),
                                            (0x51A230, "SplinePath_GetParamTable", 0xA83750),
                                            (0x51B390, "SplinePathGuide_GetParamTable", 0xA83754),
                                            (0x51BE00, "LevelTimer_GetParamTable", 0xA83778),
                                            (0x519210, "Water_GetParamTable", 0xA80304),
                                            (0x5188B0, "GraphNode_GetParamTable", 0xA801FC)])
def test_get_param_table(orig, ours, glob):
    def setup(e):
        u32(e, glob, glob ^ 0x5A5A)
        return 0, e.alloc(4)
    run(emu, orig, ours, setup)


def test_water_set_tint():
    def setup(e):
        p = e.alloc(12)
        f32(e, p, 0.25, -3.5, 100.0)
        return (p,)
    run(emu, 0x5196D0, "Water_SetTint", setup)


@pytest.mark.parametrize("val", [0, 1, 0x1FF])
def test_animsound_parse(val):
    def setup(e):
        RET["int"] = val
        a = e.alloc(12)
        u32(e, a, e.alloc(0x24), 0xA4A4, 3)
        return 0, a
    run(emu, 0x51B4B0, "AnimSound_Parse", setup)


def test_animsound_size():
    run(emu, 0x51B4E0, "AnimSound_GetSize", lambda e: (0, e.alloc(4)))


@pytest.mark.parametrize("attached,sound", [(0, 0x6B00), (0, 0), (1, 0x6B00), (2, 0)])
def test_animsound_fire(attached, sound):
    def setup(e):
        RET["sound"] = sound
        d = e.alloc(0x24)
        e.write(d, b"door_open\0")
        u8(e, d + 0x20, attached)
        a = e.alloc(12)
        u32(e, a, 0x0A0A, d, e.alloc(24))
        return 0, a
    run(emu, 0x51B4F0, "AnimSound_Fire", setup)


@pytest.mark.parametrize("cw", CWS)
def test_editormagic_parse(cw):
    def setup(e):
        RET["real"] = 2.718281828
        a = e.alloc(12)
        u32(e, a, e.alloc(12), 0xA4A4, 2)
        return 0, a
    run(emu, 0x51BA60, "EditorMagicObj_Parse", setup, cw=cw)


def test_editormagic_size():
    run(emu, 0x51BAA0, "EditorMagicObj_GetArgSize", lambda e: (0, e.alloc(4)))


def magic(e, rng, scale=(1.0, 2.0, 1.5)):
    o = task(e, 0x140, 0x70)
    d64(e, o + 0x20, 10.0, 20.0, 30.0)
    u32(e, o + 0x68, 0x6868, 0x6C6C)
    f32(e, o + 0x70, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, o + 0x94, 7)
    f32(e, o + 0x98, *scale)
    u32(e, o + 0xD4, 3)
    f32(e, o + 0x108, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, o + 0x12C, 11)
    return o


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("spin", [(0.5, 3, 9), (0.25, 4, 4), (-1.75, -2, 6)])
def test_editormagic_move(spin, cw):
    def setup(e):
        rng = rnd(sum(spin) * 10 + cw)
        o = magic(e, rng)
        s = e.alloc(12)
        e.write(s, struct.pack("<fii", *spin))
        u32(e, o + 0x138, s)
        mv = e.alloc(0x40)
        f32(e, mv + 0x18, *[rng.uniform(-1, 1) for _ in range(9)])
        u32(e, mv + 0x3C, 5)
        return o, mv
    run(emu, 0x51B540, "EditorMagicObj_Move", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("scale", [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 1.5, 2.0), (2.0, 2.0, 2.0),
                                   (float("nan"), 1.0, 2.0), (1.0, float("nan"), 2.0), (1.0, 2.0, float("nan"))])
def test_editormagic_create(scale, cw):
    def setup(e):
        RET["radius"] = 3.3
        o = magic(e, rnd(1), scale)
        a = e.alloc(0x90)
        u32(e, a, cstr(e, b"magicmodel"), 0x3030)
        d64(e, a + 8, 1.5, -2.5, 3.25)
        f32(e, a + 0x38, *[i * 0.5 for i in range(9)])
        u32(e, a + 0x5C, 21)
        f32(e, a + 0x60, *[i * -0.25 for i in range(9)])
        u32(e, a + 0x84, 22, 0x8888, 0x8C8C)
        return o, a
    run(emu, 0x51B850, "EditorMagicObj_Create", setup, cw=cw)


def test_editormagic_delete():
    run(emu, 0x51B940, "EditorMagicObj_Delete", lambda e: (magic(e, rnd(2)),))


def timer(e, kids=0, registered=0, run_=1, tick=10):
    t = task(e, 0x110, TIMER)
    u8(e, t + 0x60, run_)
    u32(e, t + 0x64, tick)
    u8(e, t + 0x68, registered)
    for i in range(kids):
        add_child(e, t, task(e, 0x40, CHILD if i != 1 else 0x62))
    return t


def test_timer_create():
    run(emu, 0x51BC60, "LevelTimer_Create", lambda e: (timer(e),))


@pytest.mark.parametrize("registered", [0, 1])
def test_timer_register(registered):
    run(emu, 0x51BCC0, "LevelTimer_Register", lambda e: (timer(e, registered=registered),))


@pytest.mark.parametrize("registered", [0, 1])
def test_timer_unregister(registered):
    run(emu, 0x51BD40, "LevelTimer_Unregister", lambda e: (timer(e, registered=registered),))


@pytest.mark.parametrize("id_,typ", [(-1, TIMER), (0, TIMER), (4, CHILD), (4, 0x63)])
def test_timer_delete(id_, typ):
    def setup(e):
        RET["id"] = id_
        t = timer(e)
        u16(e, t + 0x1C, typ)
        return (t,)
    run(emu, 0x51BDB0, "LevelTimer_Delete", setup)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("kids", [0, 1, 3])
@pytest.mark.parametrize("has,run_val,reset_val,run0", [
    ((0, 0), 0.0, 0.0, 1), ((0, 0), 0.0, 0.0, 0), ((1, 0), 0.7, 0.0, 0), ((1, 0), 1.5, 0.0, 0),
    ((1, 1), 255.6, 0.0, 1), ((1, 1), -0.5, -0.0, 1), ((0, 1), 0.0, 2.0, 1), ((0, 1), 0.0, float("nan"), 1),
    ((1, 1), 2.5, -3.0, 0), ((1, 1), 0.0, 0.0, 1)])
def test_timer_run(kids, has, run_val, reset_val, run0, cw):
    def setup(e):
        t = timer(e, kids, run_=run0)
        RET[("has", t + 0x6C)], RET[("has", t + 0xBC)] = has
        RET[("eval", t + 0x6C)] = run_val
        RET[("eval", t + 0xBC)] = reset_val
        return (t,)
    run(emu, 0x51BE10, "LevelTimer_Run", setup, cw=cw)


@pytest.mark.parametrize("name", [None, b"", b"pos"])
@pytest.mark.parametrize("kids", [0, 1, 3])
def test_timer_params_changed(name, kids):
    def setup(e):
        return timer(e, kids), cstr(e, name) if name is not None else 0
    run(emu, 0x51BEE0, "LevelTimer_ParamsChanged", setup)
