import random
import struct

import pytest

from emu import Emu, compare

MEM_INIT, TERRAIN_NEW, ADD, ADD_BOX = 0x4B0BA0, 0x4C5B20, 0x4C7140, 0x4C7350
BOX, LINES = 0x4C7770, 0x4C8130
TASK_TYPES, HANDLERS, SLOT_EVENT = 0xAF5EE0, 0xA96AE0, 0xA969E0

emu = Emu()


def new_handler(e):
    p = e.fake_top
    e.fake_top += 16
    e.write(p, b"\xc3")
    e.fake(p, 0, 2)
    return p


SLOTS = [3, 8, 40]
# type: (event mask, flags, slots with a handler)
TYPES = {
    1: (0x0001, 0x1, [3, 8]),
    2: (0x0006, 0x3, [8, 40]),
    3: (0x0010, 0x2, [3]),
    4: (0x0101, 0x4, [3, 8, 40]),
    5: (0x0002, 0x1, [40]),
}
HANDLER = {(t, s): new_handler(emu) for t, (_, _, slots) in TYPES.items() for s in slots}
emu.baseline = emu.snapshot()

# cell mask tests faked, so their arguments end up in the call log
CELL_TEST, CELL_TEST_RECT = 0x4E5580, 0x4E5BA0
emu_fk = Emu()
assert {(t, s): new_handler(emu_fk) for t, (_, _, slots) in TYPES.items() for s in slots} == HANDLER
emu_fk.fake(CELL_TEST, 0, 3)
emu_fk.fake(CELL_TEST_RECT, 0, 5)
emu_fk.baseline = emu_fk.snapshot()


def pick_emu(cells):
    return emu if cells == "real" else emu_fk


def set_cells(e, cells):
    if cells != "real":
        e.set_fake_ret(CELL_TEST, cells)
        e.set_fake_ret(CELL_TEST_RECT, cells)


def f32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def vec(e, x, y, z):
    p = e.alloc(24)
    e.write(p, struct.pack("<3d", x, y, z))
    return p


def vecs(e, pts):
    p = e.alloc(24 * max(len(pts), 1))
    for i, v in enumerate(pts):
        e.write(p + 24 * i, struct.pack("<3d", *v))
    return p


def setup_types(e):
    for t, (mask, flags, slots) in TYPES.items():
        e.write(TASK_TYPES + t * 0x30 + 0x28, struct.pack("<H", mask))
        e.write(TASK_TYPES + t * 0x30 + 0x2C, struct.pack("<I", flags))
        for s in slots:
            e.write(HANDLERS + (s * 0x180 + t) * 4, struct.pack("<I", HANDLER[(t, s)]))


def nodes(e, t):
    out, todo = [], [e.u32(t + 0x10 + 4 * i) for i in range(6)]
    while todo:
        n = todo.pop()
        out.append(n)
        todo += [c for c in struct.unpack("<8I", e.read(n + 0xC, 32)) if c]
    return out


