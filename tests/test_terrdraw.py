"""Terrain drawing: blend modes, render chunks, vertex buffer callbacks (0x5144E0 - 0x516770)."""
import struct

import pytest

from emu import ErrorShown
from test_d3d_util import D3D, D3DEmu, check, check_raises, com, out, u32

DEV = {0: 3, 1: 1, 2: 1, 20: 3, 37: 4}
G_D3D, G_DEVICE = 0x5CA110, 0x5CA114
FOG_ENABLE, FOG_COLOR = 0x5470B4, 0xA7E068
HASH, POOL, VB, HWTL = 0xA7E080, 0xA7F878, 0xA7F890, 0xBCAD74

e = D3DEmu()


def device(em):
    u32(em, G_DEVICE, com(em, "dev", DEV, size=64))
    u32(em, FOG_ENABLE, 1)
    u32(em, FOG_COLOR, 0x00335577)


@pytest.mark.parametrize("mode", [0, 1, 2])
def test_set_blend_mode(mode):
    log = check(e, 0x5144E0, "Terrain_SetBlendMode", setup=device, args=(mode,), check_eax=False)
    assert len(log) == 6 + (mode != 0) * 2


@pytest.mark.parametrize("mode", [3, -1, 0x100])
def test_set_blend_mode_bad(mode):
    assert "Unknown blendmode" in check_raises(e, 0x5144E0, "Terrain_SetBlendMode", setup=device, args=(mode,))


def bucket(x, y):
    return ((y * 0xB5AF + x * 0x9D) & 0xFFFFFFFF) % 0x1FD


def colliding(n, x0=3, y0=5):
    """n coordinate pairs that share the bucket of (x0, y0)."""
    out_ = [(x0, y0)]
    y = y0
    while len(out_) < n:
        y += 1
        for x in range(-40, 40):
            if bucket(x, y) == bucket(x0, y0) and (x, y) not in out_:
                out_.append((x, y))
                break
    return out_


SAME = colliding(4)


def chunks(em, coords, used=None):
    em.call(0x516600)
    em.call(0x4B0B00, POOL, 0x2EF4, 0x80, 0)
    for x, y in coords:
        em.call(0x514680, x, y)
    if used is not None:
        u32(em, POOL + 8, used)


@pytest.mark.parametrize("xy", SAME + [(SAME[0][0], SAME[0][1] + 1000), (0, 0), (-7, 9), (0x7FFF, -0x8000)])
def test_chunk_get(xy):
    def setup(em):
        chunks(em, SAME[:3] + [(1, 2), (100, 200)])
        return xy
    check(e, 0x514680, "TerrChunk_Get", setup=setup)


def test_chunk_get_empty():
    def setup(em):
        chunks(em, [])
        return (11, 12)
    check(e, 0x514680, "TerrChunk_Get", setup=setup)


def test_chunk_get_full():
    def setup(em):
        chunks(em, SAME[:2], used=0x80)
        return SAME[3]
    assert "Out of terrain renderchunks" in check_raises(e, 0x514680, "TerrChunk_Get", setup=setup)


def test_chunk_get_full_found():
    def setup(em):
        chunks(em, SAME[:2], used=0x80)
        return SAME[1]
    check(e, 0x514680, "TerrChunk_Get", setup=setup)


def test_init():
    check(e, 0x516600, "TerrDraw_Init", check_eax=False)


def d3d(em):
    u32(em, G_D3D, com(em, "d3d", D3D, methods={5: (0, out(2, 0x7777, extra=lambda em: em.read(em.arg(1), 16)))}))


@pytest.mark.parametrize("hw", [0, 1])
def test_set_mode(hw):
    def setup(em):
        d3d(em)
        u32(em, HWTL, hw)
    log = check(e, 0x516640, "TerrDraw_SetMode", setup=setup, check_eax=False)
    assert log[0][0] == "d3d.5"


def test_unset_mode():
    def setup(em):
        em.call(0x4B0B00, POOL, 0x2EF4, 0x80, 0)
        u32(em, VB, com(em, "vb", {0: 3, 1: 1, 2: 1}))
    log = check(e, 0x5166C0, "TerrDraw_UnsetMode", setup=setup, check_eax=False)
    assert log == [("vb.2", ("vb",))] or log[0][0] == "vb.2"


@pytest.mark.parametrize("hw", [0, 1])
def test_restore(hw):
    def setup(em):
        d3d(em)
        u32(em, HWTL, hw)
        u32(em, VB, com(em, "vb", {0: 3, 1: 1, 2: 1}))
    log = check(e, 0x5166F0, "TerrDraw_Restore", setup=setup, check_eax=False)
    assert [x[0] for x in log] == ["vb.2", "d3d.5"]
