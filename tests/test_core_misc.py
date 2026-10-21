import struct

import pytest

from emu import Emu, compare
from test_core import list_with_nodes, new_list

emu = Emu()


@pytest.mark.parametrize("n", [0, 1, 3])
def test_list_rem_head(n):
    compare(emu, 0x4AF930, "List_RemHead", setup=lambda e: (list_with_nodes(e, n)[0],))


@pytest.mark.parametrize("which", [0, 2, "tail"])
def test_list_insert_before(which):
    def setup(e):
        lst, nodes = list_with_nodes(e, 3)
        pos = lst + 4 if which == "tail" else nodes[which]
        return pos, e.alloc(8)
    compare(emu, 0x4AF990, "List_InsertBefore", setup=setup, check_eax=False)


@pytest.mark.parametrize("n", [0, 1, 5])
def test_list_count(n):
    compare(emu, 0x4AF9B0, "List_Count", setup=lambda e: (list_with_nodes(e, n)[0],))


@pytest.mark.parametrize("blocks", [0, 1, 4])
@pytest.mark.parametrize("bad_count", [False, True])
def test_mem_shutdown(blocks, bad_count):
    def setup(e):
        from test_core_error import recorder
        e.write(0x936274, struct.pack("<I", recorder(e)[0]))
        e.call(0x4B0BA0)
        for i in range(blocks):
            e.call(0x4B0C60, 16 + i, 4)
        if bad_count:
            e.write(0x942300, struct.pack("<I", 77))
    compare(emu, 0x4B0BE0, "Mem_Shutdown", setup=setup, check_eax=False)


def test_mem_clear_mode():
    compare(emu, 0x4B0D50, "Mem_ClearMode", setup=lambda e: [e.call(0x4B0BA0)] and None, check_eax=False)


@pytest.mark.parametrize("mode", [0, 1, 0x85])
def test_mem_get_mode(mode):
    compare(emu, 0x4B0DB0, "Mem_GetMode", setup=lambda e: e.write(0x9422FD, bytes([mode])), eax_mask=0xFF)


@pytest.mark.parametrize("size,count,align", [(4, 10, 0), (0x40, 100, 4), (7, 3, 16), (12, 0, 8), (1, 5, 2)])
def test_pool_init(size, count, align):
    def setup(e):
        e.call(0x4B0BA0)
        return e.alloc(0x18), size, count, align
    compare(emu, 0x4B0B00, "Pool_Init", setup=setup, check_eax=False)


def test_pool_free():
    def setup(e):
        e.call(0x4B0BA0)
        pool = e.alloc(0x18)
        e.call(0x4B0B00, pool, 8, 4, 4)
        return (pool,)
    compare(emu, 0x4B0B70, "Pool_Free", setup=setup, check_eax=False)