def add_masks(e, t, rng, density):
    """Gives random nodes a 64x64 cell mask with the given share of bits set."""
    for n in nodes(e, t):
        if rng.random() < 0.5:
            continue
        m = e.alloc(0xC + 64 * 64 // 8)
        bits = bytes(sum(1 << b for b in range(8) if rng.random() < density) for _ in range(512))
        e.write(m, struct.pack("<3I", 0, 6, 64) + bits)
        e.write(n + 0x3C, struct.pack("<I", m))


def offsets(offset):
    return offset, offset + 0x123457, offset - 0x2468AC


def populate(e, rng, n, scale, center=(0.0, 0.0, 0.0), density=0.0, offset=0x20000000):
    e.call(MEM_INIT)
    setup_types(e)
    t = e.call(TERRAIN_NEW, 3000)[0]
    e.write(t + 0xDE0, struct.pack("<3i", *offsets(offset)))
    e.points = []
    for _ in range(n):
        o = e.alloc(0x80)
        e.write(o + 0x1C, struct.pack("<H", rng.choice(list(TYPES))))
        p = [c + rng.uniform(-scale, scale) for c in center]
        e.points.append(p)
        cat = rng.choice([1, 2, 4, 8])
        if rng.random() < 0.7:
            e.call(ADD, t, o, vec(e, *p), f32(rng.choice([0.0, 20.0, 900.0, 5000.0])), cat)
        else:
            ext = rng.choice([10.0, 3000.0])
            e.call(ADD_BOX, t, o, vec(e, *p), vec(e, *[c - ext for c in p]), vec(e, *[c + ext for c in p]), cat)
    if density:
        add_masks(e, t, rng, density)
    return t


def set_event(e, rng, slot):
    e.write(SLOT_EVENT + slot, bytes([rng.choice([0xFF, 0xFF, 0, 1, 4, 8, 20])]))


WORLDS = [
    # (objects, scale, center, mask density, offset)
    (30, 60000.0, (0.0, 0.0, 0.0), 0.0, 0x20000000),
    (30, 60000.0, (0.0, 0.0, 0.0), 0.3, 0x20000000),
    (25, 3000.0, (70000.0, -50000.0, 9000.0), 0.0, 0x20000000),
    (25, 3000.0, (70000.0, -50000.0, 9000.0), 0.05, 0x20000000),
    (40, 400000.0, (0.0, 0.0, 0.0), 0.5, 0x20000000),
    (1, 10.0, (100.0, 100.0, 100.0), 0.0, 0x20000000),
    (40, 60000.0, (0.0, 0.0, 0.0), 0.0, 0x40000000),
    (40, 200000.0, (0.0, 0.0, 0.0), 0.4, 0x40000000),
    (60, 200000.0, (0.0, 0.0, 0.0), 0.002, 0x40000000),
]


def world_terrain(e, rng, w):
    return populate(e, rng, w[0], w[1], w[2], w[3], w[4])


def query_box(rng, world):
    scale, center = world[1], world[2]
    p = [c + rng.uniform(-scale, scale) for c in center]
    size = rng.choice([0.4, 1.9, 2000.0, scale * 0.5, scale, scale * 2])
    ext = [size * rng.uniform(0.8, 1.0) for _ in range(3)]
    lo, hi = [a - b for a, b in zip(p, ext)], [a + b for a, b in zip(p, ext)]
    if rng.random() < 0.4:
        lo, hi = snap(rng, world, lo), snap(rng, world, hi)
    return lo, hi


def snap(rng, world, p):
    """Moves p onto a cell boundary, where the clipping compares are equal."""
    grid = 1 << rng.randrange(13, 19)
    offs = offsets(world[4])
    return [float(round((c + o) / grid) * grid - o) for c, o in zip(p, offs)]


@pytest.mark.parametrize("seed", range(12))
@pytest.mark.parametrize("world", range(len(WORLDS)))
def test_send_event_box(seed, world):
    w = WORLDS[world]

    def setup(e):
        rng = random.Random(seed * 31 + world)
        t = world_terrain(e, rng, w)
        slot = rng.choice(SLOTS + [9])
        set_event(e, rng, slot)
        lo, hi = query_box(rng, w)
        return t, slot | 0x1200, 0xABCD, vec(e, *lo), vec(e, *hi), rng.choice([1, 3, 0xFF, 4]), rng.randrange(6)
    compare(emu, BOX, "DynCube_SendEventBox", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("seed", range(12))
@pytest.mark.parametrize("world", range(len(WORLDS)))
def test_send_event_lines(seed, world):
    w = WORLDS[world]

    def setup(e):
        rng = random.Random(seed * 37 + world)
        t = world_terrain(e, rng, w)
        slot = rng.choice(SLOTS + [9])
        set_event(e, rng, slot)
        starts, ends = [], []
        for _ in range(rng.choice([1, 1, 2, 5])):
            lo, hi = query_box(rng, w)
            if rng.random() < 0.5:
                lo, hi = hi, lo
            if rng.random() < 0.3:
                lo[1] = hi[1]
            starts.append(lo)
            ends.append(hi)
        return t, slot | 0x1200, 0xABCD, vecs(e, starts), vecs(e, ends), len(starts), rng.choice([1, 3, 0xFF, 4]), \
            rng.randrange(6)
    compare(emu, LINES, "DynCube_SendEventLines", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("world", [0, 1])
def test_send_event_lines_none(world):
    w = WORLDS[world]

    def setup(e):
        rng = random.Random(5)
        t = world_terrain(e, rng, w)
        return t, 8, 0, vecs(e, []), vecs(e, []), 0, 0xFF, rng.choice([0, 1])
    compare(emu, LINES, "DynCube_SendEventLines", setup=setup, check_eax=False, normalized=True)


def test_send_event_no_handlers():
    def setup(e):
        t = populate(e, random.Random(1), 10, 1000.0)
        return t, 77, 0, vec(e, -1e5, -1e5, -1e5), vec(e, 1e5, 1e5, 1e5), 0xFF, 0
    compare(emu, BOX, "DynCube_SendEventBox", setup=setup, check_eax=False, normalized=True)


DENSE_WORLDS = [0, 1, 3, 6, 7, 8]


def near_object(e, rng, sizes):
    p = rng.choice(e.points)
    size = rng.choice(sizes)
    q = [c + rng.uniform(-size, size) for c in p]
    return q, size


@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("tree", [0, 1, 3, 4, 5])
@pytest.mark.parametrize("world", DENSE_WORLDS)
@pytest.mark.parametrize("cells", ["real", 0, 1])
def test_send_event_box_dense(seed, tree, world, cells):
    def setup(e):
        set_cells(e, cells)
        rng = random.Random(seed * 7 + tree * 101 + world)
        t = world_terrain(e, rng, WORLDS[world])
        slot = rng.choice(SLOTS)
        e.write(SLOT_EVENT + slot, bytes([rng.choice([0xFF, 0xFF, 0, 1])]))
        q, size = near_object(e, rng, [0.3, 1.5, 300.0, 5000.0, 40000.0, 1e6])
        lo = [c - size * rng.uniform(0.5, 1) for c in q]
        hi = [c + size * rng.uniform(0.5, 1) for c in q]
        return t, slot, 0x5555, vec(e, *lo), vec(e, *hi), 0xFF, tree
    compare(pick_emu(cells), BOX, "DynCube_SendEventBox", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("tree", [0, 1, 3, 4, 5])
@pytest.mark.parametrize("world", DENSE_WORLDS)
@pytest.mark.parametrize("cells", ["real", 0, 1])
def test_send_event_lines_dense(seed, tree, world, cells):
    def setup(e):
        set_cells(e, cells)
        rng = random.Random(seed * 11 + tree * 103 + world)
        t = world_terrain(e, rng, WORLDS[world])
        slot = rng.choice(SLOTS)
        e.write(SLOT_EVENT + slot, bytes([rng.choice([0xFF, 0xFF, 0, 1])]))
        starts, ends = [], []
        for _ in range(rng.choice([1, 2, 3])):
            q, size = near_object(e, rng, [0.3, 1.5, 300.0, 5000.0, 40000.0])
            if rng.random() < 0.5:
                # vertical
                starts.append([q[0], q[1], q[2] + size * 3])
                ends.append([q[0] + rng.uniform(-1, 1), q[1], q[2] - size * 3])
            else:
                d = [rng.uniform(-size, size) * 3 for _ in range(3)]
                starts.append([a - b for a, b in zip(q, d)])
                ends.append([a + b for a, b in zip(q, d)])
        return t, slot, 0x5555, vecs(e, starts), vecs(e, ends), len(starts), 0xFF, tree
    compare(pick_emu(cells), LINES, "DynCube_SendEventLines", setup=setup, check_eax=False, normalized=True)


UPDATE_START = 0x4CE620


@pytest.mark.parametrize("seed", range(3))
@pytest.mark.parametrize("world", [0, 6, 7])
@pytest.mark.parametrize("axis", [0, 1, 2])
@pytest.mark.parametrize("side", [0, 1])
@pytest.mark.parametrize("kind", ["box", "lines"])
@pytest.mark.parametrize("cells", ["real", 1])
def test_send_event_edge(seed, world, axis, side, kind, cells):
    """The query ends exactly on the start node's center planes."""
    def setup(e):
        set_cells(e, cells)
        rng = random.Random(seed * 5 + axis + side * 3)
        tree = rng.choice([0, 1, 4, 5])
        t = world_terrain(e, rng, WORLDS[world])
        e.call(UPDATE_START, t, tree)
        c = list(struct.unpack("<3d", e.read(t + 0x40 + 24 * tree, 24)))
        # a small object just across the plane, touching it
        o = e.alloc(0x80)
        e.write(o + 0x1C, struct.pack("<H", 4))
        p = [x + rng.uniform(-50, 50) for x in c]
        p[axis] = c[axis] + (20.0 if side else -20.0)
        e.call(ADD, t, o, vec(e, *p), f32(20.0), {0: 1, 1: 2, 4: 4, 5: 8}[tree])
        e.call(UPDATE_START, t, tree)
        c = list(struct.unpack("<3d", e.read(t + 0x40 + 24 * tree, 24)))
        lo = [x - rng.uniform(100, 20000) for x in c]
        hi = [x + rng.uniform(100, 20000) for x in c]
        (hi if side else lo)[axis] = c[axis]
        if kind == "box":
            return t, 3, 0, vec(e, *lo), vec(e, *hi), 0xFF, tree
        return t, 3, 0, vecs(e, [lo]), vecs(e, [hi]), 1, 0xFF, tree
    compare(pick_emu(cells), BOX if kind == "box" else LINES, "DynCube_SendEventBox" if kind == "box" else
            "DynCube_SendEventLines", setup=setup, check_eax=False, normalized=True)
