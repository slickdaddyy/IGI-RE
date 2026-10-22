"""Gunx2, KnifeStab and AIStationaryGunHolder."""
import random
import struct

import pytest

from emu import Emu
from test_comprun_util import (LOG, RET, add_child, check_init, d64, f32, floatfake, handler, init_emu, logfake, obj,
                               run, set_handler, task, trap, u32, vfake)

emu = Emu()
for a, n in ((0x4C5710, 3), (0x4C4950, 3), (0x4015F0, 1), (0x4C7560, 2), (0x4F1320, 1), (0x4F1340, 1),
             (0x4F1400, 2), (0x489B90, 1), (0x4896C0, 6), (0x4C48D0, 2), (0x4C1830, 1)):
    logfake(emu, a, n)
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
logfake(emu, 0x4AF810, 2, strings=(0, 1))  # Warning_Show
logfake(emu, 0x4E0930, 2, effect=lambda e, a: RET.get("child", 0x7A00))
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
logfake(emu, 0x416830, 0, ret=0x4430)  # Game_GetEvent7
logfake(emu, 0x416840, 0, ret=0x4442)  # Game_GetEvent9
logfake(emu, 0x460BE0, 0, ret=0x77)  # Human_GetType
logfake(emu, 0x401CF0, 2, masks={0: 0xFFFF, 1: 0xFFFF})  # TaskType_IsA
logfake(emu, 0x4528F0, 1)  # AI_HasTarget
logfake(emu, 0x464250, 0, ret=0x99)
logfake(emu, 0x4D9FB0, 0, ret=0x3F)  # MagicObj_GetMoveEvent
logfake(emu, 0x4F1A70, 0, ret=0x35)  # Param_GetChangedEvent
logfake(emu, 0x4F16C0, 1, effect=lambda e, a: RET["has"].pop(0))  # VarString_HasExpr
floatfake(emu, 0x4F16E0, 2, effect=lambda e, a: RET["evals"].pop(0))  # VarString_EvalReal
floatfake(emu, 0x4D0950, 1)  # VirModel_GetRadius
logfake(emu, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
logfake(emu, 0x4C6940, 4, derefs={2: 24})  # DynCube_Move
logfake(emu, 0x489750, 4, derefs={1: 0x78})  # AIDetection_Update
logfake(emu, 0x467DF0, 1, derefs={0: 0x1C})  # ComputerObject_DrawSprite


def coll(e, a):
    hits = RET["hits"]
    found, victim = hits.pop(0) if hits else (0, 0)
    u32(e, a[1] + 0x58, found)
    u32(e, a[1], victim)


vfake(emu, 0x4CC2A0, 8, effect=coll,
      show=lambda e, a: (a[0], e.read(a[2], 24), e.read(a[3], 24), a[4], a[5] & 0xFF, e.read(a[6], 20), a[7]))

RUN = handler(emu, "run", 1)
MOVE = handler(emu, "move", 2, derefs={1: 12})
CHANGED = handler(emu, "changed", 2)




def stabbed(e):
    msg = e.arg(1)
    if LOG:
        LOG[-1].append(("stabbed", e.normalize(e.arg(0)), e.read(msg + 4, 0x1C), e.read(e.u32(msg), 0x70)))
    e.ret(0)


STABBED = trap(emu, "stabbed", stabbed)
emu.baseline = emu.snapshot()

init = init_emu()


@pytest.mark.parametrize("addr,name", [(0x46DD00, "Gunx2_Init"), (0x46DF10, "KnifeStab_Init"),
                                       (0x46E370, "GunHolder_Init")])
def test_init(addr, name):
    check_init(init, addr, name)


@pytest.mark.parametrize("addr,name,globs", [(0x46DF00, "Gunx2_Close", (0x53FE68,)),
                                             (0x46E200, "KnifeStab_Close", (0x53FE84,)),
                                             (0x46EAA0, "GunHolder_Close", (0x5BE328, 0x5BE318, 0x5BE330))])
def test_close(addr, name, globs):
    def setup(e):
        for i, g in enumerate(globs):
            u32(e, g, 0xABCD0010 + i)
    run(emu, addr, name, setup)


def kids(e, parent, n, slot, fn):
    for i in range(n):
        c = task(e, 0x20, 0x60 + i)
        add_child(e, parent, c)
        set_handler(e, slot, 0x60 + i, fn if i != 1 else 0)


# Gunx2

@pytest.mark.parametrize("model", [0, 0x7800])
def test_gunx2_create(model):
    def setup(e):
        RET[0x4CEC10] = model
        t = obj(e, 0x138, 0x58)
        a = e.alloc(0x90)
        e.write(a, bytes((i * 5 + 1) & 0xFF for i in range(0x90)))
        u32(e, a, 0x5C0000 + 0x10)
        name = e.alloc(16)
        e.write(name, b"gunx2_model\0")
        u32(e, a, name, 0x9999)
        u32(e, a + 0x88, 0x6400)
        return t, a
    run(emu, 0x46DDC0, "Gunx2_Create", setup)


@pytest.mark.parametrize("n", [0, 1, 3])
def test_gunx2_run(n):
    def setup(e):
        t = obj(e, 0x138, 0x58)
        c = task(e, 0x40, 0x59)
        u32(e, t + 0xE8, c)
        kids(e, c, n, 1, RUN)
        return (t,)
    run(emu, 0x46DE70, "Gunx2_Run", setup)


def test_gunx2_destroy():
    def setup(e):
        t = obj(e, 0x138, 0x58)
        u32(e, t + 0xE8, 0x7B00)
        return (t,)
    run(emu, 0x46DEE0, "Gunx2_Destroy", setup)


# KnifeStab

def fire_args(e, rng):
    f = e.alloc(0x30)
    pos = e.alloc(24)
    d64(e, pos, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
    m = e.alloc(40)
    f32(e, m, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, m + 36, 3)
    w = e.alloc(0x700)
    f32(e, w + 0x654, 25.5)
    u32(e, f, 0x1111, 0x2222, pos, 0x3333, 0x4444, m, 0x5555, 0x6666, w, 0x7777)
    return f


def victim(e, human, ai):
    v = task(e, 0x900, 0x62)
    u32(e, v + 0x800, ai)
    return v


HITS = [[], [(1, 0)], [(1, None)], [(0, 0), (1, None)], [(0, 0), (0, 0), (1, None)], [(0, 0)] * 3]


@pytest.mark.parametrize("hits", HITS)
@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("human,ai,target", [(0, 0, 0), (1, 0, 0), (1, 0x7C00, 0), (1, 0x7C00, 1)])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_knife_fire(hits, has, human, ai, target, cw):
    def setup(e):
        rng = random.Random(len(hits))
        v = victim(e, human, ai)
        RET["hits"] = [(f, v if w is None else w) for f, w in hits]
        RET[0x401CF0] = human
        RET[0x4528F0] = target
        set_handler(e, 0x42, 0x62, STABBED if has else 0)
        return 0x6000, fire_args(e, rng)
    run(emu, 0x46DF70, "KnifeStab_Fire", setup, cw=cw)


@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("angle", [0.0, -0.2617994, 0.5])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_knife_try(seed, angle, cw):
    def setup(e):
        rng = random.Random(seed)
        v = victim(e, 1, 0)
        RET["hits"] = [(1, v)]
        RET[0x401CF0] = seed & 1
        set_handler(e, 0x42, 0x62, STABBED)
        return fire_args(e, rng), struct.unpack("<I", struct.pack("<f", angle))[0]
    run(emu, 0x46DFB0, "KnifeStab_Try", setup, cw=cw, check_eax=True)


# AIStationaryGunHolder

def holder(e, scale=(1.0, 2.0, 1.5)):
    t = obj(e, 0x26A0, 0x57, scale=scale)
    e.write(t + 0xF0, bytes((i * 3 + 7) & 0xFF for i in range(0x78)))
    e.write(t + 0x2598, bytes((i * 11 + 5) & 0xFF for i in range(0x108)))
    e.write(t + 0x25D8, b"gun_holder\0")
    f32(e, t + 0x25E8, 90.0, 45.5, 12.25)
    return t


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_holder_create(cw):
    def setup(e):
        RET[0x4D0950] = 321.5
        return (holder(e),)
    run(emu, 0x46E210, "GunHolder_Create", setup, cw=cw)


@pytest.mark.parametrize("team", [(0, 0.0), (1, 2.5), (1, -3.5)])
@pytest.mark.parametrize("active", [(0, 0.0), (1, 0.0), (1, 1.0), (1, 257.0)])
@pytest.mark.parametrize("was", [0, 1])
@pytest.mark.parametrize("child", [0, 1, 3])
@pytest.mark.parametrize("n", [0, 2])
def test_holder_run(team, active, was, child, n):
    def setup(e):
        t = holder(e)
        RET["has"] = [team[0], active[0]]
        RET["evals"] = [v for has, v in (team, active) if has]
        e.write(t + 0x2698, bytes([was]))
        u32(e, t + 0x2694, 3)
        kids(e, t, n, 1, RUN)
        if child:
            c = task(e, 0x40, 0x59)
            set_handler(e, 1, 0x59, RUN)
            u32(e, t + 0xE8, c)
            kids(e, c, child, 0x3F, MOVE)
        return (t,)
    run(emu, 0x46E5B0, "GunHolder_Run", setup)


@pytest.mark.parametrize("child", [0, 0x7D00])
def test_holder_destroy(child):
    def setup(e):
        t = holder(e)
        u32(e, t + 0xE8, child)
        return (t,)
    run(emu, 0x46E7A0, "GunHolder_Destroy", setup)


@pytest.mark.parametrize("old", [0, 0x7D00])
@pytest.mark.parametrize("n", [0, 3])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_holder_changed(old, n, cw):
    def setup(e):
        RET[0x4CEC10] = 0x7800
        RET[0x4D0950] = 55.0
        t = holder(e)
        u32(e, t + 0xE8, old)
        c = task(e, 0x40, 0x59)
        RET["child"] = c
        kids(e, c, n, 0x35, CHANGED)
        return (t,)
    run(emu, 0x46E7F0, "GunHolder_Changed", setup, cw=cw)


def test_holder_get_params():
    run(emu, 0x46E9D0, "GunHolder_GetParams", lambda e: (u32(e, 0x5BE328, 0x6543), (0, e.alloc(4)))[1])


def test_holder_get_detection():
    run(emu, 0x46E9E0, "GunHolder_GetDetection", lambda e: (holder(e), e.alloc(4)))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_holder_draw(cw):
    def setup(e):
        RET[0x4D0950] = 1.25
        a = e.alloc(0x20)
        e.write(a, b"\x33" * 0x20)
        return holder(e, (2.0, 3.0, 1.0)), a
    run(emu, 0x46E9F0, "GunHolder_Draw", setup, cw=cw)
