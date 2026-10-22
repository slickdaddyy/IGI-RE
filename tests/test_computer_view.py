"""HumanView setters at 0x465910 - 0x4659E0."""
import struct

import pytest

from emu import Emu, compare
from test_menu_util import f32, u32

emu = Emu()
CW = {}


def with_cw(e, target, cw):
    """Entry that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    if (addr, cw) not in CW:
        p = e.fake_top
        e.fake_top += 32
        e.write(p + 16, struct.pack("<H", cw))
        e.write(p, b"\xd9\x2d" + struct.pack("<I", p + 16) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        e.baseline = e.snapshot()
        CW[(addr, cw)] = p
    return CW[(addr, cw)]


def view(e):
    v = e.alloc(0x200)
    e.write(v, bytes((i * 7 + 3) & 0xFF for i in range(0x200)))
    return v


def test_clear_cam_override():
    compare(emu, 0x465910, "HumanView_ClearCamOverride", setup=lambda e: (view(e),), check_eax=False)


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("flag", [0, 1, 0x80])
def test_set_target(has, flag):
    def setup(e):
        v = view(e)
        src = 0
        if has:
            src = e.alloc(24)
            u32(e, src, *range(0x100, 0x106))
        return v, src, flag | 0x1200
    compare(emu, 0x465920, "HumanView_SetTarget", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("frames", [1, 3, 7, 15, 45, -9, 0])
def test_shake(cw, frames):
    def setup(e):
        return view(e), 0x4100CCCD, 0x3F400000, frames
    compare(emu, with_cw(emu, 0x465970, cw), with_cw(emu, "HumanView_Shake", cw), setup=setup, check_eax=False)


@pytest.mark.parametrize("v", [0.0, -1024.0, 3.25, -0.001])
def test_set_dip(v):
    def setup(e):
        return view(e), struct.unpack("<I", struct.pack("<f", v))[0]
    compare(emu, 0x4659B0, "HumanView_SetDip", setup=setup, check_eax=False)


def test_set_zoomed():
    compare(emu, 0x4659D0, "HumanView_SetZoomed", setup=lambda e: (view(e),), check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_reset(cw):
    def setup(e):
        v = view(e)
        f32(e, v + 0x1E4, 9.0, 9.0)
        return (v,)
    compare(emu, with_cw(emu, 0x4659E0, cw), with_cw(emu, "HumanTaskViewReset", cw), setup=setup, check_eax=False)
