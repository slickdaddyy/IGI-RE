"""Camera filter constructors, band textures and the line filter render method (0x4A23E0 - 0x4A3530)."""
import random
import struct

import pytest

from test_bonemesh_util import LOG, RET, com, cstr, f32, logfake, make_emu, run, task, u32, with_cw

RNG = 0x935BF0
PIXELS = 0x935C10

emu = make_emu()
logfake(emu, 0x4012A0, 3, masks={1: 0xFFFF}, effect=lambda e, a: RET.get("new", 0))  # Task_New
logfake(emu, 0x4B2320, 8)  # Texture_Alloc
logfake(emu, 0x4B22F0, 1)  # Texture_Free
logfake(emu, 0x4B7000, 1, strings=(0,), ret=0xF047)  # Sprite_New
logfake(emu, 0x4B7030, 1)  # Sprite_Delete
logfake(emu, 0x4B6720, 2, strings=(1,))  # Sprite_Load
logfake(emu, 0x4B66A0, 1)  # Sprite_Free
logfake(emu, 0x4B0DB0, 0, effect=lambda e, a: RET.get("mode", 0))  # Mem_GetMode
logfake(emu, 0x4B0D40, 0)
logfake(emu, 0x4B0D50, 0)
logfake(emu, 0x490370, 0, ret=123456)  # Timer_GetMs
logfake(emu, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
logfake(emu, 0x4018F0, 0, ret=7)  # Task_GetRootType
logfake(emu, 0x401900, 4, strings=(0,), masks={2: 0xFFFF},
        effect=lambda e, a: 0x100 + len(e.read_cstr(a[0])))  # TaskType_New
logfake(emu, 0x401530, 3, masks={0: 0xFFFF})  # TaskType_SetHandler2
logfake(emu, 0x4E7530, 0, ret=0x37)  # Screen_GetDrawEvent
logfake(emu, 0x4B0E70, 1, strings=(0,), effect=lambda e, a: 0x200 + len(e.read_cstr(a[0])))
logfake(emu, 0x4950E0, 0, ret=3)  # Direct3D_GetRenderMode
logfake(emu, 0x4B0EC0, 3)  # RenderMode_SetMethod
logfake(emu, 0x4978D0, 1, strings=(0,), effect=lambda e, a: 0x300 + len(e.read_cstr(a[0])))
logfake(emu, 0x4978F0, 2)
BUF = emu.alloc(0x8000)


def lock(e, a):
    n = RET.get("locks", 0)
    RET["locks"] = n + 1
    u32(e, a[2], 0 if n == RET.get("null", -1) else BUF)
    return 0x80004005 if n == RET.get("fail", -1) else 0


def draw(e, a):
    return e.read(BUF + 0x1C * a[3], 0x1C * a[4]) if a[2] == RET.get("vb") else None


def transform(e, a):
    f32(e, a[2], *[0.5 + 0.25 * i for i in range(16)])


VB = com(emu, "vb", {3: 4}, effects={3: lock})
DEV = com(emu, "dev", {12: 3, 20: 3, 31: 6, 35: 3, 37: 4}, effects={12: transform},
          derefs={31: {5: draw}})
emu.baseline = emu.snapshot()


def new_task(e, size):
    RET["new"] = task(e, size, 0x44)
    e.write(RET["new"] + 0x20, b"\xAB" * (size - 0x20))


def test_line_new():
    def setup(e):
        new_task(e, 0x38)
        e.write(0x543D4C, b"\x31\x00")
        return (0x7777, 0x3F000000, 0x3E800000, 0x3F400000, 0x43960000, 1)
    run(emu, 0x4A23E0, "LineFilter_New", setup, check_eax=True)


def test_widescreen_new():
    def setup(e):
        new_task(e, 0x44)
        e.write(0x543D4E, b"\x32\x00")
        return (0x7777, 0x3F000000, 0x3E800000, 0x3F400000, 5, 640, 480, 0, 0, 0x42200000)
    run(emu, 0x4A2430, "WideScreenFilter_New", setup, check_eax=True)


@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("rgb", [(0.5, 0.25, 0.75), (-1.0, 2.0, 1.5), (0.0, 1.0, -0.0), (1.0000001, -1e-9, 3.0)])
def test_band_new(mode, rgb):
    def setup(e):
        new_task(e, 0x74)
        RET["mode"] = mode
        e.write(0x543D50, b"\x33\x00")
        u32(e, 0x543D54, 4)
        u32(e, 0x935C04, cstr(e, b"LOCAL:computer/font1.fnt"))
        return (0x7777, 0x43960000) + tuple(struct.unpack("<I", struct.pack("<f", v))[0] for v in rgb) + \
            (40, 80, 0x3F000000)
    run(emu, 0x4A2490, "BandFilter_New", setup, check_eax=True)


def test_band_delete():
    def setup(e):
        t = task(e, 0x74)
        u32(e, t + 0x70, 0xF047)
        return (t,)
    run(emu, 0x4A25E0, "BandFilter_Delete", setup)


def band(e, rng, rgb=None, level=None):
    t = task(e, 0x74)
    f32(e, t + 0x64, *(rgb or [rng.uniform(-0.2, 1.2) for _ in range(3)]))
    f32(e, t + 0x24, rng.uniform(0, 1) if level is None else level)
    e.write(PIXELS, bytes(rng.randrange(256) for _ in range(0x404)))
    u32(e, RNG, rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32))
    return t


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_band_fill_stripes(seed, cw):
    run(emu, with_cw(emu, 0x4A2600, cw), with_cw(emu, "BandFilter_FillStripes", cw),
        lambda e: (band(e, random.Random(seed), [(0.5, 0.25, 1.0), None][seed > 0]),))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_band_fill_solid(seed, cw):
    run(emu, with_cw(emu, 0x4A26C0, cw), with_cw(emu, "BandFilter_FillSolid", cw),
        lambda e: (band(e, random.Random(seed)),))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed,level", [(0, 0.0), (1, -0.5), (2, 0.001), (3, 0.1), (4, 0.5), (5, 1.0), (6, 3.0),
                                        (7, 0.25)])
