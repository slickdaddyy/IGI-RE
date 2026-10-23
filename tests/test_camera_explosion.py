"""Explosion tail: FireBall delete, Explosion create/run/close/new/spawn."""
import struct

import pytest

from test_camera_util import RET, cstr, d64, f32, handler, make_emu, run, set_handler, task, u16, u32

emu = make_emu()
BURN_TYPE = 0x44
BURN = handler(emu, "burn", 2, derefs={1: 16})
CWS = [0x37F, 0x7F]


def test_fireball_delete():
    run(emu, 0x480F00, "FireBall_DeleteHandler", lambda e: (task(e, 0x118, 3),))


def test_create():
    def setup(e):
        t = task(e, 0x88, 3)
        u32(e, t + 0x40, 7)
        u32(e, t + 0x84, 9)
        return (t,)
    run(emu, 0x480F30, "Explosion_CreateHandler", setup)


@pytest.mark.parametrize("timer", [0, 1, 2])
@pytest.mark.parametrize("count", [0, 1, 3])
def test_run(timer, count):
    def setup(e):
        RET[0x4B47C0] = 17
        t = task(e, 0x88, 3)
        u32(e, t + 0x38, count, 25, timer)
        return (t,)
    run(emu, 0x480F40, "Explosion_RunHandler", setup)


def test_close():
    def setup(e):
        u16(e, 0x5C1154, 0x51)
        u16(e, 0x5C1156, 0x52)
        u16(e, 0x5C115C, 0x53)
        u16(e, 0x5C1168, 0x54)
    run(emu, 0x480FA0, "Explosion_Close", setup)


def as_int(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def new_args(e, dir_, burn, noise, rf):
    RET["rf"] = list(rf)
    u32(e, 0x5C1164, 0x6400)
    pos = e.alloc(24)
    d64(e, pos, 1000.5, -2000.25, 30.75)
    ref = 0
    if burn:
        t = task(e, 0x40, BURN_TYPE)
        ref = e.alloc(4)
        u32(e, ref, t if burn != "empty" else 0)
        set_handler(e, 0x2C, BURN_TYPE, BURN if burn != "nohandler" else 0)
    d = e.alloc(12)
    f32(e, d, *dir_)
    return [pos, 3, 12, as_int(0.75), as_int(4096.5), as_int(1536.0), as_int(2048.0), 5, 2, ref, cstr(e, b"1009_01_1"),
            cstr(e, b"explo_03_l"), noise, 2, as_int(102400.0), d, as_int(1.5707964), as_int(272.0), as_int(1360.0), 7]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("dir_", [(0.0, 0.0, 0.0), (0.3, -0.4, 0.7), (0.0, 0.0, -2.5), (1e-3, 3.0, 1e5)])
@pytest.mark.parametrize("burn", [None, "empty", "nohandler", "yes"])
@pytest.mark.parametrize("noise", [-1, 0x15])
def test_new(cw, dir_, burn, noise):
    def setup(e):
        return new_args(e, dir_, burn, noise, [0.125, 0.9, 0.333])
    run(emu, 0x4810A0, "Explosion_New", setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("rf", [(0.5, 0.5, 0.3), (0.1, 0.77, 0.9), (0.999, 0.01, 0.4)])
def test_spawn(cw, rf):
    def setup(e):
        a = new_args(e, (0, 0, 0), "yes", 0x15, list(rf) + [0.2, 0.4, 0.6])
        return a[:15] + [a[19]]
    run(emu, 0x480FE0, "Explosion_Spawn", setup, cw=cw, check_eax=True)
