"""AIDetection: the detection list, mask types and detectors."""
import math
import random
import struct

import pytest

from emu import Emu
from test_levelflow_util import LOG, RET, d64, f32, logfake, run, set_handler, task, trap, u16, u32, vfake

LIST = 0x5C8AC0
MASKS = 0x5C8AC4
PHASE = 0x5C8B08
EV1 = 0x54127D
INFO_SLOT = 0x3C

emu = Emu()
logfake(emu, 0x4017C0, 1, masks={0: 0xFFFF}, effect=lambda e, a: RET["events"].pop(0))  # TaskEvent_Alloc
logfake(emu, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
logfake(emu, 0x4C1830, 1)  # QTaskList_Free
logfake(emu, 0x4C1790, 2)  # QTaskList_Add
logfake(emu, 0x4C17C0, 2)  # QTaskList_Remove
vfake(emu, 0x4C1800, 1, effect=lambda e, a: e.alloc(8 + 4 * a[0]))  # QTaskList_New
logfake(emu, 0x4AF810, 1, strings=(0,))  # Warning_Show
logfake(emu, 0x4028B0, 0, effect=lambda e, a: RET.get("ticks", 0))  # Timer_GetTicks
logfake(emu, 0x401B20, 1)  # TaskRef_Remove


def ref_add(e, a):
    r = e.alloc(4)
    u32(e, r, a[0])
    return r


vfake(emu, 0x401AE0, 1, effect=ref_add)  # TaskRef_Add
logfake(emu, 0x489B20, 4, derefs={3: 24}, effect=lambda e, a: RET["los"].pop(0) if RET.get("los") else 0)


def get_info(e):
    t, out = e.arg(0), e.arg(1)
    if LOG:
        LOG[-1].append(("info", e.normalize(t)))
    e.write(out, RET["info"][t])
    e.ret(0)


INFO = trap(emu, "info", get_info)
emu.baseline = emu.snapshot()


def test_init():
    def setup(e):
        RET["events"] = [0x1241, 0x1242, 0x1243]
        u32(e, 0x5C8B0C, 1, 2, 3)
    run(emu, 0x489560, "AIDetection_Init", setup)


def test_shutdown():
    def setup(e):
        e.write(0x54127C, b"\x41\x42\x43")
        u32(e, LIST, 0x7300)
    run(emu, 0x4895D0, "AIDetection_Shutdown", setup)


@pytest.mark.parametrize("addr,name", [(0x489610, "AIDetection_GetEvent0"), (0x489620, "AIDetection_GetEvent1"),
                                       (0x489630, "AIDetection_GetEvent2")])
def test_get_event(addr, name):
    run(emu, addr, name, lambda e: e.write(0x54127B, b"\x10\x41\x42\x43\x44"), check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("used", [0, 1, 5, 31, 32])
def test_add_mask_type(used):
    def setup(e):
        u16(e, MASKS, *([0x50 + i for i in range(used)] + [0x181] * (32 - used)))
        return (0xABCD0077,)
    run(emu, 0x489640, "AIDetection_AddMaskType", setup, check_eax=True)


def test_add_mask_type_hole():
    def setup(e):
        u16(e, MASKS, *([0x50] * 7 + [0x181] + [0x51] * 24))
        return (0x77,)
    run(emu, 0x489640, "AIDetection_AddMaskType", setup, check_eax=True)


@pytest.mark.parametrize("addr,name", [(0x489680, "AIDetection_Add"), (0x4896A0, "AIDetection_Remove")])
def test_add_remove(addr, name):
    def setup(e):
        u32(e, LIST, 0x7300)
        return (0x7400,)
    run(emu, addr, name, setup)


DETECTOR = 0x30 + 0xC0 * 0x30


def detector(e, flags=0xF, mask=0x6, phase=1):
    d = e.alloc(DETECTOR)
    e.write(d, bytes((i * 13 + 5) & 0xFF for i in range(DETECTOR)))
    u32(e, d, 0x5500, 3, 5, phase, flags, mask)
    for lst in (d + 0x18, d + 0x24):
        u32(e, lst, lst + 4, 0, lst)
    for i in range(0xC0):
        e.write(d + 0x30 + 0x30 * i + 0x2A, b"\0")
    return d


@pytest.mark.parametrize("phase", [0, 3, 0x7FFFFFFF, -1, -6])
@pytest.mark.parametrize("a,b", [(0, 0), (4, 8), (7, -7), (-1, 13)])
def test_detector_init(phase, a, b):
    def setup(e):
        u32(e, PHASE, phase)
        d = e.alloc(DETECTOR)
        e.write(d, bytes((i * 13 + 5) & 0xFF for i in range(DETECTOR)))
        return d, 0x5500, 0xB, 0x6, a, b
    run(emu, 0x4896C0, "AIDetector_Init", setup)


def add_slot(e, d, i, target, lst):
    s = d + 0x30 + 0x30 * i
    ref = e.alloc(4)
    u32(e, ref, target)
    u32(e, s + 8, ref)
    u32(e, s + 0x10, 0, 0)
    e.write(s + 0x2A, b"\1")
    e.call(0x4AF8F0, lst, s)
    return s


@pytest.mark.parametrize("i", [0, 5])
@pytest.mark.parametrize("friends", [0, 1])
def test_add_slot(i, friends):
    def setup(e):
        RET["ticks"] = 0x1234
        d = detector(e)
        add_slot(e, d, 2, 0x7100, d + 0x18)
        return d, d + (0x24 if friends else 0x18), 0x7200, i
    run(emu, 0x489500, "AIDetector_AddSlot", setup, check_eax=True)


@pytest.mark.parametrize("which", [0, 1, 2])
def test_remove_slot(which):
    def setup(e):
        d = detector(e)
        slots = [add_slot(e, d, i, 0x7100 + i, d + 0x18) for i in range(3)]
        return (slots[which],)
    run(emu, 0x4894E0, "AIDetector_RemoveSlot", setup)


def view(e, pos=(1000.0, -2000.0, 300.0), yaw=0.3, pitch=-0.1, limits=(0.6, 1.2, 40000.0), second=None):
    v = e.alloc(0x78)
    e.write(v, bytes(0x78))
    d64(e, v, *pos)
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    f32(e, v + 0x18, cy, -sy * cp, sy * sp, sy, cy * cp, -cy * sp, 0.0, sp, cp)
    d64(e, v + 0x40, *limits)
    if second:
        d64(e, v + 0x58, *second)
        e.write(v + 0x70, b"\1")
    return v


def vec(e, *p):
    a = e.alloc(24)
    d64(e, a, *p)
    return a


def random_cases(n, seed, spread, wide):
    rng = random.Random(seed)
    out = []
    for _ in range(n):
        pos = tuple(rng.uniform(-60000, 60000) for _ in range(3))
        target = tuple(p + rng.uniform(-spread, spread) for p in pos)
        limits = (rng.uniform(wide, 1.6), rng.uniform(wide, 3.2), rng.uniform(1000, 70000))
        second = (rng.uniform(wide, 1.6), rng.uniform(wide, 3.2), rng.uniform(1000, 90000))
        if rng.random() >= 0.6:
            second = None
        out.append((pos, target, rng.uniform(-3, 3), rng.uniform(-1, 1), limits, second))
    return out


CASES = random_cases(30, 7, 50000, 0.05) + random_cases(40, 8, 15000, 0.8) + [
    ((0.0, 0.0, 0.0), (100.0, 0.0, 0.0), 0.0, 0.0, (0.5, 0.5, 1000.0), None),
    ((0.0, 0.0, 0.0), (1000.0, 0.0, 0.0), 0.0, 0.0, (0.5, 0.5, 1000.0), None),
    ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), 0.0, 0.0, (0.5, 0.5, 1000.0), None),
    ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), 0.0, 0.0, (0.0, 0.0, 0.0), (0.5, 0.5, 10.0)),
    ((5.0, 5.0, 5.0), (5.0, 4000.0, -300.0), 1.0, 0.2, (3.0, 3.0, 100000.0), None),
]


