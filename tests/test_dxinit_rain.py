import math
import random
import struct

import pytest

from emu import compare
from test_d3d_util import D3DEmu, check, com, pyfake, u32
from test_dxinit_util import check_cw, d3d, device, f32, f64, noise, set_view, vertex_buffer

e = D3DEmu()

RAIN = [0]
RAIN_SIZE = 0x56D0
G_RAIN, G_ENABLED, G_TYPE = 0x6E5BD0, 0x543C58, 0xC311C0


def ground(x, y):
    return 1000.0 * math.sin(x / 5000.0) + 500.0 * math.cos(y / 7000.0) - 3000.0


def drop_points(e):
    hit, pts = e.arg(1), e.arg(2)
    x, y = struct.unpack("<2d", e.read(pts, 16))
    e.write(hit + 0x18, f64(ground(x, y)))
    return (None, (e.read(pts, 24), e.read(e.arg(4), 8)))


def sort_add(e):
    p = e.alloc(e.arg(1))
    return (p, e.arg(2))


pyfake(e, 0x4012A0, "Task_New", 3, before=lambda e: RAIN[0])
pyfake(e, 0x4C6890, "DynCube_ListAdd", 3)
pyfake(e, 0x4C6840, "DynCube_ListRemove", 3)
pyfake(e, 0x4015F0, "Task_Delete", 1)
pyfake(e, 0x401900, "TaskType_New", 2, ret=0x7FFF0123,
       before=lambda e: (None, (e.arg(2) & 0xFFFF, e.arg(3), e.arg(4))))  # parent is a 16 bit type
pyfake(e, 0x401400, "TaskType_SetHandler", 3)
pyfake(e, 0x401A20, "TaskType_Free", 1)
pyfake(e, 0x495200, "Direct3D_AddRestoreCallback", 1)
pyfake(e, 0x4CB9F0, "Coll_DropPointsMat", 8, before=drop_points)
pyfake(e, 0x4B72E0, "SortRender_Add", 3, before=sort_add)


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def make_rain(e, rng, snow=0, count=None, fade=None, texture=True):
    r = e.alloc(RAIN_SIZE)
    e.write(r, noise(RAIN_SIZE, rng.randrange(50)))
    u32(e, r + 0x68, 0x13579B)
    count = rng.randrange(81) if count is None else count
    for i in range(80):
        d = r + 0x57C + i * 0x1C
        e.write(d, f32(rng.uniform(0, 10895.36), rng.uniform(0, 10895.36), rng.uniform(0, 24576.0)))
        e.write(d + 0xC, struct.pack("<i", rng.randrange(-2, 6)))
        e.write(d + 0x10, f32(rng.uniform(-70, 70), rng.uniform(-70, 70)))
        e.write(d + 0x18, struct.pack("<i", rng.randrange(4)))
    if rng.random() < 0.2:
        e.write(r + 0x57C, f32(-5.0, 10900.0))
    u32(e, r + 0xE3C, count)
    u32(e, r + 0x5650, snow)
    tex = 0
    if texture:
        tex = e.alloc(0x40)
        u32(e, tex + 0x20, 0x2468ACE)
    u32(e, r + 0x5654, tex)
    e.write(r + 0x5698, f64(rng.uniform(1e4, 9e4), rng.uniform(1e4, 6e4)))
    e.write(r + 0x56A8, f32(rng.choice([0.0, 0.01, 0.5, 0.99, 1.0]) if fade is None else fade))
    e.write(r + 0x56AC, bytes([rng.randrange(2), rng.randrange(2)]))
    u32(e, r + 0x56C8, 0x80FFFFFF)
    return r


def scene(e, rng, snow=0, **kw):
    set_view(e, rng)
    r = make_rain(e, rng, snow, **kw)
    vb, buf = vertex_buffer(e)
    u32(e, r + 0x56CC, vb)
    d3d(e, vb)
    device(e)
    return r


# --- creation -----------------------------------------------------------------------

@pytest.mark.parametrize("rain", [0, 1, 7])
@pytest.mark.parametrize("tnl", [0, 1])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_rain_new(rain, tnl, cw):
    def setup(e):
        RAIN[0] = e.alloc(RAIN_SIZE)
        e.write(RAIN[0], noise(RAIN_SIZE, 3))
        u32(e, 0xBCAD74, tnl)
        u32(e, G_TYPE, 0x4455)
        vb, _ = vertex_buffer(e, 16)
        d3d(e, vb)
        return 0x13579B, 0x2468A0, rain, fbits(51234.567), fbits(20480.123)
    check_cw(e, 0x498960, "Rain_New", cw, setup=setup)


def test_rain_free():
    def setup(e):
        r = scene(e, random.Random(1))
        u32(e, G_RAIN, r)
        return r, 0x13579B
    check(e, 0x498A70, "Rain_Free", setup=setup, check_eax=False)


def test_rain_init():
    check(e, 0x498AB0, "Rain_Init", setup=lambda e: e.write(0xC311D0, noise(64)), check_eax=False)


def test_rain_close():
    check(e, 0x499CE0, "Rain_Close", setup=lambda e: u32(e, G_TYPE, 0x12340042), check_eax=False)


@pytest.mark.parametrize("have", [0, 1])
@pytest.mark.parametrize("tnl", [0, 1])
def test_rain_restore(have, tnl):
    def setup(e):
        r = scene(e, random.Random(2))
        u32(e, 0xBCAD74, tnl)
        u32(e, G_RAIN, r if have else 0)
    check(e, 0x498B00, "Rain_Restore", setup=setup, check_eax=False)


