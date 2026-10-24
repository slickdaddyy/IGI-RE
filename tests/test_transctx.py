import random
import struct

import pytest

from emu import compare
from test_d3d_util import D3DEmu, check, check_raises, com, cstr, logged, trap, u32
from test_transctx_util import noise, with_cw

emu = D3DEmu()
FAIL = 0x887600E1

FUNCS = 0xA94E84
TC = 0xBCAAE0
SCENE = 0xBCABA0
DEVICE_P = 0x5CA114
MODE = 0xC28B40
SET_ACTIVE, SET_NEAR, TC_PRIM = 0x6E5BA4, 0x6E5BA8, 0x543A68
DEVICE = {11: 3, 13: 2}


def modes(e, n=3):
    e.call(0x4B0DC0)
    for i in range(n):
        e.call(0x4B0E00, cstr(e, b"M%d" % i))
    e.call(0x4B0E70, cstr(e, b"Old"))


def display(e, w=800, h=600):
    u32(e, MODE + 4, w)
    u32(e, MODE + 8, h)


def scene(e, rng):
    x, y, w, h = rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(100, 1000), rng.uniform(100, 800)
    e.write(SCENE + 0x18, struct.pack("<6f", x, y, w, h, w * 0.5, h * 0.5))


def ctx(e, rng, layer=1):
    p = e.alloc(0xA8)
    e.write(p, struct.pack("<9f", *(rng.uniform(-1, 1) for _ in range(9))) + b"\x01\x02\x03\x04"
            + struct.pack("<3d", *(rng.uniform(-1e5, 1e5) for _ in range(3)))
            + struct.pack("<2f", rng.uniform(0.2, 2), rng.uniform(0.2, 2)) + noise(0x18, 3)
            + struct.pack("<i", layer) + noise(0x44, 4))
    return p


def test_small():
    compare(emu, 0x497940, "TransContext_SetAutoNear", args=(0x1234,), check_eax=False)
    compare(emu, 0x497E60, "TransContext_SetUnk5C", setup=lambda e: (e.alloc(0xA8), 0x3F400000), check_eax=False)
    compare(emu, 0x497EC0, "TransContext_GetSelectCount", setup=lambda e: u32(e, 0x6E5BB4, 77))
    compare(emu, 0x497ED0, "TransContext_Invalidate", setup=lambda e: u32(e, 0x6E5BB0, 1), check_eax=False)
    compare(emu, 0x498070, "TransContext_GetNearClipMethod", setup=lambda e: u32(e, SET_NEAR, 9))


def test_register_mode_cb():
    def setup(e):
        modes(e)
        u32(e, TC_PRIM, 3)
        info = e.alloc(8)
        e.write(info, struct.pack("<II", 1, 0))
        return (info,)
    compare(emu, 0x497950, "TransContext_RegisterModeCB", setup=setup, check_eax=False, normalized=True)


def set_active_log(name):
    """gRenderModeCurrent.methods[SetActiveTransContext] logs the context it is handed."""
    return logged(name, 0, pop=0, before=lambda e: (None, e.read(e.u32(e.arg(0)), 0xA8)))


@pytest.mark.parametrize("has_fn", [0, 1])
def test_select(has_fn):
    def setup(e):
        u32(e, SET_ACTIVE, 4)
        u32(e, FUNCS + 16, trap(e, "setactive", set_active_log("setactive")) if has_fn else 0)
        u32(e, 0x6E5BB4, 5)
        return (ctx(e, random.Random(1)),)
    check(emu, 0x497E70, "TransContext_Select", setup=setup, check_eax=False)

    def setup2(e):
        c = setup(e)[0]
        n = e.alloc(0xB8)
        e.write(n, noise(0x10) + e.read(c, 0xA8))
        return (n,)
    check(emu, 0x497970, "TransContext_DrawPrim", setup=setup2, check_eax=False)


def test_queue():
    def setup(e):
        e.call(0x4B0BA0)
        e.call(0x4977F0)
        s = e.alloc(0x4C)
        e.call(0x497570, s, 3)
        e.call(0x497620, s, 0)
        u32(e, TC_PRIM, 6)
        return (ctx(e, random.Random(2)),)
    compare(emu, 0x497EE0, "TransContext_Queue", setup=setup, check_eax=False)


