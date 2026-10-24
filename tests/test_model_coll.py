import random
import struct

import pytest
from unicorn import UC_HOOK_CODE

from emu import Emu, compare, compare_hooked


@pytest.fixture(autouse=True, params=[0x37F, 0x7F, 0xC7F, 0x77F], ids=hex)
def cw(request):
    """Every test at the emulator default and at the control words the game uses."""
    Emu.cw = request.param
    yield
    Emu.cw = None


emu = Emu()

MEM_INIT, TERRAIN_NEW, ADD = 0x4B0BA0, 0x4C5B20, 0x4C7140
TASK_TYPES, HANDLERS = 0xAF5EE0, 0xA96AE0
MATERIAL_IDS = 0xC28FE0
LINES_EX = 0x4CDB20

LOG = []


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


# handlers: slot 9/10 get a line query, slot 11 a point query whose active[0] they may clear
H_LINE, H_POINT = new_code(emu), new_code(emu)
CLEAR = {"on": False}


def line_handler(e, log):
    q = e.arg(1)
    log.append(("line", e.arg(0), e.read(q, 12), e.read(q, 13)[12], e.read(q + 0x10, 0x14)))


def point_handler(e, log):
    q = e.arg(1)
    log.append(("point", e.arg(0), e.read(e.u32(q), 24), e.u32(q + 4), e.u32(q + 8), e.read(q + 0xC, 1),
                e.read(q + 0x8C, 1), e.read(q + 0x90, 8)))
    if CLEAR["on"]:
        e.write(q + 0xC, b"\0")


pyfake(emu, H_LINE, line_handler)
pyfake(emu, H_POINT, point_handler)


def put(e, data):
    p = e.alloc(len(data))
    e.write(p, data)
    return p


def vecs(*xs):
    return struct.pack("<%dd" % len(xs), *xs)


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


def terrain(e, rng, nnodes=48, density=0.97, origin=(0, 0, 0), objects=12):
    """Dyncube trees from the game's constructor plus a random terrain octree."""
    e.call(MEM_INIT)
    for ty in range(1, 4):
        e.write(TASK_TYPES + ty * 0x30 + 0x28, struct.pack("<HHI", 0xFFFF, 0, 0xFFFFFFFF))
        for s in (9, 10):
            e.write(HANDLERS + (s * 0x180 + ty) * 4, struct.pack("<I", H_LINE))
        if ty != 3:
            e.write(HANDLERS + (11 * 0x180 + ty) * 4, struct.pack("<I", H_POINT))
    t = e.call(TERRAIN_NEW, 2000)[0]
    leaves = [put(e, make_leaf(rng)) for _ in range(6)]
    nodes = e.alloc(0x20 * nnodes)
    for i in range(nnodes):
        child = [rng.randrange(1, nnodes) for _ in range(8)]
        code = [rng.randrange(8) for _ in range(8)]
        mask = sum(1 << b for b in range(8) if rng.random() < density)
        e.write(nodes + 0x20 * i, struct.pack("<8H8BB3xI", *child, *code, mask, rng.choice(leaves)))
    e.write(t + 0xC, struct.pack("<I", nodes))
    mats = [put(e, struct.pack("<4f", 1, 2, 3, i)) for i in range(41)]
    e.write(t + 0xD3C, struct.pack("<41I", *mats))
    e.write(t + 0xDE0, struct.pack("<3i", *origin))
    for i in range(8):
        e.write(MATERIAL_IDS + 3600 * i, bytes([rng.randrange(1, 41)]))
    for _ in range(objects):
        o = e.alloc(0x80)
        e.write(o + 0x1C, struct.pack("<H", rng.randrange(1, 4)))
        p = put(e, vecs(*[rng.uniform(-60000, 60000) for _ in range(3)]))
        e.call(ADD, t, o, p, struct.unpack("<I", struct.pack("<f", rng.choice([500.0, 20000.0])))[0],
               rng.choice([1, 2, 4, 8]))
    return t


