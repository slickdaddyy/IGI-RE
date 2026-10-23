"""Dirlight, DirlightKeyframe and the light source list (0x5268E0 - 0x527460)."""
import math
import random

import pytest

from emu import Emu
from test_dirlight_util import (CHANGED_EVENT, CWS, DIRLIGHT_POOL, LIGHTSOURCE_POOL, LOG, RET, add_child, check_error,
                                f32, handler, logfake, pools, run, set_handler, task, u8, u16, u32)
from test_vehicle_util import init_emu, setup_init

DL_TYPE, KF_TYPE, OTHER_TYPE = 0x60, 0x61, 0x62
CHANGED = 0x31

e = Emu()
logfake(e, 0x4CE3D0, 1)
logfake(e, 0x4CEA30, 1)
logfake(e, 0x4DAA60, 5, derefs={0: 12, 1: 12, 2: 12}, masks={3: 0xFF, 4: 0xFF}, effect=lambda e, a: e.alloc(0x28))
logfake(e, 0x4DAAF0, 1)
logfake(e, 0x5274D0, 1)
logfake(e, 0x521030, 0, ret=0x77)
logfake(e, 0x4F17C0, 0, ret=0x5A5A0)
logfake(e, 0x401E70, 4, masks={1: 0xFF, 2: 0xFFFF})
KF_PARENT_CHANGED = handler(e, "dl_changed", 2)
e.baseline = e.snapshot()


def types(e):
    u16(e, 0xA83D34, DL_TYPE)
    u16(e, 0xA83D4C, KF_TYPE)


def unit(rng):
    v = [rng.uniform(-1, 1) for _ in range(3)]
    n = math.sqrt(sum(x * x for x in v)) or 1.0
    return [x / n for x in v]


def dirlight(e, rng, light=None):
    dl = task(e, 0x64, DL_TYPE)
    world = e.alloc(0x10)
    u32(e, dl + 0x20, world)
    f32(e, dl + 0x24, *[rng.uniform(-2, 2) for _ in range(9)])
    if light is None:
        light = rng.random() < 0.5
    if light:
        lt = e.alloc(0x28)
        e.write(lt, bytes(rng.randrange(256) for _ in range(0x28)))
        u32(e, dl + 0x48, lt)
    else:
        u32(e, dl + 0x48, 0)
    u8(e, dl + 0x4C, rng.randrange(256), rng.randrange(256))
    f32(e, dl + 0x50, rng.uniform(0, 100), 0.0, 0.0, 0.0)
    return dl


def keyframe(e, rng, dl, t, type_=KF_TYPE):
    k = task(e, 0x54, type_)
    f32(e, k + 0x20, rng.uniform(-7, 7), rng.uniform(-7, 7))
    f32(e, k + 0x28, *unit(rng))
    f32(e, k + 0x34, *[rng.uniform(0, 2) for _ in range(6)])
    f32(e, k + 0x4C, rng.uniform(0, 100), t)
    add_child(e, dl, k)
    return k


# --- light source list -------------------------------------------------------

def sources(e, n):
    items = [e.alloc(8) for _ in range(n)]
    for p in items:
        e.call(0x5268E0, p)
    return items


@pytest.mark.parametrize("n", [0, 1, 5])
def test_lightsource_add(n):
    def setup(e):
        pools(e)
        sources(e, n)
        return (0x1234560,)
    run(e, 0x5268E0, "LightSource_Add", setup)


@pytest.mark.parametrize("n,which", [(1, 0), (3, 0), (3, 1), (3, 2), (6, 4)])
def test_lightsource_remove(n, which):
    def setup(e):
        pools(e)
        items = sources(e, n)
        e.call(0x526920, items[(which + 1) % n])  # shuffle the slot table
        e.call(0x5268E0, items[(which + 1) % n])
        return (items[which],)
    run(e, 0x526920, "LightSource_Remove", setup)


def test_lightsource_remove_missing():
    def setup(e):
        pools(e)
        sources(e, 3)
        return (0x999,)
    check_error(e, 0x526920, "LightSource_Remove", setup)


# --- Dirlight ------------------------------------------------------------------

