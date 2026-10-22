import random
import struct

import pytest

from emu import Emu
from test_patrol_util import RET, d64, f32, fake, list_add, list_init, logfake, run, u32

e = Emu()

GRIDS_USED, GRIDS_FREE = 0x57D768, 0x58A9E8
MG_USED, MG_FREE, MT_FREE = 0x58AA18, 0x57C748, 0x57E1B8
CLAIMS_USED, CLAIMS_FREE = 0x5BDA28, 0x57D758
CFG = {}


def get_node(e, args):
    return CFG["nodes"].get(args[1], 0)


def node_pos(e, args):
    d64(e, args[0], *CFG["np"])


def node_matrix(e, args):
    f32(e, args[0], *CFG["mat"])


def target_pos(e, args):
    d64(e, args[0], *CFG["tpos"])


def find_nearest(e, args):
    r = CFG["nearest"].pop(0) if CFG["nearest"] else 0
    u32(e, args[0], r)


logfake(e, 0x4F9720, 2, effect=get_node)
fake(e, 0x4F9770, 2, skip=(0,), effect=node_pos)
fake(e, 0x4F98A0, 3, skip=(0,), effect=node_matrix)
fake(e, 0x451380, 2, skip=(0,), effect=target_pos)
logfake(e, 0x4513E0, 1, effect=lambda e, a: CFG["target"])
logfake(e, 0x4028B0, 0, effect=lambda e, a: CFG["tick"])
fake(e, 0x5096F0, 3, skip=(0,), derefs={2: 24}, effect=find_nearest)
logfake(e, 0x4AF810, 1, strings=(0,))


def graph(e, gid):
    g = e.alloc(0x60)
    u32(e, g + 0x50, gid)
    return g


def node(e, nid, radius):
    n = e.alloc(0x40)
    u32(e, n, nid)
    f32(e, n + 0x28, radius)
    return n


def grids(e, used, free, rng):
    """used: list of (graph id, node id, refs, w, h)."""
    list_init(e, GRIDS_USED)
    list_init(e, GRIDS_FREE)
    out = []
    for gid, nid, refs, w, h in used:
        g = e.alloc(0x660)
        u32(e, g + 8, gid, nid, refs, w, h)
        u32(e, g + 0x20, *[rng.randrange(-300, 300) for _ in range(400)])
        list_add(e, GRIDS_USED, g)
        out.append(g)
    for _ in range(free):
        g = e.alloc(0x660)
        u32(e, g + 8, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0)
        list_add(e, GRIDS_FREE, g)
    return out


def test_has_unk3aa8():
    for v in (0, 0x1234):
        def setup(e, v=v):
            ai = e.alloc(0x3C00)
            u32(e, ai + 0x3AA8, v)
            return (ai,)
        run(e, 0x4528F0, "sub_4528F0", setup, check_eax=True)


@pytest.mark.parametrize("t", range(20))
def test_aitype_name(t):
    run(e, 0x452910, "AIType_GetName", lambda e: (t,), check_eax=True)


def test_plus8():
    run(e, 0x452920, "sub_452920", lambda e: (0x12345678,), check_eax=True)


@pytest.mark.parametrize("seed", range(12))
def test_stamp(seed):
    def setup(e):
        rng = random.Random(seed)
        w, h = rng.randrange(1, 21), rng.randrange(1, 21)
        g = grids(e, [(1, 2, 1, w, h)], 0, rng)[0]
        return (g, rng.randrange(-3, w + 3), rng.randrange(-3, h + 3), rng.choice((0, 1, 0x100)))
    run(e, 0x452B80, "NodeGrid_Stamp", setup)


@pytest.mark.parametrize("key", [(1, 2), (3, 4), (5, 6), (1, 4)])
def test_find(key):
    def setup(e):
        grids(e, [(1, 2, 1, 5, 5), (3, 4, 1, 5, 5), (5, 6, 1, 5, 5)], 1, random.Random(1))
        return key
    run(e, 0x452BE0, "NodeGrid_Find", setup, check_eax=True)


