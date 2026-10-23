"""RainDynCubeObj (0x4F65E0 - 0x4F80B0)."""
import math
import random
import struct

import pytest

from test_cutscene_util import CWS, RET, d64, f32, handler, make_rain_emu, run, u32

emu = make_rain_emu()
CALLBACK = handler(emu, "rain_cb", 1, derefs={0: 2})
CAM = (12345.5, -6789.25, 1024.75)


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def rain(e, seed=1, size=15.0, cam=CAM, drops=5, pos=None, **kw):
    """A RainDynCubeObj with its cells placed around cam and some drops."""
    rng = random.Random(seed)
    r = e.alloc(0x4290)
    RET["base"] = r
    cell = size * 4096 / 7
    f = dict(center=(12000.0, -7000.0, 0.0), sizes=(500.0, 400.0), wind=(-0.2, 0.1), fade=1.0, density=0.25,
             max_density=0.5, intensity=3.0, accum=0.5, rate=2.0, inside=1, in_world=1, no_collide=0, visible=None,
             count=drops, view=(0.0, 0.6, 0.8), move=(100.0, -50.0, 25.0))
    f.update(kw)
    p = cam if pos is None else pos
    for i in range(7):
        for j in range(7):
            for k in range(7):
                c = r + 0x68 + 0x20 * (49 * i + 7 * j + k)
                d64(e, c, p[0] + (i - 3) * cell + rng.uniform(-cell, cell),
                    p[1] + (j - 3) * cell + rng.uniform(-cell, cell), p[2] + (k - 3) * cell + rng.uniform(-cell, cell))
                f32(e, c + 0x18, rng.uniform(0, 0.6))
                vis = f["visible"](i, j, k) if f["visible"] else rng.choice([0, 1, 1])
                u32(e, c + 0x1C, vis)
    mem = e.alloc(800)
    u32(e, r + 0x2B48, mem)
    for i in range(20):
        u32(e, r + 0x2B4C + 4 * i, mem + 0x28 * i)
        f32(e, mem + 0x28 * i, *[rng.uniform(-cell / 2, cell / 2) for _ in range(3)],
            *[rng.uniform(-300, 300) for _ in range(2)], rng.uniform(-900, -100),
            *[rng.uniform(-400, 400) for _ in range(3)], rng.uniform(0.7, 1.0))
    d64(e, r + 0x2C08, *p)
    d64(e, r + 0x2C20, *f["center"], *f["sizes"])
    f32(e, r + 0x2C48, *f["view"], *f["move"], 104.0, 112.0, 127.0, *f["wind"])
    f32(e, r + 0x2C7C, cell, 1.5, f["accum"], f["rate"], 0.157, 5461.33)
    u32(e, r + 0x2C94, 0x2C94)
    f32(e, r + 0x2C98, size, f["density"], f["max_density"], f["fade"])
    u32(e, r + 0x2CA8, f["count"], f["inside"], f["in_world"])
    e.write(r + 0x2CB4, bytes([f["no_collide"]]))
    f32(e, r + 0x2D08, f["intensity"])
    u32(e, r + 0x2D0C, 77, 1)
    return r


def test_small():
    def setup(e):
        u32(e, 0xA76CCC, 0xCCCC)
        return 0, e.alloc(4)
    run(emu, 0x4F7620, "RainObj_GetParamTableHandler", setup)


def test_queue_draw():
    def setup(e):
        u32(e, 0xA76C8C, 0x8C)
        return (rain(e, drops=0),)
    run(emu, 0x4F7960, "RainObj_QueueDraw", setup)


def test_open():
    run(emu, 0x4F7360, "RainObj_Open")


