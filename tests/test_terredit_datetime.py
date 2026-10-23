"""DateTime (0x52D440 - 0x52D630)."""
import struct

import pytest

from emu import Emu, compare

emu = Emu()
TIME = [2001, 12, 3, 31, 23, 59, 58, 999]  # year, month, day of week, day, hour, minute, second, ms


def get_local_time(e):
    e.write(e.arg(0), struct.pack("<8H", *TIME))
    e.ret(0, 4)


emu.stub("GetLocalTime", get_local_time)


def dt(e, v):
    p = e.alloc(28)
    e.write(p, struct.pack("<7i", *v))
    return p


A = (2000, 1, 2, 3, 4, 5, 6)


def test_get():
    def setup(e):
        p = e.alloc(28)
        e.write(p, b"\xCC" * 28)
        return (p,)
    compare(emu, 0x52D440, "DateTime_Get", setup=setup, check_eax=False)


@pytest.mark.parametrize("fn,name", [(0x52D5B0, "DateTime_FormatDate"), (0x52D5E0, "DateTime_FormatTime"),
                                     (0x52D610, "DateTime_FormatMs")])
@pytest.mark.parametrize("v", [A, (12345, 100, -1, 0, 99, 1000, 123456)])
def test_format_parts(name, fn, v):
    def setup(e):
        buf = e.alloc(64)
        e.write(buf, b"\xEE" * 64)
        return dt(e, v), buf, 64
    compare(emu, fn, name, setup=setup)


@pytest.mark.parametrize("flags", [(a, b, c) for a in (0, 1) for b in (0, 1) for c in (0, 1)])
def test_format(flags):
    def setup(e):
        buf = e.alloc(64)
        e.write(buf, b"\xEE" * 64)
        return (dt(e, A), buf, 64) + flags
    compare(emu, 0x52D4B0, "DateTime_Format", setup=setup)


@pytest.mark.parametrize("b", [A, (2000, 1, 2, 3, 4, 5, 7), (1999, 12, 31, 23, 59, 59, 999), (2000, 1, 2, 3, 4, 5, 5),
                               (2000, 10, 2, 3, 4, 5, 6)])
def test_compare(b):
    def setup(e):
        return dt(e, A), dt(e, b)
    compare(emu, 0x52D630, "DateTime_Compare", setup=setup)
