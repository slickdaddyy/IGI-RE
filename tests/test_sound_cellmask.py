import random
import struct

import pytest

from emu import Emu, compare
from test_menu_util import LOG, check, logfake, task_system, u16, u32, f32

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]
POOL = 0xB019E0

e = Emu()
# DynCube_SendEventBox: log slot, arg {mask, size, center, range}, min, max, flags, unk
logfake(e, 0x4C7770, 7, masks={1: 0xFF}, derefs={2: 16, 3: 24, 4: 24})
rng = random.Random(5)
_tramps = {}


def with_cw(target, cw):
    """Entry point that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    if (addr, cw) not in _tramps:
        p = e.fake_top
        e.fake_top += 32
        e.write(p + 16, struct.pack("<H", cw))
        e.write(p, b"\xd9\x2d" + struct.pack("<I", p + 16) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        _tramps[(addr, cw)] = p
        e.baseline = e.snapshot()
    return _tramps[(addr, cw)]


def run(orig, ours, setup=None, cw=None, **kw):
    if cw is not None:
        orig, ours = with_cw(orig, cw), with_cw(ours, cw)
    kw.setdefault("check_eax", False)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def base(e):
    task_system(e)
    e.call(0x4E55C0)  # CellMask_Init


def mask(e, size, bits=()):
    m = e.alloc(12 + max(size * size // 8, 1))
    u32(e, m, 0, size.bit_length() - 1, size)
    for x, y in bits:
        e.call(0x4E5550, m, x, y)
    return m


def test_dyncubeobj():
    def setup(e):
        task_system(e)
        u16(e, 0x548638, e.call(0x4018F0)[0] & 0xFFFF)
    run(0x4E5370, "DynCubeObj_Init", setup)

    def setup2(e):
        setup(e)
        e.call(0x4E5370)
    run(0x4E53A0, "DynCubeObj_Shutdown", setup2)
    run(0x4E5360, "DynCubeObj_GetMaskVisibleEvent", lambda e: e.write(0x54875E, b"\x33"), check_eax=True,
        eax_mask=0xFF)
    run(0x4E55B0, "CellMask_GetDrawEvent", lambda e: e.write(0xA5E408, b"\x34"), check_eax=True, eax_mask=0xFF)


def test_init_shutdown():
    run(0x4E55C0, "CellMask_Init", task_system)
    run(0x4E55F0, "CellMask_Shutdown", base)
    run(0x4E5C90, "CellMask_Enable")
    run(0x4E5CA0, "CellMask_Disable", lambda e: u32(e, 0x54876C, 1))


@pytest.mark.parametrize("size", [1, 3, 8, 64, 256])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_create(size, cw):
    def setup(e):
        base(e)
        pos = e.alloc(24)
        e.write(pos, struct.pack("<3d", 1234.567891, -98765.4321, 5.5))
        return size, 0x5000, 7, pos, struct.unpack("<I", struct.pack("<f", 333.3))[0]
    run(0x4E53C0, "CellMask_Create", setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("which", [0, 1, 2])
def test_free(which):
    def setup(e):
        base(e)
        pos = e.alloc(24)
        ms = [e.call(0x4E53C0, 8, 0, 0, pos, 0)[0] for _ in range(3)]
        return (ms[which],)
    run(0x4E54E0, "CellMask_Free", setup)


def test_set_test():
    for _ in range(40):
        size = rng.choice([8, 16, 64])
        x, y = rng.randrange(size), rng.randrange(size)
        bits = [(rng.randrange(size), rng.randrange(size)) for _ in range(20)] + [(x, y)] * rng.randrange(2)
        run(0x4E5550, "CellMask_Set", lambda e: (mask(e, size, bits[:-1]), x, y))
        run(0x4E5580, "CellMask_Test", lambda e: (mask(e, size, bits), x, y), check_eax=True)


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


TRIS = [(1.2, 1.7, 10.4, 3.3, 5.5, 12.9), (5, 5, 5, 5, 5, 5), (-3.5, -2, 40, 7.5, 12.25, 70.75),
        (0.5, 0.5, 0.5, 9.5, 9.5, 0.5), (3, 2, 3, 2, 8, 2), (60.7, 1.1, -4.2, 30.3, 20.5, 62.6)]


@pytest.mark.parametrize("cw", CWS)
def test_fill_triangle(cw):
    tris = TRIS + [tuple(rng.uniform(-8, 72) for _ in range(6)) for _ in range(30)]
    for t in tris:
        run(0x4E5610, "CellMask_FillTriangle", lambda e: (mask(e, 64),) + tuple(fbits(v) for v in t), cw=cw)


@pytest.mark.parametrize("cw", CWS)
def test_test_rect(cw):
    for _ in range(60):
        bits = [(rng.randrange(32), rng.randrange(32)) for _ in range(rng.choice([0, 1, 3]))]
        r = [rng.uniform(-10, 40), rng.uniform(-10, 40), rng.uniform(-5, 20), rng.uniform(-5, 20)]
        run(0x4E5BA0, "CellMask_TestRect", lambda e: (mask(e, 32, bits),) + tuple(fbits(v) for v in r), cw=cw,
            check_eax=True)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_view_scale(cw):
    for _ in range(5):
        c = [(rng.uniform(-50, 50), rng.uniform(-50, 50), 1.0) for _ in range(8)]

        def setup(e):
            f32(e, 0xBCAB20, 0.7371, 0.5529)
            for i in range(8):
                e.write(0xB04280 + 24 * i, struct.pack("<3d", *c[i]))
        run(0x4E5CB0, "QCube_UpdateViewScale", setup, cw=cw)


def test_tables():
    run(0x4E5D90, "QCube_InitTables", task_system)
    run(0x4E5EA0, "QCube_FreeTables", lambda e: (task_system(e), e.call(0x4E5D90)) and None)
