import random
import struct

import pytest
from unicorn import UC_HOOK_CODE

from emu import Emu, compare

emu = Emu()

MEM_INIT, TERRAIN_NEW, ADD, ADD_BOX = 0x4B0BA0, 0x4C5B20, 0x4C7140, 0x4C7350
TASK_TYPES, HANDLERS, HANDLER_CHANGES = 0xAF5EE0, 0xA96AE0, 0x536148
CELLMASK_ON = 0x54876C
OFFSET = 0xDE0

LOG = []


def pyfake(e, addr, fn):
    """Run fn(e, log) instead of the code at addr; fn returns eax."""
    def hook(uc, a, size, user):
        e.ret(fn(e, LOG[-1]) or 0)
    e.uc.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr)


def new_code(e):
    p = e.fake_top
    e.fake_top += 16
    e.write(p, b"\xc3")
    return p


def cmp(e, orig, ours, setup=None, **kw):
    LOG.clear()

    def s(em):
        LOG.append([])
        return setup(em) if setup else None
    r = compare(e, orig, ours, setup=s, **kw)
    assert LOG[0] == LOG[1], "calls differ:\n orig %s\n ours %s" % (LOG[0], LOG[1])
    return r


HANDLER = new_code(emu)
pyfake(emu, HANDLER, lambda e, log: log.append(("handler", e.arg(0), e.arg(1))))


def mask_create(e, log):
    log.append(("create", e.arg(0), e.arg(1), e.arg(2), e.read(e.arg(3), 24), e.arg(4)))
    return 0x7000 + len(log)


pyfake(emu, 0x4E53C0, mask_create)
pyfake(emu, 0x4E54E0, lambda e, log: log.append(("free", e.arg(0))))
pyfake(emu, 0x50FAE0, lambda e, log: log.append(("50fae0", e.arg(0), e.arg(1))))

# line of sight: event 8 handlers may lower visibility or block, Coll_Lines may hit
SIGHT = {"vis": None, "block": 0, "hit": 0}


def sight_event(e, log):
    q = e.arg(2)
    log.append(("4c8130", e.arg(0), e.arg(1), e.read(q, 9), e.read(q + 0xC, 9), e.read(q + 0x18, 4),
                e.arg(3), e.arg(4), e.arg(5), e.arg(6), e.arg(7)))
    if SIGHT["vis"] is not None:
        e.write(q + 0x18, struct.pack("<f", SIGHT["vis"]))
    if SIGHT["block"]:
        e.write(q + 0x14, b"\x01")


def sight_lines(e, log):
    log.append(("lines", e.arg(0), e.arg(2), e.arg(3), e.arg(4), e.arg(5) & 0xFF, e.arg(6), e.arg(7)))
    e.write(e.arg(1) + 0x58, struct.pack("<I", SIGHT["hit"]))


pyfake(emu, 0x4C8130, sight_event)
pyfake(emu, 0x4CC2A0, sight_lines)

# type: (task flags, handler slots)
TYPES = {
    1: (0x0001, [1, 8]),
    2: (0x0006, [4, 9, 30, 8]),
    3: (0x0010, [5, 8]),
    4: (0x0000, [11, 8]),
    5: (0x0102, [0, 2, 253]),
}


def u32(e, p, v):
    e.write(p, struct.pack("<I", v & 0xFFFFFFFF))


def f32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def vec(e, x, y, z):
    p = e.alloc(24)
    e.write(p, struct.pack("<3d", x, y, z))
    return p


def setup_types(e):
    for t, (flags, slots) in TYPES.items():
        e.write(TASK_TYPES + t * 0x30 + 0x28, struct.pack("<HHI", flags & 0xFFFF, 0, flags))
        for s in slots:
            u32(e, HANDLERS + (s * 0x180 + t) * 4, HANDLER)


def terrain(e, offset=(0x20000000, 0x20000000, 0x20000000)):
    e.call(MEM_INIT)
    setup_types(e)
    t = e.call(TERRAIN_NEW, 2000)[0]
    e.write(t + OFFSET, struct.pack("<3i", *offset))
    return t


def populate(e, seed, n=24, scale=60000.0, center=(0, 0, 0), radii=(0.0, 50.0, 900.0, 5000.0), cats=(1, 2, 4, 8)):
    rng = random.Random(seed)
    t = terrain(e)
    objs = []
    for i in range(n):
        o = e.alloc(0x80)
        e.write(o + 0x1C, struct.pack("<H", rng.choice(list(TYPES))))
        cat = rng.choice(cats)
        p = [c + rng.uniform(-scale, scale) for c in center]
        if rng.random() < 0.7:
            e.call(ADD, t, o, vec(e, *p), f32(rng.choice(radii)), cat)
        else:
            ext = rng.choice([10.0, 3000.0, 20000.0])
            e.call(ADD_BOX, t, o, vec(e, *p), vec(e, *[c - ext for c in p]), vec(e, *[c + ext for c in p]), cat)
        objs.append(o)
    return t, objs


