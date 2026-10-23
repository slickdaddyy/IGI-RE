"""Particle2D helpers and the TagItem types (0x4FE5D0 - 0x4FEB80)."""
import random
import struct

import pytest

from emu import Emu, compare
from test_computer_util import (LOG, add_child, cstr, handler, logfake, make_emu, run, set_handler, setup_open, task,
                                u16, u32, with_cw)
from test_vehicle_util import d64

plain = Emu()


def particles(e, n, seed=1):
    rng = random.Random(seed)
    ps = e.alloc(0x100)
    arr = e.alloc(4 * max(n, 1))
    items = []
    for i in range(n):
        p = e.alloc(0x58)
        d64(e, p, *[rng.uniform(-1e4, 1e4) for _ in range(3)])
        u32(e, p + 0x30, 0x11111111 * (i + 1))
        items.append(p)
    u32(e, arr, *items) if items else None
    u32(e, ps + 0x6C, arr)
    u32(e, ps + 0xF0, n)
    return ps


def test_particle_set_emitting():
    compare(plain, 0x4FE5D0, "Particle2D_SetAutoDelete", setup=lambda e: (particles(e, 1), 0xABCD01), check_eax=False)


@pytest.mark.parametrize("n", [0, 1, 3])
@pytest.mark.parametrize("bits", [0x3E99999A, 0x7F800001])
def test_particle_set_level(n, bits):
    compare(plain, 0x4FE5E0, "Particle2D_SetLevel", setup=lambda e: (particles(e, n), bits), check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F, 0xC7F])
@pytest.mark.parametrize("n", [0, 1, 4])
def test_particle_translate(n, cw):
    def setup(e):
        d = e.alloc(24)
        d64(e, d, 0.1, -12345.678, 3.0e-3)
        return particles(e, n, seed=n), d
    compare(plain, with_cw(plain, 0x4FE610, cw), with_cw(plain, "Particle2D_Translate", cw), setup=setup,
            check_eax=False)


# --- TagItem on the original task system ---------------------------------

init = make_emu()

emu = make_emu()
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
logfake(emu, 0x4C1830, 1)  # QTaskList_Free
emu.baseline = emu.snapshot()


def test_init():
    compare(init, 0x4FE7C0, "TagItem_Init", setup=setup_open, check_eax=False, normalized=True)


def test_shutdown():
    def setup(e):
        u16(e, 0xA7A4E0, 0x41)
        u32(e, 0xA7A4E4, 0xE4E4)
        u16(e, 0xA7A4E8, 0x42)
        u32(e, 0xA7A4EC, 0xECEC, 0xF0F0)
        u16(e, 0xA7A4F4, 0x2A, 0x43)
        u32(e, 0xA7A4F8, 0xF8F8)
        u16(e, 0xA7A4FC, 0x44, 0x45)
        u32(e, 0xA7A500, 0x500)
    run(emu, 0x4FEB80, "TagItem_Shutdown", setup)


def tag_setup(e):
    """Task system with the TagItem types; returns a parent task."""
    setup_open(e)
    e.call(0x4FE7C0)
    return task(e, 0x40, e.call(0x4018F0)[0] & 0xFFFF)


@pytest.mark.parametrize("bits", [0x40490FDB, 0xFF800001, 0])
def test_add_real32(bits):
    def setup(e):
        parent = tag_setup(e)
        e.call(0x4FE650, parent, cstr(e, b"first"), 0x3F800000)
        return parent, cstr(e, b"speed_limit"), bits
    run(init, 0x4FE650, "TagItem_AddReal32", setup, check_eax=True)


@pytest.mark.parametrize("value", [b"", b"abc", b"fifteen chars!!"])
def test_add_string16(value):
    def setup(e):
        parent = tag_setup(e)
        return parent, cstr(e, b"label"), cstr(e, value)
    run(init, 0x4FE6B0, "TagItem_AddString16", setup, check_eax=True)


@pytest.mark.parametrize("name", [b"a", b"ab", b"abc", b"abd", b"b", b"", b"zz", b"other"])
def test_find(name):
    def setup(e):
        parent = tag_setup(e)
        e.call(0x4FE650, parent, cstr(e, b"ab"), 0)
        other = task(e, 0x200, e.call(0x4018F0)[0] & 0xFFFF)
        e.write(other + 0x20, b"abc\0")
        add_child(e, parent, other)
        e.call(0x4FE6B0, parent, cstr(e, b"abd"), cstr(e, b"x"))
        e.call(0x4FE650, parent, cstr(e, b""), 0)
        e.call(0x4FE650, parent, cstr(e, b"zz"), 0)
        return parent, cstr(e, name)
    run(init, 0x4FE730, "TagItem_Find", setup, check_eax=True)


def test_find_empty():
    run(init, 0x4FE730, "TagItem_Find", lambda e: (tag_setup(e), cstr(e, b"x")), check_eax=True)


CHANGED = handler(init, "changed", 2)


@pytest.mark.parametrize("case", ["none", "nohandler", "handler"])
def test_on_changed(case):
    def setup(e):
        e.write(0xA7A4F4, bytes([0x25, 0x77, 0x77, 0x77]))
        owner = task(e, 0x40, 0x33)
        item = task(e, 0x128, 0x34, parent=owner if case != "none" else 0)
        if case == "handler":
            set_handler(e, 0x25, 0x33, CHANGED)
        return (item,)
    run(init, 0x4FEA60, "TagItem_OnChanged", setup)
    if case == "handler":
        assert LOG[-1] and LOG[-1][0][0] == "changed"


@pytest.mark.parametrize("orig,ours", [
    (0x4FEAA0, "TagItemInt32_Create"),
    (0x4FEAF0, "TagItemReal32_Create"),
    (0x4FEB20, "TagItemString16_Create"),
    (0x4FEB50, "TagItemString256_Create"),
])
def test_create(orig, ours):
    def setup(e):
        t = e.alloc(0x224)
        e.write(t, b"\x55" * 0x224)
        return t, 0x1234
    run(init, orig, ours, setup)


def test_set_param_type():
    def setup(e):
        t = e.alloc(0x224)
        e.write(t, b"\x55" * 0x224)
        return t, 0xCAFE
    compare(plain, 0x4FEAC0, "TagItem_SetParamType", setup=setup, check_eax=False)


@pytest.mark.parametrize("orig,ours,glob", [
    (0x4FEAE0, "TagItemInt32_GetParams", 0xA7A4F8),
    (0x4FEB10, "TagItemReal32_GetParams", 0xA7A4F0),
    (0x4FEB40, "TagItemString16_GetParams", 0xA7A4E4),
    (0x4FEB70, "TagItemString256_GetParams", 0xA7A500),
])
def test_get_params(orig, ours, glob):
    def setup(e):
        u32(e, glob, 0x12345678)
        return 0, e.alloc(4)
    compare(plain, orig, ours, setup=setup, check_eax=False)
