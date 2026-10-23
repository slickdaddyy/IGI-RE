"""TerrainLightMap_Changed (0x4F8780): snapping, ground samples and children."""
import random

import pytest

from emu import ErrorShown
from test_graphnode_util import FISTP_CWS, LOG, RET, d64, logfake, new_emu, pyfake, run, set_handler, trap, u16, u32, \
    with_cw

TYPE = 0x55
TASKS = 0xBC00A0
LIGHTMAPS = 0xB81B20
RANDOM = 0xA76CD0

e = new_emu()
logfake(e, 0x48F250, 0, effect=lambda e, a: RET.get("used", 1))
logfake(e, 0x4015F0, 1)  # Task_Delete
logfake(e, 0x4018F0, 0, ret=1)  # Task_GetRootType


def new_task(e, a):
    t = e.alloc(0x110)
    u16(e, t + 0x1C, a[1] & 0xFFFF)
    return t


logfake(e, 0x4012A0, 3, masks={1: 0xFFFF}, effect=new_task)  # Task_New
logfake(e, 0x4B5B60, 1, strings=(0,), effect=lambda e, a: RET.get("exists", 0))  # Resource_Exists
logfake(e, 0x4B5F00, 2, strings=(0,), ret=0xDA7A)  # Resource_Load
logfake(e, 0x4B6380, 1, strings=(0,))  # Resource_Unload
logfake(e, 0x4B2670, 0, effect=lambda e, a: RET.get("tmus", 1))  # Texture_GetTMUs
logfake(e, 0x4B2610, 1)  # Texture_SetActiveTMU
logfake(e, 0x4B23B0, 9)  # Texture_Create
logfake(e, 0x4C7350, 6, derefs={2: 24, 3: 24, 4: 24})
logfake(e, 0x4C6E70, 5, derefs={2: 24, 3: 24, 4: 24})


def drop(e):
    """Coll_DropPointsMat: the next (found, z) from RET["hits"] goes into the result."""
    a = [e.arg(i) for i in range(8)]
    LOG[-1].append(("drop", a[0], e.read(a[2], 24), a[3], e.read(a[4], 8), a[5], a[6], a[7]))
    hits = RET.setdefault("hits", [])
    found, z = hits.pop(0) if hits else (0, 0.0)
    u32(e, a[1] + 0x58, found)
    d64(e, a[1] + 0x18, z)
    e.ret(0)


pyfake(e, 0x4CB9F0, drop)


def changed(e):
    t = e.arg(0)
    LOG[-1].append(("changed", t, e.arg(1), e.read(t + 0x70, 24), e.read(t + 0xB8, 8), e.read(t + 0xC8, 8),
                    e.read(t + 0xD0, 16), e.u32(t + 0x108)))
    e.ret(0)


e.write(0x54E0E5, bytes([0x31]))
set_handler(e, 0x31, TYPE, trap(e, "changed", changed))
e.baseline = e.snapshot()


def world(e, count, lmp, offset):
    w = e.alloc(0xF00)
    u32(e, w + 0xDE0, *offset)
    u32(e, w + 0xDEC, count)
    u32(e, w + 0xDF4, 0x9A1E77E)
    u32(e, w + 0xDF8, 0x1A9 if lmp else 0)
    return w


def lightmap(e, w, rng, shift, size, child=0, placed=0, lm=-1, place=(0, 0, 0), pos=(0, 0, 0)):
    t = e.alloc(0x110)
    e.write(t, bytes(rng.randrange(256) for _ in range(0x110)))
    u16(e, t + 0x1C, TYPE)
    d64(e, t + 0x20, *pos)
    u32(e, t + 0x68, w)
    d64(e, t + 0x70, *place)
    u32(e, t + 0xB8, shift)
    e.write(t + 0xC7, bytes([placed]))
    e.write(t + 0xCA, bytes([child]))
    u32(e, t + 0xD4, size)
    u32(e, t + 0xD8, 3)
    u32(e, t + 0xDC, lm)
    u32(e, t + 0x108, 0x7E47)
    u32(e, t + 0x10C, 0x7A5C if placed else 0)
    return t