def boundary_cases():
    """Identity view at the origin with limits right at the target's angles and distance."""
    rng = random.Random(3)
    out = []
    for i in range(12):
        t = (rng.uniform(-9000, 9000), rng.uniform(100, 9000), rng.uniform(-9000, 9000))
        exact = (abs(math.atan2(t[2], t[1])), abs(math.atan2(-t[0], t[1])), math.sqrt(sum(c * c for c in t)))
        scale = (1.0, 1 + 1e-9, 1 - 1e-9, 1 + 1e-7, 1 - 1e-7, 1 + 1e-6)[i % 6]
        limits = tuple(x * scale for x in exact)
        second = tuple(x * (2 - scale) for x in exact) if i % 2 else None
        out.append(((0.0, 0.0, 0.0), t, 0.0, 0.0, limits, second))
    return out


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("pos,target,yaw,pitch,limits,second", CASES + boundary_cases())
def test_view_contains(cw, pos, target, yaw, pitch, limits, second):
    def setup(e):
        return view(e, pos, yaw, pitch, limits, second), vec(e, *target)
    run(emu, 0x489960, "AIView_Contains", setup, cw=cw, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("view_timer,sight_timer", [(0, 0), (2, 0), (0, 3), (1, 1)])
@pytest.mark.parametrize("target,in_view", [((5000.0, -1000.0, 400.0), 0), ((90000.0, 0.0, 0.0), 1),
                                            ((1000.0, -2000.0, 300.0), 0)])
@pytest.mark.parametrize("los", [0, 1])
def test_update_slot(cw, view_timer, sight_timer, target, in_view, los):
    def setup(e):
        RET["los"] = [los]
        d = detector(e)
        s = add_slot(e, d, 4, 0x7100, d + 0x18)
        u32(e, s + 0x10, view_timer, sight_timer)
        e.write(s + 0x28, bytes([in_view, 1 - los]))
        return d, s, vec(e, *target), view(e), 0x7900
    run(emu, 0x4898D0, "AIDetector_UpdateSlot", setup, cw=cw)


def info(team=1, alive=1, valid=1, mask=0x6, pos=(5000.0, -1000.0, 400.0)):
    return struct.pack("<II3dIBB2x", mask, 0xEEEE, *pos, team, alive, valid)


def targets(e, entries):
    """entries: list of (task, info bytes or None) on the detection list."""
    lst = e.alloc(8 + 4 * len(entries) + 4)
    u32(e, lst, len(entries), 0xC0, *[t for t, _ in entries])
    u32(e, LIST, lst)
    RET["info"] = {}
    for t, inf in entries:
        if inf is not None:
            RET["info"][t] = inf


TEAM = 1
UPDATE_CASES = [
    # (flags, entries as (kind, info kwargs), active slots as (index, target index or other))
    (0xF, [("self", None), ("t", {}), ("t", dict(team=2)), ("t", dict(alive=0)), ("t", dict(team=2, alive=0))], []),
    (0x5, [("t", {}), ("t", dict(team=2)), ("t", dict(alive=0)), ("t", dict(team=2, alive=0))], [(2, 2), (3, 3)]),
    (0xA, [("t", {}), ("t", dict(team=2)), ("t", dict(alive=0)), ("t", dict(team=2, alive=0))], [(0, 0)]),
    (0x3, [("t", dict(valid=0)), ("t", dict(mask=0x10)), ("t", dict(team=2)), ("t", {})], [(0, 0), (1, 1), (2, 2)]),
    (0xC, [("t", {}), ("t", dict(team=2, pos=(90000.0, 0.0, 0.0))), ("self", None)], [(1, "other"), (2, "other")]),
    (0xF, [("t", dict(pos=(1000.0, -2000.0, 300.0))), ("t", dict(team=2))], [(0, 0), (1, "other")]),
    (0x0, [("t", {}), ("t", dict(team=2))], [(1, 1)]),
]


@pytest.mark.parametrize("flags,entries,active", UPDATE_CASES)
@pytest.mark.parametrize("ticks,phase", [(5, 1), (6, 1), (-3, -3), (-3, 1)])
def test_update(flags, entries, active, ticks, phase):
    def setup(e):
        RET.update(ticks=ticks, los=[1, 0, 1, 1, 0, 1])
        e.write(EV1, bytes([INFO_SLOT]))
        d = detector(e, flags=flags, phase=phase)
        self_ = task(e, 0x40, 0x70)
        ts = []
        for kind, kw in entries:
            if kind == "self":
                ts.append((self_, None))
            else:
                t = task(e, 0x40, 0x71)
                ts.append((t, info(**kw)))
        set_handler(e, INFO_SLOT, 0x71, INFO)
        targets(e, ts)
        for i, which in active:
            tgt = 0x7F00 if which == "other" else ts[which][0]
            add_slot(e, d, i, tgt, d + 0x18)
        return d, view(e), self_, TEAM
    run(emu, 0x489750, "AIDetector_Update", setup)