@pytest.mark.parametrize("used", [0, 3, 8])
def test_create_delete(used):
    def setup(e):
        r = rain(e)
        u32(e, 0xA76C9C, 0x10000)
        for i in range(used):
            u32(e, 0xA76CA4 + 4 * i, 0x1000 + i)
        if used == 3:
            u32(e, 0xA76CA4 + 8, r)
            u32(e, 0xA76CA4 + 4, 0)
        return (r,)
    run(emu, 0x4F6C00, "RainObj_CreateHandler", setup)
    run(emu, 0x4F72E0, "RainObj_DeleteHandler", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("where", ["cell", "random", "far"])
def test_find_cell(cw, seed, where):
    def setup(e):
        rng = random.Random(seed)
        r = rain(e, seed=seed)
        p = e.alloc(24)
        if where == "cell":
            c = r + 0x68 + 0x20 * rng.randrange(343)
            d64(e, p, *struct.unpack("<3d", e.read(c, 24)))
        elif where == "random":
            d64(e, p, *[v + rng.uniform(-9000, 9000) for v in CAM])
        else:
            d64(e, p, 1e7, 0, 0)
        return r, p
    run(emu, 0x4F65E0, "RainObj_FindCell", setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("count", [0, 7, 19, 20])
def test_add_drop(cw, count):
    def setup(e):
        RET["rf"] = [0.37]
        r = rain(e, count=count)
        p, v = e.alloc(12), e.alloc(12)
        f32(e, p, 1.5, -2.5, 300.25)
        f32(e, v, 10.1, -20.2, -800.3)
        return r, p, v, 9
    run(emu, 0x4F66A0, "RainObj_AddDrop", setup, cw=cw)


DIRS = [(-0.2, 0.1, -1.0), (0.0, 0.0, 0.0), (0.9, 0.1, -0.3), (-3.0, 0.5, 0.25), (0.0, 0.0, -1.0), (0.0, 0.0, 2.0),
        (1e-20, 0.0, 0.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("d", DIRS)
@pytest.mark.parametrize("seed", range(3))
@pytest.mark.parametrize("count", [3, 20])
def test_spawn_drop(cw, d, seed, count):
    def setup(e):
        rng = random.Random(seed)
        RET["rf"] = [rng.random() for _ in range(4)]
        r = rain(e, count=count)
        p, v = e.alloc(12), e.alloc(12)
        f32(e, p, 1.5, -2.5, 300.25)
        f32(e, v, *d)
        return r, p, v, fbits(rng.choice([0.157, 0.0, 1.3])), fbits(5461.33), 0x55
    run(emu, 0x4F6750, "RainObj_SpawnDrop", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("size", [15.0, 3.3, 0.01])
def test_place_cells(cw, size):
    run(emu, 0x4F6B00, "RainObj_PlaceCells", lambda e: (rain(e, size=size),), cw=cw)


SIM = [dict(), dict(intensity=0.0), dict(intensity=30.0, fade=1.0), dict(inside=0), dict(fade=0.01),
       dict(move=(30000.0, 0.0, 0.0)), dict(accum=0.99, intensity=29.0), dict(wind=(0.0, 0.0)),
       dict(intensity=-1.0), dict(max_density=0.0), dict(accum=7.5, density=1.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(SIM)))
@pytest.mark.parametrize("val", [0.0, 1.0])
@pytest.mark.parametrize("outside", [0, 1])
def test_simulate(cw, case, val, outside):
    def setup(e):
        rng = random.Random(case)
        RET["rf"] = [rng.random() for _ in range(80)]
        RET[("val", 0x2CB8)] = val
        r = rain(e, seed=case, count=2, **SIM[case])
        if outside:
            d64(e, r + 0x2C08, 1e6, 0, 0)
        return (r,)
    run(emu, 0x4F6D70, "RainObj_Simulate", setup, cw=cw)


OUTSIDE = [CAM, (12000.0, -7000.0, 0.0), (0.0, 0.0, 0.0), (5.0, -5.0, 0.0), (1e7, -7000.0, 0.0),
           (-1e7, -7000.0, 0.0), (12000.0, 1e7, 0.0), (12000.0, -1e7, 0.0), (12000.0 + 1024000, -7000.0, 9.0),
           (12000.0 - 1024000, -7000.0 - 819200, 9.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("pos", OUTSIDE)
@pytest.mark.parametrize("sizes", [(500.0, 400.0), (-1.0, 400.0), (0.0, 0.0), (math.nan, 1.0), (1.0, math.nan)])
def test_is_camera_outside(cw, pos, sizes):
    run(emu, 0x4F6FE0, "RainObj_IsCameraOutside", lambda e: (rain(e, pos=pos, sizes=sizes, drops=0),), cw=cw,
        check_eax=True)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("count", [0, 1, 20])
def test_move_drops(cw, seed, count):
    run(emu, 0x4F70B0, "RainObj_MoveDrops", lambda e: (rain(e, seed=seed, count=count),), cw=cw)


@pytest.mark.parametrize("count,i", [(0, 0), (1, 0), (5, 4), (5, 0), (5, 2), (20, 19)])
def test_remove_drop(count, i):
    run(emu, 0x4F7120, "RainObj_RemoveDrop", lambda e: (rain(e, count=count), i))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("callback", [0, 1])
@pytest.mark.parametrize("val", [0.0, 2.0])
@pytest.mark.parametrize("pos", [CAM, (1e7, 0.0, 0.0)])
def test_run(cw, callback, val, pos):
    def setup(e):
        RET["rf"] = [0.3, 0.6, 0.1, 0.9] * 20
        RET[("val", 0x2CB8)] = val
        u32(e, 0xA76CC4, CALLBACK if callback else 0)
        r = rain(e, pos=pos, center=CAM, count=4)
        return (r,)
    run(emu, 0x4F7170, "RainObj_RunHandler", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("sizes", [(500.0, 400.0), (400.0, 500.0), (900.0, 1.0), (math.nan, 3.0), (3.0, math.nan),
                                   (-2.0, -5.0)])
@pytest.mark.parametrize("named", [0, 1])
def test_get_info(cw, sizes, named):
    def setup(e):
        info = e.alloc(0x54)
        e.write(info, b"\xEE" * 0x50)
        u32(e, info + 0x50, e.alloc(32) if named else 0)
        return rain(e, sizes=sizes, center=(1.5, 2.5, 3.5), drops=0), info
    run(emu, 0x4F71F0, "RainObj_GetInfoHandler", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(3))
@pytest.mark.parametrize("count", [0, 3])
def test_draw(cw, seed, count):
    def setup(e):
        prim = e.alloc(16)
        u32(e, prim + 12, rain(e, seed=seed, count=count))
        return (prim,)
    run(emu, 0x4F7630, "RainObj_Draw", setup, cw=cw)


def view(e, pos, seed):
    rng = random.Random(seed)
    f32(e, 0xBCAAE0, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, 0xBCAAE0 + 36, 3)
    d64(e, 0xBCAB08, *pos)


CAMS = [(12345.5 + 500, -6789.25 - 300, 1024.75 + 10), (12345.5 + 60000, -6789.25 - 60000, 9000.0),
        (12345.5 - 1e5, 0.0, 0.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("cam", range(len(CAMS)))
@pytest.mark.parametrize("pos", [CAM, (0.0, 1.0, 2.0)])
@pytest.mark.parametrize("inside", [0, 1])
@pytest.mark.parametrize("no_collide", [0, 1])
def test_render(cw, cam, pos, inside, no_collide):
    def setup(e):
        rng = random.Random(cam)
        RET["angle"] = [rng.uniform(-2, 2) for _ in range(343)]
        RET["hits"] = [rng.choice([None, rng.uniform(-5000, 5000)]) for _ in range(49)]
        view(e, CAMS[cam], cam)
        return (rain(e, seed=cam, pos=pos, inside=inside, no_collide=no_collide),)
    run(emu, 0x4F77C0, "RainObj_RenderHandler", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("columns", ["all", "none", "some"])
def test_update_cells(cw, seed, columns):
    def setup(e):
        rng = random.Random(seed)
        RET["angle"] = [rng.choice([0.1, -0.5, 1.0, 1.0995574, -1.2, 3.0]) for _ in range(343)]
        RET["hits"] = [rng.choice([None, rng.uniform(-8000, 8000) + CAM[2]]) for _ in range(49)]
        r = rain(e, seed=seed)
        d64(e, r + 0x2C08, *[v + rng.uniform(-3000, 3000) for v in CAM])
        if columns == "none":
            f32(e, r + 0x2C48, 0, 0, 0)
            RET["angle"] = [3.0] * 343
            f32(e, r + 0x2C7C, 1.0)
        return (r,)
    run(emu, 0x4F7990, "RainObj_UpdateCells", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("near", [0, 1])
def test_camera_cell_visible(cw, seed, near):
    def setup(e):
        r = rain(e, seed=seed)
        if not near:
            d64(e, r + 0x2C08, 1e7, 1e7, 1e7)
        return (r,)
    run(emu, 0x4F7C20, "RainObj_IsCameraCellVisible", setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("dist", [0.0, 0.4, 0.6, 3.0, 50.0])
def test_wrap_cell(cw, seed, dist):
    def setup(e):
        rng = random.Random(seed)
        r = rain(e, seed=seed, drops=0)
        size = 15.0 * 4096
        c = r + 0x68 + 0x20 * rng.randrange(343)
        d64(e, c, *[v + rng.choice([-1, 1]) * dist * size + rng.uniform(-900, 900) for v in CAM])
        return r, c, fbits(size)
    run(emu, 0x4F7CC0, "RainObj_WrapCell", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("ang", [0.2, 1.0995574, 1.0995575, -1.5])
def test_check_cell(cw, seed, ang):
    def setup(e):
        RET["angle"] = [ang]
        r = rain(e, seed=seed, drops=0)
        c = r + 0x68 + 0x20 * random.Random(seed).randrange(343)
        if seed == 0:
            d64(e, c, *CAM)
        u32(e, 0xA76C88, 5)
        return r, c
    run(emu, 0x4F7DF0, "RainObj_CheckCell", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("in_world", [0, 1])
@pytest.mark.parametrize("mode", [0, 1])
def test_param_changed(cw, in_world, mode):
    def setup(e):
        RET[0x4B0DB0] = mode
        u32(e, 0xA76C9C, 0x100)
        return (rain(e, in_world=in_world, size=[15.0, 7.7][in_world]),)
    run(emu, 0x4F7F00, "RainObj_ParamChangedHandler", setup, cw=cw)
    run(emu, 0x4F8040, "RainObj_AllocDrops", setup, cw=cw)
