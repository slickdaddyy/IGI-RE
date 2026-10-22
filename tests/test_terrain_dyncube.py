import random
import struct

import pytest

from emu import Emu, ErrorShown, compare

emu = Emu()
# separate Emu where the node data free function is faked
emu_fk = Emu()
emu_fk.fake(0x4E54E0, 0, 1)
emu_fk.baseline = emu_fk.snapshot()

MEM_INIT, TERRAIN_NEW = 0x4B0BA0, 0x4C5B20
ADD, ADD_BOX, REMOVE, MOVE, MOVE_BOX = 0x4C7140, 0x4C7350, 0x4C7560, 0x4C6940, 0x4C6E70
UNLINK = 0x4C6CE0
TASK_TYPES, HANDLERS = 0xAF5EE0, 0xA96AE0
OFFSET = 0xDE0

# type: (event mask, handler slots)
TYPES = {
    1: (0x0001, [1]),
    2: (0x0006, [4, 9, 30]),
    3: (0x0010, [5]),
    4: (0x0000, [11]),
    5: (0x0102, [0, 2, 253]),
}


def f32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def vec(e, x, y, z):
    p = e.alloc(24)
    e.write(p, struct.pack("<3d", x, y, z))
    return p


def setup_types(e):
    for t, (mask, slots) in TYPES.items():
        e.write(TASK_TYPES + t * 0x30 + 0x28, struct.pack("<H", mask))
        for s in slots:
            e.write(HANDLERS + (s * 0x180 + t) * 4, struct.pack("<I", 0x401000 + s))


def terrain(e, offset=(0x20000000, 0x20000000, 0x20000000), pool=2000):
    e.call(MEM_INIT)
    setup_types(e)
    t = e.call(TERRAIN_NEW, pool)[0]
    e.write(t + OFFSET, struct.pack("<3i", *offset))
    return t


def obj(e, type_):
    o = e.alloc(0x80)
    e.write(o + 0x1C, struct.pack("<H", type_))
    return o


def rand_pos(rng, scale):
    return [rng.uniform(-scale, scale) for _ in range(3)]


def populate(e, seed, n=24, scale=60000.0):
    """Adds n objects with the original code; returns (terrain, objects)."""
    rng = random.Random(seed)
    t = terrain(e)
    objs = []
    for i in range(n):
        o = obj(e, rng.choice(list(TYPES)))
        cat = rng.choice([1, 2, 4, 8])
        if rng.random() < 0.7:
            e.call(ADD, t, o, vec(e, *rand_pos(rng, scale)), f32(rng.choice([0.0, 50.0, 900.0, 5000.0])), cat)
        else:
            p = rand_pos(rng, scale)
            ext = rng.choice([10.0, 3000.0, 20000.0])
            e.call(ADD_BOX, t, o, vec(e, *p), vec(e, *[c - ext for c in p]), vec(e, *[c + ext for c in p]), cat)
        objs.append(o)
    return t, objs


@pytest.mark.parametrize("seed", range(8))
def test_cell_size(seed):
    rng = random.Random(seed)
    for _ in range(30):
        a = [rng.randrange(0, 1 << 31) for _ in range(3)]
        bits = rng.choice([0, 4, 13, 14, 20, 30, 31])
        b = [x ^ (rng.randrange(0, 1 << bits) if bits else 0) for x in a]
        compare(emu, 0x4C67E0, "DynCube_CellSize", args=a + b)


@pytest.mark.parametrize("lst", [-1, 0, 3, 7])
@pytest.mark.parametrize("count", [0, 1, 3])
def test_list_add(lst, count):
    def setup(e):
        t = terrain(e)
        os = [obj(e, 1) for _ in range(count + 1)]
        for o in os[:-1]:
            e.call(0x4C6890, t, o, lst)
        return t, os[-1], lst
    compare(emu, 0x4C6890, "DynCube_ListAdd", setup=setup, check_eax=False)


@pytest.mark.parametrize("lst", [-1, 0, 5])
@pytest.mark.parametrize("which", [0, 1, 2])
def test_list_remove(lst, which):
    def setup(e):
        t = terrain(e)
        os = [obj(e, 1) for _ in range(3)]
        for o in os:
            e.call(0x4C6890, t, o, lst)
        return t, os[which], lst
    compare(emu, 0x4C6840, "DynCube_ListRemove", setup=setup, check_eax=False)


@pytest.mark.parametrize("cat", [1, 2, 4, 8])
@pytest.mark.parametrize("type_", list(TYPES) + [9])
def test_get_tree(cat, type_):
    compare(emu, 0x4C7290, "DynCube_GetTree", setup=lambda e: (setup_types(e), (cat, type_ | 0xABC0000))[1])


def test_get_tree_invalid():
    for target in (0x4C7290, "DynCube_GetTree"):
        emu.reset()
        with pytest.raises(ErrorShown):
            emu.call(target, 3, 1)


@pytest.mark.parametrize("seed", range(10))
def test_add(seed):
    def setup(e):
        rng = random.Random(seed + 100)
        t, _ = populate(e, seed, n=seed * 3)
        o = obj(e, rng.choice(list(TYPES)))
        return t, o, vec(e, *rand_pos(rng, 80000.0)), f32(rng.choice([0.0, 1.0, 400.0, 7000.0, 1e9])), \
            rng.choice([1, 2, 4, 8])
    compare(emu, ADD, "DynCube_Add", setup=setup, check_eax=False)


