"""RigidDynCubeObj_LineHit (0x4C1840): segments against a rigid object's collision mesh."""
import math
import random
import struct

import pytest
from unicorn import UC_HOOK_CODE

from emu import Emu, compare

LINE_HIT = 0x4C1840
SUB_4BD070 = 0x4BD070
SLOT12_HANDLER = 0x4C35D0  # *arg = obj != arg[1]
HANDLERS = 0xA96AE0
TYPE = 0x33

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


class Scene:
    """A random object, collision mesh and line query in emulator memory."""

    def __init__(self, rng, nseg=8, ntri=24):
        self.rng = rng
        size = rng.choice([1.0, 5.0, 40.0])
        self.size = size
        verts, tris = [], []
        for _ in range(ntri):
            kind = rng.random()
            c = [rng.uniform(-size, size) for _ in range(3)]
            if kind < 0.15:
                # axis aligned
                ax = rng.randrange(3)
                vs = []
                for _ in range(3):
                    p = [c[i] + rng.uniform(-size, size) / 2 for i in range(3)]
                    p[ax] = c[ax]
                    vs.append(p)
            elif kind < 0.2 and verts:
                # degenerate
                p = [c[i] + rng.uniform(-1, 1) for i in range(3)]
                vs = [p, list(p), [c[i] + rng.uniform(-1, 1) for i in range(3)]]
            else:
                vs = [[c[i] + rng.uniform(-size, size) / 2 for i in range(3)] for _ in range(3)]
            base = len(verts)
            verts += vs
            tris.append([base, base + 1, base + 2])
            if rng.random() < 0.5:
                # a near copy, maybe flipped, to land within 0.1 of the first
                n = self.normal(vs)
                off = rng.choice([0.0, 0.03, -0.05, 0.08, 0.12, -0.2]) * (size if size < 2 else 1)
                ws = [[v[i] + n[i] * off for i in range(3)] for v in vs]
                if rng.random() < 0.5:
                    ws = [ws[0], ws[2], ws[1]]
                base = len(verts)
                verts += ws
                tris.append([base, base + 1, base + 2])
        rng.shuffle(tris)
        self.verts, self.tris = verts, tris
        nfaces = 4
        self.faces = [(rng.choice([-1, -1, 0, 3]), rng.randrange(0, 6) if rng.random() < 0.7 else 0xFFFF)
                      for _ in range(nfaces)]
        self.tri_face = [rng.randrange(nfaces) for _ in tris]
        self.build_tree()

    @staticmethod
    def normal(vs):
        a = [vs[1][i] - vs[0][i] for i in range(3)]
        b = [vs[2][i] - vs[0][i] for i in range(3)]
        n = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
        ln = math.sqrt(sum(x * x for x in n)) or 1.0
        return [x / ln for x in n]

    def sphere(self, first, count):
        pts = [self.verts[v] for t in self.tris[first:first + count] for v in t] or [[0.0, 0.0, 0.0]]
        c = [sum(p[i] for p in pts) / len(pts) for i in range(3)]
        r = max(math.sqrt(sum((p[i] - c[i]) ** 2 for i in range(3))) for p in pts)
        k = self.rng.random()
        if k < 0.15:
            r *= self.rng.uniform(0.1, 0.9)
        elif k < 0.25:
            r = r * 3 + 1
        return c, r

    def build_tree(self):
        rng = self.rng
        nodes = []

        def make(first, count, depth):
            idx = len(nodes)
            nodes.append(None)
            kids = []
            own = 0
            if depth < 3 and count > 2 and rng.random() < 0.8:
                own = rng.randrange(0, 2) if depth else 0
                rest = count - own
                cuts = sorted(rng.sample(range(1, rest), min(rest - 1, rng.randrange(1, 4))))
                bounds = [0] + cuts + [rest]
                for a, b in zip(bounds, bounds[1:]):
                    kids.append(make(first + own + a, b - a, depth + 1))
            else:
                own = count
            c, r = self.sphere(first, count)
            nodes[idx] = [c, r, first, own, kids[0] if kids else 0, 0]
            for a, b in zip(kids, kids[1:]):
                nodes[a][5] = b
            return idx

        make(0, len(self.tris), 0)
        if rng.random() < 0.2:
            # extra triangle range on the root's sibling
            nodes[0][5] = len(nodes)
            c, r = self.sphere(0, len(self.tris))
            nodes.append([c, r, 0, len(self.tris) // 2, 0, 0])
        self.nodes = nodes

    def write(self, em, cfg):
        rng = random.Random(cfg["seed"])
        verts = em.alloc(16 * len(self.verts) + 16)
        for i, v in enumerate(self.verts):
            em.write(verts + 16 * i, f32(*v) + struct.pack("<I", 0xABCD0000 + i))
        tris = em.alloc(8 * len(self.tris) + 8)
        for i, t in enumerate(self.tris):
            em.write(tris + 8 * i, struct.pack("<4H", *t, self.tri_face[i]))
        faces = em.alloc(12 * len(self.faces))
        for i, (flag, mat) in enumerate(self.faces):
            em.write(faces + 12 * i, struct.pack("<ihhHH", 7, flag, 0, mat, 0))
        nodes = em.alloc(0x18 * len(self.nodes))
        for i, (c, r, first, count, child, sib) in enumerate(self.nodes):
            em.write(nodes + 0x18 * i, f32(*c, r) + struct.pack("<4H", first, count, child, sib))
        meshes = em.alloc(0x20 * 3)
        mesh = meshes + 0x20 * cfg["mesh"]
        em.write(mesh, struct.pack("<8I", cfg.get("mesh_unk0", 1), 0, 0, cfg.get("node_count", len(self.nodes)),
                                   tris, verts, faces, nodes))
        data = em.alloc(0x80)
        em.write(data + 0x28, struct.pack("<II", cfg.get("data_unk28", 0), 0 if cfg.get("no_coll") else meshes))
        em.write(data + 0x78, f32(cfg["radius"]))
        part = em.alloc(0x10)
        em.write(part + 0xC, struct.pack("<I", data))
        lod = em.alloc(0x10)
        em.write(lod + 0xC, struct.pack("<I", part))
        lods = em.alloc(8)
        em.write(lods, struct.pack("<II", lod, lod))

        world = em.alloc(0xD3C + 4 * 8)
        for i in range(8):
            em.write(world + 0xD3C + 4 * i, struct.pack("<I", 0x5A000000 + i * 0x100))
        obj = em.alloc(0xF0)
        em.write(obj + 0x1C, struct.pack("<H", TYPE))
        pos = cfg["pos"]
        em.write(obj + 0x20, f64(*pos))
        em.write(obj + 0x68, struct.pack("<II", world, 0 if cfg.get("no_lods") else lods))
        m = cfg["matrix"]
        em.write(obj + 0x70, f32(*m))
        sc = cfg["scale"]
        em.write(obj + 0x98, f32(*sc) + struct.pack("<i", cfg.get("lod", 1)))

        # segments aimed at points of the mesh
        n = cfg["count"]
        starts, ends = em.alloc(24 * max(n, 1)), em.alloc(24 * max(n, 1))
        results = em.alloc(0x70 * max(n, 1))
        for i in range(n):
            t = self.tris[rng.randrange(len(self.tris))]
            u, v = rng.random(), rng.random()
            if u + v > 1:
                u, v = 1 - u, 1 - v
            p = [self.verts[t[0]][k] + u * (self.verts[t[1]][k] - self.verts[t[0]][k]) +
                 v * (self.verts[t[2]][k] - self.verts[t[0]][k]) for k in range(3)]
            p = [x * sc[0] for x in p]
            w = [pos[r] + m[3 * r] * p[0] + m[3 * r + 1] * p[1] + m[3 * r + 2] * p[2] for r in range(3)]
            d = [rng.gauss(0, 1) for _ in range(3)]
            dn = math.sqrt(sum(x * x for x in d))
            k = rng.random()
            if k < 0.1:
                d = [0.0, 0.0, -1.0]
                dn = 1.0
            span = self.size * 3 * max(sc)
            a = rng.uniform(0.5, 2) * span
            b = rng.uniform(-0.3, 1.5) * span
            s = [w[j] + d[j] / dn * a for j in range(3)]
            en = [w[j] - d[j] / dn * b for j in range(3)]
            if k > 0.93:
                # miss the bounding box along one axis
                ax = rng.randrange(3)
                sign = rng.choice([-1, 1])
                off = sign * (cfg["radius"] * max(sc) + span * 4)
                s[ax] = pos[ax] + off
                en[ax] = pos[ax] + off * rng.uniform(1, 2)
            if k > 0.97:
                s, en = en, s
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
        q = em.alloc(0x24)
        em.write(q, struct.pack("<3IB3x5I", starts, ends, n, cfg.get("slot", 0xFF), arg, cfg["mesh"],
                                cfg.get("all_faces", 0), 0x41424344, results))
        if cfg.get("slot") == 12:
            em.write(HANDLERS + 4 * (12 * 0x180 + TYPE), struct.pack("<I", SLOT12_HANDLER))
        reject["mode"] = cfg.get("reject", 0)
        return obj, q


def config(seed, **over):
    rng = random.Random(seed * 7919 + 13)
    cfg = {
        "seed": seed,
        "mesh": rng.randrange(3),
        "pos": [rng.uniform(-5000, 5000) for _ in range(3)],
        "matrix": rotation(rng) if rng.random() < 0.85 else [rng.uniform(-1.5, 1.5) for _ in range(9)],
        "scale": rng.choice([[1.0, 1.0, 1.0], [1.0, 1.0, 1.0], [2.0, 2.0, 2.0],
                             [rng.uniform(0.3, 3) for _ in range(3)]]),
        "radius": 1000.0,
        "count": rng.randrange(4, 12),
        "prior": rng.choice([0.0, 0.5, 1.0]),
        "all_faces": rng.choice([0, 1]),
        "reject": rng.choice([0, 0, 0, 2, 3]),
    }
    cfg.update(over)
    return cfg


def run(scene, cfg, cw):
    """Original against ours from the same scene; the 0x4BD070 calls must match too."""
    logs = []

    def setup(em):
        logs.append(list(calls))
        calls.clear()
        return scene.write(em, cfg)

    compare(e, with_cw(LINE_HIT, cw), with_cw("RigidDynCubeObj_LineHit", cw), setup=setup, check_eax=False)
    assert logs[1] == calls, "0x4BD070 calls differ:\n orig %s\n ours %s" % (logs[1], calls)
    return list(calls)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(40))
def test_line_hit(seed, cw):
    rng = random.Random(seed)
    scene = Scene(rng, ntri=rng.choice([6, 16, 30]))
    run(scene, config(seed), cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_line_hit_near_copies(seed, cw):
    rng = random.Random(1000 + seed)
    scene = Scene(rng, ntri=40)
    run(scene, config(1000 + seed, prior=0.7, count=16, reject=0), cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("scale", [[1.0, 1.0, 1.0], [1.0, 2.0, 1.0], [0.5, 0.5, 0.5], [1.5, 0.7, 2.5]])
def test_line_hit_scale(scale, cw):
    rng = random.Random(77)
    scene = Scene(rng, ntri=20)
    run(scene, config(77, scale=scale, prior=0.5), cw)


@pytest.mark.parametrize("over", [
    {"no_lods": 1}, {"lod": -1}, {"data_unk28": 1}, {"no_coll": 1, "mesh": 0}, {"mesh_unk0": 0},
    {"node_count": 0}, {"count": 0}, {"slot": 12}, {"slot": 12, "slot_reject": 1}, {"slot": 5},
    {"radius": 0.01}, {"radius": 0.0},
])
def test_line_hit_paths(over):
    rng = random.Random(5)
    scene = Scene(rng, ntri=12)
    run(scene, config(5, **over), 0x37F)
