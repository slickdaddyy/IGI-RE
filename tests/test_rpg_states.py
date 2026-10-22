"""Movement states of HumanSoldierRPG and the first ones of HumanSoldierPistol."""
import struct

import pytest

from emu import Emu
from test_rpg import CWS, Fakes, check, f32, f64, i32

cfg = {}


def c(key):
    return lambda em, a: cfg[key]


def make():
    e = Emu()
    fk = Fakes(e)
    for a, n in ((0x489E70, 3), (0x4D61D0, 5), (0x4D65B0, 3), (0x413C80, 1), (0x4F2350, 2), (0x463770, 1),
                 (0x4E6C30, 2), (0x4F2390, 1), (0x4F2070, 3), (0x4F2290, 1), (0x45D5B0, 4), (0x463760, 2),
                 (0x464700, 1), (0x4635F0, 1), (0x489EA0, 4), (0x4E6C60, 3), (0x4E6CA0, 2)):
        fk.add(a, n)
    fk.add(0x4D74F0, 3, ret=c("exists"))
    fk.add(0x4AF810, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))
    fk.add(0x4D6520, 2, ret=c("phase"), st0="d")
    fk.add(0x4D64E0, 2, ret=c("length"), st0="d")
    fk.add(0x4F2060, 2, ret=0x7070)
    fk.add(0x4F23D0, 2, fmt=lambda em, a: (a[0],))
    fk.add(0x4F0EE0, 1, ret=0x5150, fmt=lambda em, a: (em.read_cstr(a[0]),))
    fk.add(0x4E6B00, 2, ret=c("snd"), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
    fk.add(0x489E10, 2, ret=c("anims"))
    fk.add(0x416D20, 0, ret=0x4444)
    fk.add(0x4B47C0, 2, ret=c("rint"))
    fk.add(0x45DAA0, 2, ret=c("side"))
    fk.add(0x4638A0, 3)
    fk.add(0x4D64C0, 2, ret=c("frame"))
    fk.add(0x4B4770, 1, ret=c("rand"), st0="d")
    fk.add(0x460C40, 0, ret=0x21)
    fk.add(0x463180, 1, ret=c("b463180"))
    fk.add(0x416FB0, 0, ret=c("rain"))
    fk.add(0x4A53B3, 3, fmt=lambda em, a: (em.read_cstr(a[1]), a[2]),
           effect=lambda em, a: em.write(a[0], b"walk_rain_%d\0" % a[2]))
    stubs = fk.stubs(40)
    punch = fk.add(fk.code(b"\xc3"), 2, deref={1: 8})
    e.baseline = e.snapshot()
    return e, fk, stubs, punch


se, sf, STUBS, PUNCH = make()

DEFAULTS = dict(exists=1, phase=0.4375, length=1.7, snd=0x5D5D, rint=1, side=0, frame=7, rand=0.3, b463180=0,
                rain=0)


def soldier(e, bits=0x4, prev=0, state=0, fields=(), conf=None):
    cfg.clear()
    cfg.update(DEFAULTS)
    cfg.update(conf or {})
    h = e.alloc(0xD50)
    table = e.alloc(4 * 40)
    e.write(table, struct.pack("<40I", *STUBS))
    e.write(h + 0x20, f64(10.5, 20.25, -3.0))
    e.write(h + 0x2EC, struct.pack("<IIIiiI", 0x1111, table, bits, prev, state, 0))
    e.write(h + 0x2E8, i32(9))
    e.write(h + 0x688, f32(1.5, 2.5, -3.5))
    e.write(h + 0x664, f32(9.0, 9.0, 9.0))
    e.write(h + 0x6A8, f64(1.0, 2.0, 3.0))
    e.write(h + 0xCF8, i32(2, 5))
    e.write(h + 0xD04, i32(33))
    e.write(h + 0xD28, f64(0.5, -0.25, 4.0))
    anims = e.alloc(16)
    e.write(anims, i32(1, 77, 88, 99))
    cfg["anims"] = anims
    for off, data in fields:
        if off == "ai":
            a = e.alloc(0x4000)
            e.write(h + 0x800, i32(a))
            for o, d in data:
                e.write(a + o, d)
        elif off == "ground":
            g = e.alloc(0x200)
            e.write(g + 0x114, b"land_dirt\0")
            for i in range(6):
                e.write(g + 0x54 + i * 0x20, b"step_%d\0" % i)
            e.write(h + 0x6EC, i32(g))
        else:
            e.write(h + off, data)
    return h


BITS = [0x1, 0x2, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x800, 0x10000, 0x20000, 0x40000, 0x80000, 0x100000,
        0x200000, 0x800000, 0x1000000, 0x2000000, 0x4000000, 0x8000000]
INPUTS = [0, 0x4] + [0x4 | b for b in BITS] + [0x4 | 0x80 | b for b in BITS] + [0x4 | 0x40000 | b for b in BITS] \
    + [0x4 | 0x40000 | 0x80 | b for b in BITS] + [0x4 | 0x800000 | 0x80, 0x4 | 0x1000000 | 0x800000]


def run(addr, name, bits=0x4, prev=0, state=0, fields=(), conf=None, cw=None):
    def setup(e):
        h = soldier(e, bits, prev, state, fields, conf)
        return h + 0x2EC, h
    check(se, sf, addr, name, setup=setup, cw=cw)


UPDATES = [
    (0x455A40, "SoldierRPG_StandUpdate"), (0x455BE0, "SoldierRPG_WalkUpdate"), (0x455D80, "SoldierRPG_RunUpdate"),
    (0x455EE0, "SoldierRPG_FireUpdate"), (0x456010, "SoldierRPG_RaiseUpdate"),
    (0x4560F0, "SoldierRPG_AimRaiseUpdate"), (0x4561A0, "SoldierRPG_AimLowerUpdate"),
    (0x456250, "SoldierRPG_AimUpdate"), (0x456380, "SoldierRPG_AimFireUpdate"),
    (0x4564B0, "SoldierRPG_ReloadUpdate"), (0x4565A0, "SoldierRPG_AimFireRaiseUpdate"),
    (0x4566A0, "SoldierRPG_FallUpdate"), (0x456920, "SoldierRPG_DieUpdate"), (0x456A10, "SoldierRPG_AnimUpdate"),
    (0x457A00, "SoldierPistol_StandUpdate"), (0x457D30, "SoldierPistol_State1Update"),
    (0x457FC0, "SoldierPistol_State17Update"), (0x4582C0, "SoldierPistol_State3Update"),
    (0x458810, "SoldierPistol_State11Update"),
]


@pytest.mark.parametrize("addr,name", UPDATES)
@pytest.mark.parametrize("bits", INPUTS)
def test_update(addr, name, bits):
    fields = [(0x1C8, f32(1.0 if bits & 0x200 else 0.0))]
    run(addr, name, bits, fields=fields)


@pytest.mark.parametrize("ground", [0, 1])
@pytest.mark.parametrize("snd", [0, 0x5D5D])
def test_rpg_fall_landing(ground, snd):
    run(0x4566A0, "SoldierRPG_FallUpdate", 0x4, fields=[("ground", ())] if ground else [], conf=dict(snd=snd))


@pytest.mark.parametrize("anim", [-1, 0, 12])
@pytest.mark.parametrize("bits", [0x4, 0x5, 0x4 | 0x80000, 0x4 | 0x40, 0x4 | 0x10])
def test_rpg_anim_update(anim, bits):
    run(0x456A10, "SoldierRPG_AnimUpdate", bits, fields=[(0xD04, i32(anim))])


@pytest.mark.parametrize("cw", CWS)
def test_rpg_die_update(cw):
    run(0x456920, "SoldierRPG_DieUpdate", 0x5, cw=cw)


SIMPLE_ENTERS = [
    (0x455BA0, "SoldierRPG_WalkEnter"), (0x455EB0, "SoldierRPG_FireEnter"), (0x455FE0, "SoldierRPG_RaiseEnter"),
    (0x4560C0, "SoldierRPG_AimRaiseEnter"), (0x456170, "SoldierRPG_AimLowerEnter"),
    (0x456220, "SoldierRPG_AimEnter"), (0x456350, "SoldierRPG_AimFireEnter"), (0x456480, "SoldierRPG_ReloadEnter"),
    (0x456570, "SoldierRPG_AimFireRaiseEnter"), (0x456650, "SoldierRPG_FallEnter"), (0x456770, "SoldierRPG_HitEnter"),
    (0x4581B0, "SoldierPistol_State18Enter"), (0x458510, "SoldierPistol_State4Enter"),
    (0x458540, "SoldierPistol_State5Enter"), (0x458570, "SoldierPistol_State6Enter"),
    (0x4585A0, "SoldierPistol_State7Enter"), (0x4585D0, "SoldierPistol_State8Enter"),
    (0x458600, "SoldierPistol_MeleeEnter"), (0x4587B0, "SoldierPistol_State10Enter"),
    (0x4587E0, "SoldierPistol_State11Enter"), (0x4588C0, "SoldierPistol_State12Enter"),
]


@pytest.mark.parametrize("addr,name", SIMPLE_ENTERS)
def test_simple_enter(addr, name):
    run(addr, name, state=6, fields=[(0x33D, b"\x07"), (0x696, b"\x07"), (0xD18, b"\x07\x07")])


PHASE_ENTERS = [
    (0x455D10, "SoldierRPG_RunEnter"), (0x457CB0, "SoldierPistol_State1Enter"),
    (0x457F50, "SoldierPistol_State17Enter"), (0x4581E0, "SoldierPistol_State2Enter"),
    (0x458250, "SoldierPistol_State3Enter"),
]


@pytest.mark.parametrize("addr,name", PHASE_ENTERS)
@pytest.mark.parametrize("prev", [-1, 0, 1, 2, 3, 4, 16, 17, 18])
@pytest.mark.parametrize("cw", CWS)
def test_phase_enter(addr, name, prev, cw):
    run(addr, name, state=2, prev=prev, cw=cw)


IDLE_ENTERS = [(0x455990, "SoldierRPG_StandEnter"), (0x457950, "SoldierPistol_StandEnter")]


@pytest.mark.parametrize("addr,name", IDLE_ENTERS)
@pytest.mark.parametrize("idle,exists", [(-1, 1), (5, 1), (5, 0)])
def test_idle_enter(addr, name, idle, exists):
    run(addr, name, fields=[(0xCFC, i32(idle))], conf=dict(exists=exists))


@pytest.mark.parametrize("anim,exists", [(-1, 1), (40, 1), (40, 0)])
def test_rpg_anim_enter(anim, exists):
    run(0x456960, "SoldierRPG_AnimEnter", state=14, fields=[(0xD04, i32(anim))], conf=dict(exists=exists))


@pytest.mark.parametrize("ai,weapon,kind,part,rint,side", [
    (5, 1, 2, 15, 0, 0), (-1, 0, 0, 3, 0, 0), (None, 1, 1, 3, 1, 1), (None, 1, 1, 3, 1, 0),
    (-1, 1, 2, 15, 1, 0), (-1, 1, 2, 16, 0, 0), (-1, 1, 2, 4, 0, 0), (-1, 1, 2, 4, 1, 0), (None, 0, 3, 4, 1, 0),
    (None, 0, -1, 4, 1, 0)])
def test_rpg_die_enter(ai, weapon, kind, part, rint, side):
    fields = [(0xD08, i32(kind)), (0x2E8, i32(part))]
    if ai is not None:
        fields.append(("ai", [(0x3AD0, i32(ai))]))
    sf.rets[0x4F2060] = 0x7070 if weapon else 0
    try:
        run(0x4567C0, "SoldierRPG_DieEnter", state=13, fields=fields, conf=dict(rint=rint, side=side))
    finally:
        sf.rets[0x4F2060] = 0x7070


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("anim,frame,done,rand,limit,target,handler,bits", [
    (0xC, 7, 0, 0.3, 0.5, 2, 1, 0x4), (0xC, 7, 0, 0.5, 0.5, 2, 1, 0x5), (0xC, 7, 0, 0.6, 0.5, 2, 1, 0x4),
    (0xC, 7, 1, 0.3, 0.5, 2, 1, 0x5), (0xC, 6, 0, 0.3, 0.5, 2, 1, 0x4), (0xB, 7, 0, 0.3, 0.5, 2, 1, 0x4),
    (0xC, 7, 0, 0.3, 0.5, 0, 1, 0x4), (0xC, 7, 0, 0.3, 0.5, 1, 1, 0x4), (0xC, 7, 0, 0.3, 0.5, 3, 1, 0x4),
    (0xC, 7, 0, 0.3, 0.5, 2, 0, 0x5), (0xC, 7, 0, 0.3, 0.5, 2, 1, 0x0), (0xC, 7, 0, 0.3, 0.5, 2, 1, 0x20004)])
def test_pistol_melee_update(cw, anim, frame, done, rand, limit, target, handler, bits):
    def setup(e):
        h = soldier(e, bits, conf=dict(frame=7, rand=rand))
        e.write(h + 0x538, i32(anim, frame))
        e.write(h + 0xD19, bytes([done]))
        ai = e.alloc(0x4000)
        e.write(h + 0x800, i32(ai))
        e.write(ai + 0x3DA8, f32(limit) + i32(0x41200000))
        tgt = e.alloc(0x40)
        e.write(tgt + 0x1C, struct.pack("<H", 0x55))
        e.write(0xA96AE0 + 4 * (0x21 * 0x180 + 0x55), i32(PUNCH if handler else 0))
        if target:
            obj = e.alloc(0x20)
            e.write(ai + 0x3AA8, i32(obj))
            if target >= 2:
                lst = e.alloc(8)
                e.write(obj + 0xC, i32(lst))
                if target >= 3:
                    e.write(lst, i32(tgt))
        return h + 0x2EC, h
    check(se, sf, 0x458630, "SoldierPistol_MeleeUpdate", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("anim,frame,step,ground,snd,b463180,rain", [
    (0x3F2, 7, 0, 1, 0x5D5D, 0, 0), (0x3F2, 7, 11, 1, 0x5D5D, 1, 1), (0x3F2, 7, 5, 1, 0, 0, 1),
    (0x3F2, 6, 0, 1, 0x5D5D, 0, 0), (0x3F1, 7, 0, 1, 0x5D5D, 0, 0), (0x3F2, 7, 3, 0, 0x5D5D, 0, 1)])
def test_pistol_footstep(cw, anim, frame, step, ground, snd, b463180, rain):
    def setup(e):
        fields = [(0x538, i32(anim, frame)), (0x84C, i32(step))]
        if ground:
            fields.append(("ground", ()))
        h = soldier(e, fields=fields, conf=dict(snd=snd, b463180=b463180, rain=rain, rand=0.71))
        return (h,)
    check(se, sf, 0x457810, "SoldierPistol_Footstep", setup=setup, cw=cw)
