import random
import struct

import pytest
from unicorn import UC_HOOK_CODE

from emu import Emu, compare

emu = Emu()

SEND_EVENT = 0x4CE090
SEND_EVENT_ALL = 0x4CE390
LINES_VERTICAL = 0x4C7770
LINES_SLANTED = 0x4C8130
LINES_EX = 0x4CDB20

WALK_TASKS = 0xB97D20
HOLE_LIST = 0xB97DA0
HOLE_COUNT = 0xB97DF8
BUMPS = 0xBA2030
BUMP_COUNT = 0xBA2058
HOLES = 0xBA5080
MATERIAL_IDS = 0xC28FE0

# Callees outside the terrain code are Python hooks that log their arguments by value,
# so original and rewrite compare even when they pass pointers to their own stack.
LOG = []
MODE = {}


def pyfake(e, addr, fn):
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


def point_query(e, q, n):
    return (e.u32(q + 4), e.u32(q + 8), e.read(q + 0xC, n), e.read(q + 0x8C, 1), e.read(q + 0x90, 8))


def send_all(e, log):
    t, slot, q, p, mask = (e.arg(i) for i in range(5))
    if slot == 13:
        log.append(("all13", t, q, e.read(p, 24), mask))
        holes = MODE.get("holes", [])
        e.write(HOLE_COUNT, struct.pack("<I", len(holes)))
        e.write(HOLE_LIST, struct.pack("<%dI" % len(holes), *holes))
        return
    n = e.u32(q + 4)
    log.append(("all", t, slot, e.u32(q), point_query(e, q, n), p, mask))
    # take over some points, sometimes leaving a material behind
    for k, (clear, mat) in enumerate(MODE.get("points", [])[:n]):
        if clear:
            e.write(q + 0xC + k, b"\0")
        if mat:
            e.write(e.u32(q + 8) + 4 * k, struct.pack("<I", mat))


def send(e, log):
    t, slot, q, p, mask, tree = (e.arg(i) for i in range(6))
    log.append(("send", t, slot, e.read(e.u32(q), 24), point_query(e, q, 1), e.read(p, 24), mask, tree))
    if MODE.get("clear") and len(log) % 2:
        e.write(q + 0xC, b"\0")


def line_query(e, q):
    n = e.u32(q + 8)
    return (e.read(e.u32(q), 24 * n), e.read(e.u32(q + 4), 24 * n), n, e.read(q + 0xC, 1), e.read(q + 0x10, 0x14))


def mark_found(e, q):
    pos = MODE.get("found")
    if pos:
        r = e.u32(q + 0x20)
        e.write(r + 8, struct.pack("<3d", *pos))
        e.write(r + 0x58, struct.pack("<I", 1))


def lines_vertical(e, log):
    t, slot, q, lo, hi, flags, tree = (e.arg(i) for i in range(7))
    log.append(("vert", t, slot, line_query(e, q), e.read(lo, 24), e.read(hi, 24), flags, tree))
    mark_found(e, q)


def lines_slanted(e, log):
    t, slot, q, s, en, n, flags, tree = (e.arg(i) for i in range(8))
    log.append(("slant", t, slot, line_query(e, q), e.read(s, 24 * n), e.read(en, 24 * n), n, flags, tree))
    mark_found(e, q)


def lines_ex(e, log):
    args = [e.arg(i) for i in range(9)]
    n = args[4]
    log.append(("ex", args[0], args[1], e.read(args[2], 24 * n), e.read(args[3], 24 * n), *args[4:]))


H_ENTER, H_LEAVE = new_code(emu), new_code(emu)
pyfake(emu, H_ENTER, lambda e, log: log.append(("enter", e.arg(0), e.arg(1))))
pyfake(emu, H_LEAVE, lambda e, log: log.append(("leave", e.arg(0), e.arg(1))))
pyfake(emu, SEND_EVENT_ALL, send_all)
pyfake(emu, SEND_EVENT, send)
pyfake(emu, LINES_VERTICAL, lines_vertical)
pyfake(emu, LINES_SLANTED, lines_slanted)
pyfake(emu, LINES_EX, lines_ex)


def put(e, data):
    p = e.alloc(len(data))
    e.write(p, data)
    return p


