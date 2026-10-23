"""Sky dome (0x52CB30 - 0x52D420)."""
import random
import struct

import pytest

from test_render_util import DEVICE, D3D, VB, Rig, device_slots, u32

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]
DOME, MODEL, VEC = 0xA84568, 0xA84570, 0x54F720
LIGHTS, LIGHT_INDEX, LIGHT_COUNT, LIGHT_STRIDE = 0xB041E0, 0xB041E4, 0xB041E8, 0xB041F0
TNL, RADIUS, SCALE = 0xBCAD74, 0xBCADBC, 0xBCABEC
LISTS = (0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4)

rig = Rig()
emu = rig.e
vb = VB(rig, "vb")
rig.fake(0x4B0C60, "Mem_Alloc", 2, ret=lambda e, a: e.alloc(a[0]))
rig.fake(0x4B0D10, "Mem_Free", 1)
rig.fake(0x495200, "Direct3D_AddRestoreCallback", 1)
rig.fake(0x4CEBA0, "VirModel_Add", 1, ret=0xCBA0, fmt=lambda e, a: (e.read_cstr(a[0]),))


def create_vb(e, a):
    u32(e, a[2], vb.obj)
    return 0


d3d = rig.com("d3d", {5: (4, create_vb, lambda e, a: (e.read(a[0], 16), a[1], a[2]))})
dev = rig.com("dev", device_slots())
emu.baseline = emu.snapshot()


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def base(e, tnl=0, radius=40000.0):
    u32(e, D3D, d3d)
    u32(e, DEVICE, dev)
    u32(e, TNL, tnl)
    e.write(RADIUS, f32(radius))
    buf = e.alloc(16 * 321 + 64)
    e.write(buf, b"\xAB" * (16 * 321 + 64))
    vb.set(e, buf)
    return buf


def dome(e, rng, horizon=0.4, lo=0.0, hi=1.0):
    d = e.alloc(0xEDC)
    e.write(d, f32(*[rng.uniform(lo, hi) for _ in range(18)]) + f32(horizon) + struct.pack("<II", 0x4C4C, vb.obj))
    return d


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("u,v", [(0.0, 0.0), (1.0, 1.0), (0.3, 0.7), (0.9, 0.1), (-0.5, 1.5), (2.0, -1.0)])
def test_color(u, v, cw):
    def setup(e):
        p = e.alloc(48)
        e.write(p, f32(0.1, 0.5, 0.9, 0.2, 0.33, 1.4, 0.75, 0.0, 0.6, 1.0, 0.01, 0.123))
        return p, p + 12, p + 24, p + 36, struct.unpack("<I", f32(u))[0], struct.unpack("<I", f32(v))[0]
    rig.check(0x52CE90, "SkyDome_Color", setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("horizon", [-1.0, 0.0, 0.2, 0.4, 0.9, 1.4, 1.6, 3.0])
@pytest.mark.parametrize("seed", [1, 2])
def test_fill(seed, horizon, cw):
    def setup(e):
        rng = random.Random(seed)
        base(e, radius=rng.choice([40000.0, 1.0, 12345.678]))
        lo, hi = (0.0, 1.0) if seed == 1 else (-0.5, 1.5)
        return (dome(e, rng, horizon, lo, hi),)
    rig.check(0x52CC40, "SkyDome_Fill", setup, cw=cw)


def test_init_indices():
    def setup(e):
        d = e.alloc(0xEDC)
        e.write(d, b"\x77" * 0xEDC)
        return (d,)
    rig.check(0x52CFA0, "SkyDome_InitIndices", setup)


@pytest.mark.parametrize("tnl", [0, 1])
def test_new(tnl):
    def setup(e):
        base(e, tnl)
        u32(e, DOME, 0x1111)
    rig.check(0x52CB30, "SkyDome_New", setup, check_eax=True)


def test_set_colors_angle():
    def setup(e):
        rng = random.Random(5)
        base(e)
        d = dome(e, rng)
        p = e.alloc(60)
        e.write(p, f32(*[rng.uniform(0, 1) for _ in range(15)]))
        return d, p, p + 12, p + 24, p + 36, p + 48
    rig.check(0x52D2B0, "SkyDome_SetColors", setup)

    def setup_angle(e):
        return e.alloc(0xEDC), struct.unpack("<I", f32(0.75))[0]
    rig.check(0x52D350, "SkyDome_SetAngle", setup_angle)


def test_free():
    def setup(e):
        base(e)
        d = dome(e, random.Random(3))
        u32(e, DOME, d)
        return (d,)
    rig.check(0x52D030, "SkyDome_Free", setup)


@pytest.mark.parametrize("tnl", [0, 1])
@pytest.mark.parametrize("has_dome", [0, 1])
def test_restore(has_dome, tnl):
    def setup(e):
        base(e, tnl)
        d = dome(e, random.Random(4))
        u32(e, DOME, d if has_dome else 0)
    rig.check(0x52D370, "SkyDome_Restore", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("light", [None, (1.0, 0.0), (0.3, -0.8), (-0.6, -0.2), (0.0, 0.0)])
def test_render(light, cw):
    def setup(e):
        base(e)
        d = dome(e, random.Random(6))
        e.write(SCALE, f32(0.001))
        for lst in LISTS:
            p = e.alloc(8)
            u32(e, p, 0xABCD, 3)
            u32(e, lst, p)
        if light:
            lights = e.alloc(0x100)
            e.write(lights + 0x40, f32(*light))
            idx = e.alloc(8)
            u32(e, idx, 2, 0)
            u32(e, LIGHTS, lights)
            u32(e, LIGHT_INDEX, idx)
            u32(e, LIGHT_STRIDE, 0x20)
        u32(e, LIGHT_COUNT, 1 if light else 0)
        return (d,)
    rig.check(0x52D060, "SkyDome_Render", setup, cw=cw)


def test_small():
    def setup(e):
        v = e.alloc(12)
        u32(e, v, 0x3F800000, 0xBF000000, 0x7FC00001)
        u32(e, MODEL, 0x1234)
        return (v,)
    rig.check(0x52D360, "SkyDome_Init", setup)
    rig.check(0x52D400, "sub_52D400", setup)
    rig.check(0x52D420, "Sky_Init", setup)
