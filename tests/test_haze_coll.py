"""BoneDynCubeObj_LineHit (0x4D7620): segments against a bone object's skinned collision mesh."""
import math
import random
import struct

import pytest
from unicorn import UC_HOOK_CODE

from emu import Emu, compare

LINE_HIT = 0x4D7620
COLL_LINE = 0x4D88D0
SUB_4BD070 = 0x4BD070
SLOT12_HANDLER = 0x4C35D0  # *arg = obj != arg[1]
HANDLERS = 0xA96AE0
WORLD = 0xA44344
TYPE = 0x35

e = Emu()
calls = []
reject = {"mode": 0}


def _sub_4bd070(uc, addr, size, user):
    args = [e.arg(i) for i in range(4)]
    calls.append(tuple(args))
    mode = reject["mode"]
    r = 0 if not mode else (args[1] >> 3) % mode == 0
    e.ret(int(r))


e.uc.hook_add(UC_HOOK_CODE, _sub_4bd070, begin=SUB_4BD070, end=SUB_4BD070)
_tramps = {}


def with_cw(target, cw):
    addr = e.symbols[target] if isinstance(target, str) else target
    if (addr, cw) not in _tramps:
        p = e.fake_top
        e.fake_top += 32
        e.write(p, struct.pack("<H", cw) + b"\0\0" + b"\xd9\x2d" + struct.pack("<I", p) + b"\xe9" +
                struct.pack("<i", addr - (p + 15)))
        _tramps[(addr, cw)] = p + 4
    return _tramps[(addr, cw)]


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def rotation(rng):
    a, b, c, d = (rng.gauss(0, 1) for _ in range(4))
    n = math.sqrt(a * a + b * b + c * c + d * d)
    a, b, c, d = a / n, b / n, c / n, d / n
    return [a * a + b * b - c * c - d * d, 2 * (b * c - a * d), 2 * (b * d + a * c),
            2 * (b * c + a * d), a * a - b * b + c * c - d * d, 2 * (c * d - a * b),
            2 * (b * d - a * c), 2 * (c * d + a * b), a * a - b * b - c * c + d * d]


def normal(vs):
    a = [vs[1][i] - vs[0][i] for i in range(3)]
    b = [vs[2][i] - vs[0][i] for i in range(3)]
    n = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
    ln = math.sqrt(sum(x * x for x in n)) or 1.0
    return [x / ln for x in n]


