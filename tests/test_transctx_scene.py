import struct

import pytest

from emu import compare
from test_d3d_util import D3DEmu, check, cstr, logged, trap, u32
from test_transctx_util import noise, with_cw

emu = D3DEmu()

SCENE_LIST, SCENE_VERSION, SCENE = 0x684100, 0x684110, 0xBCABA0
FRAME_PTR, FRAME_MEM, FRAME_PEAK = 0x6E5B98, 0x684118, 0x6E5BA0
FUNCS = 0xA94E84


def mem_init(e):
    e.call(0x4B0BA0)


def handler(e, slot, name):
    """gRenderModeCurrent.methods[slot] logs the 0x24 bytes of the node it gets."""
    fn = trap(e, name, logged(name, 0, pop=0, before=lambda e: (None, e.read(e.arg(0), 0x24))))
    for table in (FUNCS, 0xA950A4, 0xA950A4 + 0x208):
        u32(e, table + 4 * slot, fn)


def scene(e, queues=3, mode=-1, rect=(10.0, 20.0, 300.0, 200.0)):
    s = e.alloc(0x4C)
    e.write(s, noise(0x4C, 4))
    e.call(0x497570, s, queues)
    r = e.alloc(16)
    e.write(r, struct.pack("<4f", *rect))
    e.call(0x497680, s, r)
    u32(e, s + 8, mode)
    return s


def node(e, t, tag):
    n = e.alloc(0x24)
    e.write(n, struct.pack("<II", 0, t) + bytes([tag]) * 0x1C)
    return n


def test_set_bounds():
    def setup(e):
        s, a, b = e.alloc(0x4C), e.alloc(8), e.alloc(8)
        e.write(a, noise(8, 1))
        e.write(b, noise(8, 2))
        return s, a, b
    compare(emu, 0x497520, "Scene_SetBounds", setup=setup, check_eax=False)


def test_init_list():
    compare(emu, 0x497550, "Scene_InitList", setup=lambda e: e.write(SCENE_LIST, noise(12)), check_eax=False)


@pytest.mark.parametrize("n", [1, 4])
def test_scene_init(n):
    def setup(e):
        mem_init(e)
        s = e.alloc(0x4C)
        e.write(s, noise(0x4C, 2))
        return s, n
    compare(emu, 0x497570, "Scene_Init", setup=setup, check_eax=False)