def setup_case(e, seed, child=0, placed=0, lmp=0, lm=-1, size=0x40, shift=10, mode=0, count=5, gaps=(2,),
               place=None, pos=None, hits=None, offset=(0x10000000, -0x2000000, 0x123), exists=0, tmus=1):
    rng = random.Random(seed)
    RET.clear()
    RET["exists"] = exists
    RET["tmus"] = tmus
    e.write(0x9422FD, bytes([mode]))
    e.call(0x4B47E0, RANDOM, 0x19111911 + seed)
    u16(e, 0xA76CDC, TYPE)
    e.write(0xB97C20, b"LOCAL:missions/location0/level1/terrain/terrain.qvm\0")
    w = world(e, count, lmp, offset)
    for i in range(count):
        u32(e, TASKS + 4 * i, 0 if i in gaps else 0x1000 + i)
    u32(e, TASKS + 4 * count, 0x5555)
    u32(e, LIGHTMAPS + 0x2C * 4 + 4, 1)
    u32(e, LIGHTMAPS + 0x2C * 6 + 4, 0)
    cell = 0x40000000 >> shift
    place = place or (rng.uniform(-1e7, 1e7), rng.uniform(-1e7, 1e7), rng.uniform(-1e5, 1e5))
    pos = pos or (place[0] + 0.25, place[1] - 0.75, place[2] + 3.5)
    if hits is None:
        hits = [(0, 0.0), (1, pos[2] + 77.5)] + [(rng.randrange(2), pos[2] + rng.uniform(-3.5, 3.5) * cell)
                                                 for _ in range(32)]
    RET["hits"] = list(hits)
    return (lightmap(e, w, rng, shift, size, child, placed, lm, place, pos),)


def check_cws(setup, cws=FISTP_CWS):
    for cw in cws:
        run(e, 0x4F8780, "TerrainLightMap_Changed", setup, cw=cw)


def test_unused():
    def setup(e):
        args = setup_case(e, 1)
        RET["used"] = 0
        return args
    check_cws(setup, [0x37F])


@pytest.mark.parametrize("seed", range(6))
def test_child(seed):
    check_cws(lambda e: setup_case(e, seed, child=1, shift=seed * 5))


@pytest.mark.parametrize("seed", range(8))
def test_random(seed):
    check_cws(lambda e: setup_case(e, seed, lmp=seed & 1, lm=(-1, 4, 6, 9)[seed % 4], mode=seed >> 2 & 1,
                                   shift=(10, 12, 8, 14)[seed % 4], size=(0x40, 0x100, 8, 0x10)[seed % 4]))


@pytest.mark.parametrize("exists,tmus", [(0, 1), (1, 1), (1, 2)])
def test_load_texture(exists, tmus):
    check_cws(lambda e: setup_case(e, 3, lmp=0, lm=12, exists=exists, tmus=tmus), [0x37F])


@pytest.mark.parametrize("placed", [0, 1])
def test_placed(placed):
    check_cws(lambda e: setup_case(e, 4, placed=placed, gaps=()), [0x37F])


def test_no_ground():
    check_cws(lambda e: setup_case(e, 5, hits=[(0, 1.0)] * 40))


# cell edges and halves, where the rounding mode decides the cell
@pytest.mark.parametrize("shift", [10, 29, 30])
@pytest.mark.parametrize("frac", [-0.5, 0.5, 1.5, -1.25])
def test_snap_rounding(shift, frac):
    half = (0x40000000 >> shift) * 2
    edge = 0x10000000 - (0x10000000 % half if half else 0)

    def setup(e):
        p = (edge - 0x10000000 + frac, edge + 0x2000000 + frac, frac)
        return setup_case(e, 6, child=1, shift=shift, place=p, pos=p, offset=(0x10000000, -0x2000000, 0))
    check_cws(setup)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("size", [0x80, 0x20, 0x41, 0])
def test_sizes(size, cw):
    def setup(e):
        return setup_case(e, 7, size=size)
    if size in (0x80, 0x20):
        run(e, 0x4F8780, "TerrainLightMap_Changed", setup, cw=cw)
    else:
        errors(setup, cw)


def test_too_many():
    def setup(e):
        return setup_case(e, 8, count=0x7FF, gaps=())
    errors(setup, 0x37F)


def errors(setup, cw):
    out = []
    targets = [with_cw(e, t, cw) for t in (0x4F8780, "TerrainLightMap_Changed")]
    for target in targets:
        e.reset()
        LOG.append([])
        args = setup(e)
        before = e.snapshot()
        with pytest.raises(ErrorShown) as x:
            e.call(target, *args)
        out.append((str(x.value), LOG.pop(), e.changes(before)))
    assert out[0] == out[1]
