"""Water (0x518930 - 0x5196D0)."""
import struct

import pytest

from emu import Emu
from test_nodereg_util import CWS, FISTP_CWS, LOG, RET, d64, f32, floatret, logfake, rnd, run, task, u32

WATERS, WATER_COUNT, DEPTH, STACK, TYPES = 0xA80200, 0xA8101C, 0xA81020, 0xA80308, 0xA80408
WORLD = 0xA44344

emu = Emu()
logfake(emu, 0x4F1160, 0, ret=1)  # Level_IsRunning
logfake(emu, 0x4CEC10, 1, strings=(0,), ret=0xEC10)  # VirModel_Find
logfake(emu, 0x4C5710, 3)  # RigidDynCubeObj_Init
logfake(emu, 0x4C7140, 5)  # DynCube_Add
logfake(emu, 0x4C7560, 2)  # DynCube_Remove
logfake(emu, 0x4C6E70, 5, derefs={2: 24, 3: 24, 4: 24})  # DynCube_MoveBox
logfake(emu, 0x52D6C0, 1, derefs={0: 16})
logfake(emu, 0x4E5610, 7)  # CellMask_FillTriangle
emu.baseline = emu.snapshot()


def world(e, offset=(1000, -2000, 300)):
    w = e.alloc(0xE00)
    u32(e, w + 0xD3C, 0xD3C)
    u32(e, w + 0xD64, 0xD64)
    u32(e, w + 0xDE0, *offset)
    u32(e, w + 0xDFC, 0xDFC)
    u32(e, WORLD, w)
    return w


def water(e, rng=None, shift=10, level=0.25, res=0x200, typ=3, wid=7, nids=(-1, -1, -1, -1)):
    rng = rng or rnd(1)
    w = task(e, 0x4C48, 0x50)
    d64(e, w + 0x20, rng.uniform(-5000, 5000), rng.uniform(-5000, 5000), rng.uniform(-500, 500))
    u32(e, w + 0xF0, e.u32(WORLD) or world(e))
    d64(e, w + 0x4BD0, rng.uniform(-50000, 50000), rng.uniform(-50000, 50000), rng.uniform(-3000, 3000))
    d64(e, w + 0x4BE8, -1000.5, -2000.25, -30.0, 1000.75, 2000.5, 30.0)
    f32(e, w + 0x4C18, level)
    f32(e, w + 0x4C1C, 123.5)
    u32(e, w + 0x4C20, wid, shift, typ, 0x2C2C, 0x3030, res, *nids)
    f32(e, w + 0x4B78, 7.25)
    f32(e, w + 0x4B8C, 13.5)
    return w


def test_draw_cellmask_cases():
    for cw in CWS:
        for size, half in ((64, 2048.0), (128, 1000.5), (7, -3.0)):
            def setup(e):
                w = water(e)
                o = e.alloc(24)
                d64(e, o, 100.25, -300.5, 0.0)
                a = e.alloc(16)
                u32(e, a, 0x3A3A, size, o)
                f32(e, a + 12, half)
                return w, a
            run(emu, 0x518930, "Water_DrawCellMask", setup, cw=cw)


def water_stack(e, depth):
    ws = [water(e) for _ in range(4)]
    for i, w in enumerate(ws):
        f32(e, w + 0x4C1C, 10.5 * (i + 1))
        u32(e, STACK + 4 * (i + 1), w)
    u32(e, DEPTH, depth)
    return ws


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("depth", [0, 2])
def test_predraw(depth, cw):
    def setup(e):
        world(e)
        f32(e, 0xB818A0, 0.75, 0.3, 1.9)
        f32(e, 0xBA2030, 0.1, 0.2, 0.4)
        return (water_stack(e, depth)[0],)
    run(emu, 0x518A00, "Water_PreDraw", setup, cw=cw)


@pytest.mark.parametrize("depth", [0, 2])
def test_push_level(depth):
    def setup(e):
        world(e)
        return (water_stack(e, depth)[3],)
    run(emu, 0x518B00, "Water_PushLevel", setup)


@pytest.mark.parametrize("orig,ours", [(0x518AB0, "Water_PostDraw"), (0x518B40, "Water_PopLevel")])
@pytest.mark.parametrize("depth", [1, 2, 3])
def test_pop_level(orig, ours, depth):
    def setup(e):
        world(e)
        return (water_stack(e, depth)[0],)
    run(emu, orig, ours, setup)


def test_create():
    def setup(e):
        world(e)
        u32(e, WATER_COUNT, 2)
        u32(e, DEPTH, 3)
        return (water(e),)
    run(emu, 0x518B90, "Water_Create", setup)


def test_run():
    run(emu, 0x518C90, "Water_Run", lambda e: (water(e),))


def test_delete():
    def setup(e):
        world(e)
        u32(e, WATER_COUNT, 3)
        u32(e, WATERS, 0x111, 0x222, 0x333)
        return (water(e),)
    run(emu, 0x5191E0, "Water_Delete", setup)


@pytest.mark.parametrize("shift", [0, 1, 10, 30, 31])
def test_get_info(shift):
    def setup(e):
        info = e.alloc(0x54)
        u32(e, info + 0x50, 0x5050)
        return water(e, shift=shift), info
    run(emu, 0x519220, "Water_GetInfo", setup)