@pytest.mark.parametrize("refs", [1, 2])
def test_release(refs):
    def setup(e):
        g = grids(e, [(1, 2, 0, 9, 9), (1, 3, refs, 12, 7)], 2, random.Random(refs))[1]
        return (g, 4, 3)
    run(e, 0x452C10, "NodeGrid_Release", setup)


def setup_nodes(e, radius):
    g = graph(e, 7)
    CFG["nodes"] = {3: node(e, 3, radius), 4: node(e, 4, radius)}
    return g


@pytest.mark.parametrize("nid,free", [(3, 1), (4, 1), (4, 0)])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_acquire(nid, free, cw):
    def setup(e):
        g = setup_nodes(e, 23.3)
        grids(e, [(7, 3, 2, 11, 11)], free, random.Random(5))
        return (g, nid, 5, 6)
    run(e, 0x452C60, "NodeGrid_Acquire", setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("radius", [21.0, 22.9, 0.7])
def test_get_size(cw, radius):
    def setup(e):
        g = setup_nodes(e, radius)
        out = e.alloc(8)
        u32(e, out, 0x55, 0x66)
        return (out, out + 4, g, 3)
    run(e, 0x453040, "NodeGrid_GetSize", setup, cw=cw)


@pytest.mark.parametrize("seed", range(60))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_place(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        radius = rng.uniform(2, 40)
        g = setup_nodes(e, radius)
        nid = rng.choice((3, 4))
        grids(e, [(7, 3, 1, int(radius / 2 + 0.5), int(radius / 2 + 0.5))], rng.choice((0, 1)), rng)
        CFG["np"] = [rng.uniform(-1e6, 1e6) for _ in range(3)]
        CFG["mat"] = [rng.uniform(-1, 1) for _ in range(9)] + [0]
        span = radius * 4096 * 1.4
        pos = e.alloc(24)
        d64(e, pos, *[c + rng.uniform(-span, span) for c in CFG["np"]])
        out = e.alloc(40)
        outs = [out if rng.random() < 0.8 else 0, out + 4 if rng.random() < 0.8 else 0,
                out + 8 if rng.random() < 0.8 else 0]
        return tuple(outs) + (g, nid, pos)
    run(e, 0x452D20, "NodeGrid_Place", setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("seed", range(10))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_view(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        obj = e.alloc(0x80)
        f32(e, obj + 0x50, *[rng.uniform(-1, 1) for _ in range(9)])
        u32(e, obj + 0x74, rng.randrange(1000))
        CFG["tpos"] = [rng.uniform(-1e5, 1e5) for _ in range(3)]
        frm = e.alloc(24)
        d64(e, frm, *[c + rng.uniform(-3e4, 3e4) for c in CFG["tpos"]])
        out = e.alloc(64)
        scale = rng.uniform(0.5, 9.0)
        return (out, out + 40, obj, frm, struct.unpack("<I", struct.pack("<f", scale))[0])
    run(e, 0x452930, "sub_452930", setup, cw=cw)


# --- HumanMonitor ------------------------------------------------------------

def monitor(e, entries, free_targets, free_graphs, used_graphs=()):
    """entries: (target, node id, tick) in one graph entry for graph G."""
    list_init(e, MG_USED)
    list_init(e, MG_FREE)
    list_init(e, MT_FREE)
    gr = graph(e, 7)
    CFG["graph"] = gr
    mg = None
    for gptr in list(used_graphs) + [gr]:
        m = e.alloc(0x18)
        u32(e, m + 8, gptr)
        list_init(e, m + 0xC)
        list_add(e, MG_USED, m)
        mg = m
    for tgt, nid, tick in entries:
        t = e.alloc(0x14)
        u32(e, t + 8, nid, tick, tgt)
        list_add(e, mg + 0xC, t)
    for _ in range(free_targets):
        t = e.alloc(0x14)
        u32(e, t + 8, 0xFFFFFFFF, 0xFFFFFFFF, 0)
        list_add(e, MT_FREE, t)
    for _ in range(free_graphs):
        m = e.alloc(0x18)
        u32(e, m + 8, 0)
        list_init(e, m + 0xC)
        list_add(e, MG_FREE, m)
    ai = e.alloc(0x3C00)
    u32(e, ai + 0x3B6C, gr)
    CFG["nodes"] = {5: node(e, 5, 3.0), 9: node(e, 9, 3.0)}
    CFG["tpos"] = [1.5, -2.5, 3.5]
    return ai, mg


LOOKUP = [
    # target, entries, free, tick, nearest results
    (0, [], 2, 10, []),
    (0x77, [(0x66, 5, 10), (0x77, 0xFFFFFFFF, 10)], 2, 10, []),
    (0x77, [(0x77, 9, 10)], 2, 10, []),
    (0x77, [(0x77, 9, 8)], 2, 10, ["n5"]),
    (0x77, [(0x77, 9, 8)], 2, 10, [0]),
    (0x77, [(0x66, 9, 8)], 0, 10, []),
    (0x77, [(0x66, 9, 8)], 3, 10, [0, 0, "n9"]),
    (0x77, [(0x66, 9, 8)], 3, 10, [0, 0, 0]),
    (0x77, [], 3, 10, ["n9"]),
]


def nearest(lst):
    return [CFG["nodes"][int(x[1:])] if isinstance(x, str) else x for x in lst]


@pytest.mark.parametrize("target,entries,free,tick,near", LOOKUP)
def test_lookup(target, entries, free, tick, near):
    def setup(e):
        ai, mg = monitor(e, entries, free, 0)
        CFG.update(target=target, tick=tick)
        CFG["nearest"] = nearest(near)
        return (ai, mg)
    run(e, 0x4530A0, "HumanMonitor_Lookup", setup, check_eax=True)


@pytest.mark.parametrize("known,free", [(True, 0), (False, 2), (False, 0)])
def test_find_target_node(known, free):
    def setup(e):
        ai, mg = monitor(e, [(0x77, 9, 8)], 2, free, used_graphs=[0x1111])
        if not known:
            u32(e, ai + 0x3B6C, 0x2222)
        CFG.update(target=0x77, tick=10)
        CFG["nearest"] = nearest(["n5"])
        return (ai,)
    run(e, 0x453200, "HumanMonitor_FindTargetNode", setup, check_eax=True)


def test_monitor_reset():
    def setup(e):
        monitor(e, [(0x77, 9, 8), (0x66, 5, 1), (0x55, 5, 2)], 2, 1, used_graphs=[0x1111, 0x3333])
        m = e.u32(MG_USED)
        for i in range(2):
            t = e.alloc(0x14)
            u32(e, t + 8, i, i, i)
            list_add(e, m + 0xC, t)
        return ()
    run(e, 0x453270, "HumanMonitor_Reset", setup)


def test_monitor_reset_empty():
    def setup(e):
        monitor(e, [], 2, 1)
        list_init(e, MG_USED)
        return ()
    run(e, 0x453270, "HumanMonitor_Reset", setup)


# --- claims ------------------------------------------------------------------

def claims(e, used, free):
    list_init(e, CLAIMS_USED)
    list_init(e, CLAIMS_FREE)
    for t in used:
        c = e.alloc(12)
        u32(e, c + 8, t)
        list_add(e, CLAIMS_USED, c)
    out = []
    for _ in range(free):
        c = e.alloc(12)
        list_add(e, CLAIMS_FREE, c)
        out.append(c)
    return out


@pytest.mark.parametrize("task", [0x10, 0x20, 0x99])
def test_is_claimed(task):
    run(e, 0x4532D0, "AIClaim_IsClaimed", lambda e: (claims(e, [0x10, 0x20], 1) and task,), eax_mask=0xFF,
        check_eax=True)


@pytest.mark.parametrize("task,on,free", [(0x10, 0, 1), (0x20, 0, 1), (0x99, 0, 1), (0x99, 1, 1), (0x99, 1, 0),
                                          (0x10, 0x100, 1)])
def test_claim_set(task, on, free):
    def setup(e):
        claims(e, [0x10, 0x20], free)
        return (task, on)
    run(e, 0x453300, "AIClaim_Set", setup, check_eax=True)


def test_claim_release():
    def setup(e):
        claims(e, [0x10, 0x20], 1)
        c = e.u32(CLAIMS_USED)
        return (c,)
    run(e, 0x453390, "AIClaim_Release", setup)
