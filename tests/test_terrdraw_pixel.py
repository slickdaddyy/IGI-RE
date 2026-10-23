"""Pixel conversion for texture uploads (0x517B00 - 0x518121)."""
import random
import struct

import pytest

from emu import Emu, compare

e = Emu()


def noise(n, seed):
    rng = random.Random(seed)
    return bytes(rng.randrange(256) for _ in range(n))


def buf(em, data, pad=64):
    p = em.alloc(len(data) + 2 * pad) + pad
    em.write(p - pad, b"\xaa" * pad + data + b"\xbb" * pad)
    return p


@pytest.mark.parametrize("w,h", [(2, 2), (4, 4), (8, 2), (3, 5), (16, 8), (1, 4), (4, 1), (0, 4), (6, -2), (-4, 4)])
@pytest.mark.parametrize("seed", [0, 1, 2])
def test_halve(w, h, seed):
    def setup(em):
        src = noise(4 * max(w, 1) * max(h, 1) + 64, seed)
        if seed == 2:
            src = b"\xff" * len(src)
        dst = buf(em, b"\xcc" * (max(w, 1) * max(h, 1) + 64))
        return dst, buf(em, src), w, h
    compare(e, 0x517B00, "Pixel_Halve32", setup=setup, check_eax=False)


RECTS = [None, (0, 0, 4, 4), (1, 2, 3, 2), (2, 1, 100, 100), (0, 0, 0, 0), (3, 3, -1, 2), (1, 0, 6, 1)]


def rect(em, r):
    if r is None:
        return 0
    return buf(em, struct.pack("<4i", *r))


@pytest.mark.parametrize("w,h,pitch,rows", [(4, 4, 4, 4), (6, 3, 4, 4), (2, 5, 8, 6), (5, 5, 3, 2), (0, 2, 4, 4),
                                            (3, 0, 4, 4)])
@pytest.mark.parametrize("r", range(len(RECTS)))
@pytest.mark.parametrize("addr,name", [(0x517C90, "Pixel_1555To32"), (0x517DD0, "Pixel_565To32")])
def test_16bit(addr, name, w, h, pitch, rows, r):
    def setup(em):
        src = buf(em, noise(2 * pitch * rows + 256, w * 7 + r))
        dst = buf(em, b"\xcc" * (4 * w * h + 16))
        return dst, w, h, src, pitch, rows, rect(em, RECTS[r])
    compare(e, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("w,h,pitch,rows", [(4, 4, 4, 4), (6, 3, 4, 4), (2, 5, 8, 6), (5, 5, 3, 2), (0, 2, 4, 4)])
@pytest.mark.parametrize("r", range(len(RECTS)))
def test_copy32(w, h, pitch, rows, r):
    def setup(em):
        src = buf(em, noise(16 * pitch * rows + 1024, w + r))
        dst = buf(em, b"\xcc" * (4 * w * h + 16))
        return dst, w, h, src, pitch, rows, rect(em, RECTS[r])
    compare(e, 0x517F00, "Pixel_Copy32", setup=setup, check_eax=False)


@pytest.mark.parametrize("w,h,pitch,rows", [(4, 4, 4, 4), (6, 3, 4, 4), (2, 5, 8, 6), (5, 5, 3, 2), (0, 2, 4, 4)])
@pytest.mark.parametrize("r", range(len(RECTS)))
@pytest.mark.parametrize("alpha", [0, 1])
def test_8bit(w, h, pitch, rows, r, alpha):
    def setup(em):
        src = bytearray(noise(pitch * rows + 64, w * 3 + r))
        for i in range(0, len(src), 3):
            src[i] = 0
        s = buf(em, bytes(src))
        a = buf(em, noise(pitch * rows + 64, 99)) if alpha else 0
        pal = buf(em, noise(1024, 5))
        dst = buf(em, b"\xcc" * (4 * w * h + 16))
        return dst, w, h, a, s, pal, pitch, rows, rect(em, RECTS[r])
    compare(e, 0x517FD0, "Pixel_8To32", setup=setup, check_eax=False)