def test_init():
    def setup(e):
        modes(e)
        u32(e, 0x6E5BB0, 1)
    compare(emu, 0x497980, "TransContext_Init", setup=setup, check_eax=False, normalized=True)


def test_shutdown():
    def setup(e):
        modes(e)
        e.call(0x497980)
    compare(emu, 0x497D40, "TransContext_Shutdown", setup=setup, check_eax=False)


# --- Direct3D -----------------------------------------------------------------

def device(e, viewport=0, proj=0, view=0):
    def vp(e):
        return (None, e.read(e.arg(1), 24))

    def tf(e):
        r = {3: proj, 2: view}.get(e.arg(1), 0)
        return (r, e.read(e.arg(2), 64))
    u32(e, DEVICE_P, com(e, "dev", DEVICE, {13: (viewport, vp), 11: (0, tf)}))


def zplanes(e, rng):
    for a in (0xBCABF0, 0xBCACF0, 0xBCAD8C, 0xBCAD9C, 0xBCADB4, 0xBCADE4):
        e.write(a, struct.pack("<f", rng.uniform(0, 1)))
    for near, far in ((0xBCADD8, 0xBCADB8), (0xBCADA0, 0xBCADC8), (0xBCADB0, 0xBCADA8)):
        n = rng.uniform(0.1, 50)
        e.write(near, struct.pack("<f", n))
        e.write(far, struct.pack("<f", n + rng.uniform(10, 1e5)))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("layer", [0, 1, 2])
@pytest.mark.parametrize("seed", range(3))
def test_set_active(layer, seed, cw):
    def setup(e):
        rng = random.Random(seed * 7 + layer)
        display(e)
        zplanes(e, rng)
        device(e)
        p = e.alloc(4)
        u32(e, p, ctx(e, rng, layer))
        return (p,)
    check(emu, with_cw(emu, 0x497A00, cw), with_cw(emu, "Direct3D_SetActiveTransContext", cw), setup=setup,
          check_eax=False)


@pytest.mark.parametrize("layer,fail", [(0, "viewport"), (1, "viewport"), (2, "viewport"), (1, "proj"),
                                        (2, "view"), (3, None), (-1, None)])
