"""Graph tasks and their nodes and links (0x4F90B0 - 0x4F9CD0)."""
import math
import random
import struct

import pytest

from emu import ErrorShown
from test_graphnode_util import (CWS, LOG, RET, alloc_fake, d64, f32, handler, logfake, new_emu, run, set_handler,
                                 u16, u32)

GTYPE, GTYPE2, RIGID = 0x60, 0x61, 0x42
NODE_SIZE = 0x240
GRAPHS = 0xA77248
SIZES = 0xA77228
ADD_EVENT, ALLOC_EVENT = 0x54E36F, 0x54E369

e = new_emu()
alloc_fake(e)
logfake(e, 0x4B0D10, 1)  # Mem_Free
logfake(e, 0x4B0D60, 1)  # Mem_PushMode
logfake(e, 0x4B0D90, 0)  # Mem_PopMode


def task_ref(e, a):
    p = e.alloc(4)
    u32(e, p, a[0])
    return p


logfake(e, 0x401AE0, 1, effect=task_ref)
logfake(e, 0x401CF0, 2, masks={0: 0xFFFF, 1: 0xFFFF}, effect=lambda e, a: int(a[0] & 0xFFFF in RET.get("isa", ())))
logfake(e, 0x4C48C0, 0, ret=RIGID)
logfake(e, 0x4F1050, 1, effect=lambda e, a: RET.get(("id", a[0]), 77))
logfake(e, 0x4B8770, 1, strings=(0,), ret=0x5A5A)
logfake(e, 0x4AF810, 1, strings=(0,))
e.write(ADD_EVENT, bytes([0x40]))
e.write(ALLOC_EVENT, bytes([0x41]))
set_handler(e, 0x40, GTYPE, handler(e, "added", 2))
set_handler(e, 0x41, GTYPE, handler(e, "alloc", 2))
e.baseline = e.snapshot()

T = 2.0 ** -70
VALUES = [1.0, -1.0, T, -T, 3.0, 0.5, 2.0 ** 30, 1e-3, 123.456]
TINY = [1.0, -1.0, T]


def vals(seed):
    """Random values for even seeds; sums whose order shows for odd ones."""
    return (VALUES if seed & 2 else TINY) if seed & 1 else None


def list_init(e, lst):
    u32(e, lst, lst + 4, 0, lst)


def list_add(e, lst, node):
    last = e.u32(lst + 8)
    u32(e, node, lst + 4, last)
    u32(e, last, node)
    u32(e, lst + 8, node)


def sizes(e, *entries):
    """Node size table: (type, size) pairs, the rest free."""
    entries = list(entries) + [(0x181, 0)] * (4 - len(entries))
    for i, (t, s) in enumerate(entries):
        u16(e, SIZES + 8 * i, t)
        u16(e, SIZES + 8 * i + 2, 0xEEEE)
        u32(e, SIZES + 8 * i + 4, s)


def rigid(e, rng, values=None):
    p = e.alloc(0xA0)
    u16(e, p + 0x1C, RIGID)
    pick = (lambda: rng.choice(values)) if values else (lambda: rng.uniform(-1000, 1000))
    d64(e, p + 0x20, pick(), pick(), pick())
    f32(e, p + 0x70, *[rng.choice(values) if values else rng.uniform(-1, 1) for _ in range(9)])
    u32(e, p + 0x94, rng.randrange(100))
    return p


def graph(e, rng=None, type_=GTYPE, attached=0, parent=None, cap=0):
    rng = rng or random.Random(3)
    g = e.alloc(0xC0)
    e.write(g, bytes(rng.randrange(256) for _ in range(0xC0)))
    u16(e, g + 0x1C, type_)
    if parent is None:
        parent = e.alloc(0x40)
        u16(e, parent + 0x1C, 0x10)
    u32(e, g + 0x14, parent)
    e.write(g + 0x64, bytes([attached]))
    list_init(e, g + 0x70)
    u32(e, g + 0x58, cap, 0, 0)
    u32(e, g + 0x6C, 0)
    if cap:
        nodes = e.alloc(NODE_SIZE * cap)
        for i in range(cap):
            u32(e, nodes + NODE_SIZE * i, 0xFFFFFFFF)
        u32(e, g + 0x6C, nodes)
    return g


def node(e, g, nid, rng=None, values=None):
    """Fill node nid of g (which must have room)."""
    rng = rng or random.Random(nid)
    n = e.u32(g + 0x6C) + NODE_SIZE * nid
    pick = (lambda: rng.choice(values)) if values else (lambda: rng.uniform(-50000, 50000))
    u32(e, n, nid)
    d64(e, n + 8, pick(), pick(), pick())
    f32(e, n + 0x20, rng.uniform(-4, 4), rng.uniform(0.1, 3))
    u32(e, n + 0x134, g)
    for i in range(64):
        u32(e, n + 0x13C + 4 * i, 0)
    return n


