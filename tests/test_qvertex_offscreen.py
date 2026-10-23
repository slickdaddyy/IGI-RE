import random
import struct

import pytest

from emu import compare
from test_d3d_util import D3DEmu, check, check_raises, pyfake, u32
from test_dxinit_util import check_cw, f32, noise

e = D3DEmu()

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]
WIDTH, PIXELS, CLIP = 0xBCAAA0, 0xBCAAA8, 0xBCAAC0
TEX, PAL = 0xA84A50, 0xA84A58
SLOTS = 0xA7CEC8

pyfake(e, 0x498370, "Offscreen_GetMode", 0, ret=5)
pyfake(e, 0x4B0EC0, "RenderMode_SetMethod", 3)
FRAME = [1]
pyfake(e, 0x497800, "Frame_Alloc", 1, before=lambda e: e.alloc(e.arg(0)) if FRAME[0] else 0)
# the stack copy of a sprite is passed by pointer: log its contents
pyfake(e, 0x497860, "Primitive_Draw", 1, before=lambda e: (None, e.read(e.arg(0) + 4, 0x80)))
pyfake(e, 0x4978B0, "RenderQueue_Add", 2)


def test_init_textures():
    def setup(em):
        for a, v in ((0x546FA8, 11), (0x546FAC, 12), (0x546FB0, 13), (0xA84A48, 14), (0xA84A40, 15), (0xA84A44, 16)):
            u32(em, a, v)
        em.write(SLOTS, b"\x01\x02\x03\x04\x05\x06\x07\x08")
    check(e, 0x50BD80, "OffscreenBuffer_InitTextures", setup=setup, check_eax=False)

    def setup2(em):
        u32(em, 0xA84A54, 7)
        u32(em, 0x54E890, 9)
    check(e, 0x50EC00, "OffscreenBuffer_InitQVertex", setup=setup2, check_eax=False)


@pytest.mark.parametrize("pitch,height", [(64, 16), (30, 7), (0, 5)])
def test_alloc_texture(pitch, height):
    def setup(em):
        em.call(0x4B0BA0)
        tex = em.alloc(0x24)
        em.write(tex, noise(0x24, 1))
        data = em.alloc(max(pitch * height, 1))
        em.write(data, noise(pitch * height, 2))
        d = em.alloc(0x30)
        em.write(d, struct.pack("<12I", tex, 1, 2, pitch, 4, 5, 0x10033, 0x20000 + height, 0x7FFF0003, 9, 10, data))
        return (d,)
    compare(e, 0x50BE10, "OffscreenBuffer_AllocTexture", setup=setup, check_eax=False)

    def setup_free(em):
        d = setup(em)[0]
        em.call(0x50BE10, d)
        return (em.u32(d),)
    compare(e, 0x50BE70, "OffscreenBuffer_DeAllocTexture", setup=setup_free, check_eax=False)


def test_palette():
    def setup(em):
        pal = em.alloc(12)
        em.write(pal, noise(12, 3))
        colors = em.alloc(0x400)
        em.write(colors, bytes(random.Random(4).randrange(256) for _ in range(0x400)))
        d = em.alloc(12)
        em.write(d, struct.pack("<3I", pal, 256, colors))
        return (d,)
    compare(e, 0x50BE90, "OffscreenBuffer_AllocPalette", setup=setup, check_eax=False)

    for slot in (1, 3):
        def setup_free(em):
            em.write(SLOTS, b"\x01\x01\x01\x01")
            pal = em.alloc(12)
            em.write(pal, struct.pack("<3I", 0, slot, 0))
            return (pal,)
        compare(e, 0x50BF10, "OffscreenBuffer_DeAllocPalette", setup=setup_free, check_eax=False)


def test_palette_busy_hangs():
    for target in (0x50BE90, "OffscreenBuffer_AllocPalette"):
        e.reset()
        e.write(SLOTS, b"\x01")
        d = e.alloc(12)
        with pytest.raises(RuntimeError, match="did not return"):
            e.call(target, d, limit=10000)


def strip(em, rng, count=2, queue=-1, flags=0):
    verts = em.alloc(0x24 * count)
    em.write(verts, f32(*[rng.uniform(-50, 300) for _ in range(9 * count)]))
    s = em.alloc(0x14)
    em.write(s, struct.pack("<IIiiI", rng.randrange(2), flags, queue, count, verts))
    return s


@pytest.mark.parametrize("queue", [-1, 3])
@pytest.mark.parametrize("tex", [0, 1])
@pytest.mark.parametrize("pal", [0, 1])
@pytest.mark.parametrize("flags", [0, 0x80000])
def test_qvertex_strip(queue, tex, pal, flags):
    def setup(em):
        rng = random.Random(queue + tex * 2 + pal * 4 + flags)
        FRAME[0] = 1
        u32(em, 0x54E890, 0x1234)
        t = em.alloc(0x24)
        em.write(t, noise(0x24, 5))
        p = em.alloc(12)
        em.write(p, noise(12, 6))
        u32(em, TEX, t if tex else 0)
        u32(em, PAL, p if pal else 0)
        return (strip(em, rng, queue=queue, flags=flags),)
    check(e, 0x50EC40, "OffscreenBuffer_QVertexStrip", setup=setup, check_eax=False)


