import random
import struct

import pytest

from emu import compare
from test_freecam_util import mem_init, new_emu, u32

e = new_emu()


def bitstream(e, buf, mask=0x80, rack=0):
    bs = e.alloc(12)
    u32(e, bs, buf, mask, rack)
    return bs


def data(kind, n, seed=1):
    r = random.Random(seed)
    if kind == "random":
        return bytes(r.randrange(256) for _ in range(n))
    if kind == "zeros":
        return bytes(n)
    if kind == "text":
        words = [b"the ", b"quick ", b"brown ", b"fox ", b"jumps ", b"over ", b"lazy ", b"dog "]
        out = b""
        while len(out) < n:
            out += r.choice(words)
        return out[:n]
    return bytes(r.randrange(4) for _ in range(n))


@pytest.mark.parametrize("bit", [0, 1, 5])
@pytest.mark.parametrize("mask", [0x80, 0x10, 1])
def test_write_bit(bit, mask):
    def setup(e):
        buf = e.alloc(16)
        return bitstream(e, buf, mask, 0x55 & ~(mask * 2 - 1) if mask < 0x80 else 0), bit
    compare(e, 0x530810, "BitStream_WriteBit", setup=setup, check_eax=False)


@pytest.mark.parametrize("value,count", [(0, 11), (0x5A5, 11), (0x7F, 7), (0xAB, 8), (1, 1), (0x12345678, 32),
                                         (3, 0)])
@pytest.mark.parametrize("mask", [0x80, 0x8, 1])
def test_write_bits(value, count, mask):
    def setup(e):
        buf = e.alloc(16)
        return bitstream(e, buf, mask, 0), value, count
    compare(e, 0x530850, "BitStream_WriteBits", setup=setup, check_eax=False)


@pytest.mark.parametrize("mask", [0x80, 0x40, 2, 1])
def test_read_bit(mask):
    for byte in (0x00, 0xFF, 0xA5, 0x5A):
        def setup(e):
            buf = e.alloc(16)
            e.write(buf, bytes([byte, 0x3C, 0xC3]))
            return (bitstream(e, buf, mask, 0x96),)
        compare(e, 0x5308B0, "BitStream_ReadBit", setup=setup)


@pytest.mark.parametrize("count", [0, 1, 7, 8, 11, 32])
@pytest.mark.parametrize("mask", [0x80, 0x4, 1])
def test_read_bits(count, mask):
    def setup(e):
        buf = e.alloc(16)
        e.write(buf, bytes(random.Random(count).randrange(256) for _ in range(8)))
        return bitstream(e, buf, mask, 0xB7), count
    compare(e, 0x5308F0, "BitStream_ReadBits", setup=setup)


def test_codec_register():
    compare(e, 0x530950, "Codec_Register", args=(3, 0x12345678), check_eax=False)


def test_lzss_register():
    compare(e, 0x530800, "Lzss_Register", check_eax=False)


@pytest.mark.parametrize("r", [1, 0x7FF, 0x400])
def test_init_tree(r):
    def setup(e):
        tree = e.alloc(0x3008)
        e.write(tree, b"\x77" * 0x3008)
        return tree, r
    compare(e, 0x530470, "Lzss_InitTree", setup=setup, check_eax=False)


def tree_state(e, kind, n, seed=1):
    """A tree and window filled the way Lzss_Compress does it, with the original code."""
    mem_init(e)
    tree = e.alloc(0x3006)
    window = e.alloc(0x800)
    e.write(window, data(kind, 0x800, seed))
    match = e.alloc(4)
    e.call(0x530470, tree, 1)
    for pos in range(2, n):
        e.call(0x530630, tree, window, pos, match)
    return tree, window, match


def tree_nodes(e, tree, n):
    """Positions in the tree, with their links."""
    out = []
    for p in range(1, n):
        par, sm, lg = struct.unpack("<3h", e.read(tree + 6 * p, 6))
        if par:
            out.append((p, sm, lg))
    return out


KINDS = ["random", "zeros", "text", "small"]


@pytest.mark.parametrize("kind", KINDS)
@pytest.mark.parametrize("n", [3, 40, 300])
def test_add_string(kind, n):
    def setup(e):
        tree, window, match = tree_state(e, kind, n)
        return tree, window, n, match
    compare(e, 0x530630, "Lzss_AddString", setup=setup)


def test_add_string_zero():
    def setup(e):
        tree, window, match = tree_state(e, "text", 20)
        return tree, window, 0, match
    compare(e, 0x530630, "Lzss_AddString", setup=setup)


def pick_nodes(kind, n):
    """Tree positions with no child, one child and two children."""
    tree, window, match = tree_state(e, kind, n)
    nodes = tree_nodes(e, tree, n)
    e.reset()
    out = {}
    for p, sm, lg in nodes:
        out.setdefault((sm != 0, lg != 0), p)
    return out


@pytest.mark.parametrize("kind", ["random", "text", "small"])
def test_delete_string(kind):
    nodes = pick_nodes(kind, 200)
    assert len(nodes) >= 3
    for p in list(nodes.values()) + [250]:
        def setup(e):
            tree, window, match = tree_state(e, kind, 200)
            return tree, p
        compare(e, 0x5304B0, "Lzss_DeleteString", setup=setup, check_eax=False)


@pytest.mark.parametrize("kind", ["random", "text"])
def test_find_next_and_replace(kind):
    nodes = pick_nodes(kind, 200)
    p = nodes[(True, True)]
    compare(e, 0x5305F0, "Lzss_FindNextNode", setup=lambda e: (tree_state(e, kind, 200)[0], p))
    for old, new in ((p, 210), (nodes[(True, False)], 211)):
        compare(e, 0x530580, "Lzss_ReplaceNode", setup=lambda e: (tree_state(e, kind, 200)[0], old, new),
                check_eax=False)
    for key in ((True, False), (False, True)):
        old = nodes[key]

        def setup(e):
            tree = tree_state(e, kind, 200)[0]
            par, sm, lg = struct.unpack("<3h", e.read(tree + 6 * old, 6))
            return tree, old, sm or lg
        compare(e, 0x530520, "Lzss_ContractNode", setup=setup, check_eax=False)


def compress_setup(src):
    def setup(e):
        mem_init(e)
        p = e.alloc(len(src) + 1)
        e.write(p, src)
        pp = e.alloc(4)
        u32(e, pp, p)
        out = e.alloc(len(src) * 2 + 64)
        return 0, len(src), pp, bitstream(e, out)
    return setup


@pytest.mark.parametrize("kind", KINDS)
@pytest.mark.parametrize("n", [0, 1, 2, 5, 131, 1000, 3000])
def test_compress(kind, n):
    compare(e, 0x5302D0, "Lzss_Compress", setup=compress_setup(data(kind, n, n)), check_eax=False)


@pytest.mark.parametrize("kind", KINDS)
@pytest.mark.parametrize("n", [0, 1, 5, 1000, 3000])
def test_decompress(kind, n):
    src = data(kind, n, n + 7)

    def setup(e):
        mem_init(e)
        p = e.alloc(n + 1)
        e.write(p, src)
        pp = e.alloc(4)
        u32(e, pp, p)
        buf = e.alloc(n * 2 + 64)
        e.call(0x5302D0, 0, n, pp, bitstream(e, buf))
        out = e.alloc(n + 16)
        outp = e.alloc(4)
        u32(e, outp, out)
        return 0, bitstream(e, buf), outp
    compare(e, 0x530740, "Lzss_Decompress", setup=setup, check_eax=False)