def errors(e, orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        LOG.append([])
        args = setup(e)
        with pytest.raises(ErrorShown) as x:
            e.call(target if isinstance(target, int) else e.symbols[target], *args)
        msgs.append((str(x.value), LOG.pop()))
    assert msgs[0] == msgs[1]


@pytest.mark.parametrize("isa", [False, True])
def test_create(isa):
    def setup(e):
        RET["isa"] = (0x10,) if isa else ()
        list_init(e, GRAPHS)
        list_add(e, GRAPHS, e.alloc(12))
        return (graph(e),)
    run(e, 0x4F90B0, "Graph_Create", setup)


@pytest.mark.parametrize("nodes", [0, 0x1230])
def test_delete(nodes):
    def setup(e):
        g = graph(e)
        u32(e, g + 0x6C, nodes)
        return (g,)
    run(e, 0x4F9160, "Graph_Delete", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("isa,attached", [(0, 0), (0, 1), (1, 0), (1, 1)])
@pytest.mark.parametrize("seed", range(8))
def test_params_changed(isa, attached, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        RET["isa"] = (RIGID,) if isa else ()
        g = graph(e, rng, attached=attached, parent=rigid(e, rng, vals(seed)))
        d64(e, g + 0x20, *[rng.choice(vals(seed)) if seed & 1 else rng.uniform(-9999, 9999) for _ in range(3)])
        return (g,)
    run(e, 0x4F9180, "Graph_ParamsChanged", setup, cw=cw)


@pytest.mark.parametrize("addr,name,mask", [
    (0x4F9200, "Graph_GetParamTable", 0xFFFFFFFF), (0x4F9210, "Graph_GetType", 0xFFFF),
    (0x4F9220, "Graph_GetLoadEvent", 0xFF), (0x4F9230, "Graph_GetSaveEvent", 0xFF),
    (0x4F9240, "Graph_GetNodeLoadEvent", 0xFF), (0x4F9250, "sub_4F9250", 0xFF),
    (0x4F9260, "Graph_GetNodeAddEvent", 0xFF), (0x4F9270, "sub_4F9270", 0xFF),
    (0x4F9280, "Graph_GetAllocEvent", 0xFF)])
def test_getters(addr, name, mask):
    def setup(e):
        e.write(0x54E364, bytes(range(0x91, 0xA1)))
        u32(e, 0xA76D10, 0x12345678)
    run(e, addr, name, setup, check_eax=True, eax_mask=mask)


@pytest.mark.parametrize("want", [-1, 5, 6, 9, 77])
def test_find(want):
    def setup(e):
        list_init(e, GRAPHS)
        for i, gid in enumerate([5, -1, 9, 6, -1]):
            g = graph(e, random.Random(i))
            u32(e, g + 0x50, gid)
            RET[("id", g)] = 70 + i
            r = e.alloc(12)
            ref = e.alloc(4)
            u32(e, ref, g)
            u32(e, r + 8, 0 if i == 2 else ref)
            list_add(e, GRAPHS, r)
        return (want,)
    run(e, 0x4F9290, "Graph_Find", setup, check_eax=True)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("attached", [0, 1])
@pytest.mark.parametrize("seed", range(16))
def test_world_pos(attached, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        v = vals(seed)
        g = graph(e, rng, attached=attached, parent=rigid(e, rng, v))
        d64(e, g + 0x20, *[rng.choice(v) if v else rng.uniform(-9999, 9999) for _ in range(3)])
        out = e.alloc(24)
        return out, g
    run(e, 0x4F92E0, "Graph_GetWorldPos", setup, cw=cw)

    def setup2(e):
        out, g = setup(e)
        rng = random.Random(seed + 100)
        d64(e, out, *[rng.choice(vals(seed)) if seed & 1 else rng.uniform(-9999, 9999) for _ in range(3)])
        return g, out
    run(e, 0x4F93B0, "Graph_SetWorldPos", setup2, cw=cw)


@pytest.mark.parametrize("type_,entries", [
    (GTYPE, []), (GTYPE, [(1, 5)]), (GTYPE, [(1, 5), (GTYPE, 7)]), (GTYPE, [(1, 5), (0x181, 0), (GTYPE, 7)]),
    (GTYPE, [(1, 5), (2, 6), (3, 7), (4, 8)]), (GTYPE, [(1, 5), (2, 6), (3, 7), (GTYPE, 8)])])
def test_set_node_size(type_, entries):
    def setup(e):
        sizes(e, *entries)
        return type_ | 0xABCD0000, 0x240
    if len(entries) == 4 and all(t != type_ for t, _ in entries):
        errors(e, 0x4F9460, "Graph_SetNodeSize", setup)
    else:
        run(e, 0x4F9460, "Graph_SetNodeSize", setup)


@pytest.mark.parametrize("entries", [[(GTYPE, 0x240)], [(1, 5), (2, 6), (3, 7), (GTYPE, 9)], [(1, 5)]])
def test_get_node_size(entries):
    def setup(e):
        sizes(e, *entries)
        return (graph(e),)
    if any(t == GTYPE for t, _ in entries):
        run(e, 0x4F94D0, "Graph_GetNodeSize", setup, check_eax=True)
    else:
        errors(e, 0x4F94D0, "Graph_GetNodeSize", setup)


@pytest.mark.parametrize("cap,count,nodes", [(10, 10, 1), (10, 0, 1), (10, -3, 1), (10, 4, 0)])
def test_clear_nodes(cap, count, nodes):
    def setup(e):
        sizes(e, (GTYPE, NODE_SIZE))
        g = graph(e, cap=cap)
        for i in range(cap):
            u32(e, e.u32(g + 0x6C) + NODE_SIZE * i, i)
        if not nodes:
            u32(e, g + 0x6C, 0)
        return g, count
    run(e, 0x4F96E0, "Graph_ClearNodes", setup)


@pytest.mark.parametrize("nid", [-5, -1, 0, 3, 4, 9, 10, 11])
@pytest.mark.parametrize("nodes", [0, 1])
def test_get_node(nid, nodes):
    def setup(e):
        sizes(e, (GTYPE, NODE_SIZE))
        g = graph(e, cap=10)
        node(e, g, 3)
        node(e, g, 9)
        if not nodes:
            u32(e, g + 0x6C, 0)
        return g, nid
    run(e, 0x4F9720, "Graph_GetNode", setup, check_eax=True)


@pytest.mark.parametrize("type_,cap,nid,old", [
    (GTYPE, 0, 0, 0), (GTYPE, 0, 250, 0), (GTYPE, 100, 100, 1), (GTYPE, 100, 377, 1), (GTYPE2, 100, 101, 1)])
def test_grow(type_, cap, nid, old):
    def setup(e):
        sizes(e, (GTYPE, NODE_SIZE), (GTYPE2, 0x30))
        g = graph(e, type_=type_, cap=cap)
        if cap:
            for i in range(cap):
                u32(e, e.u32(g + 0x6C) + (NODE_SIZE if type_ == GTYPE else 0x30) * i + 4, i * 3)
        return g, nid
    run(e, 0x4F9620, "Graph_Grow", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("nid,cap", [(0, 10), (9, 10), (10, 10), (250, 0), (999, 100), (1000, 10), (5000, 10)])
def test_add_node(nid, cap, cw):
    def setup(e):
        sizes(e, (GTYPE, NODE_SIZE))
        g = graph(e, cap=cap)
        u32(e, g + 0x5C, 4)
        pos = e.alloc(24)
        d64(e, pos, 1.5, -2.25, 1e9)
        crit = e.alloc(0x100)
        e.write(crit, b"NODECRITERIA_DOOR | 4\0")
        return g, nid, pos, struct.unpack("<I", struct.pack("<f", 1.25))[0], \
            struct.unpack("<I", struct.pack("<f", 3.3))[0], 0x3333, crit
    run(e, 0x4F9510, "Graph_AddNode", setup, cw=cw, check_eax=True)


def test_add_node_used():
    def setup(e):
        sizes(e, (GTYPE, NODE_SIZE))
        g = graph(e, cap=10)
        node(e, g, 4)
        pos = e.alloc(24)
        return g, 4, pos, 0, 0, 0, pos
    errors(e, 0x4F9510, "Graph_AddNode", setup)


def nodes_setup(e, seed, attached, vals=None):
    rng = random.Random(seed)
    sizes(e, (GTYPE, NODE_SIZE))
    g = graph(e, rng, attached=attached, parent=rigid(e, rng, vals), cap=4)
    d64(e, g + 0x20, *[rng.choice(vals) if vals else rng.uniform(-9999, 9999) for _ in range(3)])
    return g, node(e, g, 2, rng, vals)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("attached", [0, 1])
@pytest.mark.parametrize("seed", range(16))
def test_get_node_pos(attached, seed, cw):
    def setup(e):
        g, n = nodes_setup(e, seed, attached, vals(seed))
        return e.alloc(24), n
    run(e, 0x4F9770, "Graph_GetNodePos", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("attached,rotate", [(0, 0), (0, 1), (1, 0), (1, 1)])
@pytest.mark.parametrize("seed", range(6))
def test_get_node_matrix(attached, rotate, seed, cw):
    def setup(e):
        g, n = nodes_setup(e, seed, attached, vals(seed))
        out = e.alloc(0x28)
        u32(e, out + 0x24, 5)
        return out, n, rotate | 0xFFFFFF00
    run(e, 0x4F98A0, "Graph_GetNodeMatrix", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("attached", [0, 1])
@pytest.mark.parametrize("check_z", [0, 1])
@pytest.mark.parametrize("seed", range(12))
def test_is_in_node(attached, check_z, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        g, n = nodes_setup(e, seed, attached)
        f32(e, n + 0x20, rng.uniform(-math.pi, math.pi), rng.choice([0.5, 1.0, 2.5]))
        c = e.alloc(24)
        e.call(0x4F9770, c, n)
        cx, cy, cz = struct.unpack("<3d", e.read(c, 24))
        r = 4096 * 3.5
        if seed == 0:
            p = (cx, cy, cz + 100)
        elif seed == 1:
            p = (cx + 10, cy, float("nan"))
        else:
            p = (cx + rng.uniform(-r, r), cy + rng.uniform(-r, r), cz + rng.uniform(-6000, 6000))
        d64(e, c, *p)
        tol = struct.unpack("<2I", struct.pack("<d", rng.choice([0.0, 0.25, -0.5])))
        return (c, n) + tol + (check_z | 0x1200,)
    run(e, 0x4F9A40, "Graph_IsInNode", setup, cw=cw, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("full", [0, 1])
def test_add_link(full):
    def setup(e):
        sizes(e, (GTYPE, NODE_SIZE))
        g = graph(e, cap=8)
        u32(e, g + 0x60, 3)
        a, b = node(e, g, 1), node(e, g, 6)
        if full:
            for i in range(64):
                u32(e, a + 0x13C + 4 * i, 0x100 + i)
        u32(e, b + 0x13C, 0x99, 0x98, 0, 0x97)
        return g, 1, 6, 0x1234
    run(e, 0x4F9B80, "Graph_AddLink", setup, check_eax=True)


@pytest.mark.parametrize("pos", [0, 5, 63, 64])
def test_link_free(pos):
    def setup(e):
        sizes(e, (GTYPE, NODE_SIZE))
        g = graph(e, cap=8)
        u32(e, g + 0x60, 3)
        a, b = node(e, g, 1), node(e, g, 6)
        link = e.alloc(0x18)
        list_add(e, g + 0x70, link)
        u32(e, link + 8, 1, 6, 7, g)
        if pos < 64:
            u32(e, a + 0x13C + 4 * pos, link)
        u32(e, b + 0x13C + 4 * 63, link)
        return (link,)
    run(e, 0x4F9C50, "GraphLink_Free", setup)


@pytest.mark.parametrize("free", [0, 17, 63, 64])
def test_node_add_link(free):
    def setup(e):
        n = e.alloc(NODE_SIZE)
        for i in range(64):
            u32(e, n + 0x13C + 4 * i, 0 if i == free else 0x100 + i)
        return n, 0x4242
    run(e, 0x4F9C10, "GraphNode_AddLink", setup)


# rows (T, 1, 1) and a vector (1, -1, 1): any other summation order loses T
ORDER_MATRIX = [T, 1.0, 1.0] * 3


def order_parent(e):
    p = e.alloc(0xA0)
    u16(e, p + 0x1C, RIGID)
    d64(e, p + 0x20, 0.0, 0.0, 0.0)
    f32(e, p + 0x70, *ORDER_MATRIX)
    return p


@pytest.mark.parametrize("cw", CWS)
def test_world_pos_order(cw):
    def setup(e):
        g = graph(e, attached=1, parent=order_parent(e))
        d64(e, g + 0x20, 1.0, -1.0, 1.0)
        return e.alloc(24), g
    run(e, 0x4F92E0, "Graph_GetWorldPos", setup, cw=cw)

    def setup2(e):
        out, g = setup(e)
        d64(e, out, 1.0, -1.0, 1.0)
        f32(e, e.u32(g + 0x14) + 0x70, T, T, T, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0)
        return g, out
    run(e, 0x4F93B0, "Graph_SetWorldPos", setup2, cw=cw)


@pytest.mark.parametrize("cw", CWS)
def test_node_pos_order(cw):
    def setup(e):
        sizes(e, (GTYPE, NODE_SIZE))
        g = graph(e, attached=1, parent=order_parent(e), cap=4)
        d64(e, g + 0x20, 0.0, 0.0, 0.0)
        n = node(e, g, 2)
        d64(e, n + 8, 1.0, -1.0, 1.0)
        return e.alloc(24), n
    run(e, 0x4F9770, "Graph_GetNodePos", setup, cw=cw)


@pytest.mark.parametrize("pos", [0, 17, 63, 64])
def test_node_remove_link(pos):
    def setup(e):
        n = e.alloc(NODE_SIZE)
        for i in range(64):
            u32(e, n + 0x13C + 4 * i, 0x4242 if i in (pos, pos + 1) else 0x100 + i)
        return n, 0x4242
    run(e, 0x4F9CA0, "GraphNode_RemoveLink", setup)
