"""SplineObj_GetPoints (0x4FEC10) on generated splines."""
import math
import random
import struct

import pytest

from emu import Emu, compare
from test_computer_util import RET, add_child, f32, logfake, task, u32, with_cw
from test_vehicle_util import d64

WP = 0x51
emu = Emu()
logfake(emu, 0x4F1030, 1)  # Task_FindById
emu.baseline = emu.snapshot()


def rot(rng, kind):
    """A rotation matrix: random, near identity, or a half turn (trace <= 0)."""
    if kind == "same":
        return [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    a, b, c = (rng.uniform(-math.pi, math.pi) for _ in range(3))
    if kind == "small":
        a, b, c = a * 1e-4, b * 1e-4, c * 1e-4
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    rx = [[1, 0, 0], [0, ca, -sa], [0, sa, ca]]
    ry = [[cb, 0, sb], [0, 1, 0], [-sb, 0, cb]]
    rz = [[cc, -sc, 0], [sc, cc, 0], [0, 0, 1]]
    mul = lambda p, q: [[sum(p[i][k] * q[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    m = mul(mul(rz, ry), rx)
    return [m[i][j] for i in range(3) for j in range(3)]


def segment(e, rng, start, n, rots, wiggly=False):
    """A SplineObjSegment with n parts from start, joints wandering forward."""
    s = e.alloc(0x5B8)
    base = (rng.uniform(-1e4, 1e4), rng.uniform(-1e4, 1e4), rng.uniform(-100, 100))
    d64(e, s + 0x20, *base)
    pts = [start]
    for _ in range(n):
        x, y, z = pts[-1]
        lo = -60 if wiggly else 20
        pts.append((x + rng.uniform(lo, 80), y + rng.uniform(-30, 30), z + rng.uniform(-5, 5)))
    total = 0.0
    for i, p in enumerate(pts):
        f32(e, s + 0x104 + 12 * i, *[c - b for c, b in zip(p, base)])
        kind = rots if rots != "mixed" else rng.choice(["random", "same", "small"])
        f32(e, s + 0x200 + 40 * i, *rot(rng, kind))
        u32(e, s + 0x200 + 40 * i + 36, rng.randrange(100))
    lens = [struct.unpack("<f", struct.pack("<f", math.dist(pts[i], pts[i + 1])))[0] for i in range(n)]
    for i, v in enumerate(lens):
        f32(e, s + 0x548 + 4 * i, v)
    f32(e, s + 0x544, 7.0)
    total = sum(lens)
    f32(e, s + 0xF8, total)
    u32(e, s + 0x59C, n)
    return s, pts[-1], total


def spline(e, rng, nwp=4, rots="mixed", zero_len=False, wiggly=False, extra_len=0.0):
    e.write(0xA7A620, struct.pack("<H", WP))
    o = task(e, 0x68, 0x50)
    add_child(e, o, task(e, 0x40, 0x52))
    first = task(e, 0x1A8, WP)
    d64(e, first + 0xF8, 1.5, 2.5, 3.5)
    f32(e, first + 0x110, *rot(rng, "random"))
    add_child(e, o, first)
    p, length = (0.0, 0.0, 0.0), 0.0
    for _ in range(nwp - 1):
        w = task(e, 0x1A8, WP)
        s, p, total = segment(e, rng, p, rng.randrange(1, 6), rots, wiggly)
        u32(e, w + 0xF0, s)
        length += total
        add_child(e, o, w)
    f32(e, o + 0x28, 0.0 if zero_len else length + extra_len)
    RET[0x4F1030] = o
    return o, length


def call_setup(seed, dist, count=0, spacing=(), with_m=True, with_off=False, rots="mixed", off_scale=1.0, **kw):
    def setup(e):
        rng = random.Random(seed)
        RET.clear()
        _, length = spline(e, rng, rots=rots, **kw)
        dd = dist(length) if callable(dist) else dist
        m = e.alloc(40 * (count + 1)) if with_m else 0
        out = e.alloc(24 * (count + 1))
        found = e.alloc(4)
        e.write(found, b"\x77")
        sp = e.alloc(4 * max(count, 1))
        if count:
            f32(e, sp, *spacing)
        off = 0
        if with_off:
            off = e.alloc(12)
            f32(e, off, 1.5 * off_scale, -2.25 * off_scale, 3.0 * off_scale)
        return 7, m, out, found, struct.unpack("<I", struct.pack("<f", dd))[0], count, sp, off
    return setup


def check(setup, cw=0x37F):
    compare(emu, with_cw(emu, 0x4FEC10, cw), with_cw(emu, "SplineObj_GetPoints", cw), setup=setup, check_eax=False)


CWS = [0x37F, 0x7F]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("where", [0.13, 0.5, 0.77, 0.999, -0.3, 1.6, 2.2, 0.0])
def test_single(where, seed, cw):
    check(call_setup(seed, lambda n: n * where, with_off=seed % 2 == 0), cw)


@pytest.mark.parametrize("rots", ["random", "same", "small"])
@pytest.mark.parametrize("seed", range(4))
def test_single_rotations(rots, seed):
    check(call_setup(seed, lambda n: n * 0.41, rots=rots))


@pytest.mark.parametrize("seed", range(3))
def test_no_matrix(seed):
    check(call_setup(seed, lambda n: n * 0.6, count=3, spacing=(30.0, -15.0, 50.0), with_m=False))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(10))
def test_chain_wiggly(seed, cw):
    sp = (40.0, 40.0, -70.0, -70.0, 120.0)
    check(call_setup(seed, lambda n: n * 0.5, count=5, spacing=sp, wiggly=True, with_off=seed % 2 == 1), cw)


@pytest.mark.parametrize("seed", range(12))
def test_chain_big_offset(seed):
    """Joints moved far by the offset, so neighbouring segments do not meet."""
    sp = (60.0, -60.0, 90.0, -90.0, 150.0, -150.0)
    check(call_setup(seed, lambda n: n * 0.5, count=6, spacing=sp, wiggly=True, with_off=True, off_scale=12.0))


def test_past_last_waypoint():
    check(call_setup(1, lambda n: n + 50.0, extra_len=100.0))


def test_zero_length():
    check(call_setup(1, 12.0, count=2, spacing=(10.0, 10.0), zero_len=True))


def test_missing():
    def setup(e):
        args = call_setup(1, 5.0)(e)
        RET[0x4F1030] = 0
        return args
    check(setup)


def test_no_waypoints():
    def setup(e):
        args = call_setup(1, 5.0)(e)
        o = task(e, 0x68, 0x50)
        f32(e, o + 0x28, 10.0)
        RET[0x4F1030] = o
        return args
    check(setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("spacing", [(25.0, 25.0, 25.0, 25.0), (-25.0, -40.0, -10.0, -60.0), (70.0, -35.0, 120.0, -200.0),
                                     (5.0, 300.0, -500.0, 1.0)])
def test_chain(spacing, seed, cw):
    check(call_setup(seed, lambda n: n * (0.15 + 0.1 * seed), count=4, spacing=spacing, with_off=seed % 3 == 0), cw)


@pytest.mark.parametrize("seed", range(6))
def test_chain_off_ends(seed):
    check(call_setup(seed, lambda n: n * 0.02, count=3, spacing=(-80.0, -2000.0, 5000.0), with_off=True))
    check(call_setup(seed, lambda n: n * 0.98, count=2, spacing=(5000.0, -9000.0)))


def waypoints(e):
    o = RET[0x4F1030]
    w = e.u32(e.u32(o + 8))  # after the non-waypoint child
    out = []
    while e.u32(w):
        out.append(w)
        w = e.u32(w)
    return out


@pytest.mark.parametrize("dist", [4.0, 3.5, 3.0, 16777220.0])
def test_rounding_quirks(dist):
    """Segment lengths 3 and 2^24: the running sum rounds up, so the start of the second
    segment lands on or past dist."""
    def setup(e):
        args = list(call_setup(2, dist, count=1, spacing=(1.0,), with_off=True)(e))
        ws = waypoints(e)
        f32(e, e.u32(ws[1] + 0xF0) + 0xF8, 3.0)
        f32(e, e.u32(ws[2] + 0xF0) + 0xF8, 16777216.0)
        f32(e, RET[0x4F1030] + 0x28, 1e9)
        return tuple(args)
    check(setup)


@pytest.mark.parametrize("extra", [0.5, 30.0])
def test_segment_tail(extra):
    """The segment is longer than its parts: dist in the tail gives the last joint."""
    def setup(e):
        args = list(call_setup(3, 0.0, count=2, spacing=(1.0, -1.0))(e))
        ws = waypoints(e)
        s = e.u32(ws[1] + 0xF0)
        n = e.u32(s + 0x59C)
        parts = sum(struct.unpack("<%df" % n, e.read(s + 0x548, 4 * n)))
        f32(e, s + 0xF8, parts + extra)
        args[4] = struct.unpack("<I", struct.pack("<f", parts + extra * 0.9))[0]
        return tuple(args)
    check(setup)
