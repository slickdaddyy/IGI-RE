"""GlobalLight and GlobalLightKeyframe (0x527520 - 0x528460)."""
import random
import struct

import pytest

from emu import Emu
from test_animcam_util import (CHANGED_EVENT, CWS, NAN, SNAN, bits, chain_fakes, compare_open, fval, handler,
                               logfake, open_emu, run, set_events, set_handler, task, u16, u32)

GL, KF, OTHER = 0x61, 0x62, 0x63
CHAIN = []

e = Emu()
chain_fakes(e, CHAIN)
logfake(e, 0x4CE3D0, 1)
logfake(e, 0x50FE40, 1, derefs={0: 12})  # TerrainMesh_SetAmbient
logfake(e, 0x50FD20, 2, derefs={0: 12})  # TerrainMesh_SetFog
logfake(e, 0x52D400, 1, derefs={0: 36})
logfake(e, 0x5196D0, 1, derefs={0: 36})
logfake(e, 0x4CEA30, 1)
logfake(e, 0x4B26C0, 0)  # Texture_ReloadAll
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
set_events(e)
for t in (GL, OTHER):
    set_handler(e, CHANGED_EVENT, t, handler(e, "changed_%x" % t, 2))
u16(e, 0xA8408C, GL)
u16(e, 0xA84374, KF)
e.baseline = e.snapshot()


def fill(e, p, size, rng, lo=-2.0, hi=2.0):
    n = (size - 0x20) // 4
    e.write(p + 0x20, struct.pack("<%df" % n, *[rng.uniform(lo, hi) for _ in range(n)]))


def light(e, rng, time=0.0, tmin=0.0, tmax=10.0):
    g = task(e, 0x1B8, GL)
    fill(e, g, 0x1B8, rng)
    u32(e, g + 0x20, 0x2020)
    u32(e, g + 0x18C, fval(tmin), fval(tmax), fval(time))
    return g


def keyframe(e, rng, time, parent=0, links=None):
    k = task(e, 0x1A4, KF, parent)
    fill(e, k, 0x1A4, rng)
    u32(e, k + 0x48, fval(time))
    for i, l in enumerate(links or [-1] * 5):
        u32(e, k + 0x8C + 0x44 * i, l)
    return k


# (time, min, max, keyframe times)
APPLY = [
    (1.0, 0.0, 10.0, []),
    (3.0, 5.0, 5.0, [5.0, 2.0]),
    (3.0, NAN, 5.0, [5.0, 2.0]),
    (4.0, 0.0, 10.0, [2.0, 4.0, 8.0]),
    (5.0, 0.0, 10.0, [8.0, 2.0, 3.0, 6.0, 10.0]),
    (0.5, 0.0, 10.0, [8.0, 2.0, 3.0]),
    (12.0, 0.0, 10.0, [8.0, 2.0, 3.0]),
    (NAN, 0.0, 10.0, [8.0, 2.0]),
    (5.0, 0.0, 10.0, [8.0, NAN, 2.0]),
    (5.0, 0.0, 10.0, [2.0, 8.0, 7.0, 4.5]),
    (-1e30, -3e30, 5.0, [-3e30, 5.0]),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(APPLY)))
def test_apply(case, cw):
    time, tmin, tmax, times = APPLY[case]

    def setup(e):
        rng = random.Random(case)
        g = light(e, rng, time, tmin, tmax)
        CHAIN[:] = [keyframe(e, rng, t, g) for t in times]
        return (g,)
    run(e, 0x527520, "GlobalLight_Apply", setup, cw=cw)


def test_create():
    def setup(e):
        u32(e, 0xA44344, 0x4444)
        return (light(e, random.Random(1)),)
    run(e, 0x5280E0, "GlobalLight_Create", setup)


def test_delete():
    run(e, 0x528160, "GlobalLight_Delete", lambda e: u32(e, 0xA8437C, 0x7C7C))


@pytest.mark.parametrize("orig,ours,table", [(0x528170, "GlobalLight_GetParams", 0xA84088),
                                             (0x5283F0, "GlobalLightKeyframe_GetParams", 0xA83F44)])
def test_get_params(orig, ours, table):
    def setup(e):
        u32(e, table, 0x5151)
        return 0x1234, e.alloc(4)
    run(e, orig, ours, setup)


@pytest.mark.parametrize("reset,apply", [(0, 0), (1, 0), (0, 1), (1, 1)])
@pytest.mark.parametrize("times", [[], [3.0], [3.0, -1.0, 7.0], [NAN, 2.0], [2.0, NAN], [SNAN, 1.0]])
@pytest.mark.parametrize("current", [False, True])
def test_params_changed(reset, apply, times, current):
    def setup(e):
        rng = random.Random(len(times))
        g = light(e, rng)
        e.write(g + 0x1B4, bytes([reset, apply]))
        CHAIN[:] = [keyframe(e, rng, t, g) for t in times]
        u32(e, 0xA8437C, g if current else 0)
        return (g,)
    run(e, 0x528180, "GlobalLight_ParamsChanged", setup)


def test_keyframe_create():
    def setup(e):
        return (keyframe(e, random.Random(2), 1.0),)
    run(e, 0x528260, "GlobalLightKeyframe_Create", setup)


@pytest.mark.parametrize("link_all,link1", [(0, 0), (1, 0), (1, 3)])
@pytest.mark.parametrize("links", [[-1] * 5, [4, 0, 1, 2, 3], [1, 2, 3, 4, -1], [5, -7, 0, 9, 2]])
@pytest.mark.parametrize("parent", [None, GL, OTHER])
@pytest.mark.parametrize("changed", [0, 1])
def test_keyframe_params_changed(link_all, link1, links, parent, changed):
    def setup(e):
        rng = random.Random(3)
        p = task(e, 0x1B8, parent) if parent else 0
        ls = list(links)
        if link_all:
            ls[1] = link1
        k = keyframe(e, rng, 4.0, p, ls)
        e.write(k + 0x1A0, bytes([link_all]))
        u32(e, 0xA84380, 0x8080 if changed else 0)
        return (k,)
    run(e, 0x5282C0, "GlobalLightKeyframe_ParamsChanged", setup)


def test_keyframe_delete():
    def setup(e):
        g = light(e, random.Random(4))
        return (keyframe(e, random.Random(5), 1.0, g),)
    run(e, 0x528400, "GlobalLightKeyframe_Delete", setup)


def test_close():
    def setup(e):
        u32(e, 0xA83F44, 0x4444)
        u32(e, 0xA84088, 0x8888)
    run(e, 0x528420, "GlobalLight_Close", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("time", [0.0, 5.0, 4.0, NAN])
@pytest.mark.parametrize("current", [False, True])
def test_set_time(time, current, cw):
    def setup(e):
        rng = random.Random(6)
        g = light(e, rng, 5.0)
        CHAIN[:] = [keyframe(e, rng, t, g) for t in (2.0, 8.0)]
        u32(e, 0xA8437C, g if current else 0)
        return (bits(time),)
    run(e, 0x528460, "GlobalLight_SetTime", setup, cw=cw)


def test_open():
    compare_open(open_emu(), 0x527C90, "GlobalLight_Open")
