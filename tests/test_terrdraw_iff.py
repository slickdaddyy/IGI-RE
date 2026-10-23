"""IFF tree parser (0x517840 - 0x517AFB)."""
import struct

import pytest

from emu import Emu, compare
from test_d3d_util import u32

e = Emu()


def be(tag):
    return tag if isinstance(tag, bytes) else struct.pack(">I", tag)


def chunk(tag, data):
    out = be(tag) + struct.pack(">I", len(data)) + data
    return out + (b"\0" if len(data) & 1 else b"")


def form(kind, *kids):
    body = be(kind) + b"".join(kids)
    return chunk(b"FORM", body)


TREE = form(b"MODL",
            chunk(b"HEAD", b"abc"),
            form(b"MESH", chunk(b"VERT", b"0123456789ab"), chunk(b"FACE", b"xy"), form(b"SUBM", chunk(b"VERT", b"q"))),
            chunk(b"VERT", b"zz"),
            form(b"MESH", chunk(b"FACE", b"12345"), chunk(b"NAME", b"")),
            form(b"EMPT"),
            chunk(b"TAIL", b"!"))

BLOBS = [TREE, form(b"ONE"), form(b"TWO", chunk(b"A", b"")), chunk(b"HEAD", b"not a form"), b"FORX\0\0\0\0",
         form(b"ODD", chunk(b"X", b"123"), chunk(b"Y", b"45"))]


def blob(em, data):
    p = em.alloc(len(data) + 16)
    em.write(p, data + b"\0" * 16)
    return p


@pytest.mark.parametrize("i", range(len(BLOBS)))
def test_parse(i):
    compare(e, 0x517A00, "Iff_Parse", setup=lambda em: (blob(em, BLOBS[i]),))


@pytest.mark.parametrize("off,size", [(0, 1), (0, 0), (12, len(TREE) - 12), (12, 5), (12, -3), (12, 30)])
def test_parse_list(off, size):
    def setup(em):
        return blob(em, TREE) + off, size, 0x5151
    compare(e, 0x517A30, "Iff_ParseList", setup=setup)


@pytest.mark.parametrize("data", [b"\x12\x34\x56\x78", b"\xff\x00\x80\x01", b"FORM"])
def test_read_long(data):
    compare(e, 0x5179D0, "Iff_ReadLong", setup=lambda em: (blob(em, data),))


def nodes(em, root):
    """All nodes depth first."""
    out = []

    def walk(n):
        while n:
            out.append(n)
            child = em.u32(n + 8)
            if child:
                walk(child)
            n = em.u32(n)
    walk(root)
    return out


def tree(em):
    return em.call(0x517A00, blob(em, TREE))[0]


def test_get_data():
    compare(e, 0x5179C0, "Iff_GetData", setup=lambda em: (nodes(em, tree(em))[3],))


@pytest.mark.parametrize("owned", [0, 1])
def test_free(owned):
    def setup(em):
        root = tree(em)
        for k, n in enumerate(nodes(em, root)):
            if owned and k % 2:
                u32(em, n + 0xC, 1)
                u32(em, n + 0x1C, em.call(0x4B0C60, 8, 4)[0])
        return (root,)
    compare(e, 0x517840, "Iff_Free", setup=setup, check_eax=False)


def test_free_null():
    compare(e, 0x517840, "Iff_Free", args=(0,), check_eax=False)


TYPES = [b"MODL", b"MESH", b"SUBM", b"EMPT", b"VERT", b"FACE", b"NONE", b"TAIL", b"\0\0\0\0"]


@pytest.mark.parametrize("start", range(0, 16, 3))
@pytest.mark.parametrize("kind", TYPES)
@pytest.mark.parametrize("addr,name", [(0x517890, "Iff_FindForm"), (0x5178E0, "Iff_FindNextForm"),
                                       (0x517940, "Iff_FindChunk"), (0x517980, "Iff_FindNextChunk")])
def test_find(addr, name, kind, start):
    def setup(em):
        ns = nodes(em, tree(em))
        return ns[start % len(ns)], struct.unpack(">I", kind)[0]
    compare(e, addr, name, setup=setup)


@pytest.mark.parametrize("kind", TYPES[:3])
def test_find_form_null(kind):
    compare(e, 0x517890, "Iff_FindForm", args=(0, struct.unpack(">I", kind)[0]))
    compare(e, 0x517980, "Iff_FindNextChunk", args=(0, struct.unpack(">I", kind)[0]))
