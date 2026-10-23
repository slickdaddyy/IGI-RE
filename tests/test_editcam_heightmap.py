"""HeightMap (0x506DE0 - 0x507D60)."""
import pytest

from emu import ErrorShown
from test_editcam_util import CWS, RET, d64, handler, make, run, set_handler, task, u16, u32, u8

HMAP, CHANGED = 0x61, 0x34
TYPE, PARAMS, COUNT = 0xA7B248, 0xA7B244, 0xA7B250
MAPS = 0xA7AD10
BUMP_LIST, BUMP_DEPTH, BUMP_COUNT, BUMPS = 0xB97D40, 0xB97DF4, 0xBA2058, 0xBA2060
RESIDENT = 0xB81920

emu = make()
set_handler(emu, CHANGED, HMAP, handler(emu, "changed", 2, derefs={0: 0x100}))
emu.baseline = emu.snapshot()


def world(e, entries=None, full=False):
    """A Terrain with a .hmp table: entries maps index -> (size, flag, resident)."""
    w = e.alloc(0xF00)
    tbl = e.alloc(12 * 258) + 12
    u32(e, w + 0xDD0, tbl)
    u32(e, w + 0xDE0, -0x2345, 0x5000, 0x7100)
    for i in range(256 if full else 0):
        u32(e, tbl + 12 * i, 0x5000 + i, 0, 16)
    for i, (size, flag, resident) in (entries or {}).items():
        u32(e, tbl + 12 * i, e.alloc((size + 1) ** 2), flag, size)
        u8(e, RESIDENT + i, resident)
    u32(e, 0xA44344, w)
    u16(e, TYPE, HMAP)
    return w


def hmap(e, w, place=(-1234.5, 9876.25, -300.75), pos=(-1200.0, 9800.0, -280.0), shift=12, idx=-1, last=-1,
         unk=5, size=0x80, exclusive=0, moved=1, child=0, drop=1, slot=-1, children=0, shown=0):
    t = task(e, 0x100, HMAP)
    d64(e, t + 0x20, *pos)
    u32(e, t + 0x68, w)
    d64(e, t + 0x70, *place)
    u32(e, t + 0xB8, shift, idx, last, unk, 0x11, 0x22)
    u8(e, t + 0xDC, exclusive, 0, 0, moved, child, drop)
    u32(e, t + 0xE4, size, shown, 0, 0, 0, slot, children)
    return t


def test_create():
    def setup(e):
        u32(e, 0xA44344, 0x4444)
        t = task(e, 0x100, HMAP)
        e.write(t + 0x20, bytes(range(0xE0)))
        return (t,)
    run(emu, 0x506DE0, "HeightMap_Create", setup)


@pytest.mark.parametrize("idx", [-1, 0, 5])
@pytest.mark.parametrize("data", [0, 1])
@pytest.mark.parametrize("depth", [0, 1, 3])
def test_enter_leave(idx, data, depth):
    def setup(e):
        w = world(e, {5: (16, 0, 0)} if data else {})
        t = hmap(e, w, idx=idx, slot=7, shown=data)
        u32(e, BUMP_LIST, 9, 8, 7, 6, 5)
        u32(e, BUMP_DEPTH, depth)
        u32(e, BUMP_COUNT, 0x33)
        u32(e, BUMPS + 7 * 0x30 + 0x28, data)
        return (t,)
    run(emu, 0x506F00, "HeightMap_Enter", setup)
    run(emu, 0x506F60, "HeightMap_Leave", setup)
    run(emu, 0x507D00, "HeightMap_Collect", setup)


def test_init():
    def setup(e):
        u32(e, BUMP_DEPTH, 5)
    run(emu, 0x506FA0, "HeightMap_Init", setup)


@pytest.mark.parametrize("child", [0, 1])
@pytest.mark.parametrize("slot", [-1, 0, 2, 3])
@pytest.mark.parametrize("last,resident", [(-1, 0), (4, 0), (4, 1)])
def test_delete(child, slot, last, resident):
    def setup(e):
        w = world(e, {4: (16, 1, resident)})
        maps = [hmap(e, w, slot=i, last=last) for i in range(4)]
        u32(e, MAPS, *maps)
        u32(e, COUNT, 4)
        t = maps[slot] if slot >= 0 else hmap(e, w, last=last)
        u8(e, t + 0xE0, child)
        return (t,)
    run(emu, 0x507340, "HeightMap_Delete", setup)


