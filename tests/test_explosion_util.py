"""Helpers shared by the test_explosion tests."""
import math
import random
import struct

from emu import Emu
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, task, trap, u16, u32)
from test_vehicle_util import d64, floatfake, vfake  # noqa: F401

GAME, WORLD, ROOT = 0x57BABC, 0x9900, 0x7200
RANDOM = []
STATE = {}


def make_emu():
    """An Emu with a game object (world, random generator), the task root and a
    Random_Float that returns the values queued in RANDOM."""
    e = Emu()
    g = e.alloc(0x100)
    u32(e, GAME, g)
    u32(e, g + 0x20, WORLD)
    u32(e, 0xA758AC, ROOT)
    floatfake(e, 0x4B4770, 1, effect=lambda e, a: RANDOM.pop(0) if RANDOM else 0.375)
    e.baseline = e.snapshot()
    return e


def with_cw(e, target, cw):
    """An entry point that loads x87 control word cw and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    tramps = e.__dict__.setdefault("cw_tramps", {})
    if (addr, cw) not in tramps:
        p = e.fake_top
        e.fake_top += 32
        e.write(p, struct.pack("<H", cw))
        e.write(p + 4, b"\xd9\x2d" + struct.pack("<I", p) + b"\xe9" + struct.pack("<i", addr - (p + 15)))
        tramps[(addr, cw)] = p + 4
    return tramps[(addr, cw)]


def run(e, orig, ours, setup, cw=None, **kw):
    """check() of orig and ours, normalized, optionally under control word cw."""
    RET.clear()
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def randoms(rng, n=64):
    RANDOM[:] = [rng.random() for _ in range(n)]


def rot(rng):
    """A random rotation matrix (row major, 9 floats)."""
    a, b, c = rng.uniform(-3, 3), rng.uniform(-3, 3), rng.uniform(-3, 3)
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    return [cb * cc, -cb * sc, sb, sa * sb * cc + ca * sc, -sa * sb * sc + ca * cc, -sa * cb,
            -ca * sb * cc + sa * sc, ca * sb * sc + sa * cc, ca * cb]


def unit(rng):
    v = [rng.uniform(-1, 1) for _ in range(3)]
    n = math.sqrt(sum(x * x for x in v)) or 1
    return [x / n for x in v]


def pool(e, addr, size, cap):
    stride = (size + 4 + 3) & ~3
    data = e.alloc(stride * cap + 4 * cap)
    slots = data + stride * cap
    u32(e, slots, *range(cap))
    u32(e, addr, data, slots, 0, cap, stride, size)


def pool_take(e, addr, value=None):
    data, slots, used, cap, stride, size = struct.unpack("<6I", e.read(addr, 24))
    item = data + e.u32(slots + 4 * used) * stride
    u32(e, item + size, used)
    u32(e, addr + 8, used + 1)
    if value is not None:
        u32(e, item, value)
    return item


def f32s(e, p, n):
    return struct.unpack("<%df" % n, e.read(p, 4 * n))


def seeded(seed):
    return random.Random(seed)