def vec(*xs):
    return struct.pack("<%dd" % len(xs), *xs)


# --- terrain data ----------------------------------------------------------

GRID = [4, 16, 28, 40, 52]


def make_leaf(rng, extra=6):
    verts = []
    for yi in GRID:
        for xi in GRID:
            verts.append((xi + rng.randint(-3, 3), yi + rng.randint(-3, 3), rng.randint(0, 63)))
    for _ in range(4):
        verts.append((rng.randint(0, 63), rng.randint(0, 63), rng.randint(0, 63)))
    tris = []
    for j in range(4):
        for i in range(4):
            a, b, c, d = j * 5 + i, j * 5 + i + 1, j * 5 + i + 6, j * 5 + i + 5
            for t in ((a, b, c), (a, c, d)):
                t = list(t)
                if rng.random() < 0.5:
                    t[1], t[2] = t[2], t[1]
                tris.append(t)
    for _ in range(extra):
        tris.append([rng.randrange(len(verts)) for _ in range(3)])
    rng.shuffle(tris)
    n1 = rng.randint(0, len(verts))
    data = struct.pack("<hhhh", len(tris), 4 * len(tris), n1, len(verts) - n1)
    data += b"".join(struct.pack("<I", t[0] | t[1] << 8 | t[2] << 16 | rng.randrange(256) << 24) for t in tris)
    data += b"".join(struct.pack("<I", (x << 26 | y << 20 | z << 14 | rng.randrange(1 << 14)) & 0xFFFFFFFF)
                     for x, y, z in verts)
    return data


def make_terrain(e, rng, nnodes=48, density=0.85, origin=(0, 0, 0), objects=None):
    leaves = [put(e, make_leaf(rng)) for _ in range(6)]
    nodes = e.alloc(0x20 * nnodes)
    for i in range(nnodes):
        child = [rng.randrange(1, nnodes) for _ in range(8)]
        code = [rng.randrange(8) for _ in range(8)]
        mask = sum(1 << b for b in range(8) if rng.random() < density)
        e.write(nodes + 0x20 * i, struct.pack("<8H8BB3xI", *child, *code, mask, rng.choice(leaves)))
    t = e.alloc(0xDEC)
    mats = []
    for i in range(41):
        m = put(e, struct.pack("<4f", 1, 2, 3, rng.choice([0.0, 1.0])))
        mats.append(m)
    e.write(t + 0xC, struct.pack("<I", nodes))
    e.write(t + 0x20, struct.pack("<I", objects or 0))
    e.write(t + 0xD3C, struct.pack("<41I", *mats))
    e.write(t + 0xDE0, struct.pack("<3i", *origin))
    for i in range(8):
        e.write(MATERIAL_IDS + 900 * i, bytes([rng.randrange(1, 41)]))
    return t


def make_objects(e, rng, n=12):
    """Object octree nodes with a few tasks each; returns the root."""
    for ty in range(4):
        e.write(0xA9C4E0 + 4 * ty, struct.pack("<I", H_ENTER))
        e.write(0xA9CAE0 + 4 * ty, struct.pack("<I", H_LEAVE))
    objs = [e.alloc(0x40) for _ in range(n)]
    for o in objs:
        tasks = [e.alloc(0x60) for _ in range(rng.randint(1, 3))]
        for k, tk in enumerate(tasks):
            nxt = tasks[k + 1] if k + 1 < len(tasks) else 0
            e.write(tk + 0x1C, struct.pack("<H", rng.randrange(4)))
            e.write(tk + 0x48, struct.pack("<I", rng.randrange(1 << 32)))
            e.write(tk + 0x58, struct.pack("<IH", nxt, rng.choice([0, 0x80, 0x100, 0x180])))
        kids = [rng.choice(objs + [0, 0]) for _ in range(8)]
        e.write(o + 0xC, struct.pack("<8I", *kids))
        e.write(o + 0x2C, struct.pack("<I", tasks[0]))
        e.write(o + 0x38, struct.pack("<H", rng.choice([0, 0x80, 0x100, 0x180])))
    return objs[0]


