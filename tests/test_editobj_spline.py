"""SplineObj and SplineObjWaypoint (0x501D70 - 0x5031B0)."""
import pytest

from emu import compare
from test_editobj_util import (CHAIN, CWS, RET, add_child, chain_fakes, cstr, d64, f32, floatfake, handler, init_emu, sprintf_fake,
                               logfake, mat, obj, rnd, run, set_events, set_handler, setup_types, task, u16, u32)

SPLINE, WAYPOINT, OTHER, RIGID = 0x60, 0x61, 0x62, 0x63

emu = init_emu()
logfake(emu, 0x401CC0, 2, masks={1: 0xFFFF})  # Task_FindAncestor
logfake(emu, 0x4F1030, 1)  # Task_FindById
chain_fakes(emu)
floatfake(emu, 0x4D0950, 1)  # VirModel_GetRadius
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4C1830, 1)  # QTaskList_Free
sprintf_fake(emu)
set_events(emu)
u16(emu, 0xA7A620, WAYPOINT)
u16(emu, 0x547F42, RIGID)
u32(emu, 0xA7A61C, 0x61C61C, 0x7A508)
u32(emu, 0xA7A508, 0x7A508)
for slot in (8, 9, 10, 11, 0x31):
    for t in (WAYPOINT, OTHER, RIGID):
        set_handler(emu, slot, t, handler(emu, "%x_%x" % (slot, t), 2, derefs={1: 4}))
emu.baseline = emu.snapshot()


def waypoint(e, rng, parent=0, seg=None, unk=3):
    w = obj(e, 0x1A8, WAYPOINT, rng, parent=parent)
    if seg is not None:
        s = e.alloc(0x110)
        f32(e, s + 0xF8, *seg)
        u32(e, w + 0xF0, s)
    d64(e, w + 0xF8, rng.uniform(-5000, 5000), rng.uniform(-5000, 5000), rng.uniform(-500, 500))
    mat(e, w + 0x110, rng, unk)
    mat(e, w + 0x150, rng, 9)
    d64(e, w + 0x138, 1.0, 2.0, 3.0)
    return w


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("found", [False, True])
@pytest.mark.parametrize("seed", [1, 2])
def test_update_attach(found, seed, cw):
    def setup(e):
        rng = rnd(seed)
        a = obj(e, 0xF0, RIGID, rng)
        RET[0x401CC0] = a if found else 0
        return (waypoint(e, rng, parent=0x1234),)
    run(emu, 0x501D70, "SplineObjWaypoint_UpdateAttach", setup, cw=cw)


def spline(e, rng, segs, kids=()):
    o = task(e, 0x68, SPLINE)
    CHAIN[:] = []
    for s in segs:
        w = waypoint(e, rng, seg=s)
        add_child(e, o, w)
        CHAIN.append(w)
    for k in kids:
        add_child(e, o, task(e, 0x40, k))
    return o


SEGS = [[], [None], [(10.5, 1.0, 2.0)], [(10.5, 1.0, 2.0), None, (3.25, 0.5, 9.0), (1e-3, 0, 0), (777.7, 0, 0)]]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("segs", SEGS)
@pytest.mark.parametrize("found", [False, True])
def test_get_length(segs, found, cw):
    def setup(e):
        o = spline(e, rnd(3), segs)
        RET[0x4F1030] = o if found else 0
        return (7,)
    run(emu, 0x5020C0, "SplineObj_GetLength", setup, cw=cw, check_st0=True)


@pytest.mark.parametrize("segs", SEGS)
@pytest.mark.parametrize("found", [False, True])
def test_get_waypoint_count(segs, found):
    def setup(e):
        o = spline(e, rnd(3), segs)
        RET[0x4F1030] = o if found else 0
        return (7,)
    run(emu, 0x502130, "SplineObj_GetWaypointCount", setup, check_eax=True)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("index", [-1, 0, 2, 9])
@pytest.mark.parametrize("found", [False, True])
def test_get_length_to(index, found, cw):
    def setup(e):
        o = spline(e, rnd(4), SEGS[3])
        RET[0x4F1030] = o if found else 0
        return 7, index
    run(emu, 0x502180, "SplineObj_GetLengthTo", setup, cw=cw, check_st0=True)