class Scene:
    """A random bone object, skinned collision mesh and line query in emulator memory."""

    def __init__(self, rng, ntri=24, nbones=3):
        self.rng = rng
        size = rng.choice([1.0, 5.0, 40.0])
        self.size = size
        self.bones = []
        for _ in range(nbones):
            m = rotation(rng) if rng.random() < 0.9 else [rng.uniform(-1.5, 1.5) for _ in range(9)]
            self.bones.append((m, [rng.uniform(-size, size) for _ in range(3)]))
        pts, tris = [], []
        for _ in range(ntri):
            kind = rng.random()
            c = [rng.uniform(-size, size) for _ in range(3)]
            if kind < 0.15:
                ax = rng.randrange(3)
                vs = []
                for _ in range(3):
                    p = [c[i] + rng.uniform(-size, size) / 2 for i in range(3)]
                    p[ax] = c[ax]
                    vs.append(p)
            elif kind < 0.2 and pts:
                p = [c[i] + rng.uniform(-1, 1) for i in range(3)]
                vs = [p, list(p), [c[i] + rng.uniform(-1, 1) for i in range(3)]]
            else:
                vs = [[c[i] + rng.uniform(-size, size) / 2 for i in range(3)] for _ in range(3)]
            base = len(pts)
            pts += vs
            tris.append([base, base + 1, base + 2])
            if rng.random() < 0.5:
                # a near copy, maybe flipped, to land within 0.1 of the first
                n = normal(vs)
                off = rng.choice([0.0, 0.03, -0.05, 0.08, 0.12, -0.2]) * (size if size < 2 else 1)
                ws = [[v[i] + n[i] * off for i in range(3)] for v in vs]
                if rng.random() < 0.5:
                    ws = [ws[0], ws[2], ws[1]]
                base = len(pts)
                pts += ws
                tris.append([base, base + 1, base + 2])
        rng.shuffle(tris)
        # bone per vertex, mostly in runs; rest positions so that skinning gives back the point
        self.vbone, self.rest, self.skinned = [], [], []
        b = 0
        for p in pts:
            if rng.random() < 0.4:
                b = rng.randrange(nbones)
            m, bp = self.bones[b]
            if rng.random() < 0.9 and abs(m[0] * m[0] + m[3] * m[3] + m[6] * m[6] - 1) < 1e-6:
                rest = [sum(m[3 * r + k] * (p[r] - bp[r]) for r in range(3)) for k in range(3)]
            else:
                rest = [p[k] - bp[k] for k in range(3)]
            self.vbone.append(b)
            self.rest.append(rest)
            self.skinned.append([bp[r] + sum(m[3 * r + k] * rest[k] for k in range(3)) for r in range(3)])
        self.tris = tris
        nfaces = 4
        self.mats = [rng.randrange(0, 6) if rng.random() < 0.7 else 0xFFFF for _ in range(nfaces)]
        self.tri_face = [rng.randrange(nfaces) for _ in tris]

    def write(self, em, cfg):
        rng = random.Random(cfg["seed"])
        nv = len(self.rest)
        verts = em.alloc(16 * nv + 16)
        for i, v in enumerate(self.rest):
            if cfg.get("nan") and rng.random() < 0.15:
                v = list(v)
                v[rng.randrange(3)] = math.nan
            em.write(verts + 16 * i, f32(*v) + struct.pack("<i", self.vbone[i]))
        tris = em.alloc(8 * len(self.tris) + 8)
        for i, t in enumerate(self.tris):
            em.write(tris + 8 * i, struct.pack("<4H", *t, self.tri_face[i]))
        faces = em.alloc(12 * len(self.mats))
        for i, mat in enumerate(self.mats):
            em.write(faces + 12 * i, struct.pack("<ihhHH", 7, -1, 0, mat, 0))
        meshes = em.alloc(0x20 * 3)
        mesh = meshes + 0x20 * cfg["mesh"]
        em.write(mesh, struct.pack("<8i", cfg.get("tri_count", len(self.tris)), cfg.get("vert_count", nv), 0, 0, tris, verts, faces, 0))
        data = em.alloc(0x80)
        em.write(data + 0x28, struct.pack("<II", cfg.get("data_unk28", 0), 0 if cfg.get("no_coll") else meshes))
        part = em.alloc(0x10)
        em.write(part + 0xC, struct.pack("<I", data))
        lod = em.alloc(0x10)
        em.write(lod + 0xC, struct.pack("<I", part))
        lods = em.alloc(8)
        em.write(lods, struct.pack("<II", lod, 0))

        bones = em.alloc(0x84 * len(self.bones))
        for i, (m, bp) in enumerate(self.bones):
            em.write(bones + 0x84 * i, struct.pack("<i", i) + bytes(range(0x14)) + f32(*bp) + f32(*m) +
                     struct.pack("<I", 0x13572468))

        world = em.alloc(0xD3C + 4 * 8)
        for i in range(8):
            em.write(world + 0xD3C + 4 * i, struct.pack("<I", 0x5A000000 + i * 0x100))
        em.write(WORLD, struct.pack("<I", world))
        obj = em.alloc(0xD8)
        em.write(obj + 0x1C, struct.pack("<H", TYPE))
        pos = cfg["pos"]
        em.write(obj + 0x20, f64(*pos))
        em.write(obj + 0x70, struct.pack("<II", bones, 0 if cfg.get("no_model") else lods))
        em.write(obj + 0x78, f32(*cfg["matrix"]))
        em.write(obj + 0xA0, f32(cfg["radius"]))

        # segments aimed at points of the skinned mesh
        n = cfg["count"]
        starts, ends = em.alloc(24 * max(n, 1)), em.alloc(24 * max(n, 1))
        results = em.alloc(0x70 * max(n, 1))
        sk = self.skinned
        for i in range(n):
            t = self.tris[rng.randrange(len(self.tris))]
            u, v = rng.random(), rng.random()
            if u + v > 1:
                u, v = 1 - u, 1 - v
            p = [sk[t[0]][k] + u * (sk[t[1]][k] - sk[t[0]][k]) + v * (sk[t[2]][k] - sk[t[0]][k]) for k in range(3)]
            w = [pos[j] + p[j] for j in range(3)]
            d = [rng.gauss(0, 1) for _ in range(3)]
            dn = math.sqrt(sum(x * x for x in d))
            k = rng.random()
            if k < 0.1:
                d = [0.0, 0.0, -1.0]
                dn = 1.0
            span = self.size * 3
            a = rng.uniform(0.5, 2) * span
            b = rng.uniform(-0.3, 1.5) * span
            s = [w[j] + d[j] / dn * a for j in range(3)]
            en = [w[j] - d[j] / dn * b for j in range(3)]
            if k > 0.93:
                # miss the bounding sphere along one axis
                ax = rng.randrange(3)
                sign = rng.choice([-1, 1])
                off = sign * (cfg["radius"] + span * 4)
                s[ax] = pos[ax] + off
                en[ax] = pos[ax] + off * rng.uniform(1, 2)
            if k > 0.97:
                s, en = en, s
            if cfg.get("box_miss"):
                # on a line through the sphere centre, but wholly past it along one axis
                ax, sign = cfg["box_miss"]
                s, en = list(pos), list(pos)
                s[ax] += sign * (cfg["radius"] + 5)
                en[ax] += sign * (cfg["radius"] + 10)
            if cfg.get("degenerate") and rng.random() < 0.3:
                en = list(s)
            if cfg.get("nan") and rng.random() < 0.2:
                (s if rng.random() < 0.5 else en)[rng.randrange(3)] = math.nan
            em.write(starts + 24 * i, f64(*s))
            em.write(ends + 24 * i, f64(*en))
            if rng.random() < cfg.get("prior", 0.0):
                # an earlier hit somewhere along the segment
                f = rng.uniform(-0.1, 1.1)
                hp = [s[j] + (en[j] - s[j]) * f for j in range(3)]
                mat = rng.choice([0, 0x77000000])
                em.write(results + 0x70 * i + 8, f64(*hp))
                em.write(results + 0x70 * i + 0x4C, struct.pack("<II", mat, 0x66000000))
                found = rng.choice([1, 1, 1, 0x100])
                em.write(results + 0x70 * i + 0x58, struct.pack("<I", found))
                em.write(results + 0x70 * i + 0x60, f64(rng.uniform(0, 2) * span * f))
            else:
                em.write(results + 0x70 * i, b"\x11" * 0x58 + b"\0" * 4 + b"\x22" * 0x14)

        arg = em.alloc(8)
        em.write(arg, struct.pack("<II", 0x99, obj if cfg.get("slot_reject") else 0x1234))
        if cfg.get("slot") == 12:
            em.write(HANDLERS + 4 * (12 * 0x180 + TYPE), struct.pack("<I", SLOT12_HANDLER))
        reject["mode"] = cfg.get("reject", 0)
        if cfg.get("sight"):
            # slot 8 query: one segment, world materials are float pointers
            vis = em.alloc(4 * 8)
            em.write(vis, f32(0.5, 0.0, 1.0, 0.25, 0.8, -0.0, 0.6, 2.0))
            sw = em.alloc(0xD3C + 4 * 8)
            for i in range(8):
                em.write(sw + 0xD3C + 4 * i, struct.pack("<I", vis + 4 * ((i + cfg["seed"]) % 8)))
            q = em.alloc(0x1C)
            em.write(q, struct.pack("<2IB3x2IB3xf", starts, ends, cfg.get("slot", 0xFF), arg, sw,
                                    cfg.get("blocked", 0), cfg.get("visibility", 1.0)))
            return obj, q
        q = em.alloc(0x24)
        em.write(q, struct.pack("<3IB3x5I", starts, ends, n, cfg.get("slot", 0xFF), arg, cfg["mesh"],
                                0x31323334, 0x41424344, results))
        return obj, q