def test_set_active_errors(layer, fail):
    def setup(e):
        rng = random.Random(5)
        display(e)
        zplanes(e, rng)
        device(e, **({fail: FAIL} if fail else {}))
        p = e.alloc(4)
        u32(e, p, ctx(e, rng, layer))
        return (p,)
    check_raises(emu, 0x497A00, "Direct3D_SetActiveTransContext", setup=setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_set_near(cw):
    def setup(e):
        rng = random.Random(9)
        display(e)
        zplanes(e, rng)
        device(e)
        e.write(TC, e.read(ctx(e, rng, 1), 0xA8))
        e.write(0xBCABEC, struct.pack("<f", 0.37))
        u32(e, 0xBCADA4, 1)
        p = e.alloc(8)
        e.write(p, struct.pack("<fI", 2.5, 0xDEAD))
        return (p,)
    check(emu, with_cw(emu, 0x497D00, cw), with_cw(emu, "Direct3D_SetNearClippingPlane", cw), setup=setup,
          check_eax=False)


# --- cameras and projection ------------------------------------------------------

@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
def test_create(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        display(e, *rng.choice([(640, 480), (800, 600), (1280, 720)]))
        scene(e, rng)
        out, rot, pos = e.alloc(0xA8), e.alloc(40), e.alloc(24)
        e.write(out, noise(0xA8, 1))
        e.write(rot, struct.pack("<9fI", *(rng.uniform(-1, 1) for _ in range(9)), 0xABCD))
        e.write(pos, struct.pack("<3d", *(rng.uniform(-1e5, 1e5) for _ in range(3))))
        return out, rot, pos, struct.unpack("<I", struct.pack("<f", rng.uniform(0.1, 3)))[0]
    compare(emu, with_cw(emu, 0x497D70, cw), with_cw(emu, "TransContext_Create", cw), setup=setup,
            check_eax=False)


def view(e, rng):
    e.write(TC, struct.pack("<9f", *(rng.uniform(-1, 1) for _ in range(9))))
    e.write(TC + 0x28, struct.pack("<3d", *(rng.uniform(-1e4, 1e4) for _ in range(3))))
    e.write(TC + 0x48, struct.pack("<4f", rng.uniform(100, 900), rng.uniform(100, 900), rng.uniform(0, 640),
                                   rng.uniform(0, 480)))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(10))
def test_project(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        view(e, rng)
        out, depth, p = e.alloc(8), e.alloc(4), e.alloc(24)
        e.write(out, noise(8))
        e.write(p, struct.pack("<3d", *(rng.uniform(-1e4, 1e4) for _ in range(3))))
        return out, depth, p
    compare(emu, with_cw(emu, 0x497F20, cw), with_cw(emu, "TransContext_Project", cw), setup=setup,
            check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
def test_project_view_scale(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        view(e, rng)
        out, v = e.alloc(8), e.alloc(12)
        e.write(v, struct.pack("<3f", rng.uniform(-100, 100), rng.uniform(-100, 100), rng.uniform(-50, 500)))
        return out, v, struct.unpack("<I", struct.pack("<f", rng.uniform(-50, 500)))[0]
    compare(emu, with_cw(emu, 0x498000, cw), with_cw(emu, "TransContext_ProjectView", cw), setup=setup,
            check_eax=False)
    compare(emu, with_cw(emu, 0x498040, cw), with_cw(emu, "TransContext_ScaleSize", cw), setup=setup,
            check_eax=False)


# --- offscreen buffer --------------------------------------------------------------

OFF_W, OFF_H, OFF_PIX, OFF_CLIP = 0xBCAAA0, 0xBCAAA4, 0xBCAAA8, 0xBCAAC0


def test_offscreen_init():
    def setup(e):
        modes(e)
        e.write(0x542310, struct.pack("<16I", *range(20, 36)))
    compare(emu, 0x498080, "Offscreen_Init", setup=setup, check_eax=False, normalized=True)
    def setup2(e):
        modes(e)
        u32(e, 0x6E5BB8, 1)
    compare(emu, 0x498350, "Offscreen_Shutdown", setup=setup2, check_eax=False)
    compare(emu, 0x498370, "Offscreen_GetMode", setup=lambda e: u32(e, 0x6E5BB8, 2))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("rect", [(0, 0, 100, 50), (-5, -5, 10, 10), (30, 20, 10, 5), (500, 400, 600, 500),
                                  (10.5, 3.25, 2000.75, 900.5), (700, 700, 9, 9), (1, 1, 1, 1)])
def test_offscreen_clip(rect, cw):
    def setup(e):
        u32(e, OFF_W, 640)
        u32(e, OFF_H, 480)
        p = e.alloc(0x1C)
        e.write(p, noise(12) + struct.pack("<4f", *rect))
        return (p,)
    compare(emu, with_cw(emu, 0x498140, cw), with_cw(emu, "Offscreen_ClipWindow", cw), setup=setup,
            check_eax=False)


def test_offscreen_open_clear():
    def setup(e):
        e.write(OFF_W, struct.pack("<3I", 1, 2, 3))
        e.write(OFF_CLIP, noise(32))
    compare(emu, 0x498230, "Offscreen_Open", setup=setup)

    def setup2(e):
        p = e.alloc(20 * 10 * 4)
        e.write(p, noise(800))
        e.write(OFF_W, struct.pack("<3I", 20, 10, p))
    compare(emu, 0x498260, "Offscreen_Clear", setup=setup2, check_eax=False)


@pytest.mark.parametrize("x,y,w,h,pitch", [(0, 0, 5, 4, 20), (3, 2, 5, 4, 32), (16, 7, 8, 8, 40), (0, 0, -1, 3, 20),
                                          (2, 2, 3, -1, 12), (1, 1, 0, 3, 12), (4, 0, 2, 2, 11)])
def test_offscreen_blit(x, y, w, h, pitch):
    def setup(e):
        p = e.alloc(20 * 10 * 4)
        e.write(p, noise(800))
        e.write(OFF_W, struct.pack("<3I", 20, 10, p))
        src = e.alloc(400)
        e.write(src, noise(400, 3))
        prim = e.alloc(0x24)
        e.write(prim, noise(12) + struct.pack("<iiiiiI", pitch, x, y, w, h, src))
        return (prim,)
    compare(emu, 0x498290, "Offscreen_Blit", setup=setup, check_eax=False)
