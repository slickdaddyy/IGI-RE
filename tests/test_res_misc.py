import pytest

from emu import Emu, compare
from test_res_util import LOG, cstr, errors_equal, fake, u32

emu = Emu()
faked = Emu()
for addr in (0x509930, 0x509960):
    faked.fake(addr)


def test_qvertex_init_shutdown():
    compare(faked, 0x4B59A0, "QVertex_Init", check_eax=False, normalized=True)

    def setup(e):
        e.call(0x4B59A0)
    compare(faked, 0x4B59D0, "QVertex_Shutdown", setup=setup, check_eax=False, normalized=True)


# SortRender


def item_fn(e):
    LOG[-1].append(("item", e.arg(0)))
    e.ret(0)


def sort_list(e, keys):
    LOG.append([])
    e.call(0x4B0BA0)
    lst = e.call(0x4B7330, max(len(keys), 1))[0]
    e.call(0x4B73A0, lst)
    fn = fake(e, "item_fn", item_fn)
    for k in keys:
        e.call(0x4B72E0, fn, 8, k)
    return lst


KEYS = [
    [],
    [5],
    [3, 1, 2],
    [0x80000000, 0x7FFFFFFF, 0, 0xFFFFFFFF, 0x3F800000, 0xBF800000, 0x3F800000, 0x12345678, 0x00010000, 0x100],
    [0x40000000 + 0x10000 * i for i in range(40)][::-1] + [0xC0000000 + 0x101 * i for i in range(25)],
]


@pytest.mark.parametrize("keys", KEYS)
def test_sort_render_sort(keys):
    def setup(e):
        lst = sort_list(e, keys)
        return lst + 8, len(keys)
    compare(emu, 0x4B7160, "SortRender_Sort", setup=setup, check_eax=False)


@pytest.mark.parametrize("keys", KEYS)
def test_sort_render_flush(keys):
    def setup(e):
        lst = sort_list(e, keys)
        t = e.alloc(16)
        u32(e, t + 12, lst)
        return (t,)
    compare(emu, 0x4B7100, "SortRender_Flush", setup=setup, check_eax=False)
    assert LOG[-2] == LOG[-1]


def test_sort_render_add():
    def setup(e):
        sort_list(e, [1, 2])
        return 0x401000, 12, 0x77
    compare(emu, 0x4B72E0, "SortRender_Add", setup=setup)


def test_sort_render_lists():
    compare(emu, 0x4B7330, "SortRender_NewList", setup=lambda e: e.call(0x4B0BA0) and None, args=(7,))
    compare(emu, 0x4B73A0, "SortRender_Begin", args=(0x1234,), check_eax=False)


def test_sort_render_submit():
    def setup(e):
        u32(e, 0x943E48, 9)
        u32(e, 0xA43E50, 0x5555)
        u32(e, 0xA84A4C, 1)
        u32(e, 0x6E5B98, e.alloc(64))
        u32(e, 0xBCABE4, e.alloc(64))
    compare(emu, 0x4B7360, "SortRender_Submit", setup=setup, args=(0x1234,), check_eax=False)


def test_sort_render_init_shutdown():
    compare(emu, 0x4B73C0, "SortRender_Init", check_eax=False, normalized=True)
    compare(emu, 0x4B73E0, "SortRender_Shutdown", setup=lambda e: e.call(0x4B73C0) and None, check_eax=False)


# PtrList


def test_ptr_list():
    compare(emu, 0x4B73F0, "PtrList_New", setup=lambda e: e.call(0x4B0BA0) and None, args=(5,))

    def setup(e):
        e.call(0x4B0BA0)
        p = e.call(0x4B73F0, 5)[0]
        e.call(0x4B7430, p, 0x111)
        return p, 0x222
    compare(emu, 0x4B7430, "PtrList_Add", setup=setup)

    def setup_clear(e):
        p, _ = setup(e)
        e.call(0x4B7430, p, 0x222)
        return p, 1
    compare(emu, 0x4B7450, "PtrList_Clear", setup=setup_clear, check_eax=False)


# Script_Error


def error_fn(e):
    LOG[-1].append((e.read_cstr(e.arg(0)), e.arg(1)))
    e.ret(0)


@pytest.mark.parametrize("verbose", [0, 1])
def test_script_error(verbose):
    def setup(e):
        LOG.append([])
        u32(e, 0xA96598, verbose)
        u32(e, 0xA43E70, fake(e, "error_fn", error_fn))
        u32(e, 0xA43E68, 0xBEEF)
        return cstr(e, b"Unknown identifier '%s' in expression, line %d"), cstr(e, b"foo"), 42
    compare(emu, 0x4B7E10, "Script_Error", setup=setup, check_eax=False)
    assert LOG[-2] == LOG[-1]


def test_script_error_no_handler():
    def setup(e):
        u32(e, 0xA96598, 1)
        u32(e, 0xA43E70, 0)
        return cstr(e, b"bad %d"), 7
    assert errors_equal(emu, 0x4B7E10, "Script_Error", setup) == "bad 7"
