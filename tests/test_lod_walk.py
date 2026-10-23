"""QCube_Walk: random terrain cell trees, object octrees, cameras and cache states."""
import math
import random
import struct

import pytest

from emu import Emu
from test_lod_util import CTX, TASK_HANDLERS, Fakes, check, check_error, f32, f64, u32, with_cw

e = Emu()
fk = Fakes(e)

GWORLD, FRAME, ORIGIN, ORDER, MINSIZE = 0xA44344, 0xC10380, 0xB12BC0, 0xB12BE0, 0xB16C6C
OUT, COUNT = 0xB12C58, 0xB12C20
CACHE, POOLS = 0xB04340, (0xB12B60, 0xB12C40, 0xB16CC0)
SKIP = 0xB12BB8
LIGHTS, LIGHTMAPS, BUMPS, HOLES = 0xB97D24, 0xB97B20, 0xBA2058, 0xBA5060
TYPES = (0x60, 0x61, 0x62)


def skip_cells(em, a):
    if em.u32(a[0] + 0x40) & 1:
        u32(em, SKIP, 1)
    return 0


H15 = fk.func(2, ret=skip_cells, name="h15")
H16 = fk.func(2, name="h16")
for t_ in TYPES:
    u32(e, TASK_HANDLERS + 4 * (15 * 0x180 + t_), H15)
    u32(e, TASK_HANDLERS + 4 * (16 * 0x180 + t_), H16)
e.baseline = e.snapshot()


def child_order():
    out = bytearray()
    for r in range(8):
        for j in range(8):
            out.append(((j + r) & 3) + (j & 4) if r < 4 else ((r - j) & 3) + (~j & 4))
    return bytes(out)


def rotation(rng):
    q = [rng.gauss(0, 1) for _ in range(4)]
    n = math.sqrt(sum(x * x for x in q))
    w, x, y, z = (v / n for v in q)
    return [1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
            2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
            2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]


def terrain(em, rng, budget, depth):
    """Cell records; record 1 is the root."""
    recs = [None, None]
    queue = [(1, 0)]
    while queue:
        idx, d = queue.pop(0)
        mask, kids, subs = 0, [0] * 8, [0] * 8
        if d < depth:
            for c in range(8):
                if rng.random() < (0.6 if d < 3 else 0.35) and len(recs) < budget:
                    mask |= 1 << c
                    kids[c] = len(recs)
                    recs.append(None)
                    queue.append((kids[c], d + 1))
                subs[c] = rng.randrange(8)
        if rng.random() < 0.05:
            mask |= 1 << rng.randrange(8)  # a bit without its own child: index 0
        recs[idx] = struct.pack("<8H8BB3xi", *kids, *subs, mask, rng.randrange(1 << 20))
    recs[0] = bytes(0x20)
    p = em.alloc(0x20 * len(recs))
    em.write(p, b"".join(recs))
    return p


def near_terrain(em, rng, cam, origin, budget, depth, reach):
    """Cell records that follow the cubes around the camera, so the walk reaches deep cells."""
    table = struct.unpack("<64i", em.read(0x548060, 256))
    order = child_order()
    recs = [bytes(0x20), None]
    queue = [(1, [-o for o in origin], 0, 0)]
    while queue:
        idx, center, d, sel = queue.pop(0)
        mask, kids, subs = 0, [0] * 8, [rng.randrange(8) for _ in range(8)]
        half = (1 << 30) >> (d + 1)
        for i in range(8):
            c = table[sel * 8 + i]
            cc = [center[a] + (half if i >> a & 1 else -half) for a in range(3)]
            dist = max(abs(cc[a] - cam[a]) for a in range(3)) - half
            near = dist < reach * 2 * half or rng.random() < 0.03
            if d < depth and near and len(recs) < budget:
                mask |= 1 << c
                kids[c] = len(recs)
                recs.append(None)
                queue.append((kids[c], cc, d + 1, order[sel * 8 + subs[c]]))
        recs[idx] = struct.pack("<8H8BB3xi", *kids, *subs, mask, rng.randrange(1 << 20))
    p = em.alloc(0x20 * len(recs))
    em.write(p, b"".join(recs))
    return p