def test_qvertex_strip_no_memory():
    def setup(em):
        FRAME[0] = 0
        return (strip(em, random.Random(9), queue=2),)
    check(e, 0x50EC40, "OffscreenBuffer_QVertexStrip", setup=setup, check_eax=False)
    FRAME[0] = 1


@pytest.mark.parametrize("count", [1, 3])
def test_qvertex_strip_error(count):
    msg = check_raises(e, 0x50EC40, "OffscreenBuffer_QVertexStrip",
                       setup=lambda em: (strip(em, random.Random(1), count=count),))
    assert "Strip length" in msg


def target(em, rng, w=48, h=40):
    pix = em.alloc(w * h * 4)
    em.write(pix, bytes(rng.randrange(256) for _ in range(w * h * 4)))
    em.write(WIDTH, struct.pack("<iiI", w, h, pix))
    cx, cy = rng.uniform(-3, 8), rng.uniform(-3, 8)
    em.write(CLIP, f32(cx, cy, rng.uniform(20, w - cx), rng.uniform(20, h - cy)))
    return pix


def sprite(em, rng, tex=None, pal=None, w=None, h=None, rgb=None, alpha=None):
    s = em.alloc(0x84)
    em.write(s, noise(0x84, rng.randrange(50)))
    if tex:
        em.write(s + 0xC, tex)
    em.write(s + 0x40, struct.pack("<hh", 1 if pal else 0, 1 if tex else 0))
    if pal:
        em.write(s + 0x30, struct.pack("<3I", 0, 1, pal))
    uv = [rng.uniform(0, 0.4), rng.uniform(0, 0.4), rng.uniform(0.5, 1.0), rng.uniform(0.5, 1.0)]
    rgb = rgb or [rng.uniform(0, 1.2) for _ in range(3)]
    em.write(s + 0x44, f32(uv[0], uv[1], rng.uniform(-8, 30), rng.uniform(-8, 30),
                           rng.uniform(0, 1) if alpha is None else alpha, *rgb, uv[2], uv[3],
                           w if w is not None else rng.uniform(1, 30), h if h is not None else rng.uniform(1, 30)))
    return s


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
def test_render_rect(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        target(em, rng)
        return (sprite(em, rng, w=[None, 0, -1, 25.5, 7, None][seed], alpha=[None, None, 0, 1, None, 0.5][seed]),)
    check_cw(e, 0x50EDC0, "OffscreenBufferSprite_RenderRect", cw, setup=setup, check_eax=False)
    check_cw(e, 0x50EDA0, "OffscreenBufferSprite_Draw", cw, setup=setup, check_eax=False)


def test_render_rect_no_target():
    def setup(em):
        u32(em, PIXELS, 0)
        return (sprite(em, random.Random(1)),)
    check(e, 0x50EDC0, "OffscreenBufferSprite_RenderRect", setup=setup, check_eax=False)
    check(e, 0x50F070, "OffscreenBufferSprite_RenderSprite", setup=setup, check_eax=False)


def texture(em, rng, fmt, tw, th, pitch=None):
    bpp = {2: 2, 3: 4}.get(fmt, 1)
    pitch = pitch or tw * bpp
    pix = em.alloc(pitch * th)
    em.write(pix, bytes(rng.randrange(256) for _ in range(pitch * th)))
    return struct.pack("<IhhhhI", 0, tw, th, pitch, fmt, pix) + noise(0x14, 7)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("fmt", [2, 3])
@pytest.mark.parametrize("white", [0, 1])
@pytest.mark.parametrize("seed", range(6))
def test_render_sprite(fmt, white, seed, cw):
    def setup(em):
        rng = random.Random(seed * 7 + fmt)
        target(em, rng)
        tex = texture(em, rng, fmt, rng.randrange(8, 40), rng.randrange(8, 40), None if seed else 0x60)
        return (sprite(em, rng, tex=tex, rgb=[1.0, 1.0, 1.0] if white else None),)
    check_cw(e, 0x50F070, "OffscreenBufferSprite_RenderSprite", cw, setup=setup, check_eax=False)
    check_cw(e, 0x50EDA0, "OffscreenBufferSprite_Draw", cw, setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("white", [0, 1])
@pytest.mark.parametrize("seed", range(6))
def test_render_sprite_palette(white, seed, cw):
    def setup(em):
        rng = random.Random(seed + 100)
        target(em, rng)
        pal = em.alloc(0x200)
        em.write(pal, bytes(rng.randrange(256) for _ in range(0x200)))
        tex = texture(em, rng, rng.choice([0, 2, 3]), rng.randrange(8, 40), rng.randrange(8, 40), rng.randrange(40, 50))
        rgb = [1.0, 1.0, 1.0] if white else [1.0, 1.0, 0.5] if seed == 3 else None
        return (sprite(em, rng, tex=tex, pal=pal, rgb=rgb),)
    check_cw(e, 0x50F070, "OffscreenBufferSprite_RenderSprite", cw, setup=setup, check_eax=False)


def test_render_sprite_bad_format():
    def setup(em):
        rng = random.Random(3)
        target(em, rng, 300, 300)
        em.write(CLIP, f32(0, 0, 300, 300))
        s = sprite(em, rng, tex=texture(em, rng, 4, 8, 8), w=5, h=5)
        em.write(s + 0x4C, f32(10, 10))
        return (s,)
    msg = check_raises(e, 0x50F070, "OffscreenBufferSprite_RenderSprite", setup=setup)
    assert "Texture type not supported" in msg
