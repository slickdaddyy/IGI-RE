"""SplineObjWaypoint handlers (0x5008F0 - 0x500EF0) with Python fakes around them."""
import random
import struct

import pytest

from emu import Emu
from test_computer_util import LOG, RET, add_child, cstr, f32, handler, logfake, run, set_handler, task, u32, with_cw
from test_vehicle_util import d64, floatfake

WP, OBJ, EVENT = 0x51, 0x50, 0x39
PT, MAT = 24, 40

emu = Emu()
logfake(emu, 0x418D70, 1)  # Task_Delete
logfake(emu, 0x4C5710, 3)  # RigidDynCubeObj_Init
logfake(emu, 0x4C6940, 4, derefs={2: PT})  # DynCube_Move
logfake(emu, 0x4C7140, 5, derefs={2: PT})  # DynCube_Add
logfake(emu, 0x4C7560, 2)  # DynCube_Remove
logfake(emu, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
logfake(emu, 0x4E43D0, 11, derefs={2: PT, 3: PT, 4: MAT, 5: MAT}, strings=(10,), masks={7: 0xFF})  # Segment_New
logfake(emu, 0x4E4530, 10, derefs={1: PT, 2: PT, 3: MAT, 4: MAT}, strings=(9,), masks={6: 0xFF})  # Segment_Setup
logfake(emu, 0x4F1A70, 0, ret=EVENT)  # Param_GetChangedEvent
logfake(emu, 0x501D70, 1)
floatfake(emu, 0x4D0950, 1, effect=lambda e, a: 2.5 + (a[0] & 0xFF) / 7.0)  # VirModel_GetRadius
CHANGED = handler(emu, "changed", 2)
CWS = [0x37F, 0x7F]


def segment(e, rng, length=None):
    s = e.alloc(0x5B8)
    start = rng.uniform(-50, 0)
    end = start + (length if length is not None else rng.uniform(1, 300))
    f32(e, s + 0xF8, rng.uniform(1, 500), start, end)
    return s


def waypoint(e, rng, seg=False, straight=0, auto=0, name=b"model", zero_rot=False):
    w = task(e, 0x1A8, WP)
    d64(e, w + 0x20, 1, 2, 3)
    u32(e, w + 0x68, 0x6868, 0x6C00 + rng.randrange(256))
    f32(e, w + 0x98, *[rng.uniform(0.5, 3) for _ in range(3)])
    if seg:
        u32(e, w + 0xF0, segment(e, rng))
    d64(e, w + 0xF8, *[rng.uniform(-5000, 5000) for _ in range(3)])
    rot = [0.0] * 9 if zero_rot else [rng.uniform(-1, 1) for _ in range(9)]
    f32(e, w + 0x110, *rot)
    u32(e, w + 0x134, 0x77)
    e.write(w + 0x178, name + b"\0")
    e.write(w + 0x188, b"segmodel\0")
    u32(e, w + 0x198, rng.randrange(1, 30), 0x19C19C)
    e.write(w + 0x1A0, bytes([0, straight, rng.randrange(2), 0x33, rng.randrange(2), auto]))
    return w


def spline(e, rng, n, maxseg=12, flags=(0, 0, 0, 0)):
    e.write(0xA7A620, struct.pack("<H", WP))
    u32(e, 0xA44344, 0x44344)
    set_handler(e, EVENT, WP, CHANGED)
    o = task(e, 0x68, OBJ)
    u32(e, o + 0x20, 0x2020, maxseg)
    f32(e, o + 0x28, 99.0)
    e.write(o + 0x4C, bytes(flags))
    f32(e, o + 0x50, *[rng.uniform(-1, 1) for _ in range(6)])
    other = task(e, 0x40, 0x52)
    add_child(e, o, other)
    return o


# --- Create -------------------------------------------------------------------

@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", ["alone", "prev_seg", "prev_noseg", "prev_zero", "next_end", "next_seg",
                                  "next_noseg", "next_zero"])
