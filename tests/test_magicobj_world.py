import struct

import pytest

from emu import CALL_LOG, FAKE_BASE, Emu, ErrorShown, compare
from test_magicobj import add_children, logger, set_handler, task_system

emu = Emu()
emu.stub("MessageBoxA", lambda e: e.ret(1, 16))
emu.stub("OutputDebugStringA", lambda e: e.ret(0, 4))

wk = Emu()
WFAKES = {0x4CE620: 2, 0x4C7560: 2, 0x5177F0: 1, 0x4B7D30: 1, 0x4B63D0: 1, 0x4D1220: 0, 0x4B22F0: 1,
          0x4E54E0: 1, 0x4B2670: 0, 0x4B2610: 1, 0x48F250: 0, 0x4B23B0: 9}
for addr, n in WFAKES.items():
    wk.fake(addr, nargs=n)

# cells read by the hand written stubs below; setup fills them before each run
STR_POS = FAKE_BASE + 0x40000
CELLS = FAKE_BASE + 0x40010


def str_fake(e, addr, ret_cell, extra=b""):
    """Replace addr with a stub that logs addr in the call log, appends its C string argument to the buffer
    at [STR_POS] (so stack strings compare by content) and returns [ret_cell]."""
    c = (b"\x8b\x0d" + struct.pack("<I", CALL_LOG)
         + b"\xc7\x04\x8d" + struct.pack("<II", CALL_LOG + 4, addr)
         + b"\x41\x89\x0d" + struct.pack("<I", CALL_LOG)
         + b"\x56\x57\x8b\x74\x24\x0c\x8b\x3d" + struct.pack("<I", STR_POS)
         + b"\xac\xaa\x84\xc0\x75\xfa"
         + b"\x89\x3d" + struct.pack("<I", STR_POS) + b"\x5f\x5e"
         + extra
         + b"\xa1" + struct.pack("<I", ret_cell) + b"\xc3")
    stub = wk.fake_top
    wk.fake_top += (len(c) + 15) & ~15
    e.write(stub, c)
    e.write(addr, b"\xe9" + struct.pack("<i", stub - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)


RET_TEX, RET_EXISTS, RET_LOAD, LOAD_SIZE, PAL_LOG = (CELLS + 4 * i for i in range(5))
str_fake(wk, 0x5175A0, RET_TEX)
str_fake(wk, 0x4B5B60, RET_EXISTS)
# Resource_Load also stores the size: mov ecx,[esp+8]; mov edx,[LOAD_SIZE]; mov [ecx],edx
str_fake(wk, 0x4B5F00, RET_LOAD, b"\x8b\x4c\x24\x08\x8b\x15" + struct.pack("<I", LOAD_SIZE) + b"\x89\x11")
str_fake(wk, 0x4B6380, RET_TEX)
# Palette_Create: copy the 256 colours and the other two arguments to the buffer at [PAL_LOG]
_pal = (b"\x56\x57\x8b\x3d" + struct.pack("<I", PAL_LOG)
        + b"\x8b\x44\x24\x0c\xab\x8b\x44\x24\x14\xab"
        + b"\x8b\x74\x24\x10\xb9\x00\x01\x00\x00\xf3\xa5\x5f\x5e\xc3")
wk.write(wk.fake_top, _pal)
wk.write(0x4B7CF0, b"\xe9" + struct.pack("<i", wk.fake_top - 0x4B7CF0 - 5))
wk.fake_top += 0x40

COUNTERS = [0xB97DF4, 0xB97D24, 0xB97DF8, 0xBA2058, 0xBA5060]
SAVED = 0xB97D80


def fill_counters(e):
    for i, a in enumerate(COUNTERS):
        e.write(a, struct.pack("<I", 0x100 + i))
    e.write(SAVED, struct.pack("<5I", 11, 22, 33, 44, 55))


@pytest.mark.parametrize("orig,ours", [(0x4C5810, "Coll_ResetMods"), (0x4C5830, "Coll_SaveMods"),
                                       (0x4C5870, "Coll_RestoreMods")])
def test_counters(orig, ours):
    compare(emu, orig, ours, setup=fill_counters, check_eax=False)


@pytest.mark.parametrize("kind", range(6))
def test_child_count(kind):
    compare(emu, 0x4C58B0, "DynCubeTree_GetChildCount", args=(kind,))


@pytest.mark.parametrize("kind", [6, -1, 100])
def test_child_count_invalid(kind):
    for target in (0x4C58B0, "DynCubeTree_GetChildCount"):
        emu.reset()
        with pytest.raises(ErrorShown, match="Invalid dyncubetree"):
            emu.call(target, kind)


def test_open_events():
    compare(emu, 0x4C5900, "sub_4C5900", setup=task_system, check_eax=False, normalized=True)


def test_close_events():
    def setup(e):
        task_system(e)
        e.call(0x4C5900)
    compare(emu, 0x4C5AA0, "sub_4C5AA0", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("kids", [[], [0x41], [0x41, 0x42, 0x41, 0x43]])
def test_handler_4C5A10(kids):
    def setup(e):
        e.write(0x548177, b"\x33")
        log = logger(e)
        set_handler(e, 0x33, 0x41, log)
        set_handler(e, 0x33, 0x43, log)
        t = e.alloc(0x20)
        e.write(t + 8, struct.pack("<III", t + 12, 0, t + 8))
        add_children(e, t, kids)
        arg = e.alloc(4)
        e.write(arg, struct.pack("<I", 0x2000A000))
        return t, arg
    compare(wk, 0x4C5A10, "sub_4C5A10", setup=setup, check_eax=False, normalized=True)


def test_room_init():
    def setup(e):
        r = e.alloc(0x40)
        e.write(r, bytes(range(0x40)))
        return 0x1111, r, 0x22, 0x3F800000, 5
    compare(emu, 0x4C5C90, "DynCubeNode_Init", setup=setup, check_eax=False)


@pytest.mark.parametrize("rooms", [6, 10])
def test_world_new(rooms):
    def setup(e):
        e.call(0x4B0BA0)
        e.write(0x536148, struct.pack("<I", 0x12345))
        e.write(0xB81B20, b"\x77" * 0x16000)
        fill_counters(e)
        return (rooms,)
    compare(wk, 0x4C5B20, "World_New", setup=setup, normalized=True)


def world(e, full):
    e.call(0x4B0BA0)
    w = e.call(0x4C5B20, 6)[0]
    if not full:
        return w
    alloc = lambda n=16: e.call(0x4B0C60, n, 4)[0]
    e.write(w + 0xDC0, struct.pack("<II", alloc(), alloc()))
    e.write(w + 0xDBC, struct.pack("<I", 0x2000BB00))
    e.write(w + 0xDF4, struct.pack("<I", alloc(12)))
    for off, shared in ((0xDCC, 0xB81A20), (0xDD0, 0xB81920)):
        lst = alloc(0xC00)
        for i in range(0, 0x100, 3):
            e.write(lst + 12 * i, struct.pack("<I", alloc()))
            e.write(shared + i, bytes([i % 2]))
        e.write(w + off, struct.pack("<I", lst))
    e.write(w + 4, struct.pack("<III", 0x2000C004, 0x2000C008, 0x2000C00C))
    for i in (0, 5, 100, 2047):
        e.write(0xB81B24 + 0x2C * i, struct.pack("<I", 0x2000D000 + i))
    for i in range(6):
        room = e.u32(w + 0x10 + 4 * i)
        e.write(room + 4, struct.pack("<I", i))
        if i % 2:
            e.write(room + 0x3C, struct.pack("<I", 0x2000E000 + i))
    return w


@pytest.mark.parametrize("full", [False, True])
def test_world_free(full):
    compare(wk, 0x4C5CE0, "World_Free", setup=lambda e: (world(e, full),), check_eax=False, normalized=True)


def lmp_data(e):
    sizes = [4, 0, 2, 8, 0, 1]
    data = b"".join(struct.pack("<I", n) + bytes(range(n * n)) for n in sizes)
    p = e.alloc(len(data))
    e.write(p, data)
    return p, len(data)


@pytest.mark.parametrize("tmus", [1, 2])
@pytest.mark.parametrize("case", ["nolmp", "missing", "loadfail", "load", "empty"])
def test_world_load(tmus, case):
    def setup(e):
        e.call(0x4B0BA0)
        w = e.alloc(0xF00)
        e.write(w, b"\x66" * 0xF00)
        strs = e.alloc(0x200)
        pal = e.alloc(0x408)
        p, size = lmp_data(e)
        if case == "loadfail":
            p = 0
        if case == "empty":
            size = 0
        e.write(STR_POS, struct.pack("<I", strs))
        e.write(CELLS, struct.pack("<5I", 0x2000AB00, case != "missing", p, size, pal))
        wk.set_fake_ret(0x4B2670, tmus)
        wk.set_fake_ret(0x48F250, case != "nolmp")
        return w, e.alloc(32) and _name(e)
    compare(wk, 0x4C5F00, "sub_4C5F00", setup=setup, check_eax=False, normalized=True)


def _name(e):
    p = e.alloc(32)
    e.write(p, b"LOCAL:missions/location0/level1\0")
    return p