@pytest.mark.parametrize("seed", range(10))
def test_add_box(seed):
    def setup(e):
        rng = random.Random(seed + 200)
        t, _ = populate(e, seed, n=seed * 3)
        o = obj(e, rng.choice(list(TYPES)))
        p = rand_pos(rng, 80000.0)
        ext = rng.choice([0.0, 5.0, 4000.0, 30000.0])
        return t, o, vec(e, *p), vec(e, *[c - ext for c in p]), vec(e, *[c + ext * 0.5 for c in p]), \
            rng.choice([1, 2, 4, 8])
    compare(emu, ADD_BOX, "DynCube_AddBox", setup=setup, check_eax=False)


@pytest.mark.parametrize("seed", range(12))
def test_remove(seed):
    def setup(e):
        rng = random.Random(seed)
        t, objs = populate(e, seed, n=2 + seed * 2)
        return t, rng.choice(objs)
    compare(emu, REMOVE, "DynCube_Remove", setup=setup, check_eax=False)


def test_remove_frees_node_data():
    def setup(e):
        t, objs = populate(e, 3, n=1)
        node = e.u32(objs[0] + 0x50)
        e.write(node + 0x3C, struct.pack("<I", 0x12345678))
        return t, objs[0]
    compare(emu_fk, REMOVE, "DynCube_Remove", setup=setup, check_eax=False, normalized=True)


MOVES = [
    ("same", 0.0, None),
    ("radius", 0.0, 3000.0),
    ("small", 30.0, None),
    ("medium", 9000.0, None),
    ("far", 200000.0, None),
    ("grow", 0.0, 60000.0),
]


@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("kind,dist,radius", MOVES)
def test_move(seed, kind, dist, radius):
    def setup(e):
        rng = random.Random(seed)
        t, objs = populate(e, seed, n=4 + seed * 4)
        o = rng.choice(objs)
        p = struct.unpack("<3d", e.read(o + 0x20, 24))
        r = struct.unpack("<f", e.read(o + 0x44, 4))[0] if radius is None else radius
        q = [c + rng.uniform(-dist, dist) for c in p]
        return t, o, vec(e, *q), f32(r)
    compare(emu, MOVE, "DynCube_Move", setup=setup, check_eax=False)


def test_move_frees_node_data():
    def setup(e):
        t, objs = populate(e, 5, n=1)
        node = e.u32(objs[0] + 0x50)
        e.write(node + 0x3C, struct.pack("<I", 0x12345678))
        return t, objs[0], vec(e, 9e6, -9e6, 4e6), f32(10.0)
    compare(emu_fk, MOVE, "DynCube_Move", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("dist", [0.0, 30.0, 9000.0, 200000.0])
@pytest.mark.parametrize("ext", [1.0, 6000.0, 50000.0])
def test_move_box(seed, dist, ext):
    def setup(e):
        rng = random.Random(seed)
        t, objs = populate(e, seed, n=4 + seed * 4)
        o = rng.choice(objs)
        p = struct.unpack("<3d", e.read(o + 0x20, 24))
        q = [c + rng.uniform(-dist, dist) for c in p]
        return t, o, vec(e, *q), vec(e, *[c - ext for c in q]), vec(e, *[c + ext for c in q])
    compare(emu, MOVE_BOX, "DynCube_MoveBox", setup=setup, check_eax=False)


@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("up", [0, 1, 3, 6])
def test_ascend(seed, up):
    def setup(e):
        rng = random.Random(seed)
        t, objs = populate(e, seed, n=3 + seed * 3)
        o = rng.choice(objs)
        node = e.u32(o + 0x50)
        e.call(UNLINK, node, o)
        size = e.u32(node)
        ref = e.alloc(4)
        e.write(ref, struct.pack("<I", node))
        return t, min(size << up, 0x40000000), size, ref
    compare(emu, 0x4C6B20, "DynCube_Ascend", setup=setup, check_eax=False)


@pytest.mark.parametrize("seed", range(8))
def test_prune(seed):
    def setup(e):
        rng = random.Random(seed)
        t, objs = populate(e, seed, n=2 + seed * 3)
        o = rng.choice(objs)
        node = e.u32(o + 0x50)
        e.call(UNLINK, node, o)
        return t, node
    compare(emu, 0x4C75F0, "DynCube_Prune", setup=setup, check_eax=False)


@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("size", [0x40000000, 0x100000, 0x2000])
def test_insert(seed, size):
    def setup(e):
        rng = random.Random(seed)
        t, _ = populate(e, seed, n=seed * 4)
        o = obj(e, rng.choice(list(TYPES)))
        e.write(o + 0x5C, struct.pack("<H", rng.randrange(0x10000)))
        root = e.u32(t + 0x10 + 4 * rng.randrange(6))
        return t, root, o, size, rng.randrange(1 << 31), rng.randrange(1 << 31), rng.randrange(1 << 31)
    compare(emu, 0x4C6D20, "DynCube_Insert", setup=setup, check_eax=False)


@pytest.mark.parametrize("which", [0, 1, 2])
def test_unlink_link(which):
    def setup(e):
        t = terrain(e)
        node = e.u32(t + 0x10)
        os = [obj(e, 1) for _ in range(3)]
        for o in os:
            e.call(0x4C6E30, node, o)
        return node, os[which]
    compare(emu, UNLINK, "DynCubeNode_Unlink", setup=setup, check_eax=False)

    def setup_link(e):
        node, o = setup(e)
        e.call(UNLINK, node, o)
        return node, o
    compare(emu, 0x4C6E30, "DynCubeNode_Link", setup=setup_link, check_eax=False)


def test_get_root():
    for tree in range(6):
        compare(emu, 0x4C7750, "DynCube_GetRoot", setup=lambda e: (terrain(e), tree))


def test_sub_4c7760():
    compare(emu, 0x4C7760, "sub_4C7760", setup=lambda e: (obj(e, 1),), check_eax=False)