def tasks(em, rng):
    head = 0
    for _ in range(rng.randint(1, 3)):
        t = em.alloc(0x64)
        em.write(t + 0x1C, struct.pack("<H", rng.choice(TYPES)))
        u32(em, t + 0x40, rng.random() < 0.1)
        u32(em, t + 0x48, rng.randrange(1 << 16))
        u32(em, t + 0x58, head)
        em.write(t + 0x5C, struct.pack("<H", rng.choice([0, 1, 2, 3, 0x80, 0x81, 0x100, 0x102, 0x183])))
        u32(em, t + 0x60, rng.random() < 0.2)
        head = t
    return head


def cube(em, rng, parent, d, depth, counter):
    n = em.alloc(0x40)
    counter[0] += 1
    u32(em, n + 8, parent)
    mask = 0
    if d < depth:
        for c in range(8):
            if rng.random() < 0.4 and counter[0] < 300:
                mask |= 1 << c
                u32(em, n + 0xC + 4 * c, cube(em, rng, n, d + 1, depth, counter))
    u32(em, n + 0x2C, tasks(em, rng))
    em.write(n + 0x30, bytes([mask]))
    em.write(n + 0x38, struct.pack("<HH", rng.choice([0, 1, 2, 3, 0x80, 0x81, 0x100, 0x183, 0x103]),
                                   rng.choice([0, 0, 1, 0x100])))
    return n


def pool(em, base, n, stride):
    data = em.alloc(n * stride)
    for i in range(n):
        u32(em, data + i * stride + stride - 4, i)
    free = em.alloc(4 * n)
    em.write(free, struct.pack("<%di" % n, *range(n)))
    em.write(base, struct.pack("<6i", data, free, n, n, stride, stride - 4))
    return [data + i * stride for i in range(n)]


def scene(em, seed, kind):
    rng = random.Random(seed)
    w = em.alloc(0x28)
    gw = em.alloc(0xE00)
    u32(em, GWORLD, gw)
    nearz = em.alloc(8) if rng.random() < 0.7 else 0
    u32(em, gw + 0xDFC, nearz)
    near_off = rng.choice([-1e9, -3e6, -1e4, 0.0, 1e4, rng.uniform(-1e8, 1e8)])
    frame = rng.choice([0, 1, 2, 0x10000, rng.randrange(3, 0xFFFF), 0x12345])
    u32(em, FRAME, frame)
    origin = [rng.randrange(-1 << 26, 1 << 26) for _ in range(3)]
    em.write(ORIGIN, struct.pack("<3i", *origin))
    em.write(ORDER, child_order())
    u32(em, MINSIZE, rng.choice([1, 5, 20, 70, 300, rng.randrange(1, 2000)]))
    out = em.alloc(8 * 2000)
    u32(em, OUT, out)
    u32(em, COUNT, rng.randrange(5))
    u32(em, LIGHTS, 0x77)  # cleared by Coll_ResetMods
    f32(em, CTX, *rotation(rng))
    scale = rng.choice([1 << 22, 1 << 26, 1 << 29])
    pos = [rng.uniform(-scale, scale) for _ in range(3)]
    if rng.random() < 0.8:
        pos[2] = abs(pos[2]) - origin[2]
    if nearz:
        f64(em, nearz, pos[2] + near_off)
    f64(em, CTX + 0x28, *pos)
    f32(em, CTX + 0x40, rng.uniform(0.3, 1.6), rng.uniform(0.3, 1.2))
    if kind in ("terrain", "both"):
        if rng.random() < 0.25:
            u32(em, w + 0xC, terrain(em, rng, rng.choice([200, 800, 2000]), rng.choice([8, 12, 16, 18])))
        else:
            u32(em, w + 0xC, near_terrain(em, rng, [round(v) for v in pos], origin, 3000,
                                          rng.choice([9, 12, 16, 18]), rng.choice([1, 1.5, 2.5])))
    if kind in ("trees", "both"):
        counter = [0]
        for i in (0, 1, 2, 4, 5):
            if rng.random() < 0.8:
                u32(em, w + 0x10 + 4 * i, cube(em, rng, 0, 0, rng.choice([4, 8, 14]), counter))
    elems = [pool(em, b, 64, s) for b, s in zip(POOLS, (0x40, 0x50, 0x30))]
    em.write(CACHE, struct.pack("<4iiiI", 0, 0, 0, 0, -1, 0, 0) * 0x800)
    return w, rng, frame & 0xFFFF, elems


