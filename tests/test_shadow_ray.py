import math
import random
import struct

import pytest

from emu import Emu
from test_shadow_fakes import Fakes, check, f32, f64, i32, set_handler, u16

e = Emu()
fk = Fakes(e)
SCENE = {}
EVENT, OTYPE = 0x21, 0x44


def get_verts(em, a):
    return SCENE["bent"]


def skip(em, a):
    return 1 if (a[1] - SCENE["tris"]) // 8 in SCENE["skip"] else 0


def allow(em, a):
    em.write(a[1], bytes([SCENE["allow"]]))
    return 0


fk.add(0x49A050, 6, ret=get_verts, fmt=lambda em, a: (a[0], a[1], a[2], a[3], a[4], a[5]))
fk.add(0x4BD070, 4, ret=skip)
HANDLER = fk.add(fk.code(b"\xc3"), 2, ret=allow)
e.baseline = e.snapshot()
CWS = [0x37F, 0x7F]


def euler(a, b, c):
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    rx = [[1, 0, 0], [0, ca, -sa], [0, sa, ca]]
    ry = [[cb, 0, sb], [0, 1, 0], [-sb, 0, cb]]
    rz = [[cc, -sc, 0], [sc, cc, 0], [0, 0, 1]]

    def mul(p, q):
        return [[sum(p[i][k] * q[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    return mul(mul(rz, ry), rx)


def flat(m):
    return [m[i][j] for i in range(3) for j in range(3)]


def panel(rng, kind):
    """Two triangles of a quad: (verts, tris); kind 'axis' lies in a coordinate plane."""
    c = [rng.uniform(-30, 30) for _ in range(3)]
    s, t = rng.uniform(5, 25), rng.uniform(5, 25)
    if kind == "axis":
        k = rng.randrange(3)
        u = [0.0, 0.0, 0.0]
        w = [0.0, 0.0, 0.0]
        u[(k + 1) % 3] = 1.0
        w[(k + 2) % 3] = 1.0
        c = [float(round(x)) for x in c]
    else:
        m = euler(*[rng.uniform(-math.pi, math.pi) for _ in range(3)])
        u = [m[0][0], m[1][0], m[2][0]]
        w = [m[0][1], m[1][1], m[2][1]]
    pts = [[c[i] + a * s * u[i] + b * t * w[i] for i in range(3)] for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    return pts, [(0, 1, 2), (0, 2, 3)]


def build_mesh(rng, npanels):
    verts, tris, groups = [], [], []
    for p in range(npanels):
        pts, ts = panel(rng, rng.choice(["axis", "free", "free"]))
        base = len(verts)
        verts += pts
        first = len(tris)
        for a, b, c in ts:
            tris.append((base + a, base + b, base + c))
        r = rng.random()
        if r < 0.3:
            for a, b, c in ts:
                tris.append((base + a, base + c, base + b))
        elif r < 0.5:
            n = [0.0, 0.0, 0.0]
            e1 = [pts[1][i] - pts[0][i] for i in range(3)]
            e2 = [pts[3][i] - pts[0][i] for i in range(3)]
            n = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
            ln = math.sqrt(sum(x * x for x in n))
            off = rng.choice([0.05, -0.05, 0.3])
            b2 = len(verts)
            verts += [[q[i] + off * n[i] / ln for i in range(3)] for q in pts]
            for a, b, c in ts:
                tris.append((b2 + a, b2 + c, b2 + b) if rng.random() < 0.5 else (b2 + a, b2 + b, b2 + c))
        groups.append((first, len(tris) - first))
    return verts, tris, groups


def sphere(verts, tris, first, count, factor):
    pts = [verts[i] for t in tris[first:first + count] for i in t] or [[0.0, 0.0, 0.0]]
    c = [sum(p[i] for p in pts) / len(pts) for i in range(3)]
    r = max(math.sqrt(sum((p[i] - c[i]) ** 2 for i in range(3))) for p in pts)
    return c, r * factor


def build_nodes(rng, verts, tris, groups):
    """root (no triangles) -> groups as a chain of siblings; every second group gets the next one as child."""
    nodes = [None]
    links = []
    i = 0
    while i < len(groups):
        idx = len(nodes)
        nodes.append(groups[i])
        links.append(idx)
        i += 1
    # node: (first, count, child, next)
    out = [[0, 0, 0, 0] for _ in nodes]
    out[0] = [0, 0, 1 if len(nodes) > 1 else 0, 0]
    prev = None
    for k, idx in enumerate(links):
        first, count = nodes[idx]
        out[idx][0], out[idx][1] = first, count
        if prev is not None:
            if rng.random() < 0.35 and out[prev][2] == 0:
                out[prev][2] = idx
            else:
                out[prev][3] = idx
        prev = idx
    # some leaves keep only part of their triangles at the root
    if rng.random() < 0.5 and groups:
        out[0][0], out[0][1] = groups[0][0], 1
    res = []
    for k, (first, count, child, nxt) in enumerate(out):
        allt = (0, len(tris)) if k == 0 else (first, count)
        c, r = sphere(verts, tris, allt[0], allt[1], rng.choice([1.05, 1.05, 1.5, 0.4, 100.0] if k else [2.0, 0.1]))
        res.append((c, r, first, count, child, nxt))
    return res


def write_nodes(em, nodes):
    p = em.alloc(0x18 * len(nodes))
    for k, (c, r, first, count, child, nxt) in enumerate(nodes):
        em.write(p + 0x18 * k, f32(*c, r) + struct.pack("<4H", first, count, child, nxt))
    return p


def mat_apply(m, v):
    return [sum(m[i * 3 + j] * v[j] for j in range(3)) for i in range(3)]


def make_scene(seed, rigid, reversed_=0, nrays=12, radius=1000.0, event=0xFF, allow_=1, preset="mixed",
               nodecount=True, info28=0, lod=0, skipset=(), axis_rays=False, models=True, tricount=True):
    def setup(em):
        rng = random.Random(seed)
        verts, tris, groups = build_mesh(rng, rng.randrange(3, 7))
        nodes = build_nodes(rng, verts, tris, groups)
        bnodes = build_nodes(random.Random(seed + 99), verts, tris, groups)
        world = em.alloc(0xE00)
        for k in range(8):
            i32(em, world + 0xD3C + 4 * k, 0x1000 + k)
        vp = em.alloc(16 * len(verts))
        em.write(vp, b"".join(f32(*v, 7.0) for v in verts))
        bent = em.alloc(12 * len(verts))
        em.write(bent, b"".join(f32(*v) for v in verts))
        tp = em.alloc(8 * len(tris))
        info = em.alloc(12 * len(tris))
        for k, t in enumerate(tris):
            em.write(tp + 8 * k, struct.pack("<4H", *t, k))
            em.write(info + 12 * k + 8, struct.pack("<H", rng.choice([0, 2, 3, 5, 0xFFFF])))
        SCENE.update(bent=bent, tris=tp, skip=set(skipset), allow=allow_)
        parts = em.alloc(0x40)
        part = parts + 0x20 * (seed & 1)
        i32(em, part, len(tris) if tricount else 0)
        i32(em, part + 0xC, len(nodes) if nodecount else 0)
        em.write(part + 0x10, struct.pack("<4I", tp, vp, info, write_nodes(em, nodes)))
        lodinfo = em.alloc(0x40)
        i32(em, lodinfo + 0x28, info28)
        i32(em, lodinfo + 0x2C, parts)
        x = em.alloc(0x10)
        i32(em, x + 0xC, lodinfo)
        lm = em.alloc(0x10)
        i32(em, lm + 0xC, x)
        mods = em.alloc(8)
        i32(em, mods + 4 * max(lod, 0), lm)

        o = em.alloc(0x5C0)
        em.write(o, bytes(rng.randrange(256) for _ in range(0x5C0)))
        u16(em, o + 0x1C, OTYPE)
        pos = [rng.uniform(-2000, 2000) for _ in range(3)]
        em.write(o + 0x20, f64(*pos))
        i32(em, o + 0x68, world)
        i32(em, o + 0x6C, mods if models else 0)
        m = flat(euler(*[rng.uniform(-math.pi, math.pi) for _ in range(3)]))
        em.write(o + 0x70, f32(*m))
        em.write(o + 0x98, f32(rng.choice([1.0, 1.25, 0.8])))
        i32(em, o + 0xA4, lod)
        em.write(o + 0xF4, f32(radius))
        minx, maxx = rng.uniform(-60, -20), rng.uniform(20, 60)
        em.write(o + 0xFC, f32(minx, maxx))
        inv = flat(euler(*[rng.uniform(-math.pi, math.pi) for _ in range(3)]))
        em.write(o + 0x520, f32(*inv))
        bn = write_nodes(em, bnodes)
        em.write(o + 0x5A0, struct.pack("<4I", bn, len(bnodes), bn, len(bnodes)))
        em.write(o + 0x5B4, bytes([rigid, reversed_]))
        mid = (minx + maxx) * 0.5
        inv_t = [inv[j * 3 + i] for i in range(3) for j in range(3)]

        def to_world(p):
            if rigid:
                q = mat_apply(inv_t, [p[0] - mid, p[1], p[2]])
            else:
                q = p
            return [pos[i] + q[i] for i in range(3)]

        froms, tos = [], []
        for k in range(nrays):
            if axis_rays and k % 2:
                ax = rng.randrange(3)
                p = [float(round(rng.uniform(-30, 30))) for _ in range(3)]
                a, b = list(p), list(p)
                a[ax] -= 80.0
                b[ax] += 80.0
                if rigid:
                    a, b = to_world(a), to_world(b)
                else:
                    a = [pos[i] + a[i] for i in range(3)]
                    b = [pos[i] + b[i] for i in range(3)]
                    if k % 4 == 1:
                        b[(ax + 1) % 3] = a[(ax + 1) % 3]
                        b[(ax + 2) % 3] = a[(ax + 2) % 3]
            else:
                p = [rng.uniform(-35, 35) for _ in range(3)]
                d = [rng.gauss(0, 1) for _ in range(3)]
                ln = math.sqrt(sum(x * x for x in d)) or 1.0
                l1, l2 = rng.uniform(20, 90), rng.uniform(-10, 90)
                a = to_world([p[i] - d[i] / ln * l1 for i in range(3)])
                b = to_world([p[i] + d[i] / ln * l2 for i in range(3)])
            froms.append(a)
            tos.append(b)
        fp, tp2 = em.alloc(24 * nrays), em.alloc(24 * nrays)
        em.write(fp, b"".join(f64(*v) for v in froms))
        em.write(tp2, b"".join(f64(*v) for v in tos))
        hits = em.alloc(0x70 * nrays)
        em.write(hits, bytes(rng.randrange(256) for _ in range(0x70 * nrays)))
        for k in range(nrays):
            h = hits + 0x70 * k
            mode = preset if preset != "mixed" else rng.choice(["none", "far", "near", "odd"])
            if mode == "none":
                i32(em, h + 0x58, 0)
            else:
                hp = [froms[k][i] + (tos[k][i] - froms[k][i]) * rng.uniform(0.0, 1.2) for i in range(3)]
                em.write(h + 8, f64(*hp))
                i32(em, h + 0x4C, rng.choice([0, 0, 5]))
                i32(em, h + 0x58, 0x100 if mode == "odd" else 1)
                em.write(h + 0x60, f64(1e9 if mode == "far" else rng.uniform(0, 80)))
        allowp = em.alloc(4)
        em.write(allowp, b"\x07")
        q = em.alloc(0x24)
        em.write(q, struct.pack("<3I", fp, tp2, nrays) + bytes([event, 0, 0, 0]) +
                 struct.pack("<2I", allowp, seed & 1) + b"\0" * 8 + struct.pack("<I", hits))
        set_handler(em, EVENT, OTYPE, HANDLER if event != 0x22 else 0)
        return o, q
    return setup


def run(setup, cw):
    check(e, fk, 0x4E16E0, "SplineObjSegment_Hit", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(30))
@pytest.mark.parametrize("reversed_", [0, 1])
def test_bent(cw, seed, reversed_):
    run(make_scene(seed, 0, reversed_), cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(30))
@pytest.mark.parametrize("reversed_", [0, 1])
def test_rigid(cw, seed, reversed_):
    run(make_scene(seed + 1000, 1, reversed_), cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(10))
@pytest.mark.parametrize("rigid", [0, 1])
def test_axis(cw, seed, rigid):
    run(make_scene(seed + 2000, rigid, seed & 1, axis_rays=True, preset="none"), cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("rigid", [0, 1])
@pytest.mark.parametrize("preset", ["none", "far", "near"])
def test_presets(cw, rigid, preset):
    run(make_scene(3000 + rigid, rigid, 0, preset=preset, skipset=(1, 4)), cw)


@pytest.mark.parametrize("rigid", [0, 1])
@pytest.mark.parametrize("case", ["models", "lod", "info", "tris", "nodes", "radius", "small",
                                  "allow", "deny", "nohandler"])
def test_cases(rigid, case):
    kw = {"models": dict(models=False), "lod": dict(lod=-1), "info": dict(info28=1), "tris": dict(tricount=False),
          "nodes": dict(nodecount=False), "radius": dict(radius=20.0), "small": dict(radius=45.0),
          "allow": dict(event=EVENT, allow_=1), "deny": dict(event=EVENT, allow_=0),
          "nohandler": dict(event=0x22)}[case]
    run(make_scene(4000 + len(case), rigid, **kw), 0x37F)