def test_band_add_noise(seed, level, cw):
    run(emu, with_cw(emu, 0x4A2720, cw), with_cw(emu, "BandFilter_AddNoise", cw),
        lambda e: (band(e, random.Random(seed), level=level),))


@pytest.mark.parametrize("rgb", [(0.5, 0.25, 0.75), (-1.0, 2.0, 1.5), (0.0, 1.0, -0.0), (1.0000001, -1e-9, 3.0),
                                 (-2.0, -3.0, -4.0)])
def test_band_set_color(rgb):
    def setup(e):
        t = task(e, 0x74)
        return (t,) + tuple(struct.unpack("<I", struct.pack("<f", v))[0] for v in rgb)
    run(emu, 0x4A2800, "BandFilter_SetColor", setup)


def test_band_set_font():
    run(emu, 0x4A28B0, "BandFilter_SetFont", lambda e: (0x5000, 0x3F666666, 0x3E4CCCCD, 0xC2480000, 3))


@pytest.mark.parametrize("cam", [0, 1, 8, 9, 10, -5])
def test_band_next_camera(cam):
    run(emu, 0x4A28F0, "BandFilter_NextCamera", lambda e: u32(e, 0x543D54, cam))


@pytest.mark.parametrize("mode", [0, 1])
def test_handicam_new(mode):
    def setup(e):
        new_task(e, 0x54)
        RET["mode"] = mode
        e.write(0x543D58, b"\x34\x00")
        u32(e, 0x936020, cstr(e, b"LOCAL:common/sprites/battery.spr"), cstr(e, b"LOCAL:computer/font1.fnt"))
        return (0x7777, 0x3F000000, 0x3E800000, 0x3F400000, 0x43960000)
    run(emu, 0x4A2910, "HandiCamFilter_New", setup, check_eax=True)


def test_handicam_delete():
    def setup(e):
        t = task(e, 0x54)
        u32(e, t + 0x30, 0xF047)
        return (t,)
    run(emu, 0x4A29A0, "HandiCamFilter_Delete", setup)


def test_handicam_set_sprite():
    run(emu, 0x4A29C0, "HandiCamFilter_SetSprite", lambda e: (0x5000, 4, 2))


def test_handicam_set_font():
    run(emu, 0x4A29E0, "HandiCamFilter_SetFont", lambda e: (0x5000, 0x75, 0x14))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_init(cw):
    run(emu, with_cw(emu, 0x4A2A00, cw), with_cw(emu, "CameraFilter_Init", cw), None)


def line_setup(e, seed, level=0.0, w=640.0, h=480.0, x=0.0, y=0.0, used=0, size=1000, depth=300.0, **ret):
    rng = random.Random(seed)
    RET.update(ret, locks=0)
    RET["vb"] = VB
    u32(e, 0x5CA114, DEV)
    u32(e, 0xBCAD00, VB, size, used)
    for a in (0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4):
        lst = e.alloc(0x20)
        u32(e, lst + 4, 3)
        u32(e, a, lst)
    f32(e, 0xBCABB8, x, y, w, h)
    f32(e, 0xBCADC0, rng.uniform(1, 10))
    f32(e, 0xBCADA0, rng.uniform(1, 10))
    f32(e, 0xBCADB4, rng.uniform(0, 0.2))
    f32(e, 0xBCAD8C, rng.uniform(0.8, 1.0))
    f32(e, 0xBCABEC, rng.uniform(0.001, 0.1))
    u32(e, RNG, rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32))
    e.write(BUF, b"\xEE" * 0x8000)
    t = task(e, 0x38)
    f32(e, t + 0x20, *[rng.uniform(-0.1, 1.1) for _ in range(3)])
    f32(e, t + 0x2C, depth, level)
    return (t,)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed,kw", [
    (0, dict()),
    (1, dict(h=100.0)),
    (2, dict(h=1000.5, y=10.25, x=3.5, w=800.7)),
    (3, dict(level=0.05)),
    (4, dict(level=0.5, h=300.0)),
    (5, dict(level=3.0, h=200.0)),
    (6, dict(level=0.12, used=900)),
    (7, dict(used=850, level=1.0, h=120.0)),
    (8, dict(depth=0.5)),
    (9, dict(fail=0)),
    (10, dict(fail=1)),
    (11, dict(null=0)),
    (12, dict(level=0.3, fail=2)),
    (13, dict(level=2.5, null=3, h=200.0, used=900, size=300)),
    (14, dict(level=-1.0)),
    (15, dict(h=2.0)),
    (16, dict(level=0.0999)),
    (17, dict(used=700)),
])
def test_line_render(seed, kw, cw):
    run(emu, with_cw(emu, 0x4A2CD0, cw), with_cw(emu, "LineFilter_Render", cw),
        lambda e: line_setup(e, seed, **kw))