def cursor(e, t, rng=None):
    c = e.alloc(0x24)
    nodes = e.u32(t + 0xC)
    e.write(c, struct.pack("<9I", 0x40000000, 30, 0, 0, 0, 0, nodes + 0x20, t, 0))
    return c


# --- Coll_LeafHit ------------------------------------------------------------

def bump_setup(e, rng, base):
    heights = put(e, bytes(rng.randrange(256) for _ in range(81)))
    rec = BUMPS + 0x30
    e.write(rec + 0xC, struct.pack("<if", 0, 7.0 / 38400))
    e.write(rec + 0x18, vec(base[0] - 19200, base[1] - 19200))
    e.write(rec + 0x28, struct.pack("<I", heights))
    e.write(BUMP_COUNT, struct.pack("<I", 1))


@pytest.mark.parametrize("seed", range(25))
@pytest.mark.parametrize("bump", [False, True])
def test_leaf_hit(seed, bump):
    rng = random.Random(seed)
    flags = rng.randrange(8)
    up = rng.randrange(2)
    base = (rng.uniform(-1e5, 1e5), rng.uniform(-1e5, 1e5), rng.uniform(-1e5, 1e5))
    pt = (base[0] + rng.uniform(-12000, 12000), base[1] + rng.uniform(-12000, 12000),
          base[2] + rng.uniform(-20000, 30000))

    def setup(e):
        r = random.Random(seed)
        leaf = put(e, make_leaf(r))
        if bump:
            bump_setup(e, r, base)
        return (leaf, put(e, vec(*base)), put(e, vec(*pt)), e.alloc(24), 16, e.alloc(4), flags, up, e.alloc(12))
    compare(emu, 0x4CA010, "Coll_LeafHit", setup=setup)


def test_leaf_hit_hits():
    """Make sure the random cases above actually hit triangles."""
    hits = 0
    for seed in range(40):
        rng = random.Random(seed)
        flags, up = rng.randrange(8), rng.randrange(2)
        base = (rng.uniform(-1e5, 1e5), rng.uniform(-1e5, 1e5), rng.uniform(-1e5, 1e5))
        pt = (base[0] + rng.uniform(-12000, 12000), base[1] + rng.uniform(-12000, 12000),
              base[2] + rng.uniform(-20000, 30000))
        emu.reset()
        leaf = put(emu, make_leaf(random.Random(seed)))
        hits += emu.call(0x4CA010, leaf, put(emu, vec(*base)), put(emu, vec(*pt)), emu.alloc(24), 16,
                         emu.alloc(4), flags, up, emu.alloc(12))[0]
    assert 5 < hits < 38


def test_leaf_hit_no_triangles():
    def setup(e):
        leaf = put(e, struct.pack("<hhhhI", 0, 0, 1, 0, 0x12345678))
        return (leaf, put(e, vec(0, 0, 0)), put(e, vec(1, 2, 3)), e.alloc(24), 16, e.alloc(4), 0, 0, e.alloc(12))
    compare(emu, 0x4CA010, "Coll_LeafHit", setup=setup)


# --- terrain walks ------------------------------------------------------------

def world(e, seed, walk, origin=(0, 0, 0)):
    """Terrain (with an object octree when walk) and a fresh cursor."""
    r = random.Random(seed)
    objs = make_objects(e, r) if walk else 0
    t = make_terrain(e, r, density=0.95, origin=origin, objects=objs)
    e.write(WALK_TASKS, struct.pack("<I", walk))
    c = cursor(e, t)
    e.write(c + 0x20, struct.pack("<I", objs))
    return r, t, c


def point(r, scale=60000.0):
    return [r.uniform(-scale, scale) for _ in range(3)]


@pytest.mark.parametrize("seed", range(20))
@pytest.mark.parametrize("walk", [0, 1])
def test_descend(seed, walk):
    def setup(e):
        r, t, c = world(e, seed, walk, origin=(0, 0, 0) if seed % 4 else (12345, -777, 99999))
        e.write(HOLE_COUNT, struct.pack("<I", r.choice([0, 3, 9])))
        return t, c, c, put(e, vec(*point(r))), r.choice([0, 1, 5, 16, 20])
    cmp(emu, 0x4C9A30, "Coll_Descend", setup=setup)