def segments(rng, count, vertical, scale=60000.0):
    starts, ends = [], []
    for _ in range(count):
        s = [rng.uniform(-scale, scale) for _ in range(3)]
        e = [c + rng.uniform(-30000, 30000) for c in s]
        if vertical:
            e[:2] = s[:2]
        if rng.random() < 0.6:
            s[2], e[2] = rng.uniform(5e4, 2e5), rng.uniform(-2e5, -5e4)
            if rng.random() < 0.3:
                s[2], e[2] = e[2], s[2]
        starts += s
        ends += e
    return starts, ends


@pytest.mark.parametrize("seed", range(20))
@pytest.mark.parametrize("flags", [0x80000000, 0x80000003, 0x3])
def test_lines_ex(seed, flags):
    rng = random.Random(seed)
    count = rng.choice([1, 1, 2, 5])
    vertical = seed % 3 == 0
    CLEAR["on"] = seed % 4 == 1

    def setup(e):
        r = random.Random(seed)
        t = terrain(e, r)
        starts, ends = segments(r, count, vertical)
        res = e.alloc(0x70 * count)
        if seed % 2:
            for i in range(count):
                e.write(res + 0x70 * i + 8, vecs(*[r.uniform(-60000, 60000) for _ in range(3)]))
                e.write(res + 0x70 * i + 0x58, struct.pack("<I", 1))
        return t, res, put(e, vecs(*starts)), put(e, vecs(*ends)), count, 0x1234577, 0x55, flags, 0x99
    cmp(emu, LINES_EX, "Coll_LinesEx", setup=setup, check_eax=False)


def test_lines_ex_zero():
    cmp(emu, LINES_EX, "Coll_LinesEx", setup=lambda e: (0, 0, 0, 0, 0, 0, 0, 0x80000000, 0), check_eax=False)


def test_lines_ex_hits():
    """Make sure the cases above find terrain."""
    found = 0
    for seed in range(20):
        emu.reset()
        LOG.clear()
        LOG.append([])
        count = random.Random(seed).choice([1, 1, 2, 5])
        r = random.Random(seed)
        t = terrain(emu, r)
        starts, ends = segments(r, count, seed % 3 == 0)
        res = emu.alloc(0x70 * count)
        emu.call(LINES_EX, t, res, put(emu, vecs(*starts)), put(emu, vecs(*ends)), count, 0, 0, 0x80000000, 0)
        found += sum(emu.u32(res + 0x70 * i + 0x58) for i in range(count))
    assert found > 10


# --- Coll_LineHit and Coll_LeafLineHit -----------------------------------------

WALK_TASKS, BUMPS, BUMP_COUNT = 0xB97D20, 0xBA2030, 0xBA2058
H_ENTER, H_LEAVE = new_code(emu), new_code(emu)
pyfake(emu, H_ENTER, lambda e, log: log.append(("enter", e.arg(0), e.arg(1))))
pyfake(emu, H_LEAVE, lambda e, log: log.append(("leave", e.arg(0), e.arg(1))))


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


def bump_setup(e, rng, center):
    heights = put(e, bytes(rng.randrange(256) for _ in range(81 * 4)))
    rec = BUMPS + 0x30
    e.write(rec + 0xC, struct.pack("<if", 0, 7.0 / 38400))
    e.write(rec + 0x18, vecs(center[0] - 19200, center[1] - 19200))
    e.write(rec + 0x28, struct.pack("<I", heights))
    e.write(BUMP_COUNT, struct.pack("<I", 1))


def line_setup(e, seed, walk):
    r = random.Random(seed)
    t = terrain(e, r, objects=0)
    starts, ends = segments(r, 1, seed % 3 == 0)
    if seed % 5 == 4:
        ends = list(starts)
    objs = make_objects(e, r) if walk else 0
    e.write(WALK_TASKS, struct.pack("<I", walk))
    c = e.alloc(0x24)
    e.write(c, struct.pack("<9I", 0x40000000, 30, 0, 0, 0, 0, e.u32(t + 0xC) + 0x20, t, objs))
    s, en = put(e, vecs(*starts)), put(e, vecs(*ends))
    depth = 0
    if seed % 2:
        depth = r.choice([1, 4, 9])
        depth = e.call(0x4C9A30, t, c, c, s, depth)[0]
    return r, t, c, s, en, depth