# --- per frame ------------------------------------------------------------------------

@pytest.mark.parametrize("same", [0, 1])
@pytest.mark.parametrize("fade", [0.0, 0.5, -0.0])
@pytest.mark.parametrize("snow", [0, 1])
def test_rain_render(same, fade, snow):
    def setup(e):
        r = scene(e, random.Random(3), snow, fade=fade)
        u32(e, 0x5CA0E0, 5)
        u32(e, 0xA94A64, 5 if same else 6)
        return (r,)
    check(e, 0x498B80, "Rain_Render", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("move", [None, (0, 0), (300.0, -200.0), (11000.0, 25000.0), (-50000.0, 3.0)])
def test_rain_update_grid(seed, move, cw):
    def setup(e):
        rng = random.Random(seed)
        r = scene(e, rng)
        if move is not None:
            e.write(r + 0x56AD, b"\0")
            e.call(0x498BD0, r)
            pos = list(struct.unpack("<3d", e.read(0xBCAB08, 24)))
            e.write(0xBCAB08, f64(pos[0] + move[0], pos[1] + move[1], pos[2] + rng.uniform(-100, 100)))
        return (r,)
    check_cw(e, 0x498BD0, "Rain_UpdateGrid", cw, setup=setup, check_eax=False)


def gridded(e, rng, snow=0, **kw):
    r = scene(e, rng, snow, **kw)
    e.write(r + 0x56AD, b"\0")
    e.call(0x498BD0, r)
    return r


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("snow", [0, 1])
def test_rain_draw(seed, snow, cw):
    def setup(e):
        return (gridded(e, random.Random(seed), snow),)
    check_cw(e, 0x498F10, "Rain_Draw", cw, setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
def test_rain_clip_cells(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        r = gridded(e, rng)
        e.write(0xBCAB08 + 16, f64(rng.uniform(-9e4, 9e4)))
        return r, *struct.unpack("<2I", f64(rng.choice([0.0, 24576.0, -49152.0, 73728.0])))
    check_cw(e, 0x499910, "Rain_ClipCells", cw, setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("snow", [0, 1, 2])
@pytest.mark.parametrize("count", [0, 1, 2, 37, 80])
def test_rain_fill_vb(seed, snow, count, cw):
    def setup(e):
        return (scene(e, random.Random(seed), snow, count=count),)
    check_cw(e, 0x499090, "Rain_FillVB", cw, setup=setup, check_eax=False)


def test_rain_fill_vb_lock_fails():
    def setup(e):
        r = scene(e, random.Random(9), count=5)
        vb, _ = vertex_buffer(e, lock_ret=0x80004005)
        u32(e, r + 0x56CC, vb)
        return (r,)
    check(e, 0x499090, "Rain_FillVB", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(10))
@pytest.mark.parametrize("snow,texture", [(0, True), (1, True), (1, False), (2, True)])
def test_rain_draw_cell(seed, snow, texture, cw):
    def setup(e):
        rng = random.Random(seed)
        r = scene(e, rng, snow, texture=texture)
        u32(e, G_ENABLED, 0 if seed == 0 else 1)
        for q in range(4):
            lst = r + 0x6C + q * 0x144
            n = rng.choice([0, 1, 5, 80])
            u32(e, lst, n)
            e.write(lst + 4, f32(*sorted((rng.uniform(0, 24576.0) for _ in range(n)), reverse=True)))
        idx = rng.randrange(24 * 22)
        cell = r + 0xE40 + idx * 0x20
        zs = []
        for k in (0, 1, 24, 25):
            c = cell + k * 0x20
            z = rng.uniform(-5000, 5000)
            zs.append(z)
            e.write(c, f64(rng.uniform(-4e5, 4e5), rng.uniform(-4e5, 4e5), z))
        item = e.alloc(16)
        e.write(item, struct.pack("<II", r, cell) + f64(max(zs) - 73728.0 - rng.uniform(-80000, 30000)))
        return (item,)
    check_cw(e, 0x499430, "Rain_DrawCell", cw, setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("snow", [0, 1])
@pytest.mark.parametrize("falling", [0, 1])
@pytest.mark.parametrize("count", [0, 40, 79, 80])
def test_rain_update(seed, snow, falling, count, cw):
    def setup(e):
        rng = random.Random(seed)
        r = scene(e, rng, snow, count=count, fade=rng.choice([0.0, 0.01, 0.5, 0.995, 1.0]))
        e.write(r + 0x56AC, bytes([falling]))
        e.write(r + 0x564C, f32(rng.choice([0.0, 100.0, 300.0, 5000.0])))
        e.write(0xC311D0, noise(64, seed))
        return (r,)
    check_cw(e, 0x499CF0, "Rain_Update", cw, setup=setup, check_eax=False)


def test_rain_set_falling():
    def setup(e):
        return scene(e, random.Random(4)), 0x1234
    compare(e, 0x499F90, "Rain_SetFalling", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F, 0xF7F, 0x77F])
@pytest.mark.parametrize("alpha", [0.0, 0.3125, 0.5, 0.501, 1.0, -0.25, 2.0])
def test_rain_set_alpha(alpha, cw):
    def setup(e):
        return scene(e, random.Random(5)), fbits(alpha)
    check_cw(e, 0x499FA0, "Rain_SetAlpha", cw, setup=setup, check_eax=False)


def test_rain_enable():
    compare(e, 0x499FD0, "Rain_Enable", args=(0x12345678,), check_eax=False)
    compare(e, 0x499FE0, "Rain_IsEnabled", setup=lambda e: u32(e, G_ENABLED, 0x5A5A))
