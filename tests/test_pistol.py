"""HumanSoldierPistol and HumanSoldier movement states, and shared helpers for the test_pistol tests."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import Emu, compare


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def i32(*v):
    return struct.pack("<%di" % len(v), *v)


class Fakes:
    """Python replacements of original functions that log their arguments."""

    def __init__(self, e):
        self.e = e
        self.log = []
        self.rets = {}
        self.tramps = {}

    def code(self, data):
        p = self.e.fake_top
        self.e.fake_top += (len(data) + 15) & ~15
        self.e.write(p, data)
        return p

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None, mask=None, effect=None):
        """deref: {arg: byte count} logs the bytes instead of the pointer; mask: {arg: mask};
        st0: 'f' or 'd' returns ret in st(0); ret may be callable(e, args)."""
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = None
        if st0:
            stub = self.code((b"\xd9\x05" if st0 == "f" else b"\xdd\x05") + struct.pack("<I", cell) + b"\xc3")
        fk = self

        def cb(uc, a, size, user):
            em = fk.e
            args = [em.arg(i) for i in range(nargs)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = []
                for i, v in enumerate(args):
                    if deref and i in deref:
                        rec.append(em.read(v, deref[i]))
                    elif mask and i in mask:
                        rec.append(v & mask[i])
                    else:
                        rec.append(em.normalize(v))
            fk.log.append((a, tuple(rec)))
            if effect:
                effect(em, args)
            r = fk.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<f" if st0 == "f" else "<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def stubs(self, n, nargs=2):
        """n logging functions, e.g. for a state table."""
        return [self.add(self.code(b"\xc3"), nargs) for _ in range(n)]


def with_cw(e, fk, target, cw):
    """Entry point that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    key = (addr, cw)
    if key not in fk.tramps:
        cell = fk.code(struct.pack("<H", cw))
        p = fk.code(b"\0" * 16)
        e.write(p, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        fk.tramps[key] = p
    return fk.tramps[key]


def check(e, fk, orig, ours, setup=None, cw=None, **kw):
    """compare() plus equal logs of the Python fakes."""
    logs = []
    if cw is not None:
        orig, ours = with_cw(e, fk, orig, cw), with_cw(e, fk, ours, cw)

    def s(em):
        r = setup(em) if setup else None
        fk.log = []
        logs.append(fk.log)
        return r
    kw.setdefault("check_eax", False)
    res = compare(e, orig, ours, setup=s, normalized=True, **kw)
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return res


CWS = [0x37F, 0x7F]
cfg = {}


def c(key):
    return lambda em, a: cfg[key]


def make():
    e = Emu()
    fk = Fakes(e)
    for a, n in ((0x489E70, 3), (0x4D61D0, 5), (0x4D65B0, 3), (0x413C80, 1), (0x4F2350, 2), (0x4F2390, 1),
                 (0x4F2070, 3), (0x4F2290, 1), (0x45D5B0, 4), (0x463760, 2), (0x464700, 1), (0x4635F0, 1),
                 (0x489EA0, 4), (0x48A330, 1), (0x48A320, 1), (0x4E6C30, 2), (0x4E6C60, 3), (0x4E6CA0, 2),
                 (0x4638A0, 3), (0x464720, 1), (0x463310, 2), (0x463220, 2)):
        fk.add(a, n)
    fk.add(0x4D74F0, 3, ret=c("exists"))
    fk.add(0x4AF810, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))
    fk.add(0x4D6520, 2, ret=c("phase"), st0="d")
    fk.add(0x4D64E0, 2, ret=c("length"), st0="d")
    fk.add(0x4F2060, 2, ret=c("weapon"))
    fk.add(0x4F23D0, 2, fmt=lambda em, a: (a[0],))
    fk.add(0x4F0EE0, 0, ret=0x5150)
    fk.add(0x4E6B00, 2, ret=c("snd"), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
    fk.add(0x489E10, 2, ret=c("anims"))
    fk.add(0x416D20, 0, ret=0x4444)
    fk.add(0x4B47C0, 2, ret=c("rint"))
    fk.add(0x45DAA0, 2, ret=c("side"))
    fk.add(0x4D64C0, 2, ret=c("frame"))
    fk.add(0x4B4770, 1, ret=lambda em, a: cfg["rand"].pop(0) if isinstance(cfg["rand"], list) else cfg["rand"],
           st0="d")
    fk.add(0x460C40, 0, ret=0x21)
    fk.add(0x463180, 1, ret=c("b463180"))
    fk.add(0x416FB0, 0, ret=c("rain"))
    fk.add(0x4A53B3, 3, fmt=lambda em, a: (em.read_cstr(a[1]), a[2]),
           effect=lambda em, a: em.write(a[0], b"walk_rain_%d\0" % a[2]))
    # use
    fk.add(0x4636E0, 1, ret=0x6E60)
    fk.add(0x444B30, 3, fmt=lambda em, a: (a[0],),
           effect=lambda em, a: (em.write(a[1], f64(*cfg["usepos"])), em.write(a[2], f32(*cfg["usemat"]))))
    fk.add(0x4631A0, 2, deref={1: 24})
    fk.add(0x4B3C50, 4, fmt=lambda em, a: (em.read(a[0], 36),),
           effect=lambda em, a: (em.write(a[1], f32(1.5)), em.write(a[2], f32(2.5)), em.write(a[3], f32(cfg["yaw"]))))
    # throw
    fk.add(0x4528F0, 1, ret=c("target"))
    fk.add(0x452930, 5, fmt=lambda em, a: (a[2], em.read(a[3], 24), a[4]),
           effect=lambda em, a: (em.write(a[0], bytes(range(40))), em.write(a[1], f32(0.25, -0.5, 0.75))))
    fk.add(0x413BD0, 1, ret=0x7777)
    fk.add(0x477040, 7, fmt=lambda em, a: (a[0], a[1], em.read(a[2], 24), em.read(a[3], 40), em.read(a[4], 12),
                                           a[5], a[6]))
    stubs = fk.stubs(40)
    punch = fk.add(fk.code(b"\xc3"), 2, deref={1: 8})
    e.baseline = e.snapshot()
    return e, fk, stubs, punch


se, sf, STUBS, PUNCH = make()

DEFAULTS = dict(exists=1, phase=0.4375, length=1.7, snd=0x5D5D, rint=1, side=0, frame=7, rand=0.3, b463180=0,
                rain=0, weapon=0x7070, usepos=(100.5, -20.25, 7.0), usemat=(0.8, -0.6, 0.0, 0.6, 0.8, 0.0, 0, 0, 1),
                yaw=1.25, target=1)


def soldier(e, bits=0x4, prev=0, state=0, fields=(), conf=None):
    cfg.clear()
    cfg.update(DEFAULTS)
    cfg.update({k: list(v) if isinstance(v, list) else v for k, v in (conf or {}).items()})
    h = e.alloc(0xD50)
    table = e.alloc(4 * 40)
    e.write(table, struct.pack("<40I", *STUBS))
    e.write(h + 0x20, f64(10.5, 20.25, -3.0))
    e.write(h + 0x2EC, struct.pack("<IIIiiI", 0x1111, table, bits, prev, state, 0))
    e.write(h + 0x2E8, i32(9))
    e.write(h + 0x688, f32(1.5, 2.5, -3.5))
    e.write(h + 0x664, f32(9.0, 9.0, 9.0))
    e.write(h + 0x6A8, f64(1.0, 2.0, 3.0))
    e.write(h + 0x4EC, i32(0x4EC0))
    e.write(h + 0xCF8, i32(2, 5))
    e.write(h + 0xD04, i32(33))
    e.write(h + 0xD38, f64(0.5, -0.25, 4.0))
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
        elif off == "motion":
            m = e.alloc(0x900)
            e.write(h + 0x70, i32(m))
            for o, d in data:
                e.write(m + o, d)
        else:
            e.write(h + off, data)
    return h


BITS = [0x1, 0x2, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x800, 0x10000, 0x20000, 0x40000, 0x80000, 0x100000,
        0x200000, 0x800000, 0x1000000, 0x2000000, 0x4000000, 0x8000000]
INPUTS = [0, 0x4] + [0x4 | b for b in BITS] + [0x4 | 0x80 | b for b in BITS] + [0x4 | 0x40000 | b for b in BITS] \
    + [0x4 | 0x1 | b for b in BITS] + [0x4 | 0x1 | 0x80 | b for b in BITS] \
    + [0x4 | 0x80010 | b for b in BITS] + [0x4 | 0x1000000 | 0x80 | 0x1, 0x4 | 0x1000000 | 0x800000]


def run(addr, name, bits=0x4, prev=0, state=0, fields=(), conf=None, cw=None):
    def setup(e):
        h = soldier(e, bits, prev, state, fields, conf)
        return h + 0x2EC, h
    check(se, sf, addr, name, setup=setup, cw=cw)


UPDATES = [
    (0x458950, "SoldierPistol_State14Update"), (0x458A30, "SoldierPistol_State15Update"),
    (0x458B10, "SoldierPistol_State16Update"), (0x458BF0, "SoldierPistol_State19Update"),
    (0x458DD0, "SoldierPistol_State13Update"), (0x458EB0, "SoldierPistol_State22Update"),
    (0x459040, "SoldierPistol_State25Update"), (0x459180, "SoldierPistol_State28Update"),
    (0x4592F0, "SoldierPistol_HitUpdate"), (0x459680, "SoldierPistol_DieUpdate"),
    (0x45AB00, "Soldier_State18Update"), (0x45AD10, "Soldier_State2Update"), (0x45B060, "Soldier_State6Update"),
    (0x45B1C0, "Soldier_State7Update"), (0x45B6E0, "Soldier_State10Update"), (0x45B820, "Soldier_State12Update"),
]


@pytest.mark.parametrize("addr,name", UPDATES)
@pytest.mark.parametrize("bits", INPUTS)
def test_update(addr, name, bits):
    fields = [(0x1C8, f32(1.0 if bits & 0x200 else 0.0))]
    run(addr, name, bits, fields=fields)


@pytest.mark.parametrize("addr,name", [(0x458EB0, "SoldierPistol_State22Update"), (0x45AB00, "Soldier_State18Update"),
                                       (0x45B060, "Soldier_State6Update")])
@pytest.mark.parametrize("fire", [0.0, -0.0, 1.0, float("nan")])
def test_fire_input(addr, name, fire):
    run(addr, name, 0x4, fields=[(0x1C8, f32(fire))])


@pytest.mark.parametrize("cw", CWS)
def test_die_update(cw):
    run(0x459680, "SoldierPistol_DieUpdate", 0x5, cw=cw)


SIMPLE_ENTERS = [
    (0x4588F0, "SoldierPistol_State13Enter"), (0x458920, "SoldierPistol_State14Enter"),
    (0x458A00, "SoldierPistol_State15Enter"), (0x458AE0, "SoldierPistol_State16Enter"),
    (0x458BC0, "SoldierPistol_State19Enter"), (0x458D70, "SoldierPistol_State20Enter"),
    (0x458DA0, "SoldierPistol_State21Enter"), (0x458E80, "SoldierPistol_State22Enter"),
    (0x458FB0, "SoldierPistol_State23Enter"), (0x458FE0, "SoldierPistol_State24Enter"),
    (0x459010, "SoldierPistol_State25Enter"), (0x4590F0, "SoldierPistol_State26Enter"),
    (0x459120, "SoldierPistol_State27Enter"), (0x459150, "SoldierPistol_State28Enter"),
    (0x459250, "SoldierPistol_FallEnter"), (0x4592A0, "SoldierPistol_HitEnter"),
    (0x459380, "SoldierPistol_State31Enter"), (0x459850, "SoldierPistol_State37Enter"),
    (0x45AAD0, "Soldier_State18Enter"), (0x45AFD0, "Soldier_State4Enter"), (0x45B000, "Soldier_State5Enter"),
    (0x45B030, "Soldier_State6Enter"), (0x45B190, "Soldier_State7Enter"), (0x45B2C0, "Soldier_ThrowEnter"),
    (0x45B500, "Soldier_MeleeEnter"), (0x45B6B0, "Soldier_State10Enter"), (0x45B7C0, "Soldier_State11Enter"),
    (0x45B7F0, "Soldier_State12Enter"),
]


@pytest.mark.parametrize("addr,name", SIMPLE_ENTERS)
def test_simple_enter(addr, name):
    run(addr, name, state=6, fields=[(0x33D, b"\x07"), (0x696, b"\x07"), (0xD14, i32(7)), (0xD18, b"\x07\x07")])


PHASE_ENTERS = [
    (0x45A9E0, "Soldier_State1Enter"), (0x45AA60, "Soldier_State17Enter"), (0x45ACA0, "Soldier_State2Enter"),
    (0x45AF60, "Soldier_State3Enter"),
]


@pytest.mark.parametrize("addr,name", PHASE_ENTERS)
@pytest.mark.parametrize("prev", [-1, 0, 1, 2, 3, 4, 16, 17, 18, 33, -31])
@pytest.mark.parametrize("cw", CWS)
def test_phase_enter(addr, name, prev, cw):
    run(addr, name, state=2, prev=prev, cw=cw)


@pytest.mark.parametrize("idle,exists", [(-1, 1), (5, 1), (5, 0)])
def test_stand_enter(idle, exists):
    run(0x45A930, "Soldier_StandEnter", fields=[(0xCFC, i32(idle))], conf=dict(exists=exists))


@pytest.mark.parametrize("anim,exists", [(-1, 1), (40, 1), (40, 0)])
def test_anim_enter(anim, exists):
    run(0x4596C0, "SoldierPistol_AnimEnter", state=35, fields=[(0xD0C, i32(anim))], conf=dict(exists=exists))


@pytest.mark.parametrize("ai,weapon,kind,part,rint,side", [
    (5, 1, 2, 15, 0, 0), (-1, 0, 0, 3, 0, 0), (None, 1, 1, 3, 1, 1), (None, 1, 1, 3, 1, 0),
    (-1, 1, 2, 15, 1, 0), (-1, 1, 2, 16, 0, 0), (-1, 1, 2, 4, 0, 0), (-1, 1, 2, 4, 1, 0), (None, 0, 3, 4, 1, 0),
    (None, 0, -1, 4, 1, 0)])
def test_die_enter(ai, weapon, kind, part, rint, side):
    fields = [(0xD1C, i32(kind)), (0x2E8, i32(part))]
    if ai is not None:
        fields.append(("ai", [(0x3AD0, i32(ai))]))
    run(0x4593C0, "SoldierPistol_DieEnter", state=32, fields=fields,
        conf=dict(rint=rint, side=side, weapon=0x7070 if weapon else 0))


@pytest.mark.parametrize("addr,name", [(0x459520, "SoldierPistol_Die2Enter"), (0x4595D0, "SoldierPistol_Die3Enter")])
@pytest.mark.parametrize("weapon", [0, 0x7070])
def test_die_fixed_enter(addr, name, weapon):
    run(addr, name, state=33, conf=dict(weapon=weapon))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("pos,mat,yaw", [
    ((100.5, -20.25, 7.0), (0.8, -0.6, 0.0, 0.6, 0.8, 0.0, 0, 0, 1), 1.25),
    ((-3e5, 1e-3, 1e9), (0.1234567, 0.7654321, 0.3, -0.333333, -0.987654, 0.2, 0, 0, 1), -3.0),
    ((1.0, 2.0, 3.0), (0, 1.0e-7, 0, 0, 3.3333333, 0, 0, 0, 1), 3.1415925)])
def test_use_enter(cw, pos, mat, yaw):
    run(0x459750, "SoldierPistol_UseEnter", state=36, conf=dict(usepos=pos, usemat=mat, yaw=yaw), cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("anim,frame,step,ground,snd,b463180,rain", [
    (0x3F2, 7, 0, 1, 0x5D5D, 0, 0), (0x3F2, 7, 11, 1, 0x5D5D, 1, 1), (0x3F2, 7, 5, 1, 0, 0, 1),
    (0x3F2, 6, 0, 1, 0x5D5D, 0, 0), (0x3F1, 7, 0, 1, 0x5D5D, 0, 0), (0x3F2, 7, 3, 0, 0x5D5D, 0, 1),
    (0x3F2, 7, 7, 1, 0x5D5D, 1, 0)])
def test_footstep(cw, anim, frame, step, ground, snd, b463180, rain):
    def setup(e):
        fields = [(0x538, i32(anim, frame)), (0x84C, i32(step))]
        if ground:
            fields.append(("ground", ()))
        h = soldier(e, fields=fields, conf=dict(snd=snd, b463180=b463180, rain=rain, rand=0.71))
        return (h,)
    check(se, sf, 0x45A7F0, "Soldier_Footstep", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("target,anim,frame,thrown,bits,rand", [
    (1, 0xB, 7, 0, 0x4, [0.3, 0.6]), (1, 0xB, 7, 0, 0x5, [0.99, 0.01]), (1, 0xB, 7, 0, 0x2000005, [0.5, 0.5]),
    (0, 0xB, 7, 0, 0x5, [0.3, 0.6]), (1, 0xC, 7, 0, 0x4, [0.3, 0.6]), (1, 0xB, 6, 0, 0x4, [0.3, 0.6]),
    (1, 0xB, 7, 1, 0x2000005, [0.3, 0.6]), (2, 0xB, 7, 0, 0x4, [0.3, 0.6]), (1, 0xB, 7, 0, 0x0, [0.3, 0.6]),
    (1, 0xB, 7, 0, 0x10004, [0.3, 0.6]), (1, 0xB, 7, 0, 0x20004, [0.3, 0.6])])
def test_throw_update(cw, target, anim, frame, thrown, bits, rand):
    fields = [(0x538, i32(anim, frame)), (0xD18, bytes([thrown])),
              ("ai", [(0x3D98, f32(300.0, 512.5, 1.0, 2.25)), (0x3E04, i32(0x53E618))]),
              ("motion", [(0x858, f32(12.5, -40.25, 160.0))])]
    run(0x45B2F0, "Soldier_ThrowUpdate", bits, fields=fields, conf=dict(target=target, frame=7, rand=list(rand)),
        cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("anim,frame,done,rand,limit,target,handler,bits", [
    (0xC, 7, 0, 0.3, 0.5, 2, 1, 0x4), (0xC, 7, 0, 0.5, 0.5, 2, 1, 0x5), (0xC, 7, 0, 0.6, 0.5, 2, 1, 0x4),
    (0xC, 7, 1, 0.3, 0.5, 2, 1, 0x5), (0xC, 6, 0, 0.3, 0.5, 2, 1, 0x4), (0xB, 7, 0, 0.3, 0.5, 2, 1, 0x4),
    (0xC, 7, 0, 0.3, 0.5, 0, 1, 0x4), (0xC, 7, 0, 0.3, 0.5, 1, 1, 0x4), (0xC, 7, 0, 0.3, 0.5, 3, 1, 0x4),
    (0xC, 7, 0, 0.3, 0.5, 2, 0, 0x5), (0xC, 7, 0, 0.3, 0.5, 2, 1, 0x0), (0xC, 7, 0, 0.3, 0.5, 2, 1, 0x20004),
    (0xC, 7, 0, float("nan"), 0.5, 2, 1, 0x4)])
def test_melee_update(cw, anim, frame, done, rand, limit, target, handler, bits):
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
    check(se, sf, 0x45B530, "Soldier_MeleeUpdate", setup=setup, cw=cw)