@pytest.mark.parametrize("n", [0, 2])
def test_dirlight_create(n):
    def setup(e):
        rng = random.Random(n)
        pools(e)
        u32(e, 0xA44344, 0x4444)
        for _ in range(n):
            e.call(0x527160, dirlight(e, rng))
        return (dirlight(e, rng),)
    run(e, 0x527160, "Dirlight_Create", setup)


@pytest.mark.parametrize("which", [0, 1, 2])
@pytest.mark.parametrize("light", [False, True])
def test_dirlight_delete(which, light):
    def setup(e):
        rng = random.Random(which)
        pools(e)
        sources(e, 2)
        dls = [dirlight(e, rng) for _ in range(3)]
        for d in dls:
            e.call(0x527160, d)
        u32(e, dls[which] + 0x48, e.alloc(0x28) if light else 0)
        return (dls[which],)
    run(e, 0x5271E0, "Dirlight_Delete", setup)


def test_dirlight_get_params():
    def setup(e):
        u32(e, 0xA83D30, 0xABCDEF)
        return 0, e.alloc(4)
    run(e, 0x527260, "Dirlight_GetParams", setup)


@pytest.mark.parametrize("seed", range(6))
def test_dirlight_changed(seed):
    def setup(e):
        rng = random.Random(seed)
        types(e)
        dl = dirlight(e, rng)
        times = [rng.choice([rng.uniform(-50, 150), 3.5, float("nan")]) for _ in range(seed)]
        for t in times:
            keyframe(e, rng, dl, t)
        keyframe(e, rng, dl, 1e9, OTHER_TYPE)
        return (dl,)
    run(e, 0x527270, "Dirlight_Changed", setup)


@pytest.mark.parametrize("seed", range(40))
@pytest.mark.parametrize("cw", CWS)
def test_dirlight_update(seed, cw):
    kind = seed % 8

    def setup(e):
        rng = random.Random(seed)
        types(e)
        dl = dirlight(e, rng)
        times = sorted(rng.uniform(0, 100) for _ in range(rng.randint(1, 5)))
        if kind == 6:
            times = [10.0, 10.0, 30.0]
        ks = [] if kind == 0 else [keyframe(e, rng, dl, t) for t in rng.sample(times, len(times))]
        keyframe(e, rng, dl, 50.0, OTHER_TYPE)
        lo, hi = (times[0], times[-1]) if ks else (0.0, 0.0)
        if kind == 1:
            hi = lo
        if kind == 2:
            t = rng.choice(times)
        elif kind == 4:
            t = times[0] - rng.uniform(0.1, 10)
        elif kind == 5:
            t = times[-1] + rng.uniform(0.1, 10)
        elif kind == 7:
            t = float("nan")
        else:
            t = rng.uniform(times[0], times[-1])
        f32(e, dl + 0x54, lo, hi, t)
        return (dl,)
    run(e, 0x5269C0, "Dirlight_Update", setup, cw=cw)


def test_keyframe_create():
    def setup(e):
        k = e.alloc(0x54)
        e.write(k, bytes(range(0x54)))
        return (k,)
    run(e, 0x5272F0, "DirlightKeyframe_Create", setup)


@pytest.mark.parametrize("seed", range(12))
@pytest.mark.parametrize("cw", CWS)
def test_keyframe_changed(seed, cw):
    parent_kind = seed % 3

    def setup(e):
        rng = random.Random(seed)
        types(e)
        u8(e, CHANGED_EVENT, CHANGED)
        set_handler(e, CHANGED, DL_TYPE, KF_PARENT_CHANGED)
        dl = dirlight(e, rng)
        if parent_kind == 2:
            u16(e, dl + 0x1C, OTHER_TYPE)
        k = keyframe(e, rng, dl, rng.uniform(0, 100))
        if parent_kind == 0:
            u32(e, k + 0x14, 0)
        f32(e, k + 0x28, *[rng.uniform(-3, 3) for _ in range(3)])
        u32(e, 0xA83D54, [0, 0x4321, k][(seed // 3) % 3])
        return (k,)
    run(e, 0x527320, "DirlightKeyframe_Changed", setup, cw=cw)


def test_dirlight_init():
    ie = init_emu()

    def setup(e):
        setup_init(e)
    run(ie, 0x526EE0, "Dirlight_Init", setup)
