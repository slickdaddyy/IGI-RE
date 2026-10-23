"""Helpers shared by the test_editcam tests (0x505710 - 0x508F90, no tests in here)."""
import struct

import pefile

from emu import GAME_EXE

from test_menu_util import LOG, RET, add_child, handler, logfake, pyfake, set_handler, task, u16, u32  # noqa: F401
from test_mission_util import make_emu, run, with_cw  # noqa: F401
from test_vehicle_util import d64, floatfake

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]


def u8(e, p, *v):
    e.write(p, bytes(v))


def drop_points(e):
    """Coll_DropPoints: logs the point; the hit is the highest of RET['layers'] below it,
    moved by a bit of x, or nothing when RET['layers'] is empty."""
    world, hit, pts, count, dz, ev, unk, flags = (e.arg(i) for i in range(8))
    if LOG:
        LOG[-1].append(("drop", world, e.read(pts, 24 * count), count, e.read(dz, 8), ev, unk, flags))
    x, y, z = struct.unpack("<3d", e.read(pts, 24))
    below = [v + int(abs(x)) % 997 for v in RET.get("layers", ()) if v + int(abs(x)) % 997 < z]
    d64(e, hit + 8, x + 1.0, y - 1.0, max(below) if below else 0.0)
    u32(e, hit + 0x58, 1 if below else 0)
    e.ret(0)


def rand(e, a):
    """Random_Float: cycles through RET['rand']."""
    seq = RET.get("rand", (0.25,))
    i = RET.get("rand_i", 0)
    RET["rand_i"] = i + 1
    return seq[i % len(seq)]


def make():
    """make_emu() of the mission tests plus the outside callees of this range."""
    e = make_emu()
    # make_emu fakes Container_GetType, which is in this range
    pe = pefile.PE(GAME_EXE, fast_load=True)
    e.write(0x505D40, pe.get_data(0x105D40, 8))
    e.uc.ctl_remove_cache(0x505D40, 0x505D48)
    for addr, n in ((0x4B4720, 1), (0x4B47E0, 2), (0x4C7560, 2), (0x4F1CD0, 1),
                    (0x4F1D80, 1), (0x4F1DB0, 2), (0x4B0D40, 0), (0x4B0D50, 0)):
        logfake(e, addr, n)
    logfake(e, 0x4C6E70, 5, derefs={2: 24, 3: 24, 4: 24})  # DynCube_MoveBox
    logfake(e, 0x4C7350, 6, derefs={2: 24, 3: 24, 4: 24})  # DynCube_AddBox
    for addr in (0x4D14D0, 0x4E5360, 0x4F0EF0, 0x4F0FA0, 0x4F0F20, 0x4F0F40, 0x4F1E20, 0x4CEBA0):
        logfake(e, addr, 0, ret=0x200 + (addr & 0xFF))
    logfake(e, 0x4B0DB0, 0, effect=lambda e, a: RET.get("mode", 1))
    logfake(e, 0x4AF810, 1, strings=(0,))
    pyfake(e, 0x4CABF0, drop_points)
    pyfake(e, 0x4CB9F0, drop_points)  # Coll_DropPointsMat
    floatfake(e, 0x4B4770, 1, effect=rand)
    e.baseline = e.snapshot()
    return e