def config(seed, **over):
    rng = random.Random(seed * 7919 + 13)
    cfg = {
        "seed": seed,
        "mesh": rng.randrange(3),
        "pos": [rng.uniform(-5000, 5000) for _ in range(3)],
        "matrix": rotation(rng) if rng.random() < 0.85 else [rng.uniform(-1.5, 1.5) for _ in range(9)],
        "radius": rng.choice([1000.0, 1000.0, 60.0]),
        "count": rng.randrange(4, 12),
        "prior": rng.choice([0.0, 0.5, 1.0]),
        "reject": rng.choice([0, 0, 0, 2, 3]),
    }
    cfg.update(over)
    return cfg


def run(scene, cfg, cw, orig=LINE_HIT, ours="BoneDynCubeObj_LineHit"):
    """Original against ours from the same scene; the 0x4BD070 calls must match too."""
    logs = []

    def setup(em):
        logs.append(list(calls))
        calls.clear()
        return scene.write(em, cfg)

    compare(e, with_cw(orig, cw), with_cw(ours, cw), setup=setup, check_eax=False)
    assert logs[1] == calls, "0x4BD070 calls differ:\n orig %s\n ours %s" % (logs[1], calls)
    return list(calls)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(40))
def test_line_hit(seed, cw):
    rng = random.Random(seed)
    scene = Scene(rng, ntri=rng.choice([6, 16, 30]), nbones=rng.choice([1, 3, 6]))
    run(scene, config(seed), cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_line_hit_near_copies(seed, cw):
    rng = random.Random(1000 + seed)
    scene = Scene(rng, ntri=40)
    run(scene, config(1000 + seed, prior=0.7, count=16, reject=0), cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
def test_line_hit_nan(seed, cw):
    rng = random.Random(2000 + seed)
    scene = Scene(rng, ntri=30)
    run(scene, config(2000 + seed, nan=1, count=16), cw)


@pytest.mark.parametrize("over", [
    {"no_model": 1}, {"data_unk28": 1}, {"no_coll": 1, "mesh": 0}, {"tri_count": 0}, {"tri_count": -1},
    {"vert_count": 0, "count": 0}, {"vert_count": -2, "count": 0},
    {"count": 0}, {"slot": 12}, {"slot": 12, "slot_reject": 1}, {"slot": 5},
    {"radius": 0.01}, {"radius": 0.0}, {"degenerate": 1},
])
def test_line_hit_paths(over):
    rng = random.Random(5)
    scene = Scene(rng, ntri=12)
    run(scene, config(5, **over), 0x37F)


def sight(scene, seed, cw, **over):
    cfg = config(seed, sight=1, mesh=0, count=1, prior=0.0, blocked=seed % 2,
                 visibility=[1.0, 0.75, 0.0][seed % 3], **over)
    return run(scene, cfg, cw, COLL_LINE, "BoneDynCubeObj_CollLine")


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(60))
def test_coll_line(seed, cw):
    rng = random.Random(3000 + seed)
    scene = Scene(rng, ntri=rng.choice([6, 16, 30]), nbones=rng.choice([1, 3, 6]))
    sight(scene, 3000 + seed, cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
def test_coll_line_nan(seed, cw):
    rng = random.Random(4000 + seed)
    scene = Scene(rng, ntri=30)
    sight(scene, 4000 + seed, cw, nan=1)


@pytest.mark.parametrize("over", [
    {"no_model": 1}, {"data_unk28": 1}, {"no_coll": 1}, {"tri_count": 0}, {"tri_count": -1},
    {"vert_count": 0, "radius": 0.0}, {"slot": 12}, {"slot": 12, "slot_reject": 1}, {"slot": 5},
    {"radius": 0.01}, {"radius": 0.0}, {"degenerate": 1}, {"reject": 2},
])
def test_coll_line_paths(over):
    rng = random.Random(6)
    scene = Scene(rng, ntri=12)
    for seed in range(6):
        sight(scene, 60 + seed, 0x37F, **over)


@pytest.mark.parametrize("sign", [-1, 1])
@pytest.mark.parametrize("axis", range(3))
def test_box_miss(axis, sign):
    rng = random.Random(7)
    scene = Scene(rng, ntri=12)
    sight(scene, 70, 0x37F, box_miss=(axis, sign))
    run(scene, config(70, box_miss=(axis, sign), count=2), 0x37F)
