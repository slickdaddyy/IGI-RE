import struct

import pytest

from emu import compare
from test_core_error import CoreEmu, cstr, escape, recorder

emu = CoreEmu()

MODES = 0xA950A0


def init(e, modes=0, methods=0):
    e.call(0x4B0DC0)
    for i in range(modes):
        e.call(0x4B0E00, 0x5000 + i)
    for i in range(methods):
        e.call(0x4B0E70, 0x6000 + i)


def test_init():
    def setup(e):
        e.write(0xA94A60, b"\x55" * 0x1700)
        e.write(0x942304, b"\x55" * 4)
    compare(emu, 0x4B0DC0, "RenderMode_Init", setup=setup, check_eax=False)


@pytest.mark.parametrize("n", [0, 3, 7])
def test_create(n):
    def setup(e):
        init(e, n)
        e.call(0x4B0E50, 1)
        return (cstr(e, b"D3D"),)
    compare(emu, 0x4B0E00, "RenderMode_Create", setup=setup)


def test_create_full():
    def setup(e):
        init(e, 8)
        escape(e, 1)
        return (cstr(e, b"D3D"),)
    compare(emu, 0x4B0E00, "RenderMode_Create", setup=setup, check_eax=False)


@pytest.mark.parametrize("n", [0, 5, 127])
def test_create_method(n):
    def setup(e):
        init(e, 0, n)
        return (cstr(e, b"DisplayOpen"),)
    compare(emu, 0x4B0E70, "RenderMode_CreateMethod", setup=setup)


def test_create_method_full():
    def setup(e):
        init(e, 0, 128)
        escape(e, 1)
        return (cstr(e, b"DisplayOpen"),)
    compare(emu, 0x4B0E70, "RenderMode_CreateMethod", setup=setup, check_eax=False)


def test_delete():
    compare(emu, 0x4B0E50, "RenderMode_Delete", setup=lambda e: init(e, 3), args=(1,), check_eax=False)
    compare(emu, 0x4B0EB0, "RenderMode_DeleteMethod", setup=lambda e: init(e, 0, 9), args=(7,), check_eax=False)


def test_set_method():
    compare(emu, 0x4B0EC0, "RenderMode_SetMethod", setup=lambda e: init(e, 3, 3), args=(2, 100, 0x401234), check_eax=False)


def test_for_each():
    def setup(e):
        init(e, 5)
        e.call(0x4B0E50, 2)
        log = e.alloc(4 + 64)
        # appends index and name of each mode plus the user pointer to log
        code = e.alloc(64)
        P = lambda v: struct.pack("<I", v)
        e.write(code, bytes.fromhex("8b442404 8b0d") + P(log) + bytes.fromhex("8b10 89148d") + P(log + 4)
                + bytes.fromhex("8b5004 89148d") + P(log + 8) + bytes.fromhex("8b542408 89148d") + P(log + 12)
                + bytes.fromhex("83c103 890d") + P(log) + b"\xc3")
        e.write(log, struct.pack("<I", 0))
        return code, 0xCAFE
    compare(emu, 0x4B0EE0, "RenderMode_ForEach", setup=setup, check_eax=False)


@pytest.mark.parametrize("cur,mode", [(-1, 2), (2, 2), (1, 3)])
def test_select(cur, mode):
    def setup(e):
        init(e, 4)
        e.call(0x4B0EC0, mode, 5, 0x401000 + mode)
        if cur >= 0:
            e.call(0x4B0F30, cur)
        e.write(0xA94E80 + 4 * 6, struct.pack("<I", 0x77))
        return (mode,)
    compare(emu, 0x4B0F30, "RenderMode_Select", setup=setup, check_eax=False)


def test_deselect_get_current():
    def setup(e):
        init(e, 2)
        e.call(0x4B0F30, 1)
    compare(emu, 0x4B0F70, "RenderMode_Deselect", setup=setup, check_eax=False)
    compare(emu, 0x4B0F80, "RenderMode_GetCurrent", setup=setup)


@pytest.mark.parametrize("pushes", [0, 1, 2])
def test_push_pop(pushes):
    def setup(e):
        init(e, 4)
        e.call(0x4B0F30, 0)
        for i in range(pushes):
            e.call(0x4B0FD0, i + 1)
        return (3,)
    compare(emu, 0x4B0FD0, "RenderMode_Push", setup=setup, check_eax=False)
    compare(emu, 0x4B0F90, "RenderMode_Pop", setup=setup)


def test_pop_empty_marker():
    def setup(e):
        init(e, 2)
        e.call(0x4B0FD0, 1)
        e.write(0x942308 + 4 * 7, struct.pack("<i", -1))
    compare(emu, 0x4B0F90, "RenderMode_Pop", setup=setup)


def test_call():
    def setup(e):
        init(e, 2, 2)
        code = e.alloc(16)
        e.write(code, bytes.fromhex("8b442404" "40" "c3"))
        e.call(0x4B0EC0, 1, 1, code)
        return 1, 1, 41
    compare(emu, 0x4B1000, "RenderMode_Call", setup=setup)
