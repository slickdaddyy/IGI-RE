import struct

import pytest

from emu import Emu, compare, RETURN_MAGIC, STACK_BASE, STACK_SIZE



class CoreEmu(Emu):
    """Runs Error_Show like any other code instead of stopping at it."""

    def _error_show(self, uc, addr, size, user):
        pass


emu = CoreEmu()


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def recorder(e):
    """Code that stores its first argument and returns. Returns (code, slot)."""
    slot = e.alloc(4)
    code = e.alloc(16)
    e.write(code, b"\x8b\x44\x24\x04\xa3" + struct.pack("<I", slot) + b"\xc3")
    return code, slot


def escape(e, nargs):
    """Exit handler that returns straight to the test, for error paths that hang."""
    code = e.alloc(16)
    esp = STACK_BASE + STACK_SIZE - 0x1000 - 4 * nargs
    e.write(code, b"\xbc" + struct.pack("<I", esp) + b"\xb8" + struct.pack("<I", RETURN_MAGIC) + b"\xff\xe0")
    e.write(0x936268, struct.pack("<I", recorder(e)[0]))
    e.write(0x93626C, struct.pack("<I", code))


@pytest.mark.parametrize("handler,exit_handler", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_error_show(handler, exit_handler):
    def setup(e):
        if handler:
            e.write(0x936268, struct.pack("<I", recorder(e)[0]))
        if exit_handler:
            e.write(0x93626C, struct.pack("<I", recorder(e)[0]))
        return cstr(e, b"Bad %s at %d (%x)"), cstr(e, b"thing"), 42, 0xBEEF
    compare(emu, 0x4AF7B0, "Error_Show", setup=setup, check_eax=False)


@pytest.mark.parametrize("handler", [0, 1])
def test_warning_show(handler):
    def setup(e):
        e.write(0x93626C, struct.pack("<I", recorder(e)[0]))
        if handler:
            e.write(0x936274, struct.pack("<I", recorder(e)[0]))
        return cstr(e, b"%d block%s left"), 3, cstr(e, b"s")
    compare(emu, 0x4AF810, "Warning_Show", setup=setup, check_eax=False)


@pytest.mark.parametrize("orig,ours", [
    (0x4AF860, "Error_SetHandler"),
    (0x4AF870, "sub_4AF870"),
    (0x4AF880, "Warning_SetHandler"),
    (0x4AF890, "sub_4AF890"),
    (0x4AF8A0, "Error_SetExitHandler"),
    (0x4AF8B0, "sub_4AF8B0"),
])
def test_setters(orig, ours):
    compare(emu, orig, ours, args=(0x12345678,), check_eax=False)


def test_reset_handlers():
    def setup(e):
        for a in (0x93625C, 0x936268, 0x93626C, 0x936270, 0x936274, 0x93627C):
            e.write(a, struct.pack("<I", 0x11111111))
    compare(emu, 0x4AF8C0, "Error_ResetHandlers", setup=setup, check_eax=False)
