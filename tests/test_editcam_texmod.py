"""TextureModifier (0x505D50 - 0x506CD0)."""
import pytest

from emu import ErrorShown
from test_editcam_util import CWS, RET, d64, handler, make, run, set_handler, task, u16, u32, u8

TEXMOD, CHANGED = 0x60, 0x34
TYPE, PARAMS, COUNT = 0xA7A8F0, 0xA7A8EC, 0xA7AD08
MODS = 0xA7A8F4
HOLE_LIST, HOLE_COUNT, HOLE_BITS, HOLES = 0xB97DA0, 0xB97DF8, 0xBA5060, 0xBA5080
RESIDENT = 0xB81A20

emu = make()
set_handler(emu, CHANGED, TEXMOD, handler(emu, "changed", 2, derefs={0: 0xF0}))
emu.baseline = emu.snapshot()


def world(e, entries=None, full=False):
    """A Terrain with a .bit table: entries maps index -> (size, flag, resident)."""
    w = e.alloc(0xF00)
    tbl = e.alloc(12 * 258) + 12
    u32(e, w + 0xDCC, tbl)
    u32(e, w + 0xDE0, 0x12345, -0x5000, 0x7000)
    for i in range(256 if full else 0):
        u32(e, tbl + 12 * i, 0x5000 + i, 0, 16)
    for i, (size, flag, resident) in (entries or {}).items():
        u32(e, tbl + 12 * i, e.alloc(max(size * size // 8, 4)), flag, size)
        u8(e, RESIDENT + i, resident)
    u32(e, 0xA44344, w)
    u16(e, TYPE, TEXMOD)
    return w


def texmod(e, w, place=(1234.5, -9876.25, 300.75), pos=(1200.0, -9800.0, 280.0), shift=10, bit=-1, last=-1,
           material=3, size=0x20, exclusive=0, moved=1, child=0, drop=1, slot=-1, children=0, shown=0):
    t = task(e, 0xF0, TEXMOD)
    d64(e, t + 0x20, *pos)
    u32(e, t + 0x68, w)
    d64(e, t + 0x70, *place)
    u32(e, t + 0xB8, shift, bit, last, material, 0x11, 0x22)
    u8(e, t + 0xD0, exclusive, 0, 0, moved, child, drop)
    u32(e, t + 0xD8, size, shown, 0, slot, children)
    return t


def test_create():
    def setup(e):
        u32(e, 0xA44344, 0x4444)
        t = task(e, 0xF0, TEXMOD)
        e.write(t + 0x20, bytes(range(0xD0)))
        return (t,)
    run(emu, 0x505D50, "TextureModifier_Create", setup)


@pytest.mark.parametrize("bit", [-1, 0, 5])
@pytest.mark.parametrize("data", [0, 1])
@pytest.mark.parametrize("count", [0, 3])
def test_enter_leave(bit, data, count):
    def setup(e):
        w = world(e, {5: (16, 0, 0)} if data else {})
        t = texmod(e, w, bit=bit, slot=7, shown=data)
        u32(e, HOLE_LIST, 9, 8, 7, 6, 5)
        u32(e, HOLE_COUNT, count)
        u32(e, HOLE_BITS, 0x01020304)
        u32(e, HOLES + 7 * 0x30 + 0x28, data)
        return (t,)
    run(emu, 0x505E40, "TextureModifier_Enter", setup)
    run(emu, 0x505ED0, "TextureModifier_Leave", setup)
    run(emu, 0x505F20, "TextureModifier_Collect", setup)


@pytest.mark.parametrize("count", [1, 4, 5, 8])
def test_leave_deep(count):
    def setup(e):
        t = texmod(e, world(e), slot=7, shown=1)
        u32(e, HOLE_LIST, 9, 8, 7, 6, 5, 4, 3, 2)
        u32(e, HOLE_COUNT, count)
        u32(e, HOLE_BITS, 0x81020304)
        return (t,)
    run(emu, 0x505ED0, "TextureModifier_Leave", setup)


def test_enter_full():
    for target in (0x505E40, "TextureModifier_Enter"):
        emu.reset()
        w = world(emu, {5: (16, 0, 0)})
        t = texmod(emu, w, bit=5, slot=7)
        u32(emu, HOLE_COUNT, 20)
        with pytest.raises(ErrorShown) as err:
            emu.call(target, t)
        assert "Too many items" in str(err.value)


def test_init():
    def setup(e):
        u32(e, HOLE_COUNT, 5)
    run(emu, 0x505F60, "TextureModifier_Init", setup)


@pytest.mark.parametrize("child", [0, 1])
@pytest.mark.parametrize("slot", [-1, 0, 2, 3])
def test_delete(child, slot):
    def setup(e):
        w = world(e)
        mods = [texmod(e, w, slot=i) for i in range(4)]
        u32(e, MODS, *mods)
        u32(e, COUNT, 4)
        t = mods[slot] if slot >= 0 else texmod(e, w)
        u8(e, t + 0xD4, child)
        return (t,)
    run(emu, 0x506240, "TextureModifier_Delete", setup)


def test_get_params_shutdown():
    def setup(e):
        u32(e, PARAMS, 0x1234)
        u16(e, TYPE, TEXMOD)
        return 0, e.alloc(4)
    run(emu, 0x506290, "TextureModifier_GetParams", setup)
    run(emu, 0x506CB0, "TextureModifier_Shutdown", setup)


@pytest.mark.parametrize("n", [-5, 0, 7, 8, 15, 16, 31, 32, 63, 64, 100, 128, 255, 256, 1000])
def test_round_size(n):
    run(emu, 0x506C60, "TextureModifier_RoundSize", lambda e: (n,), check_eax=True)


@pytest.mark.parametrize("mode", [0, 1])
def test_new_child(mode):
    def setup(e):
        RET["mode"] = mode
        w = world(e)
        t = texmod(e, w, bit=4, slot=6)
        e.write(t + 0x88, bytes(range(0x30)))
        place = e.alloc(24)
        d64(e, place, 1.5, 2.5, -3.5)
        return 0x7777, t, place
    run(emu, 0x506CD0, "TextureModifier_NewChild", setup, check_eax=False)


CASES = {
    "child": dict(child=1, bit=5, slot=4, tbl={5: (32, 1, 0)}),
    "child_nobit": dict(child=1, bit=-1, slot=4),
    "nodrop_nobit": dict(drop=0),
    "nodrop_new": dict(drop=0, bit=7, size=40, tbl={0: (16, 0, 0), 1: (16, 0, 0)}),
    "nodrop_full": dict(drop=0, bit=7, size=8, full=True),
    "nodrop_same_size": dict(drop=0, bit=5, last=5, size=0x20, tbl={5: (32, 1, 0)}, exclusive=1),
    "nodrop_resize": dict(drop=0, bit=5, last=5, size=0x80, tbl={5: (32, 1, 0)}),
    "nodrop_resize_resident": dict(drop=0, bit=5, last=5, size=0x10, tbl={5: (32, 1, 1)}, moved=0),
    "nodrop_other": dict(drop=0, bit=5, last=2, size=0x100, tbl={5: (64, 1, 0), 2: (8, 0, 0)}),
    "nodrop_exclusive": dict(drop=0, bit=5, last=2, size=0x100, exclusive=1, tbl={5: (64, 0, 0), 2: (8, 1, 0)}),
    "nodrop_clamp_hi": dict(drop=0, bit=300, size=3, tbl={0xFF: (16, 1, 0)}),
    "nodrop_clamp_lo": dict(drop=0, bit=-7, last=3, tbl={3: (16, 1, 0)}),
    "nodrop_release": dict(drop=0, bit=-1, last=3, tbl={3: (16, 1, 0)}),
    "nodrop_release_resident": dict(drop=0, bit=-1, last=3, tbl={3: (16, 1, 1)}, exclusive=1),
    "drop_none": dict(layers=()),
    "drop_flat": dict(layers=(100.0,), bit=5, last=5, tbl={5: (32, 1, 0)}),
    "drop_layers": dict(layers=(-200000.0, -3000.0, 2500.0, 70000.0), shift=16),
    "drop_high": dict(layers=(-2000.0, 1e6), shift=14, slot=9, children=0x3030),
    "drop_big": dict(layers=(-9e7, 5e7), shift=4, mode=0),
}


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", sorted(CASES))
def test_changed(case, cw):
    c = dict(CASES[case])

    def setup(e):
        RET["mode"] = c.get("mode", 1)
        RET["layers"] = c.get("layers", (0.0,))
        RET["rand"] = (0.1, 0.9, 0.5, 0.33, 0.77, 0.01, 0.999)
        RET["rand_i"] = 0
        w = world(e, c.get("tbl"), c.get("full", False))
        kw = {k: v for k, v in c.items() if k not in ("tbl", "full", "layers", "mode")}
        t = texmod(e, w, **kw)
        others = [texmod(e, w) for _ in range(3)]
        u32(e, MODS, *others)
        u32(e, COUNT, 3)
        return (t,)
    run(emu, 0x5062A0, "TextureModifier_Changed", setup, cw=cw)


def test_changed_too_many():
    for target in (0x5062A0, "TextureModifier_Changed"):
        emu.reset()
        RET.clear()
        w = world(emu)
        t = texmod(emu, w, drop=0)
        u32(emu, COUNT, 0xFF)
        with pytest.raises(ErrorShown) as err:
            emu.call(target, t)
        assert "texturemodfiers" in str(err.value)