@pytest.mark.parametrize("index", [0, 1, 4])
def test_get_waypoint_pos(index):
    def setup(e):
        o = spline(e, rnd(5), SEGS[3])
        RET[0x4F1030] = o
        return e.alloc(24), 7, index
    run(emu, 0x502200, "SplineObj_GetWaypointPos", setup)


def test_create():
    run(emu, 0x5026E0, "SplineObj_Create", lambda e: (task(e, 0x68, SPLINE),))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("arg", [None, b"", b"x"])
@pytest.mark.parametrize("angle", [(1.0, 1.0), (1.5, 0.25), (float("nan"), 1.0), (-3.0, 2.0)])
@pytest.mark.parametrize("pos", [(0, 0, 0), (100.5, 0, 0), (0, 0, -7.25), (1.5, -2.5, 3.5)])
@pytest.mark.parametrize("count", [0, 3])
def test_changed(arg, angle, pos, count, cw):
    def setup(e):
        rng = rnd(6)
        o = spline(e, rng, [None] * count, kids=(OTHER,))
        for i, w in enumerate(CHAIN):
            u32(e, w + 0x134, 999 if i else 1200)
        u32(e, o + 0x20, -5 if count else 4)
        f32(e, o + 0x2C, *pos)
        f32(e, o + 0x38, 0.0, 0.0, 0.0)
        f32(e, o + 0x44, *angle)
        return o, cstr(e, arg) if arg is not None else 0
    run(emu, 0x502740, "SplineObj_Changed", setup, cw=cw)


@pytest.mark.parametrize("orig,ours,glob", [(0x502E80, "SplineObj_GetParams", 0xA7A61C),
                                            (0x502F90, "SplineObjWaypoint_GetParams", 0xA7A508)])
def test_get_params(orig, ours, glob):
    run(emu, orig, ours, lambda e: (0, e.alloc(4)))


SCALES = [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 2.0, 2.5), (2.0, 2.0, 2.0), (float("nan"), 1.0, 2.0),
          (1.0, float("nan"), 2.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("scale", SCALES)
@pytest.mark.parametrize("count", [0, 2])
def test_get_info(scale, count, cw):
    def setup(e):
        RET[0x4D0950] = 3.3
        o = spline(e, rnd(7), [None] * count)
        for w in CHAIN:
            f32(e, w + 0x98, *scale)
        out = e.alloc(0x54)
        e.write(out, b"\xAA" * 0x54)
        return o, out
    run(emu, 0x502E90, "SplineObj_GetInfo", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("scale", SCALES)
def test_waypoint_get_info(scale, cw):
    def setup(e):
        RET[0x4D0950] = 1.7
        w = waypoint(e, rnd(8))
        f32(e, w + 0x98, *scale)
        out = e.alloc(0x54)
        e.write(out, b"\xAA" * 0x54)
        return w, out
    run(emu, 0x502FA0, "SplineObjWaypoint_GetInfo", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seg", [None, (8192.0, 100.0, 4196.0), (1234.567, -3.5, 77.125)])
def test_get_debug_text(seg, cw):
    def setup(e):
        return waypoint(e, rnd(9), seg=seg), e.alloc(4)
    run(emu, 0x503070, "SplineObjWaypoint_GetDebugText", setup, cw=cw)


@pytest.mark.parametrize("orig,ours", [(0x5030F0, "SplineObjWaypoint_CollLines"), (0x503120, "SplineObjWaypoint_CollLine"),
                                       (0x503150, "SplineObjWaypoint_Slot10"), (0x503180, "SplineObjWaypoint_Slot11")])
@pytest.mark.parametrize("collide", [0, 1])
def test_coll(orig, ours, collide):
    def setup(e):
        o = task(e, 0x68, SPLINE)
        e.write(o + 0x4D, bytes([collide]))
        a = e.alloc(4)
        u32(e, a, 0x5151)
        return waypoint(e, rnd(10), parent=o), a
    run(emu, orig, ours, setup)


def test_close():
    def setup(e):
        u16(e, 0xA7A618, 0x33)
    run(emu, 0x5031B0, "SplineObj_Close", setup)


init = init_emu()


def test_open():
    compare(init, 0x502250, "SplineObj_Open", setup=setup_types, check_eax=False, normalized=True)
