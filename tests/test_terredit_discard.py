"""DiscardTerrainArea (0x52B6C0 - 0x52BEE0) and DiscardTerrain (0x52BF00 - 0x52C610)."""
import struct

import pytest

from test_terredit_util import (CWS, RET, add_child, cstr_or_null, d64, handler, make, run, set_handler, task, u16,
                                u32, u8, world)

AREA_TYPE, AREA_PARAMS = 0xA84540, 0xA84544
DT_TYPE, DT_PARAMS = 0xA8454C, 0xA84550
DT, AREA, CHANGED = 0x60, 0x61, 0x34

emu = make()
set_handler(emu, CHANGED, DT, handler(emu, "changed", 2, derefs={0: 0x158}))
set_handler(emu, CHANGED, 0x62, handler(emu, "child", 2))
emu.baseline = emu.snapshot()


def area(e, pos=(1234.5, -9876.25, 300.75), shift=17, depth=18, regen=0):
    t = task(e, 0x78, AREA)
    d64(e, t + 0x20, *pos)
    e.write(t + 0x38, bytes(range(1, 0x31)))
    u32(e, t + 0x68, shift, depth)
    u8(e, t + 0x70, regen, 0x11, 0x22)
    return t


def discard(e, w, pos=(1234.5, -9876.25, 300.75), shift=10):
    t = task(e, 0x158, DT)
    e.write(t + 0x20, bytes(i & 0xFF for i in range(0x138)))
    u32(e, t + 0x68, w)
    d64(e, t + 0xF0, *pos)
    u32(e, t + 0x150, shift, 0x5555)
    return t


# ---- DiscardTerrainArea ----

def test_area_init():
    run(emu, 0x52B6C0, "DiscardTerrainArea_Init")


def test_area_create():
    def setup(e):
        t = task(e, 0x78, AREA)
        e.write(t + 0x20, bytes(range(0x58)))
        return (t,)
    run(emu, 0x52B7F0, "DiscardTerrainArea_Create", setup)


def test_area_get_params_shutdown():
    def setup(e):
        u32(e, AREA_PARAMS, 0x1234)
        u16(e, AREA_TYPE, AREA)
        return 0, e.alloc(4)
    run(emu, 0x52BE60, "DiscardTerrainArea_GetParams", setup)
    run(emu, 0x52BEE0, "DiscardTerrainArea_Shutdown", setup)


@pytest.mark.parametrize("shift", [0, 1, 12, 31])
def test_area_get_info(shift):
    def setup(e):
        info = e.alloc(0x58)
        e.write(info, b"\xEE" * 0x58)
        return area(e, shift=shift), info
    run(emu, 0x52BE70, "DiscardTerrainArea_GetInfo", setup)


SOLID = {
    "ok": dict(),
    "miss_first": dict(drops=[(0, 0.0, 0.0)]),
    "miss_later": dict(drops=[(1, 5.0, 10.0)] * 6 + [(0, 0.0, 0.0)]),
    "too_deep": dict(drops=[(1, 5.0, 10.0)] * 3 + [(1, 5.0, 20000.0)]),
    "deep_ok": dict(drops=[(1, 5.0, 16384.0)]),
    "no_material": dict(mats=[1.0, 2.0, 0.0]),
    "neg_zero_material": dict(mats=[-0.0]),
    "nan_material": dict(mats=[float("nan")]),
}


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("size", [0, 4095, 4096, 16384, -8192])
@pytest.mark.parametrize("case", sorted(SOLID))
def test_area_is_solid(case, size, cw):
    c = SOLID[case]

    def setup(e):
        RET.update(c, drop_i=0, mat_i=0)
        world(e)
        p = e.alloc(24)
        d64(e, p, 1234.5, -9876.25, 300.75)
        return area(e), size, p
    run(emu, 0x52BCC0, "DiscardTerrainArea_IsSolid", setup, cw=cw, check_eax=True)


FILL = {
    "solid": dict(),
    "too_deep": dict(depth=19),
    "split": dict(drops=[(0, 0.0, 0.0)] + [(1, 1.0, 1.0)] * 64),
    "split_twice": dict(drops=[(0, 0.0, 0.0), (0, 0.0, 0.0)] + [(1, 1.0, 1.0)] * 400),
    "split_limit": dict(drops=[(0, 0.0, 0.0)], max_depth=18),
}


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", sorted(FILL))
def test_area_fill(case, cw):
    c = FILL[case]

    def setup(e):
        RET.update(c, drop_i=0, mat_i=0)
        RET["task_size"] = 0x160
        world(e)
        u16(e, DT_TYPE, DT)
        p = e.alloc(24)
        d64(e, p, -2345.5, 8765.25, 1300.75)
        return area(e, depth=c.get("max_depth", 19)), c.get("depth", 17), 0x4000, p
    run(emu, 0x52B9F0, "DiscardTerrainArea_Fill", setup, cw=cw)


