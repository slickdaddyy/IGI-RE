"""RigidDynCubeObj_CollLine (0x4C35F0) and RigidDynCubeObj_CreateMagicObjs (0x4C4950)."""
import bisect
import math
import random
import struct

import pytest
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from unicorn import UC_HOOK_BLOCK, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import Emu, ErrorShown, compare

HANDLERS = 0xA96AE0
EVENTS = 0x547F3C
DEFS, DEF_COUNT = 0xB97E00, 0xA4433C
COLL, COLL_END = 0x4C35F0, 0x4C4516
MAGIC, MAGIC_END = 0x4C4950, 0x4C56CE
CWS = [0x37F, 0x7F]

NAN = float("nan")
INF = float("inf")

e = Emu()
e.stub("MessageBoxA", lambda em: em.ret(1, 16))
e.stub("OutputDebugStringA", lambda em: em.ret(0, 4))


class Fakes:
    """Python replacements of original functions that log their arguments."""

    def __init__(self, em):
        self.e = em
        self.log = []
        self.handlers = {}
        self.tramps = {}

    def code(self, data):
        p = self.e.fake_top
        self.e.fake_top += (len(data) + 15) & ~15
        self.e.write(p, data)
        return p

    def add(self, addr, nargs):
        """The return value comes from self.handlers[addr](emu, args)."""
        def cb(uc, a, size, user):
            em = self.e
            args = [em.arg(i) for i in range(nargs)]
            em.ret(self.handlers[a](em, args) or 0)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def stub(self, nargs):
        """A fresh code address handled by Python."""
        return self.add(self.code(b"\xc3"), nargs)

    def with_cw(self, target, cw):
        addr = self.e.symbols[target] if isinstance(target, str) else target
        key = (addr, cw)
        if key not in self.tramps:
            cell = self.code(struct.pack("<H", cw))
            p = self.code(b"\0" * 16)
            self.e.write(p, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
            self.tramps[key] = p
        return self.tramps[key]


fk = Fakes(e)
FILTER = fk.stub(2)
EV3 = fk.stub(2)
for a, n in ((0x4BD070, 4), (0x4D0D10, 1), (0x4D0CF0, 2), (0x4012A0, 3)):
    fk.add(a, n)


# --- branch coverage of the originals -------------------------------------------

def _branches(lo, hi):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    code = e.read(lo, hi - lo)
    insns = list(md.disasm(code, lo))
    out = {}
    for i in insns:
        if i.mnemonic.startswith("j") and i.mnemonic != "jmp":
            out[i.address] = (int(i.op_str, 16), i.address + i.size)
    return [i.address for i in insns], out


class Coverage:
    def __init__(self, lo, hi):
        self.insns, self.jcc = _branches(lo, hi)
        self.seen = set()
        self.last = None
        e.uc.hook_add(UC_HOOK_BLOCK, self._block, begin=lo, end=hi - 1)

    def _block(self, uc, addr, size, user):
        if self.last is not None:
            j = self.jcc.get(self.last)
            if j:
                self.seen.add((self.last, addr == j[0]))
        k = bisect.bisect_left(self.insns, addr + size) - 1
        self.last = self.insns[k] if k >= 0 and self.insns[k] >= addr else None

    def reset(self):
        self.last = None

    def missing(self):
        return sorted((a, t) for a in self.jcc for t in (True, False) if (a, t) not in self.seen)


cov_coll = Coverage(COLL, COLL_END)
cov_magic = Coverage(MAGIC, MAGIC_END)


def check(orig, ours, setup, cw, **kw):
    """compare() plus equal Python fake logs, run at x87 control word cw."""
    logs = []

    def s(em):
        r = setup(em)
        fk.log = []
        logs.append(fk.log)
        cov_coll.reset()
        cov_magic.reset()
        return r
    compare(e, fk.with_cw(orig, cw), fk.with_cw(ours, cw), setup=s, check_eax=False, **kw)
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return logs[0]


def f32(v):
    return struct.pack("<f", v)


def u32f(v):
    return struct.unpack("<I", f32(v))[0]


def alloc_data(em, data):
    p = em.alloc(len(data))
    em.write(p, data)
    return p


def set_handler(em, slot, type_, fn):
    em.write(HANDLERS + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


def rot(r):
    q = [r.gauss(0, 1) for _ in range(4)]
    n = math.sqrt(sum(x * x for x in q))
    w, x, y, z = [v / n for v in q]
    return [1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
            2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
            2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]


def rf(v):
    return struct.unpack("<f", f32(v))[0]


# --- RigidDynCubeObj_CollLine ---------------------------------------------------

TYPE = 0x40
FILTER_EVENT = 0x33


def build_coll(em, seed, mode):
    """A RigidDynCubeObj with a random collision mesh and sphere tree, and a query
    segment that usually crosses one of its triangles. Returns (obj, query)."""
    r = random.Random(seed)
    snap = mode in ("grid", "flat", "gridaxis", "flataxis")

    # mesh in model space
    nv = r.randint(3, 12)
    verts = []
    for _ in range(nv):
        v = [r.uniform(-1, 1) for _ in range(3)]
        if snap:
            v = [round(c * 2) / 2 for c in v]
        if mode in ("flat", "flataxis"):
            v[2] = 0.25
        verts.append(v)
    nt = r.randint(1, 14)
    tris = []
    for _ in range(nt):
        t = [r.randrange(nv) for _ in range(3)]
        if r.random() < 0.08:
            t[2] = t[1]
        tris.append(t)
    ns = r.randint(1, 4)
    nmat = 4
    surfs = []
    for _ in range(ns):
        u4 = 0xFFFF if r.random() < 0.85 else r.choice([0, 1, 0x1234])
        mat = 0xFFFF if r.random() < 0.15 else r.randrange(nmat)
        surfs.append((u4, mat))
    tri_surf = [r.randrange(ns) for _ in range(nt)]

    # sphere tree, tris split into ranges per node
    nn = r.randint(1, 8)
    parent = [None] + [r.randrange(i) for i in range(1, nn)]
    child = [0] * nn
    nxt = [0] * nn
    for i in range(1, nn):
        p = parent[i]
        if child[p] == 0:
            child[p] = i
        else:
            k = child[p]
            while nxt[k]:
                k = nxt[k]
            nxt[k] = i
    cuts = sorted(r.randint(0, nt) for _ in range(nn - 1))
    bounds = [0] + cuts + [nt]
    ranges = [(bounds[i], bounds[i + 1] - bounds[i]) for i in range(nn)]
    r.shuffle(ranges)
    nodes = []
    for i in range(nn):
        first, cnt = ranges[i]
        pts = [verts[k] for t in tris[first:first + cnt] for k in t] or [verts[r.randrange(nv)]]
        c = [sum(p[j] for p in pts) / len(pts) + r.uniform(-0.1, 0.1) for j in range(3)]
        rad = max(math.dist(c, p) for p in pts) * r.choice([0.3, 0.8, 1.05, 1.5, 3.0])
        if r.random() < 0.05:
            rad = 0.0
        nodes.append((c, rad, first, cnt, child[i], nxt[i]))

    em_tris = alloc_data(em, b"".join(struct.pack("<4H", *t, s) for t, s in zip(tris, tri_surf)))
    em_verts = alloc_data(em, b"".join(struct.pack("<4f", *v, 0.0) for v in verts))
    em_surfs = alloc_data(em, b"".join(struct.pack("<iHHHH", 0, u4, 0, mat, 0) for u4, mat in surfs))
    em_nodes = alloc_data(em, b"".join(struct.pack("<4f4H", *c, rad, a, b, ch, nx) for c, rad, a, b, ch, nx in nodes))
    mesh = alloc_data(em, struct.pack("<ii8xIIII", nt, nv, em_tris, em_verts, em_surfs, em_nodes))

    # object placement
    M = rot(r)
    if mode == "skew":
        M = [x + r.uniform(-0.2, 0.2) for x in M]
    s = r.choice([1.0, 1.0, 0.5, 2.0, 1.37])
    scale = [s, s, s]
    if mode == "skew" or r.random() < 0.1:
        scale = [s * r.choice([1.0, 0.5, 2.0]) for _ in range(3)]
    pos = [r.uniform(-500, 500) for _ in range(3)]
    if mode in ("gridaxis", "flataxis"):
        M = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
        scale = [1.0, 1.0, 1.0]
        pos = [float(r.randint(-500, 500)) for _ in range(3)]
    radius = max(math.sqrt(sum(c * c for c in v)) for v in verts) * r.choice([1.0, 1.2, 2.0, 0.5])

    info = em.alloc(0x80)
    em.write(info + 0x28, struct.pack("<I", 1 if mode == "flag28" else 0))
    em.write(info + 0x2C, struct.pack("<I", 0 if mode == "nomesh" else mesh))
    em.write(info + 0x78, f32(radius))
    b = em.alloc(0x10)
    em.write(b + 0xC, struct.pack("<I", info))
    lodp = em.alloc(0x10)
    em.write(lodp + 0xC, struct.pack("<I", b))
    model = em.alloc(8)
    em.write(model, struct.pack("<II", lodp, lodp))
    if mode == "notris":
        em.write(mesh, struct.pack("<i", 0))

    o = em.alloc(0xF0)
    em.write(o, bytes(r.randrange(256) for _ in range(0xF0)))
    em.write(o + 0x1C, struct.pack("<H", TYPE))
    em.write(o + 0x20, struct.pack("<3d", *pos))
    em.write(o + 0x6C, struct.pack("<I", 0 if mode == "nomodel" else model))
    em.write(o + 0x70, struct.pack("<9fI", *M, 0))
    em.write(o + 0x98, struct.pack("<3f", *scale))
    em.write(o + 0xA4, struct.pack("<i", -1 if mode == "nolod" else r.choice([0, 1])))

    def to_world(p):
        return [pos[i] + sum(M[i * 3 + j] * p[j] * scale[0] for j in range(3)) for i in range(3)]

    # segment
    t = tris[r.randrange(nt)]
    w = [r.random() for _ in range(3)]
    sw = sum(w)
    hit = [sum(verts[t[k]][j] * w[k] for k in range(3)) / sw for j in range(3)]
    d = [r.gauss(0, 1) for _ in range(3)]
    if mode == "flat" and r.random() < 0.5:
        d[2] = 0.0
    if mode in ("axis", "gridaxis", "flataxis"):
        d = [0.0, 0.0, 0.0]
        d[r.randrange(3)] = r.choice([-1.0, 1.0])
    a = [hit[j] + d[j] * r.uniform(-2.0, -0.05) for j in range(3)]
    bb = [hit[j] + d[j] * r.uniform(0.05, 2.0) for j in range(3)]
    if mode == "zero":
        bb = list(a)
    if mode == "short":
        a = [hit[j] + d[j] * r.uniform(-0.05, -0.001) for j in range(3)]
        bb = [hit[j] + d[j] * r.uniform(0.001, 0.05) for j in range(3)]
    if mode == "beyond_axis":
        d = [r.uniform(-0.05, 0.05) for _ in range(3)]
        d[r.randrange(3)] = r.choice([-1.0, 1.0])
        sg = r.choice([-1, 1])
        a = [hit[j] + sg * d[j] * r.uniform(1.5, 3.0) for j in range(3)]
        bb = [hit[j] + sg * d[j] * r.uniform(3.0, 5.0) for j in range(3)]
    if mode == "beyond":
        sg = r.choice([-1, 1])
        a = [hit[j] + sg * d[j] * r.uniform(1.5, 3.0) for j in range(3)]
        bb = [hit[j] + sg * d[j] * r.uniform(3.0, 5.0) for j in range(3)]
    if mode == "far":
        a = [c + r.choice([-1, 1]) * 10 for c in a]
        bb = [c + r.choice([-1, 1]) * 10 for c in bb]
    if mode == "random":
        a = [r.uniform(-2, 2) for _ in range(3)]
        bb = [r.uniform(-2, 2) for _ in range(3)]
    if r.random() < 0.3:
        a, bb = bb, a
    wa, wb = to_world(a), to_world(bb)
    if mode == "zero" and r.random() < 0.5:
        wb = list(wa)
    if mode == "nan":
        (wa if r.random() < 0.5 else wb)[r.randrange(3)] = NAN

    # world with material table
    world = em.alloc(0xD3C + 4 * nmat)
    for i in range(nmat):
        v = r.choice([0.0, 0.5, 1.0, 0.25, -0.0, 0.9])
        em.write(world + 0xD3C + 4 * i, struct.pack("<I", alloc_data(em, f32(v))))

    # filter handler
    event = 0xFF
    allow = 1
    if mode == "filter" or r.random() < 0.15:
        event = FILTER_EVENT if r.random() < 0.8 else 0x34
        allow = r.choice([0, 1])
        set_handler(em, FILTER_EVENT, TYPE, FILTER)
    filt = em.alloc(8)
    em.write(filt, b"\x77\0\0\0" + struct.pack("<I", o + 0x100))

    def filter_cb(em2, args):
        fk.log.append(("filter", args[0], args[1]))
        em2.write(args[1], bytes([allow]))
    fk.handlers[FILTER] = filter_cb

    blocked, vis = 0, 1.0
    if mode == "blocked" or r.random() < 0.1:
        blocked = 1
        vis = r.choice([0.0, -0.0, 0.5, 1.0])

    rets = [r.choice([0, 0, 0, 1]) for _ in range(64)] if r.random() < 0.3 else [0] * 64

    def check_cb(em2, args):
        n = sum(1 for x in fk.log if x[0] == "check")
        fk.log.append(("check", args[0], args[1], args[2], args[3]))
        return rets[n % 64]
    fk.handlers[0x4BD070] = check_cb

    p0 = alloc_data(em, struct.pack("<3d", *wa))
    p1 = alloc_data(em, struct.pack("<3d", *wb))
    q = em.alloc(0x1C)
    em.write(q, struct.pack("<IIBxxxIIBxxxf", p0, p1, event, filt, world, blocked, vis))
    return o, q


MODE_IDS = ["hit", "grid", "flat", "axis", "skew", "random", "far", "zero", "nan", "filter", "blocked", "short", "beyond", "beyond_axis", "gridaxis", "flataxis"]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(40))
@pytest.mark.parametrize("mode", MODE_IDS)
def test_coll_line(mode, seed, cw):
    n = 60 if mode == "hit" else 60 if mode == "beyond_axis" else 20 if mode in ("short", "beyond", "gridaxis", "flataxis") else 1
    for k in range(n if seed < 2 else 1):
        sd = seed * 1000 + k + MODE_IDS.index(mode) * 100000
        check(COLL, "RigidDynCubeObj_CollLine", lambda em: build_coll(em, sd, mode), cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("mode", ["nomodel", "nolod", "flag28", "nomesh", "notris"])
def test_coll_line_early_out(mode, cw):
    for seed in range(3):
        check(COLL, "RigidDynCubeObj_CollLine", lambda em: build_coll(em, seed, mode), cw)


# --- RigidDynCubeObj_CreateMagicObjs ------------------------------------------------

SPECIAL = [0.0, -0.0, 1.0, -1.0, 1e-30, 1e30, 1e-42, NAN, INF, -INF, 0.5]
NAMES = [b"lamp", b"door_r", b"gun", b"ladder", b"lit"]


def rmat(r, mode):
    if mode == "rot":
        return rot(r)
    if mode == "zero":
        return [0.0] * 9
    if mode == "cols":
        m = rot(r)
        for i in r.sample(range(3), r.randint(1, 2)):
            m[i] = m[i + 3] = m[i + 6] = 0.0
        return m
    if mode == "sparse":
        return [0.0 if r.random() < 0.6 else r.uniform(-2, 2) for _ in range(9)]
    if mode == "special":
        return [r.choice(SPECIAL) if r.random() < 0.4 else r.uniform(-2, 2) for _ in range(9)]
    if mode == "parallel":
        c = [r.uniform(-1, 1) for _ in range(3)]
        k = r.uniform(-2, 2)
        return [c[0], c[0] * k, r.uniform(-1, 1), c[1], c[1] * k, r.uniform(-1, 1), c[2], c[2] * k, r.uniform(-1, 1)]
    return [r.uniform(-3, 3) for _ in range(9)]


def build_magic(em, seed, mode):
    r = random.Random(seed)
    ndef = r.randint(1, 5)
    defs = []
    for i in range(ndef):
        name = NAMES[i]
        typ = 0x50 + i
        data = em.alloc(8)
        defs.append((name, typ, data))
        em.write(DEFS + 0x28 * i, name.ljust(16, b"\0") + (b"mdl_" + name).ljust(16, b"\0")[:16]
                 + struct.pack("<HHI", typ, 0x9999, data))
    em.write(DEF_COUNT, struct.pack("<i", 0 if mode == "nodefs" else ndef))
    em.write(EVENTS + 3, b"\x2a")

    natt = r.randint(1, 5) if mode != "none" else 0
    atts = em.alloc(0x44 * max(natt, 1))
    for i in range(natt):
        if mode in ("missing", "nodefs") and i == natt - 1:
            name = b"unknown"
        else:
            name = defs[r.randrange(ndef)][0]
        kind = mode if mode in ("rot", "zero", "cols", "special", "parallel", "sparse") else r.choice(
            ["rot", "rot", "rnd", "cols", "special", "parallel", "zero"])
        m = rmat(r, kind)
        p = [r.uniform(-5, 5) for _ in range(3)]
        if kind == "special":
            p = [r.choice(SPECIAL) if r.random() < 0.3 else v for v in p]
        em.write(atts + 0x44 * i, name.ljust(16, b"\0") + struct.pack("<3f9fI", *p, *m, 0))

    lod = em.alloc(0x10)
    model = em.alloc(0x40)
    em.write(model, struct.pack("<I", lod))
    em.write(model + 0x14, b"crate_big\0")
    o = em.alloc(0xF0)
    em.write(o, bytes(r.randrange(256) for _ in range(0xF0)))
    em.write(o + 0x20, struct.pack("<3d", *[r.uniform(-1000, 1000) for _ in range(3)]))
    em.write(o + 0x68, struct.pack("<I", 0x4444))
    em.write(o + 0x6C, struct.pack("<I", 0 if mode == "nomodel" else model))
    kind = r.choice(["rot", "rot", "rnd", "special", "zero"]) if mode not in ("special", "sparse") else mode
    em.write(o + 0x70, struct.pack("<9fi", *rmat(r, kind), r.choice([0, 5, 999, 1000, 1001, 4000])))

    tasks = {}
    for name, typ, data in defs:
        t = em.alloc(0x20)
        em.write(t + 0x1C, struct.pack("<H", typ))
        tasks[typ] = t
        if r.random() < 0.7:
            set_handler(em, 0x2A, typ, EV3)

    def count_cb(em2, args):
        fk.log.append(("count", args[0]))
        return natt

    def attach_cb(em2, args):
        fk.log.append(("attach", args[0], args[1]))
        return atts + 0x44 * args[1]

    def new_cb(em2, args):
        fk.log.append(("new", args[0], args[1] & 0xFFFF, em2.read(args[2], 0x90)))
        return tasks[args[1] & 0xFFFF]

    def ev3_cb(em2, args):
        a = args[1]
        fk.log.append(("ev3", args[0], em2.u32(a), em2.u32(a + 4), em2.u32(a + 0x88), em2.u32(a + 0x8C)))

    fk.handlers.update({0x4D0D10: count_cb, 0x4D0CF0: attach_cb, 0x4012A0: new_cb, EV3: ev3_cb})
    return 0x7777, o, 0x8888


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(25))
@pytest.mark.parametrize("mode", ["mix", "rot", "zero", "cols", "special", "parallel", "sparse"])
def test_create_magic_objs(mode, seed, cw):
    check(MAGIC, "RigidDynCubeObj_CreateMagicObjs", lambda em: build_magic(em, seed * 31 + 7, mode), cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("mode", ["none", "nomodel"])
def test_create_magic_objs_empty(mode, cw):
    check(MAGIC, "RigidDynCubeObj_CreateMagicObjs", lambda em: build_magic(em, 5, mode), cw)


@pytest.mark.parametrize("mode", ["missing", "nodefs"])
def test_create_magic_objs_error(mode):
    msgs, logs = [], []
    for target in (MAGIC, "RigidDynCubeObj_CreateMagicObjs"):
        e.reset()
        args = build_magic(e, 11, mode)
        fk.log = []
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append(str(err.value))
        logs.append(fk.log)
    assert msgs[0] == msgs[1] and "not defined" in msgs[0]
    assert logs[0] == logs[1]


# --- every conditional branch of the originals went both ways --------------------

# never taken / never falls through, by construction of the code:
# 4c3e18: a node with triangles always has first < first + count
# 4c417c: |nx| > |ny| >= 0 so nx is never 0 there
# 4c42c9, 4c438c: the same for ny and nz in their branches (nz 0 means all three
#   are 0, and then the denominator is 0 and the triangle was skipped earlier)
# 4c423d, 4c444c: an edge whose two other components are both 0 makes the chosen
#   normal component 0, which its branch already rules out
COLL_UNREACHABLE = {(0x4C3E18, True), (0x4C417C, True), (0x4C42C9, True), (0x4C438C, True),
                    (0x4C423D, True), (0x4C444C, True)}


def test_zz_coverage():
    if len(cov_coll.seen) < 50 or len(cov_magic.seen) < 20:
        pytest.skip("run the whole file for coverage")
    assert [hex(a) + (" taken" if t else " not taken") for a, t in cov_coll.missing()
            if (a, t) not in COLL_UNREACHABLE] == []
    assert [hex(a) + (" taken" if t else " not taken") for a, t in cov_magic.missing()] == []