def noise(em, rng, cur, elems, taken):
    """Random cache entries that never match a visited cell."""
    prev = (cur - 1) & 0xFFFF
    for _ in range(rng.choice([0, 30, 300])):
        i = rng.randrange(0x800)
        if i in taken:
            continue
        taken.add(i)
        frame = rng.choice([cur, prev, (cur - 2) & 0xFFFF, (cur + 5) & 0xFFFF, 0x10000 | cur, -5])
        kind = rng.randrange(3)
        data = elems[kind].pop() if elems[kind] and rng.random() < 0.7 else 0
        em.write(CACHE + 0x1C * i, struct.pack("<4iiiI", 0x7FFFFFF0, i, 0, 0, frame, kind, data))


def visited(seed, kind, cw):
    """Cache slots the walk fills in when it starts from an empty cache."""
    e.reset()
    w, rng, cur, elems = scene(e, seed, kind)
    e.call(with_cw(e, fk, 0x4D14E0, cw), w)
    out = []
    for i in range(0x800):
        ent = struct.unpack("<4iiiI", e.read(CACHE + 0x1C * i, 0x1C))
        if ent[4] == cur and ent[5] == 3:
            out.append((i, ent))
    return out


def prepare(seed, kind, cw, mode):
    """mode: 'fresh' (empty cache plus noise), 'reuse' (cells cached last frame), or an error case."""
    found = visited(seed, kind, cw) if mode != "fresh" else []

    def setup(em):
        w, rng, cur, elems = scene(em, seed, kind)
        prev = (cur - 1) & 0xFFFF
        taken = set()
        r2 = random.Random(seed * 7 + 1)
        for n, (i, ent) in enumerate(found):
            taken.add(i)
            if mode == "reuse":
                if r2.random() < 0.6:
                    k = r2.randrange(3)
                    data = elems[k].pop() if elems[k] else 0x1234
                    em.write(CACHE + 0x1C * i, struct.pack("<4iiiI", *ent[:4], prev, k, data))
                else:
                    em.write(CACHE + 0x1C * i, struct.pack("<4iiiI", *ent[:4], (cur + 5) & 0xFFFF, 0, 0))
            elif mode == "nodata" and n == len(found) // 2:
                em.write(CACHE + 0x1C * i, struct.pack("<4iiiI", *ent[:4], prev, 0, 0))
            elif mode == "freedata":
                em.write(CACHE + 0x1C * i, struct.pack("<4iiiI", *ent[:4], -1, 0, 0x1234))
        if mode == "badkind":
            em.write(CACHE + 0x1C * 0x7FF, struct.pack("<4iiiI", 0x7FFFFFF0, 0, 0, 0, prev, 7, 0x1234))
            taken.add(0x7FF)
        if mode in ("fresh", "reuse"):
            noise(em, rng, cur, elems, taken)
        u32(em, LIGHTS, 0)
        return (w,)
    return setup, found


KINDS = ["terrain", "trees", "both", "none"]


@pytest.mark.parametrize("cw", [0x37F, 0x7F, 0xC7F, 0x77F])
@pytest.mark.parametrize("kind", KINDS)
@pytest.mark.parametrize("seed", range(20))
def test_walk(seed, kind, cw):
    setup, _ = prepare(seed, kind, cw, "fresh")
    check(e, fk, 0x4D14E0, "QCube_Walk", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F, 0xC7F, 0x77F])
@pytest.mark.parametrize("kind", ["terrain", "both"])
@pytest.mark.parametrize("seed", range(20))
def test_walk_reuse(seed, kind, cw):
    setup, found = prepare(seed, kind, cw, "reuse")
    check(e, fk, 0x4D14E0, "QCube_Walk", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("mode", ["nodata", "freedata", "badkind"])
@pytest.mark.parametrize("seed", range(4))
def test_walk_errors(seed, mode):
    setup, found = prepare(seed, "terrain", 0x37F, mode)
    if mode != "badkind" and not found:
        pytest.skip("no cached cell in this scene")
    check_error(e, fk, 0x4D14E0, "QCube_Walk", setup)