@pytest.mark.parametrize("seed", range(20))
def test_get_leaf_material(seed):
    def setup(e):
        r, t, c = world(e, seed, 0)
        p = put(e, vec(*point(r)))
        depth = e.call(0x4C9A30, t, c, c, p, r.choice([0, 3, 8, 16]))[0]
        if seed % 3 == 0:
            p = put(e, vec(*point(r)))
        return c, p, depth
    cmp(emu, 0x4C9D70, "Coll_GetLeafMaterial", setup=setup)


def holes_setup(e, r, n):
    """n hole masks over the area around the origin; returns their indices."""
    idx = []
    for k in range(n):
        i = r.randrange(16)
        bits = put(e, bytes(r.randrange(256) for _ in range(8 * 67)))
        e.write(HOLES + 0x30 * i, struct.pack("<B11xifi", r.randrange(8), 3, 64.0 / 120000, 0))
        e.write(HOLES + 0x30 * i + 0x18, vec(-60000.0, -60000.0) + struct.pack("<I", bits))
        idx.append(i)
    return idx


@pytest.mark.parametrize("seed", range(20))
@pytest.mark.parametrize("walk", [0, 1])
def test_get_hit_material(seed, walk):
    def setup(e):
        r = random.Random(seed)
        holes = holes_setup(e, r, r.choice([0, 1, 3, 6]))
        MODE["holes"] = holes
        e.write(WALK_TASKS, struct.pack("<I", walk))
        e.write(HOLE_COUNT, struct.pack("<I", len(holes)))
        e.write(HOLE_LIST, struct.pack("<%dI" % len(holes), *holes))
        for i in range(8):
            e.write(MATERIAL_IDS + 900 * i, bytes([r.randrange(1, 41)]))
        return 0x2345678, put(e, vec(*point(r, 59000)))
    cmp(emu, 0x4CB810, "Coll_GetHitMaterial", setup=setup)


def vertical_setup(e, seed, walk):
    r, t, c = world(e, seed, walk)
    MODE["holes"] = holes_setup(e, r, r.choice([0, 2]))
    p = point(r)
    up = r.randrange(2)
    p[2] = r.uniform(-2e5, 0) if up else r.uniform(0, 2e5)
    p = put(e, vec(*p))
    depth = 0
    if seed % 2:
        depth = e.call(0x4C9A30, t, c, c, p, r.choice([1, 4, 9]))[0]
    return r, c, p, up, depth


@pytest.mark.parametrize("seed", range(30))
@pytest.mark.parametrize("walk", [0, 1])
def test_vertical_hit(seed, walk):
    def setup(e):
        r, c, p, up, depth = vertical_setup(e, seed, walk)
        return e.alloc(0x70), c, p, up, r.choice([300000, 100000, 1000]), depth, seed % 3 != 0
    cmp(emu, 0x4CB050, "Coll_VerticalHit", setup=setup)


def test_vertical_hit_hits():
    hits = 0
    for seed in range(30):
        emu.reset()
        LOG[:] = [[]]
        r, c, p, up, depth = vertical_setup(emu, seed, 0)
        hits += emu.call(0x4CB050, emu.alloc(0x70), c, p, up, 300000, depth, 1)[0]
    assert 5 < hits < 28


# --- queries --------------------------------------------------------------------

@pytest.mark.parametrize("seed", range(16))
@pytest.mark.parametrize("flags", [0x80000000, 0x80000003, 0x3, 0])
def test_get_point_materials(seed, flags):
    def setup(e):
        r, t, c = world(e, seed, seed % 2)
        count = r.choice([1, 2, 6])
        mats = [put(e, struct.pack("<4f", 1, 2, 3, z)) for z in (0.0, 5.0)]
        MODE["points"] = [(r.randrange(2), r.choice([0, mats[0], mats[1]])) for _ in range(count)]
        if seed % 5 == 0:
            e.write(t + 0xC, struct.pack("<I", 0))
        pts = put(e, vec(*sum((point(r) for _ in range(count)), [])))
        return t, e.alloc(4 * count), pts, count, 0x1234577, 0x55, flags
    cmp(emu, 0x4C9750, "Coll_GetPointMaterials", setup=setup, check_eax=False)