@pytest.mark.parametrize("ids,find", [((), 3), ((3,), 3), ((3, 4, 3), 3), ((1, 2), 5), ((-1, 0, -1), -1)])
def test_find_by_id(ids, find):
    def setup(e):
        u32(e, WATER_COUNT, len(ids))
        for i, wid in enumerate(ids):
            u32(e, WATERS + 4 * i, water(e, wid=wid))
        return (find,)
    run(emu, 0x519690, "Water_FindById", setup, check_eax=True)


HIT_SET = [(0, 4), (8, 0x20), (0x38, 0x44), (0x4C, 0x54), (0x58, 0x5C), (0x60, 0x68)]


def hit_ignore(out, n):
    """The bytes of the hit records the original copies from uninitialised stack."""
    ign = []
    for i in range(n):
        base = out + 0x70 * i
        last = 0
        for lo, hi in HIT_SET:
            if lo > last:
                ign.append((base + last, base + lo))
            last = hi
        ign.append((base + last, base + 0x70))
    return ign


LINES = [
    # (to, from, existing hit, existing dist)
    ((0.0, 0.0, -5.0), (0.0, 0.0, 50.0), 0, 0.0),
    ((0.0, 0.0, 50.0), (0.0, 0.0, -5.0), 0, 0.0),
    ((10.0, 20.0, -5.0), (0.0, 0.0, 50.0), 1, 100.0),
    ((10.0, 20.0, -5.0), (0.0, 0.0, 50.0), 1, 1.0),
    ((10.0, 20.0, 5.0), (0.0, 0.0, 50.0), 0, 0.0),
    ((-1000.5, 2000.5, -5.0), (0.0, 0.0, 50.0), 0, 0.0),
    ((-1000.6, 0.0, -5.0), (0.0, 0.0, 50.0), 0, 0.0),
    ((1000.8, 0.0, -5.0), (0.0, 0.0, 50.0), 0, 0.0),
    ((0.0, -2000.3, -5.0), (0.0, 0.0, 50.0), 0, 0.0),
    ((0.0, 2000.6, -5.0), (0.0, 0.0, 50.0), 0, 0.0),
    ((float("nan"), 0.0, -5.0), (0.0, 0.0, 50.0), 0, 0.0),
    ((0.0, 0.0, 17.125), (0.0, 0.0, 50.0), 0, 0.0),
    ((0.0, 0.0, -5.0), (0.0, 0.0, float("nan")), 0, 0.0),
    ((0.0, 0.0, float("nan")), (0.0, 0.0, 5.0), 0, 0.0),
    ((5.0, 5.0, -5.0), (0.0, 0.0, 50.0), 1, float("nan")),
]


@pytest.mark.parametrize("cw", CWS)
def test_lines(cw):
    ign = []

    def setup(e):
        world(e)
        w = water(e)
        n = len(LINES)
        to, frm, out = e.alloc(24 * n), e.alloc(24 * n), e.alloc(0x70 * n)
        for i, (t, f, hit, dist) in enumerate(LINES):
            d64(e, to + 24 * i, *t)
            d64(e, frm + 24 * i, *f)
            u32(e, out + 0x70 * i + 0x58, hit)
            d64(e, out + 0x70 * i + 0x60, dist)
        q = e.alloc(0x24)
        u32(e, q, to, frm, n)
        u32(e, q + 0x20, out)
        ign[:] = hit_ignore(out, n)
        return w, q
    run(emu, 0x518CA0, "Water_TestLines", setup, cw=cw, ignore=ign)


@pytest.mark.parametrize("cw", CWS)
def test_points(cw):
    def setup(e):
        world(e)
        w = water(e)
        pts = [t for t, _, _, _ in LINES] + [(0.0, 0.0, 16.8), (0.0, 0.0, 17.125), (0.0, 0.0, 17.2)]
        p, out = e.alloc(24 * len(pts)), e.alloc(4 * len(pts))
        for i, t in enumerate(pts):
            d64(e, p + 24 * i, *t)
            u32(e, out + 4 * i, 0xAAAA0000 + i)
        q = e.alloc(12)
        u32(e, q, p, len(pts), out)
        return w, q
    run(emu, 0x518E50, "Water_TestPoints", setup, cw=cw)


def water_types(e):
    for i in range(65):
        a = TYPES + 0x30 * i
        u32(e, a, 100 + i, 0x44, 0x80 + i, 0x90 + i, 0xA0 + i, 0xB0 + i)
        f32(e, a + 0x18, 1.5 + i * 0.37, -0.25 * i)
        u32(e, a + 0x20, 0x2020 + i, 0x2424 + i)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("shift,level,res,typ", [(10, 0.25, 0x200, 103), (8, 0.7, 0x2000, 100), (12, -0.3, 0x1000, 164),
                                                 (14, 0.5, 0x10000, 5), (3, 1.33, 0x7FFF, 130)])
def test_params_changed(shift, level, res, typ, cw):
    def setup(e):
        world(e, (12345, -6789, 1011))
        water_types(e)
        f32(e, 0xB818A0, 0.75, 0.3, 1.9)
        rng = rnd(shift * 7 + res)
        others = [water(e, rng, wid=i) for i in (1, 2, 3, 2)]
        u32(e, WATER_COUNT, len(others))
        for i, o in enumerate(others):
            u32(e, WATERS + 4 * i, o)
        return (water(e, rng, shift, level, res, typ, nids=(2, -1, 3, 9)),)
    run(emu, 0x519290, "Water_ParamsChanged", setup, cw=cw)