def test_get_params_shutdown():
    def setup(e):
        u32(e, PARAMS, 0x1234)
        u16(e, TYPE, HMAP)
        return 0, e.alloc(4)
    run(emu, 0x507400, "HeightMap_GetParams", setup)
    run(emu, 0x507D40, "HeightMap_Shutdown", setup)


@pytest.mark.parametrize("mode", [0, 1])
def test_new_child(mode):
    def setup(e):
        RET["mode"] = mode
        w = world(e)
        t = hmap(e, w, idx=4, slot=6)
        e.write(t + 0x88, bytes(range(0x30)))
        place = e.alloc(24)
        d64(e, place, 1.5, 2.5, -3.5)
        return 0x7777, t, place
    run(emu, 0x507D60, "HeightMap_NewChild", setup)


CASES = {
    "child": dict(child=1, idx=5, slot=4, tbl={5: (32, 1, 0)}),
    "child_noidx": dict(child=1, idx=-1, slot=4),
    "nodrop_noidx": dict(drop=0),
    "nodrop_new": dict(drop=0, idx=7, size=40, tbl={0: (16, 0, 0), 1: (16, 0, 0)}),
    "nodrop_full": dict(drop=0, idx=7, size=8, full=True),
    "nodrop_same_size": dict(drop=0, idx=5, last=5, size=0x20, tbl={5: (32, 1, 0)}, exclusive=1),
    "nodrop_resize": dict(drop=0, idx=5, last=5, size=0x80, tbl={5: (32, 1, 0)}),
    "nodrop_resize_resident": dict(drop=0, idx=5, last=5, size=0x10, tbl={5: (32, 1, 1)}, moved=0),
    "nodrop_other": dict(drop=0, idx=5, last=2, size=0x100, tbl={5: (64, 1, 0), 2: (8, 0, 0)}),
    "nodrop_exclusive": dict(drop=0, idx=5, last=2, size=0x100, exclusive=1, tbl={5: (64, 0, 0), 2: (8, 1, 0)}),
    "nodrop_release": dict(drop=0, idx=-1, last=3, tbl={3: (16, 1, 0)}),
    "nodrop_release_resident": dict(drop=0, idx=-1, last=3, tbl={3: (16, 1, 1)}, exclusive=1),
    "drop_none": dict(layers=()),
    "drop_flat": dict(layers=(-500.0,), idx=5, last=5, tbl={5: (32, 1, 0)}),
    "drop_layers": dict(layers=(-200000.0, -3000.0, 2500.0, 70000.0), shift=16),
    "drop_high": dict(layers=(-2000.0, 1e6), shift=14, slot=9, children=0x3030),
    "drop_big": dict(layers=(-9e7, 5e7), shift=4, mode=0),
}


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", sorted(CASES))
def test_changed(case, cw):
    c = CASES[case]

    def setup(e):
        RET["mode"] = c.get("mode", 1)
        RET["layers"] = c.get("layers", (0.0,))
        RET["rand"] = (0.1, 0.9, 0.5, 0.33, 0.77, 0.01, 0.999)
        RET["rand_i"] = 0
        w = world(e, c.get("tbl"), c.get("full", False))
        kw = {k: v for k, v in c.items() if k not in ("tbl", "full", "layers", "mode")}
        t = hmap(e, w, **kw)
        others = [hmap(e, w) for _ in range(3)]
        u32(e, MAPS, *others)
        u32(e, COUNT, 3)
        u32(e, 0xB97D20, 7)
        return (t,)
    run(emu, 0x507410, "HeightMap_Changed", setup, cw=cw)


def test_changed_too_many():
    for target in (0x507410, "HeightMap_Changed"):
        emu.reset()
        RET.clear()
        w = world(emu)
        t = hmap(emu, w, drop=0)
        u32(emu, COUNT, 0xFF)
        with pytest.raises(ErrorShown) as err:
            emu.call(target, t)
        assert "texturemodfiers" in str(err.value)
