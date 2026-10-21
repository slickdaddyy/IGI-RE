import struct

import pytest

from emu import Emu, compare
from test_res_util import GOOD, blob

emu = Emu()


def chunk(size=0x40, align=4, next=0x20, id=b"BODY", form=b"IRES"):
    return struct.pack("<4sIII4s", id, size, align, next, form)


@pytest.mark.parametrize("c", [None, chunk(), chunk(next=0)])
def test_chunk_next(c):
    compare(emu, 0x4B7470, "Chunk_Next", setup=lambda e: (blob(e, c) if c else 0,))


@pytest.mark.parametrize("c", [None, chunk(), chunk(size=0), chunk(align=3), chunk(align=0), chunk(align=16),
                               chunk(size=0x14), chunk(size=0x15), chunk(align=64, size=0x40)])
def test_chunk_first_child(c):
    compare(emu, 0x4B7490, "Chunk_FirstChild", setup=lambda e: (blob(e, c) if c else 0,))


@pytest.mark.parametrize("off,c,p", [
    (0x20, chunk(), chunk(size=0x100)),
    (0x20, chunk(size=0), chunk(size=0x100)),
    (0x20, chunk(align=16), chunk(size=0x100, align=16)),
    (0x24, chunk(align=16), chunk(size=0x100, align=4)),
    (0x20, chunk(align=6), chunk(size=0x100)),
    (0x20, chunk(), chunk(size=0x100, align=12)),
    (0x200, chunk(), chunk(size=0x100)),
    (0x20, None, chunk()),
    (0x20, chunk(), None),
])
def test_chunk_data(off, c, p):
    def setup(e):
        base = blob(e, b"\0" * 0x300)
        if p:
            e.write(base, p)
        if c:
            e.write(base + off, c)
        return base + off if c else 0, base if p else 0
    compare(emu, 0x4B74D0, "Chunk_Data", setup=setup)


@pytest.mark.parametrize("size,align,c", [(1, 1, 1), (0, 1, 1), (1, 0, 1), (1, 1, 0)])
def test_chunk_get_info(size, align, c):
    def setup(e):
        return (e.alloc(4) if size else 0, e.alloc(4) if align else 0, blob(e, chunk()) if c else 0)
    compare(emu, 0x4B7540, "Chunk_GetInfo", setup=setup, check_eax=False)


@pytest.mark.parametrize("orig,ours", [(0x4B7570, "Chunk_FormType"), (0x4B7580, "Chunk_Id")])
@pytest.mark.parametrize("c", [0, 1])
def test_chunk_fields(orig, ours, c):
    compare(emu, orig, ours, setup=lambda e: (blob(e, chunk()) if c else 0,))


@pytest.mark.parametrize("id", [b"NAME", b"PATH", b"ILFF", b"NONE"])
def test_chunk_find(id):
    def setup(e):
        form = blob(e, GOOD)
        first = e.call(0x4B7490, form)[0]
        return first, struct.unpack("<I", id)[0]
    compare(emu, 0x4B7590, "Chunk_Find", setup=setup)
    compare(emu, 0x4B7590, "Chunk_Find", args=(0, 1))