@pytest.mark.parametrize("seed", [1, 2])
def test_create(case, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        o = spline(e, rng, 0)
        zero = case.endswith("zero")
        w = waypoint(e, rng)
        if case.startswith("prev"):
            add_child(e, o, waypoint(e, rng, seg=case != "prev_noseg", zero_rot=zero))
            add_child(e, o, w)
        else:
            add_child(e, o, w)
            if case != "alone":
                add_child(e, o, waypoint(e, rng, zero_rot=zero))
            if case in ("next_seg", "next_noseg", "next_zero"):
                add_child(e, o, waypoint(e, rng, seg=case != "next_noseg"))
        RET[0x4CEC10] = 0x5CEC10
        return (w,)
    run(emu, with_cw(emu, 0x5008F0, cw), with_cw(emu, "SplineObjWaypoint_Create", cw), setup)


# --- Delete -------------------------------------------------------------------

@pytest.mark.parametrize("case", ["alone", "first", "middle", "last"])
@pytest.mark.parametrize("in_tree", [0, 1])
def test_delete(case, in_tree):
    def setup(e):
        rng = random.Random(3)
        o = spline(e, rng, 0)
        ws = [waypoint(e, rng, seg=i > 0) for i in range(3)]
        idx = {"alone": 0, "first": 0, "middle": 1, "last": 2}[case]
        for w in (ws[:1] if case == "alone" else ws):
            add_child(e, o, w)
        e.write(ws[idx] + 0x1A0, bytes([in_tree]))
        return (ws[idx],)
    run(emu, 0x500DE0, "SplineObjWaypoint_Delete", setup)


# --- Changed ------------------------------------------------------------------

CHANGED_CASES = [
    # (count, index, obj flags snap/collide/rigid/auto, straight flags, auto flags, in_tree, model found)
    (1, 0, (0, 0, 0, 0), None, None, 0, True),
    (3, 1, (0, 0, 0, 1), None, None, 0, True),
    (3, 0, (0, 1, 0, 1), None, None, 0, False),
    (3, 2, (1, 0, 0, 0), None, (0, 0, 1), 1, True),
    (3, 1, (1, 1, 1, 1), None, None, 1, True),
    (4, 2, (0, 1, 0, 0), None, (0, 0, 1, 0), 1, False),
    (5, 2, (0, 0, 0, 0), (0, 1, 1, 1, 0), None, 0, True),
    (6, 1, (0, 0, 0, 0), (0, 0, 1, 1, 0, 0), None, 1, True),
    (6, 3, (1, 1, 0, 0), (1, 1, 1, 1, 1, 1), None, 1, True),
    (4, 0, (0, 0, 0, 0), (0, 0, 0, 0), None, 0, True),
    (4, 1, (0, 0, 0, 1), (0, 1, 0, 0), None, 1, True),
]


def changed_setup(case, seed, name=b"model", first_seg=False):
    count, idx, flags, straight, auto, in_tree, found = case

    def setup(e):
        rng = random.Random(seed)
        o = spline(e, rng, count, maxseg=rng.choice([5, 12, 40]), flags=flags)
        ws = [waypoint(e, rng, seg=(i > 0 or first_seg) and rng.random() < 0.8, straight=straight[i] if straight else 0,
                       auto=auto[i] if auto else 0, name=name) for i in range(count)]
        for i, w in enumerate(ws):
            e.write(w + 0x1A0, bytes([in_tree if i == idx else rng.randrange(2)]))
            add_child(e, o, w)
        RET[0x4CEC10] = 0x5CEC10 if found else 0
        return (ws[idx],)
    return setup


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(1, 7))
@pytest.mark.parametrize("case", CHANGED_CASES)
def test_changed(case, seed, cw):
    run(emu, with_cw(emu, 0x500EF0, cw), with_cw(emu, "SplineObjWaypoint_Changed", cw), changed_setup(case, seed))


@pytest.mark.parametrize("name", [b"*model", b"", b"fifteen_chars_x"])
def test_changed_model_names(name):
    case = (2, 1, (0, 1, 0, 0), None, None, 0, False)
    run(emu, 0x500EF0, "SplineObjWaypoint_Changed", changed_setup(case, 4, name))


@pytest.mark.parametrize("seed", [1, 2, 3])
def test_changed_first_has_segment(seed):
    case = (3, 0, (0, 0, 0, 0), None, None, 0, True)
    run(emu, 0x500EF0, "SplineObjWaypoint_Changed", changed_setup(case, seed, first_seg=True))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("dirs", ["zero", "vertical"])
def test_changed_orient_degenerate(dirs, cw):
    run(emu, with_cw(emu, 0x500EF0, cw), with_cw(emu, "SplineObjWaypoint_Changed", cw), degenerate_setup(dirs))
    assert LOG[-1]


def degenerate_setup(dirs):
    def setup(e):
        rng = random.Random(5)
        o = spline(e, rng, 3, flags=(0, 0, 0, 1))
        ws = [waypoint(e, rng) for _ in range(3)]
        for i, w in enumerate(ws):
            # all on one spot, or stacked along z only
            d64(e, w + 0xF8, 10.0, -20.0, (0.0 if dirs == "zero" else 100.0 * i))
            add_child(e, o, w)
        RET[0x4CEC10] = 0x5CEC10
        return (ws[1],)
    return setup


@pytest.mark.parametrize("scales", [(1.0, 2.0, 1.5), (1.0, 1.5, 2.0), (2.5, 1.0, 2.0), (1.0, 1.0, 1.0)])
def test_changed_add_scale(scales):
    base = changed_setup((2, 1, (0, 1, 0, 0), None, None, 0, True), 1)

    def setup(e):
        args = base(e)
        f32(e, args[0] + 0x98, *scales)
        return args
    run(emu, 0x500EF0, "SplineObjWaypoint_Changed", setup)
