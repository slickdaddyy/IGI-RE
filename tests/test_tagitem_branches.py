"""Branch cases for SplineObjWaypoint_Changed (0x500EF0) and SplineObj_GetPoints (0x4FEC10)."""
import random

import pytest

from test_computer_util import RET, add_child, run, with_cw
from test_tagitem_spline import emu, spline, waypoint
from test_vehicle_util import d64

CWS = [0x37F, 0x7F]


def chain(flags, specs, idx, seed=1, maxseg=12, found=True):
    """specs: per waypoint (position or None, straight, rigid, segment, auto, in tree);
    the first waypoint never has a segment."""
    def setup(e):
        rng = random.Random(seed)
        o = spline(e, rng, len(specs), maxseg=maxseg, flags=flags)
        ws = []
        for i, (pos, straight, rigid, seg, auto, tree) in enumerate(specs):
            w = waypoint(e, rng, seg=seg and i > 0, straight=straight, auto=auto)
            if pos is not None:
                d64(e, w + 0xF8, *pos)
            e.write(w + 0x1A0, bytes([tree, straight, rigid]))
            add_child(e, o, w)
            ws.append(w)
        RET[0x4CEC10] = 0x5CEC10 if found else 0
        return (ws[idx],)
    return setup


def wp(pos=None, straight=0, rigid=0, seg=True, auto=0, tree=0):
    return (pos, straight, rigid, seg, auto, tree)


CHANGED = [
    ((0, 0, 0, 1), [wp(auto=1)], 0),                                # auto orient without neighbours
    ((0, 0, 1, 0), [wp(), wp(), wp()], 1),                          # rigid spline, no auto orient
    ((1, 0, 0, 0), [wp(), wp(), wp()], 0),                          # snap on the first waypoint
    ((0, 0, 0, 0), [wp(), wp(), wp(straight=1)], 2),                # straight run at the end
    ((0, 0, 0, 0), [wp(straight=1), wp(), wp()], 0),                # straight run of one
    ((0, 0, 0, 0), [wp(), wp(rigid=1), wp(straight=1), wp()], 2),   # the run starts at a rigid waypoint
    ((0, 0, 0, 1), [wp((0, 0, 0)), wp((0, 5, 3)), wp((0, 10, 6))], 1),          # direction in the y-z plane
    ((0, 0, 0, 1), [wp((0, 0, 0)), wp((0, 1e-22, 1.0)), wp((0, 2e-22, 2.0))], 1),
    ((0, 0, 0, 1), [wp((0, 0, 0)), wp((1e-25, 0, 0)), wp((2e-25, 0, 0))], 1),   # cross products underflow
    ((0, 0, 0, 1), [wp((0, 0, 0)), wp((0, 1e-25, 1e-25)), wp((0, 2e-25, 2e-25))], 1),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(CHANGED)))
def test_changed(case, cw):
    run(emu, with_cw(emu, 0x500EF0, cw), with_cw(emu, "SplineObjWaypoint_Changed", cw), chain(*CHANGED[case]))


# --- SplineObj_GetPoints -------------------------------------------------------

import struct  # noqa: E402

import test_tagitem_points as pts  # noqa: E402
from test_computer_util import f32, u32  # noqa: E402


def points_case(seed, dist, mod=None, **kw):
    base = pts.call_setup(seed, dist, **kw)

    def setup(e):
        args = base(e)
        return (mod(e, args) if mod else None) or args
    return setup


def segs(e):
    return [e.u32(w + 0xF0) for w in pts.waypoints(e)[1:]]


def rounding(e, args):
    """the segment lengths of test_rounding_quirks"""
    ws = pts.waypoints(e)
    f32(e, e.u32(ws[1] + 0xF0) + 0xF8, 3.0)
    f32(e, e.u32(ws[2] + 0xF0) + 0xF8, 16777216.0)
    f32(e, pts.RET[0x4F1030] + 0x28, 1e9)


def long_first(e, args):
    """the first segment is 500 longer than its parts and the point falls in that gap"""
    s = segs(e)[0]
    length = struct.unpack("<f", e.read(s + 0xF8, 4))[0]
    f32(e, s + 0xF8, length + 500.0)
    f32(e, pts.RET[0x4F1030] + 0x28, 1e9)
    return args[:4] + (struct.unpack("<I", struct.pack("<f", length + 250.0))[0],) + args[5:]


def joint_rot(m):
    """every joint of every segment gets rotation m"""
    def mod(e, args):
        for s in segs(e):
            for k in range(e.u32(s + 0x59C) + 1):
                f32(e, s + 0x200 + 40 * k, *m)
    return mod


def last_without_segment(e, args):
    u32(e, pts.waypoints(e)[-1] + 0xF0, 0)


HALF_TURN_Z = (-1.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 1.0)
NAN_ROT = (float("nan"),) * 9


P = points_case
POINTS = [
    P(1, lambda n: -n * 1.6),                                                     # wraps twice
    P(1, 12.0, count=2, spacing=(10.0, 10.0), zero_len=True, with_m=False),       # at the start, no matrix
    P(1, 12.0, count=-1, zero_len=True),
    P(2, 4.0, count=1, spacing=(1.0,), with_off=True, with_m=False, mod=rounding),  # exactly a segment start
    P(2, 4.0, count=-1, with_off=True, mod=rounding),
    P(0, 0.0, with_m=False, mod=long_first),                                      # past the last joint
    P(1, 0.0, count=-1, mod=long_first),
    P(0, lambda n: n * 0.37, mod=joint_rot(HALF_TURN_Z)),                          # z is the largest diagonal
    P(1, lambda n: n * 0.37, count=4, spacing=(30.0, 30.0, -60.0, -60.0), mod=joint_rot(HALF_TURN_Z)),
    P(0, lambda n: n * 0.37, mod=joint_rot(NAN_ROT)),                              # unordered quaternion scale
    P(1, lambda n: n * 0.37, count=4, spacing=(30.0, 30.0, -60.0, -60.0), mod=joint_rot(NAN_ROT)),
    P(0, lambda n: n * 0.3, count=3, spacing=(400.0, 400.0, 400.0), mod=last_without_segment),
    P(0, lambda n: n * 0.5, count=2, spacing=(5000.0, -9000.0), with_m=False),     # off both ends, no matrix
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(POINTS)))
def test_points(case, cw):
    pts.check(POINTS[case], cw)