@pytest.mark.parametrize("seed", range(30))
@pytest.mark.parametrize("walk", [0, 1])
def test_line_hit(seed, walk):
    def setup(e):
        r, t, c, s, en, depth = line_setup(e, seed, walk)
        return e.alloc(0x70), c, depth, s, en
    cmp(emu, 0x4CC810, "Coll_LineHit", setup=setup)


def test_line_hit_hits():
    hits = 0
    for seed in range(30):
        emu.reset()
        LOG.clear()
        LOG.append([])
        r, t, c, s, en, depth = line_setup(emu, seed, 0)
        hits += emu.call(0x4CC810, emu.alloc(0x70), c, depth, s, en)[0]
    assert 8 < hits < 28


def leaf_case(e, seed, bump):
    r = random.Random(seed)
    leaf = put(e, make_leaf(r))
    base = [r.uniform(-1e5, 1e5) for _ in range(3)]
    s = [b + r.uniform(-14000, 14000) for b in base]
    en = [b + r.uniform(-14000, 14000) for b in base]
    if seed % 3 == 0:
        s[2], en[2] = base[2] + 30000, base[2] - 30000
    if bump:
        bump_setup(e, r, base)
    return (leaf, put(e, vecs(*base)), put(e, vecs(*s)), put(e, vecs(*en)), e.alloc(24), 16, e.alloc(4),
            e.alloc(4), r.randrange(8), e.alloc(12))


@pytest.mark.parametrize("seed", range(40))
@pytest.mark.parametrize("bump", [0, 1])
def test_leaf_line_hit(seed, bump):
    cmp(emu, 0x4CD300, "Coll_LeafLineHit", setup=lambda e: leaf_case(e, seed, bump))


def test_leaf_line_hit_hits():
    hits = 0
    for seed in range(40):
        emu.reset()
        hits += emu.call(0x4CD300, *leaf_case(emu, seed, 0))[0]
    assert 8 < hits < 36


def test_leaf_line_hit_empty():
    def setup(e):
        leaf = put(e, struct.pack("<hhhhI", 0, 0, 1, 0, 0x12345678))
        return (leaf, put(e, vecs(0, 0, 0)), put(e, vecs(1, 2, 3)), put(e, vecs(4, 5, 6)), e.alloc(24), 16,
                e.alloc(4), e.alloc(4), 0, e.alloc(12))
    cmp(emu, 0x4CD300, "Coll_LeafLineHit", setup=setup)


# --- the game's Coll_Lines with our hooks in place ----------------------------

_hooked = []


def hooked_pair():
    """A plain and a hooked Emu, with the handler fakes at the same addresses as above."""
    if not _hooked:
        plain, hooked = Emu(), Emu()
        hooked.install_hooks()
        for e in (plain, hooked):
            for fn in (line_handler, point_handler, lambda e, log: None, lambda e, log: None):
                pyfake(e, new_code(e), fn)
        _hooked.extend([plain, hooked])
    return _hooked


@pytest.mark.parametrize("seed", range(12))
def test_coll_lines_hooked(seed):
    plain, hooked = hooked_pair()
    LOG[:] = [[]]

    def setup(e):
        r = random.Random(seed)
        count = r.choice([1, 2, 5])
        t = terrain(e, r)
        starts, ends = segments(r, count, seed % 3 == 0)
        return t, e.alloc(0x70 * count), put(e, vecs(*starts)), put(e, vecs(*ends)), count, 0, 0, 0x80000003
    compare_hooked(plain, hooked, 0x4CC2A0, setup=setup, check_eax=False)