POSITIONS = [(1234.5, -9876.25, 300.75), (-1e6 - 0.5, 2.5e6 + 0.5, -3333.3), (0.4, -0.6, 1.5)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("shift", [0, 12, 18, 31])
@pytest.mark.parametrize("pos", POSITIONS)
def test_area_changed(pos, shift, cw):
    def setup(e):
        world(e)
        return (area(e, pos, shift),)
    run(emu, 0x52B840, "DiscardTerrainArea_Changed", setup, cw=cw)


@pytest.mark.parametrize("shift", [16, 17])
def test_area_changed_regenerate(shift):
    def setup(e):
        RET["task_size"] = 0x160
        RET.update(drops=[(0, 0.0, 0.0)] + [(1, 1.0, 1.0)] * 400, drop_i=0, mat_i=0)
        world(e)
        u16(e, DT_TYPE, DT)
        return (area(e, shift=shift, depth=18, regen=1),)
    run(emu, 0x52B840, "DiscardTerrainArea_Changed", setup)


# ---- DiscardTerrain ----

@pytest.mark.parametrize("count", [0, 1, 7])
def test_coll_query(count):
    def setup(e):
        t = discard(e, 0)
        d64(e, t + 0x108, -100.0, -200.0, -300.0, 100.0, 200.0, 300.0)
        pts = [(0.0, 0.0, 0.0), (-100.0, 0.0, 0.0), (100.0, 0.0, 0.0), (50.0, -199.0, 299.0),
               (50.0, 250.0, 0.0), (99.9, 199.9, -299.9), (0.0, 0.0, 301.0)]
        p = e.alloc(24 * 7)
        for i, v in enumerate(pts):
            d64(e, p + 24 * i, *v)
        q = e.alloc(0x98)
        u32(e, q, p, count, 0x3333)
        e.write(q + 0xC, b"\x01" * 8)
        return t, q
    run(emu, 0x52BF00, "DiscardTerrain_CollQuery", setup)


def test_coll_query_inverted():
    def setup(e):
        t = discard(e, 0)
        d64(e, t + 0x108, 100.0, 200.0, 300.0, -100.0, -200.0, -300.0)
        p = e.alloc(48)
        d64(e, p, 0.0, 0.0, 0.0, 150.0, 0.0, 0.0)
        q = e.alloc(0x98)
        u32(e, q, p, 2, 0)
        e.write(q + 0xC, b"\x01\x01")
        return t, q
    run(emu, 0x52BF00, "DiscardTerrain_CollQuery", setup)


def test_mask_visible_visit():
    def setup(e):
        out = e.alloc(4)
        u32(e, out, 0x5555)
        u32(e, 0xB12BB8, 7)
        return 0, out
    run(emu, 0x52BFE0, "DiscardTerrain_MaskVisible", setup)
    run(emu, 0x52C0B0, "DiscardTerrain_Visit", setup)


@pytest.mark.parametrize("flags", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_sub_52bff0(flags):
    def setup(e):
        u8(e, 0xA758BD, *flags)
    run(emu, 0x52BFF0, "DiscardTerrain_Draw", setup, check_eax=True)


def test_create():
    def setup(e):
        world(e)
        return (discard(e, 0),)
    run(emu, 0x52C000, "DiscardTerrain_Create", setup)


def test_init():
    run(emu, 0x52C0C0, "DiscardTerrain_Init")


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("shift", [0, 10, 19, 31])
@pytest.mark.parametrize("pos", POSITIONS)
def test_changed(pos, shift, cw):
    def setup(e):
        w = world(e)
        u32(e, 0xA44344, 0)
        return discard(e, w, pos, shift), 0
    run(emu, 0x52C290, "DiscardTerrain_Changed", setup, cw=cw)


@pytest.mark.parametrize("arg", [None, b"", b"pos"])
@pytest.mark.parametrize("children", [0, 1, 3])
def test_changed_children(children, arg):
    def setup(e):
        w = world(e)
        t = discard(e, w)
        for i in range(children):
            c = task(e, 0x40, 0x62 if i != 1 else 0x63)
            add_child(e, t, c)
        return t, cstr_or_null(e, arg)
    run(emu, 0x52C290, "DiscardTerrain_Changed", setup)


def test_get_params_shutdown():
    def setup(e):
        u32(e, DT_PARAMS, 0x1234)
        u16(e, DT_TYPE, DT)
        return 0, e.alloc(4)
    run(emu, 0x52C500, "DiscardTerrain_GetParams", setup)
    run(emu, 0x52C5F0, "DiscardTerrain_Shutdown", setup)


@pytest.mark.parametrize("shift", [0, 10, 31])
def test_get_info(shift):
    def setup(e):
        info = e.alloc(0x58)
        e.write(info, b"\xEE" * 0x58)
        return discard(e, 0x4444, shift=shift), info
    run(emu, 0x52C510, "DiscardTerrain_GetInfo", setup)


def test_world_events():
    def setup(e):
        t = discard(e, 0x4444)
        arg = e.alloc(8)
        u32(e, arg, 0x7777, 0x8888)
        return t, arg
    run(emu, 0x52C580, "DiscardTerrain_WorldRemove", setup)
    run(emu, 0x52C5A0, "DiscardTerrain_WorldAdd", setup)


@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("has_handler", [0, 1])
def test_new(mode, has_handler):
    def setup(e):
        RET["mode"] = mode
        RET["task_size"] = 0x160
        world(e)
        u16(e, DT_TYPE, DT if has_handler else 0x64)
        p = e.alloc(24)
        d64(e, p, 1.5, -2.5, 3.25)
        return 0x5150, p, 13
    run(emu, 0x52C610, "DiscardTerrain_New", setup)