def nodes(e, t, tree):
    out = []

    def walk(n):
        out.append(n)
        for i in range(8):
            c = e.u32(n + 0xC + 4 * i)
            if c:
                walk(c)
    walk(e.u32(t + 0x10 + 4 * tree))
    return out


def cell_mask(e, rng):
    m = e.alloc(12 + 8192)
    e.write(m, struct.pack("<3I", 0, 8, 256) + bytes(rng.randrange(256) for _ in range(8192)))
    return m


def invalidate(e, t):
    for tree in range(6):
        u32(e, t + 0xE8 + 4 * tree, 0)
    u32(e, HANDLER_CHANGES, e.u32(HANDLER_CHANGES) + 1)


@pytest.mark.parametrize("seed", range(48))
def test_send_event(seed):
    clustered = seed % 2

    def setup(e):
        r = random.Random(seed)
        center = (r.uniform(-1e6, 1e6), r.uniform(-1e6, 1e6), r.uniform(-1e5, 1e5)) if clustered else (0, 0, 0)
        t, objs = populate(e, seed, n=10 + seed * 3, scale=3000.0 if clustered else 60000.0, center=center,
                           radii=(0.0, 50.0, 900.0, 5000.0, 30000.0))
        o = r.choice(objs)
        ty = struct.unpack("<H", e.read(o + 0x1C, 2))[0]
        tree = e.u32(e.u32(o + 0x50) + 0x34) if r.random() < 0.8 else r.randrange(6)
        slot = r.choice(TYPES[ty][1]) if r.random() < 0.8 else r.choice([1, 8, 9, 7])
        mask = r.choice([0xFFFFFFFF] * 4 + [0x1, 0x6, 0x100, 0x20000000])
        if seed % 3 == 0:
            ns = nodes(e, t, tree)
            for n in r.sample(ns, min(3, len(ns))):
                u32(e, n + 0x3C, cell_mask(e, r))
        invalidate(e, t)
        if r.random() < 0.8:
            rad = struct.unpack("<f", e.read(o + 0x44, 4))[0] * 1.1
            p = [c + r.uniform(-rad, rad) for c in struct.unpack("<3d", e.read(o + 0x20, 24))]
        else:
            p = [c + r.uniform(-3000, 3000) for c in center]
        return t, slot | 0x12345600, e.alloc(4), vec(e, *p), mask, tree
    cmp(emu, 0x4CE090, "Coll_SendEvent", setup=setup, check_eax=False)


@pytest.mark.parametrize("seed", range(6))
def test_send_event_all(seed):
    def setup(e):
        r = random.Random(seed)
        t, objs = populate(e, seed, n=30)
        o = r.choice(objs)
        return t, r.choice([1, 8, 11]), 0x55, o + 0x20, 0xFFFFFFFF
    cmp(emu, 0x4CE390, "Coll_SendEventAll", setup=setup, check_eax=False)


def test_send_event_hits():
    """Make sure the cases above call handlers."""
    calls = 0
    for seed in range(48):
        LOG.clear()
        try:
            test_send_event(seed)
        except AssertionError:
            raise
        calls += sum(1 for c in LOG[0] if c[0] == "handler")
    assert calls > 10


@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("tree", range(6))
def test_update_slot_counts(seed, tree):
    def setup(e):
        t, _ = populate(e, seed, n=5 + seed * 6)
        if seed % 2:
            u32(e, HANDLER_CHANGES, e.u32(HANDLER_CHANGES) + 3)
        return t, tree
    cmp(emu, 0x4CE5C0, "DynCube_UpdateSlotCounts", setup=setup, check_eax=False)


@pytest.mark.parametrize("seed", range(4))
def test_count_slots(seed):
    def setup(e):
        r = random.Random(seed)
        t, _ = populate(e, seed, n=20)
        tree = r.randrange(6)
        return t, r.choice(nodes(e, t, tree))
    cmp(emu, 0x4CE530, "DynCube_CountSlots", setup=setup, check_eax=False)


