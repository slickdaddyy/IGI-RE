import struct

import pytest

from test_bonemesh_util import make_emu, with_cw
from test_menu_util import check, f32, logfake, u32

E = None


def emu():
    global E
    if E is None:
        E = make_emu()
        E.call(0x4B0BA0)

        def layers(e, args):
            u32(e, args[0], 0x20001000)
            u32(e, args[1], 7)
        logfake(E, 0x4DA180, 2, derefs={0: 0, 1: 0}, effect=layers)

        def fog(e, args):
            f32(e, args[1], struct.unpack("<f", struct.pack("<I", e.u32(0x30000F00)))[0])
        logfake(E, 0x4DA300, 6, derefs={0: 0, 3: 24}, effect=fog)
        E.baseline = E.snapshot()
    return E


def vec(e, *v, fmt="f"):
    p = e.alloc(8 * len(v))
    e.write(p, struct.pack("<%d%s" % (len(v), fmt), *v))
    return p


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("fov,depth,scale,amount,given", [
    (1.0, 5000.0, 3.0, 20000.0, 0),
    (0.5, 100000.0, 7000.0, 40000.0, 1),
    (0.9, 300.0, 2000.0, 1000.0, 0),
    (0.3, 2.0e6, 5000.0, 60000.0, 1),
    (0.6, 1.0e7, 9000.0, 64000.0, 0),
])
def test_fog_compute(fov, depth, scale, amount, given, cw):
    e = emu()

    def setup(e):
        f32(e, 0xBCAB20, fov)
        f32(e, 0x30000F00, amount)
        out = e.alloc(4)
        pos = vec(e, 1.0, 2.0, 3.0, fmt="d")
        lay = e.alloc(16) if given else 0
        return [out, pos, struct.unpack("<I", struct.pack("<f", depth))[0],
                struct.unpack("<I", struct.pack("<f", scale))[0], lay, 2]
    check(e, with_cw(e, 0x4DA790, cw), with_cw(e, "Fog_Compute", cw), setup=setup, check_eax=False)


def test_light_open_close():
    e = emu()
    check(e, 0x4DA880, "Light_Open", check_eax=False)

    def setup(e):
        e.call(0x4DA880)
    check(e, 0x4DA8A0, "Light_Close", setup=setup, check_eax=False)


def test_light_reset():
    e = emu()

    def setup(e):
        u32(e, 0xB041D0, 5)
        u32(e, 0xB04260, 3)
        u32(e, 0xB0425C, 4)
    check(e, 0x4DA8B0, "Light_ResetFrame", setup=setup, check_eax=False)


@pytest.mark.parametrize("nest,count,points", [(0, 0, 0), (5, 3, 9), (0x16, 2, 1), (0x17, 2, 1), (3, 1, 10)])
def test_light_push_point(nest, count, points):
    e = emu()

    def setup(e):
        u32(e, 0xB03F90, nest)
        u32(e, 0xB03DF0, count)
        u32(e, 0xB04260, points)
        return [vec(e, 1.5, -2.5, 3.25, fmt="d"), vec(e, 0.5, 0.25, 0.125), 0x44800000]
    check(e, 0x4DA8D0, "Light_PushPoint", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("nest,count,spots,angle", [(0, 0, 0, 1.2), (4, 2, 9, 0.7), (0x17, 1, 1, 0.3),
                                                    (1, 1, 10, 0.5), (2, 1, 3, 2.9)])
def test_light_push_spot(nest, count, spots, angle, cw):
    e = emu()

    def setup(e):
        u32(e, 0xB03F90, nest)
        u32(e, 0xB03DF0, count)
        u32(e, 0xB0425C, spots)
        return [vec(e, 1.5, -2.5, 3.25, fmt="d"), vec(e, 0.5, 0.25, 0.125), 0x44800000,
                vec(e, 0.0, 0.6, -0.8), struct.unpack("<I", struct.pack("<f", angle))[0]]
    check(e, with_cw(e, 0x4DA950, cw), with_cw(e, "Light_PushSpot", cw), setup=setup, check_eax=False)


@pytest.mark.parametrize("nest,count", [(3, 3), (4, 3), (0, 0)])
def test_light_pop(nest, count):
    e = emu()

    def setup(e):
        u32(e, 0xB03F90, nest)
        u32(e, 0xB03DF0, count)
    check(e, 0x4DAA00, "Light_Pop", setup=setup, check_eax=False)


@pytest.mark.parametrize("used,count", [(0, 3), (10, 5), (4, 0)])
def test_light_get_active(used, count):
    e = emu()

    def setup(e):
        u32(e, 0xB041D0, used)
        u32(e, 0xB03DF0, count)
        u32(e, 0xB04200, *range(0x10000, 0x10000 + count))
        return [e.alloc(4)]
    check(e, 0x4DAA20, "Light_GetActive", setup=setup)


def lights_setup(e, n):
    e.call(0x4DA880)
    out = []
    for i in range(n):
        r = e.call(0x4DAA60, vec(e, i, 1, 2), vec(e, 0.5, i, 0.1), vec(e, 3, 4, i), 7 + i, i & 1)
        out.append(r[0])
    return out


@pytest.mark.parametrize("n", [0, 2])
def test_light_add(n):
    e = emu()

    def setup(e):
        lights_setup(e, n)
        return [vec(e, 9, 8, 7), vec(e, 0.5, 0.75, 1.0), vec(e, -1, -2, -3), 0x1234, 0x101]
    check(e, 0x4DAA60, "Light_Add", setup=setup)


@pytest.mark.parametrize("which", [0, 1, 2])
def test_light_remove(which):
    e = emu()

    def setup(e):
        ls = lights_setup(e, 3)
        if which == 0:
            e.call(0x4DAAF0, ls[1])
        return [ls[which]]
    check(e, 0x4DAAF0, "Light_Remove", setup=setup, check_eax=False)
