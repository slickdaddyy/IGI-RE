import random
import struct

import pytest

from emu import Emu, compare

emu = Emu()

RESET_MODS = 0x4C5810
SEND_EVENT = 0x4CE090
SEND_EVENT_ALL = 0x4CE390
LEAF_SIDE = 0x4C9020
LINE_HIT = 0x4CC810
LINES_VERTICAL = 0x4C7770
LINES_SLANTED = 0x4C8130
LINES_EX = 0x4CDB20
ENTER_HANDLER = 0x401010
LEAVE_HANDLER = 0x401020

WALK_TASKS = 0xB97D20
HOLE_LIST = 0xB97DA0
HOLE_COUNT = 0xB97DF8
BUMPS = 0xBA2030
BUMP_COUNT = 0xBA2058
HOLES = 0xBA5080
MATERIAL_IDS = 0xC28FE0

# control words for the stubs below; this part of the fake area is never compared
CTRL = emu.fake_top + 0x8000
_ctrl_next = [CTRL]


def cell():
    p = _ctrl_next[0]
    _ctrl_next[0] += 4
    return p


def set_cell(e, p, v):
    e.write(p, struct.pack("<I", v & 0xFFFFFFFF))


def fake_writes(e, addr, nargs, ret, writes):
    """Fake addr (logging like emu.fake), and when the enable cell is set, first store values.
    writes: ("arg", i, off, cell, size), ("deref", i, off1, off, cell, size) or ("abs", address, cell)."""
    if addr not in e.fake_ret_at:
        e.fake(addr, ret, nargs)
    # addr holds a jmp to the fake (or to earlier writes for it); chain in front of it
    rel = struct.unpack("<i", e.read(addr + 1, 4))[0]
    target = addr + 5 + rel
    enable = cell()
    body = b""
    for w in writes:
        if w[0] == "abs":
            body += b"\x8b\x0d" + struct.pack("<I", w[2]) + b"\x89\x0d" + struct.pack("<I", w[1])
            continue
        body += b"\x8b\x44\x24" + bytes([4 + 4 * w[1]])
        if w[0] == "deref":
            body += b"\x8b\x80" + struct.pack("<I", w[2])
            w = ("arg", w[1]) + w[3:]
        off, src, size = w[2], w[3], w[4]
        body += b"\x8b\x0d" + struct.pack("<I", src)
        body += (b"\x88\x88" if size == 1 else b"\x89\x88") + struct.pack("<I", off)
    code = b"\x8b\x0d" + struct.pack("<I", enable) + b"\x85\xc9\x0f\x84" + struct.pack("<i", len(body)) + body
    p = e.fake_top
    code += b"\xe9" + struct.pack("<i", target - (p + len(code) + 5))
    e.fake_top += (len(code) + 15) & ~15
    e.write(p, code)
    e.write(addr, b"\xe9" + struct.pack("<i", p - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)
    return enable


# sub_4C9020 writes the side (0 or 1) through its fifth argument
SIDE = cell()
SIDE_ON = fake_writes(emu, LEAF_SIDE, 6, 1, [("arg", 4, 0, SIDE, 4)])
set_cell(emu, SIDE_ON, 1)

# a handler may clear query->active[n] or fill query->results[n]
ZERO = cell()
SEND_ALL_ACTIVE = fake_writes(emu, SEND_EVENT_ALL, 5, 0, [("arg", 2, 0xC + 1, ZERO, 1)])
RESULT_PTR = cell()
SEND_ALL_RESULT = fake_writes(emu, SEND_EVENT_ALL, 5, 0, [("deref", 2, 8, 0, RESULT_PTR, 4)])
HOLES_N = cell()
SEND_ALL_HOLES = fake_writes(emu, SEND_EVENT_ALL, 5, 0, [("abs", HOLE_COUNT, HOLES_N)])

SEND_ACTIVE = fake_writes(emu, SEND_EVENT, 6, 0, [("arg", 2, 0xC, ZERO, 1)])

# line hit: fills the record it is given
HIT_WORDS = [cell() for _ in range(28)]
LINE_HIT_ON = fake_writes(emu, LINE_HIT, 5, 1, [("arg", 0, 8 + 4 * i, HIT_WORDS[i], 4) for i in range(26)])
set_cell(emu, LINE_HIT_ON, 1)

# line handlers mark results[0] found, with a position
PRE_WORDS = [cell() for _ in range(6)]
LINES_FOUND = cell()
PRE_ON = fake_writes(emu, LINES_VERTICAL, 7, 0,
                     [("deref", 2, 0x20, 0x58, LINES_FOUND, 4)] + [("deref", 2, 0x20, 8 + 4 * i, PRE_WORDS[i], 4)
                                                                    for i in range(6)])
PRE_ON2 = fake_writes(emu, LINES_SLANTED, 8, 0,
                      [("deref", 2, 0x20, 0x58, LINES_FOUND, 4)] + [("deref", 2, 0x20, 8 + 4 * i, PRE_WORDS[i], 4)
                                                                     for i in range(6)])
emu.fake(LINES_EX, 0, 9)
emu.fake(ENTER_HANDLER, 0, 2)
emu.fake(LEAVE_HANDLER, 0, 2)
set_cell(emu, ZERO, 0)
set_cell(emu, LINES_FOUND, 1)
emu.baseline = emu.snapshot()


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
        e.write(MATERIAL_IDS + 3600 * i, bytes([rng.randrange(1, 41)]))
    return t


def make_objects(e, rng, n=12):
    """Object octree nodes with a few tasks each; returns the root."""
    for ty in range(4):
        e.write(0xA9C4E0 + 4 * ty, struct.pack("<I", ENTER_HANDLER))
        e.write(0xA9CAE0 + 4 * ty, struct.pack("<I", LEAVE_HANDLER))
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