def test_scene_free():
    def setup(e):
        mem_init(e)
        return (scene(e),)
    compare(emu, 0x4975F0, "Scene_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("add", [0, 1])
def test_scene_select(add):
    def setup(e):
        mem_init(e)
        e.call(0x497550)
        e.call(0x497620, scene(e), 1)
        return scene(e), add
    compare(emu, 0x497620, "Scene_Select", setup=setup, check_eax=False)
    compare(emu, 0x497670, "Scene_GetVersion", setup=setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("rect", [(0, 0, 640, 480), (10.3, 20.7, 301.1, 199.9), (-5, 3e7, 1e-3, 7.77777)])
def test_set_rect(rect, cw):
    def setup(e):
        s, r = e.alloc(0x4C), e.alloc(16)
        e.write(s, noise(0x4C, 3))
        e.write(r, struct.pack("<4f", *rect))
        return s, r
    compare(emu, with_cw(emu, 0x497680, cw), with_cw(emu, "Scene_SetRect", cw), setup=setup, check_eax=False)


@pytest.mark.parametrize("count", [0, 1, 2])
def test_draw_all(count):
    def setup(e):
        mem_init(e)
        u32(e, 0x542340, 7)
        e.call(0x4B0DC0)
        e.call(0x4B0E00, cstr(e, b"A"))
        e.call(0x4B0E00, cstr(e, b"B"))
        e.call(0x4B0F30, 0)
        e.call(0x497550)
        for i in range(count):
            s = scene(e, 3, mode=1 if i else -1)
            e.call(0x497620, s, 1)
            e.call(0x4978B0, 0, node(e, 5, 0xA0 + i))
            e.call(0x4978B0, 2, node(e, 6, 0xB0 + i))
            e.call(0x4978B0, 2, node(e, 5, 0xC0 + i))
        clip = e.call(0x491DB0)[0]
        handler(e, clip, "clip")
        handler(e, 5, "prim5")
        handler(e, 6, "prim6")
        u32(e, 0x6E5BB0, 1)
    check(emu, 0x497700, "Scene_DrawAll", setup=setup, check_eax=False)


def test_queue_clip_window():
    def setup(e):
        mem_init(e)
        e.call(0x4977F0)
        e.call(0x497620, scene(e), 0)
        p = e.alloc(0x24)
        e.write(p, noise(0x24, 6))
        return p, 1
    compare(emu, 0x4977B0, "Scene_QueueClipWindow", setup=setup, check_eax=False)


def test_frame():
    compare(emu, 0x4977F0, "Frame_Reset", setup=lambda e: u32(e, FRAME_PTR, 0x12345678), check_eax=False)
    for peak in (0, 0x100, 0x1000):
        def setup(e, peak=peak):
            u32(e, FRAME_PTR, FRAME_MEM + 0x200)
            u32(e, FRAME_PEAK, peak)
            return (0x30,)
        compare(emu, 0x497800, "Frame_Alloc", setup=setup)


def test_queue_draw():
    def setup(e):
        handler(e, 5, "prim5")
        handler(e, 6, "prim6")
        a, b, c = node(e, 5, 1), node(e, 0xFFFFFFFF, 2), node(e, 6, 3)
        u32(e, a, b)
        u32(e, b, c)
        return (a,)
    check(emu, 0x497830, "RenderQueue_Draw", setup=setup, check_eax=False)
    check(emu, 0x497830, "RenderQueue_Draw", args=(0,), check_eax=False)
    check(emu, 0x497860, "Primitive_Draw", setup=lambda e: (handler(e, 6, "prim6"), (node(e, 6, 9),))[1],
          check_eax=False)


@pytest.mark.parametrize("n", [0, 1, 2, 5])
def test_queue_init(n):
    def setup(e):
        q = e.alloc(0x40)
        e.write(q, noise(0x40, 7))
        return q, n
    compare(emu, 0x497880, "RenderQueue_Init", setup=setup, check_eax=False)


def test_queue_add():
    def setup(e):
        mem_init(e)
        e.call(0x497620, scene(e), 0)
        e.call(0x4978B0, 1, node(e, 5, 1))
        return 1, node(e, 6, 2)
    compare(emu, 0x4978B0, "RenderQueue_Add", setup=setup, check_eax=False)


def modes(e, n=3):
    e.call(0x4B0DC0)
    for i in range(n):
        e.call(0x4B0E00, cstr(e, b"M%d" % i))
    e.call(0x4B0E70, cstr(e, b"Old"))


def test_primitive_register():
    def setup(e):
        modes(e)
        return (cstr(e, b"FooPrimitive"),)
    compare(emu, 0x4978D0, "Primitive_Register", setup=setup)
    compare(emu, 0x4978E0, "Primitive_Unregister", setup=lambda e: (modes(e), (0,))[1], check_eax=False)


def test_primitive_set_handler():
    def setup(e):
        modes(e)
        e.call(0x4B0E50, 1)
        return 0, 0x11223344
    compare(emu, 0x4978F0, "Primitive_SetHandler", setup=setup, check_eax=False)

    def setup2(e):
        modes(e)
        info, h = e.alloc(8), e.alloc(8)
        e.write(info, struct.pack("<II", 2, 0))
        e.write(h, struct.pack("<II", 0, 0x55667788))
        return info, h
    compare(emu, 0x497920, "Primitive_SetHandlerCB", setup=setup2, check_eax=False)