def test_get_point_materials_none():
    def setup(e):
        r, t, c = world(e, 1, 0)
        MODE["points"] = []
        return t, e.alloc(4), put(e, vec(1.0, 2.0, 3.0)), 0, 0, 0, 0x80000001
    cmp(emu, 0x4C9750, "Coll_GetPointMaterials", setup=setup, check_eax=False)


def segments(r, count, vertical):
    starts, ends = [], []
    for _ in range(count):
        s = point(r)
        e = [v + r.uniform(-30000, 30000) for v in s]
        if vertical:
            e[:2] = s[:2]
        if r.random() < 0.6:
            s[2], e[2] = r.uniform(5e4, 2e5), r.uniform(-2e5, -5e4)
            if r.random() < 0.3:
                s[2], e[2] = e[2], s[2]
        starts += s
        ends += e
    return starts, ends


@pytest.mark.parametrize("seed", range(20))
@pytest.mark.parametrize("flags", [0x80000000, 0x80000003, 0x3])
def test_lines(seed, flags):
    def setup(e):
        r, t, c = world(e, seed, seed % 2)
        count = r.choice([1, 1, 2, 5])
        starts, ends = segments(r, count, seed % 3 == 0)
        MODE["clear"] = seed % 4 == 1
        MODE["found"] = point(r) if seed % 3 == 1 else None
        return t, e.alloc(0x70 * count), put(e, vec(*starts)), put(e, vec(*ends)), count, 0x77, 0x55, flags
    cmp(emu, 0x4CC2A0, "Coll_Lines", setup=setup, check_eax=False)


def test_lines_none():
    cmp(emu, 0x4CC2A0, "Coll_Lines", setup=lambda e: (0, 0, 0, 0, 0, 0, 0, 0x80000003), check_eax=False)


def test_lines_hits():
    found = 0
    for seed in range(20):
        emu.reset()
        LOG[:] = [[]]
        MODE.clear()
        r, t, c = world(emu, seed, 0)
        count = r.choice([1, 1, 2, 5])
        starts, ends = segments(r, count, seed % 3 == 0)
        res = emu.alloc(0x70 * count)
        emu.call(0x4CC2A0, t, res, put(emu, vec(*starts)), put(emu, vec(*ends)), count, 0, 0, 0x80000000)
        found += sum(emu.u32(res + 0x70 * i + 0x58) for i in range(count))
    assert found > 5


def drop_setup(e, seed):
    r, t, c = world(e, seed, seed % 2)
    MODE["holes"] = holes_setup(e, r, r.choice([0, 2]))
    MODE["clear"] = seed % 4 == 1
    MODE["found"] = point(r) if seed % 3 == 1 else None
    count = r.choice([1, 2, 5, 20])
    pts, dz = [], []
    for _ in range(count):
        p = point(r)
        p[2] = r.uniform(-1e5, 1e5)
        pts += p
        dz.append(r.choice([-1, 1]) * r.uniform(1000, 3e5))
    return t, e.alloc(0x70 * count), put(e, vec(*pts)), count, put(e, vec(*dz))


DROPS = [(0x4CABF0, "Coll_DropPoints"), (0x4CB9F0, "Coll_DropPointsMat"), (0x4CBE40, "Coll_DropPointsEx")]


@pytest.mark.parametrize("seed", range(16))
@pytest.mark.parametrize("flags", [0x80000000, 0x80000003, 0x3])
@pytest.mark.parametrize("fn", DROPS)
def test_drop_points(seed, flags, fn):
    def setup(e):
        return drop_setup(e, seed) + (0x77, 0x55, flags, 0x99)
    cmp(emu, fn[0], fn[1], setup=setup, check_eax=False)


def test_drop_points_hits():
    found = 0
    for seed in range(16):
        emu.reset()
        LOG[:] = [[]]
        MODE.clear()
        t, res, pts, count, dz = drop_setup(emu, seed)
        MODE["clear"] = False
        emu.call(0x4CB9F0, t, res, pts, count, dz, 0, 0, 0x80000000)
        found += sum(emu.u32(res + 0x70 * i + 0x58) for i in range(count))
    assert found > 5
