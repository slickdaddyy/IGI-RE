import struct

import pytest

from emu import Emu, compare

emu = Emu()


def new_list(e):
    p = e.alloc(12)
    e.write(p, struct.pack("<III", p + 4, 0, p))
    return p


def list_with_nodes(e, n):
    lst = new_list(e)
    nodes = [e.alloc(16) for _ in range(n)]
    for node in nodes:
        e.call("List_AddTail", lst, node)
    return lst, nodes


def test_list_add_tail():
    def setup(e):
        lst, _ = list_with_nodes(e, 3)
        return lst, e.alloc(16)
    compare(emu, 0x4AF8F0, "List_AddTail", setup=setup, check_eax=False)


def test_list_add_head():
    def setup(e):
        lst, _ = list_with_nodes(e, 2)
        return lst, e.alloc(16)
    compare(emu, 0x4AF910, "List_AddHead", setup=setup, check_eax=False)


@pytest.mark.parametrize("which", [0, 1, 2])
def test_list_remove(which):
    def setup(e):
        _, nodes = list_with_nodes(e, 3)
        return (nodes[which],)
    compare(emu, 0x4AF960, "List_Remove", setup=setup, check_eax=False)


def test_mem_init():
    compare(emu, 0x4B0BA0, "Mem_Init", check_eax=False)


def test_mem_set_default_mode():
    compare(emu, 0x4B0D40, "Mem_SetDefaultMode", check_eax=False)


def test_mem_push_pop_mode():
    def setup(e):
        e.call(0x4B0BA0)
        return (5,)
    compare(emu, 0x4B0D60, "Mem_PushMode", setup=setup, check_eax=False)

    def setup_pop(e):
        e.call(0x4B0BA0)
        e.call(0x4B0D60, 7)
        e.call(0x4B0D60, 9)
    compare(emu, 0x4B0D90, "Mem_PopMode", setup=setup_pop, check_eax=False)


@pytest.mark.parametrize("size,align", [(0, 4), (1, 4), (0x1C, 4), (100, 16), (4096, 32), (7, 8)])
@pytest.mark.parametrize("ready", [False, True])
def test_mem_alloc(size, align, ready):
    def setup(e):
        if ready:
            e.call(0x4B0BA0)
            e.call(0x4B0C60, 40, 4)
        return size, align
    compare(emu, 0x4B0C60, "Mem_Alloc", setup=setup)


def test_mem_free():
    def setup(e):
        e.call(0x4B0BA0)
        ptrs = [e.call(0x4B0C60, 24, 4)[0] for _ in range(3)]
        return (ptrs[1],)
    compare(emu, 0x4B0D10, "Mem_Free", setup=setup, check_eax=False)