@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("tree", range(6))
@pytest.mark.parametrize("valid", [0, 1])
def test_update_start(seed, tree, valid):
    def setup(e):
        r = random.Random(seed)
        center = (r.uniform(-5e5, 5e5), r.uniform(-5e5, 5e5), r.uniform(-5e5, 5e5))
        if seed % 4 == 3:
            center = (-3e8, 2e8, 4e8)
        t, _ = populate(e, seed, n=seed * 2, scale=200.0 * (seed + 1), center=center)
        u32(e, t + 0xE8 + 4 * tree, valid)
        return t, tree
    cmp(emu, 0x4CE620, "DynCube_UpdateStart", setup=setup, check_eax=False)


def test_update_start_descends():
    emu.reset()
    t, _ = populate(emu, 1, n=2, scale=400.0, center=(1000, 2000, 3000))
    tree = next(i for i in range(6) if emu.u32(emu.u32(t + 0x10 + 4 * i) + 0x2C) == 0
                and emu.read(emu.u32(t + 0x10 + 4 * i) + 0x30, 1) != b"\0")
    u32(emu, t + 0xE8 + 4 * tree, 0)
    emu.call(0x4CE620, t, tree)
    assert emu.u32(t + 0xD0 + 4 * tree) < 30


@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("unk", [0, 1])
@pytest.mark.parametrize("enabled", [0, 1])
def test_build_masks(seed, unk, enabled):
    def setup(e):
        r = random.Random(seed)
        t, _ = populate(e, seed, n=12, scale=4e6, radii=(1e5, 3e5, 6e5, 9e5), cats=(1, 1, 2))
        for n in r.sample(nodes(e, t, 0), 2):
            u32(e, n + 0x3C, 0x6000 + r.randrange(100))
        u32(e, CELLMASK_ON, enabled)
        return t, unk
    cmp(emu, 0x4CE830, "DynCube_BuildMasks", setup=setup, check_eax=False)


def test_build_masks_creates():
    LOG.clear()
    test_build_masks(2, 0, 1)
    assert any(c[0] == "create" for c in LOG[0])


@pytest.mark.parametrize("seed", range(4))
def test_node_build_masks(seed):
    def setup(e):
        r = random.Random(seed)
        t, _ = populate(e, seed, n=12, scale=4e6, radii=(1e5, 3e5, 6e5, 9e5))
        return t, r.choice(nodes(e, t, r.randrange(6))), 7, vec(e, 1.5, -2.25, 1e6)
    cmp(emu, 0x4CE910, "DynCubeNode_BuildMasks", setup=setup, check_eax=False)


@pytest.mark.parametrize("trees", [0, 1, 2, 4, 8, 0xF, 0x6])
@pytest.mark.parametrize("flags", [0x7, 0x80000000, 0x80000003])
@pytest.mark.parametrize("vis,block,hit", [(None, 0, 0), (None, 0, 1), (0.0, 0, 1), (0.25, 1, 0), (0.5, 0, 1),
                                           (float("nan"), 0, 1)])
@pytest.mark.parametrize("want_vis", [0, 1])
def test_line_of_sight(trees, flags, vis, block, hit, want_vis):
    SIGHT.update(vis=vis, block=block, hit=hit)

    def setup(e):
        t = e.alloc(0xF00)
        return t, vec(e, 1, 2, 3), vec(e, 4, 5, 6), 0x3377, 0x1234, flags, trees, e.alloc(4) if want_vis else 0
    cmp(emu, 0x4CE410, "Coll_LineOfSight", setup=setup)


def test_small_ones():
    def setup(e):
        t = e.alloc(0xF00)
        u32(e, t + 0xDC8, 5)
        return (t,)
    cmp(emu, 0x4CE3D0, "Terrain_InvalidateLighting", setup=setup, check_eax=False)
    cmp(emu, 0x4CE3E0, "Terrain_SetMaterial", setup=lambda e: (e.alloc(0xF00), 7, 0x1234), check_eax=False)

    def setup_get(e):
        t = e.alloc(0xF00)
        u32(e, t + 0xD3C + 4 * 9, 0x4567)
        return t, 9
    cmp(emu, 0x4CE400, "Terrain_GetMaterial", setup=setup_get)
    for addr, name, var in ((0x4CEA10, "World_GetEvent0", 0x548176), (0x4CEA20, "World_GetEvent1", 0x548177)):
        cmp(emu, addr, name, setup=lambda e: e.write(var, b"\x9A"), eax_mask=0xFF)


@pytest.mark.parametrize("done", [0, 1])
def test_sub_4cea30(done):
    def setup(e):
        t = e.alloc(0xF00)
        u32(e, t + 0xDC8, done)
        return (t,)
    cmp(emu, 0x4CEA30, "Terrain_UpdateLighting", setup=setup, check_eax=False)
