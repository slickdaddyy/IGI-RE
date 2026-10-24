import random
import struct

import pytest

from emu import Emu
from test_sysmisc_util import Fakes, check, f32, f64, u32

HANDLERS = 0xA96AE0
TYPE, EVENT = 0x54E83C, 0x54E83E
PARAMS, DOOR, VIEW, STAIR = 0xA7B2CC, 0xA7B2D0, 0xA7B2D4, 0xA7B2D8

e = Emu()
fk = Fakes(e)
ST = {"nodes": {}, "ints": [], "id": -1}


def _read_int(em, a):
    v = ST["ints"].pop(0) if ST["ints"] else 0x5A5A5A5A
    em.write(a[1], u32(v))
    return 1


def _node_pos(em, a):
    em.write(a[0], em.read(a[1] + 8, 24))


fk.add(0x401900, 5, ret=0x1C3, fmt=lambda em, a: (em.read_cstr(a[0]), a[1], a[2] & 0xFFFF, a[3], a[4]))
fk.add(0x401400, 3, fmt=lambda em, a: (a[0] & 0xFFFF, a[1] & 0xFF, em.normalize(a[2])))
fk.add(0x401530, 3, fmt=lambda em, a: (a[0] & 0xFFFF, a[1], em.normalize(a[2])))
fk.add(0x401A20, 1, fmt=lambda em, a: (a[0] & 0xFFFF,))
fk.add(0x4017C0, 1, ret=0x37, fmt=lambda em, a: (a[0] & 0xFFFF,))
fk.add(0x4018C0, 1, fmt=lambda em, a: (a[0] & 0xFF,))
fk.add(0x4C1830, 1)
for _a, _v in ((0x4F1A60, 0x32), (0x4F1A70, 0x33), (0x4F9220, 0x34), (0x4F9230, 0x35), (0x4F9240, 0x36),
               (0x4F9250, 0x38), (0x4F9260, 0x39), (0x4F9280, 0x3A), (0x4F0F80, 0x44), (0x4F1000, 0x45),
               (0x4F9200, 0x7B0000), (0x4F9210, 0x10)):
    fk.add(_a, 0, ret=_v)
fk.add(0x4F1980, 2, ret=0x7A0000)
fk.add(0x4F1A80, 6, fmt=lambda em, a: (a[0], a[2] - a[1], a[3], a[4], a[5]))
fk.add(0x4F9460, 2, fmt=lambda em, a: (a[0] & 0xFFFF, a[1]))
fk.add(0x5186C0, 1, ret=lambda em, a: {b"NODECRITERIA_DOOR": 1, b"NODECRITERIA_VIEW": 2}.get(em.read_cstr(a[0]), 4),
       fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x518720, 1)
fk.add(0x4F9DF0, 3, fmt=lambda em, a: (em.read_cstr(a[0]), a[1], em.normalize(a[2])))
fk.add(0x4F90B0, 2)
fk.add(0x4F9180, 2)
fk.add(0x4F1050, 1, ret=lambda em, a: ST["id"])
fk.add(0x4F1240, 1, ret=9)
fk.add(0x4F1090, 2)
fk.add(0x4B0D60, 1)
fk.add(0x4B0D90, 0)
fk.add(0x4B0D10, 1)
fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(a[0]))
fk.add(0x4F9CD0, 3)
fk.add(0x4FA9D0, 3)
fk.add(0x4F9EB0, 5, fmt=lambda em, a: (a[0], em.read_cstr(a[1]), a[2], a[3], a[4]))
fk.add(0x4FA850, 2, ret=_read_int, fmt=lambda em, a: (a[0],))
fk.add(0x4F9720, 2, ret=lambda em, a: ST["nodes"].get(a[1], 0))
fk.add(0x4F9770, 2, fn=_node_pos, fmt=lambda em, a: (a[1],))
fk.add(0x518550, 0)
DELETE = fk.handler(1)


def graph(em, cap=3, routes=None, fill=0xAA):
    g = em.alloc(0xC0)
    em.write(g, bytes([fill]) * 0xC0)
    em.write(g + 0x58, u32(cap))
    em.write(g + 0xB8, u32(routes or 0))
    return g


def test_init():
    check(e, fk, 0x508FA0, "AIGraph_Init", check_eax=False)


@pytest.mark.parametrize("event", [0xFF, 5])
def test_shutdown(event):
    def setup(em):
        em.write(DOOR, u32(1) + u32(0x10) + u32(0x200))
        em.write(PARAMS, u32(0x7A0000))
        em.write(EVENT, bytes([event]))
        em.write(TYPE, struct.pack("<H", 0x1C3))
    check(e, fk, 0x509650, "AIGraph_Shutdown", setup=setup, check_eax=False)


@pytest.mark.parametrize("event", [0xFF, 5])
def test_get_event(event):
    def setup(em):
        em.write(EVENT, bytes([event]))
    check(e, fk, 0x5096A0, "AIGraph_GetEvent", setup=setup, eax_mask=0xFF)


def test_create():
    def setup(em):
        return graph(em), 0x1234
    check(e, fk, 0x509570, "AIGraph_Create", setup=setup, check_eax=False)


@pytest.mark.parametrize("routes", [False, True])
def test_delete(routes):
    def setup(em):
        em.write(HANDLERS + 4 * (2 * 0x180 + 0x10), u32(DELETE))
        return (graph(em, routes=em.alloc(16) if routes else 0),)
    check(e, fk, 0x509610, "AIGraph_Delete", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("vals", [(50.0, 150.0), (100.0, 100.0001), (409600.0, -5.0), (1e300, 3.3)])
@pytest.mark.parametrize("tid", [-1, 4])
def test_params_changed(vals, tid, cw):
    def setup(em):
        ST["id"] = tid
        g = graph(em)
        em.write(g + 0x80, f64(*vals))
        return g, 0x77
    check(e, fk, 0x5093C0, "AIGraph_ParamsChanged", setup=setup, check_eax=False, cw=cw)


def test_get_param_table():
    def setup(em):
        em.write(PARAMS, u32(0x7A0000))
        return 0, em.alloc(4)
    check(e, fk, 0x509440, "AIGraph_GetParamTable", setup=setup, check_eax=False)


@pytest.mark.parametrize("cap", [0, 3, 7])
def test_load_routes(cap):
    def setup(em):
        s = em.alloc(4)
        em.write(s, u32(0x4440))
        return graph(em, cap, routes=0x5000), s
    check(e, fk, 0x509450, "AIGraph_LoadRoutes", setup=setup, check_eax=False)


@pytest.mark.parametrize("cap,routes", [(0, 0), (1, 0), (4, 0), (4, 0x5000)])
def test_save_routes(cap, routes):
    def setup(em):
        s = em.alloc(4)
        em.write(s, u32(0x4440))
        return graph(em, cap, routes=routes), s
    check(e, fk, 0x509520, "AIGraph_SaveRoutes", setup=setup, check_eax=False)


@pytest.mark.parametrize("ints", [[0, 0], [2, 1, 2, 3, 4, 1, 5, 6], [0, 3, 1, 1, 2, 2, 3, 3], [1, 9, 9, 0], [0, 1, 7, 7]])
def test_read_node(ints):
    def setup(em):
        ST["ints"] = list(ints)
        arg = em.alloc(8)
        em.write(arg, u32(0x3330) + u32(0x4440))
        return 0, arg
    check(e, fk, 0x509480, "AIGraph_ReadNode", setup=setup, check_eax=False)


@pytest.mark.parametrize("cap,routes", [(0, 0), (1, 0x5000), (2, 0), (5, 0x5000)])
def test_read_route_table(cap, routes):
    def setup(em):
        v = em.alloc(20)
        em.write(v, u32(0x4440) + u32(graph(em, cap, routes=routes)) + u32(0) + u32(0) + u32(10))
        return (v,)
    check(e, fk, 0x5092A0, "AIGraph_ReadRouteTable", setup=setup, check_eax=False)


@pytest.mark.parametrize("cap,routes", [(-1, 0), (0, 0x5000), (1, 0), (2, 0x5000), (5, 0)])
def test_alloc_routes(cap, routes):
    def setup(em):
        return (graph(em, cap, routes=routes),)
    check(e, fk, 0x5092E0, "AIGraph_AllocRoutes", setup=setup, check_eax=False)
    check(e, fk, 0x5093B0, "AIGraph_OnAlloc", setup=setup, check_eax=False)


@pytest.mark.parametrize("cap", [0, 1, 4])
def test_clear_routes(cap):
    def setup(em):
        return (graph(em, cap, routes=em.alloc(8 * max(cap * cap, 1))),)
    check(e, fk, 0x509340, "AIGraph_ClearRoutes", setup=setup, check_eax=False)


@pytest.mark.parametrize("a,b", [(0, 0), (2, 1), (1, 3), (-1, 2)])
def test_get_route(a, b):
    def setup(em):
        return graph(em, 5, routes=0x5000), a, b
    check(e, fk, 0x509390, "AIGraph_GetRoute", setup=setup)


@pytest.mark.parametrize("routes", [False, True])
@pytest.mark.parametrize("nxt", [-1, 0, 3])
def test_get_step(routes, nxt):
    def setup(em):
        r = em.alloc(8 * 9)
        em.write(r + 8 * (3 * 2 + 1), u32(nxt) + f32(1.5))
        return graph(em, 3, routes=r if routes else 0), 1, 2
    check(e, fk, 0x5096C0, "AIGraph_GetStep", setup=setup)


def test_getters():
    def setup(em):
        em.write(DOOR, u32(0x11) + u32(0x22) + u32(0x44))
        em.write(TYPE, struct.pack("<H", 0x1C3))
    for orig, ours in ((0x509860, "AIGraph_GetDoorCriteria"), (0x509870, "AIGraph_GetViewCriteria"),
                       (0x509880, "AIGraph_GetStairCriteria")):
        check(e, fk, orig, ours, setup=setup)
    check(e, fk, 0x5098A0, "AIGraph_GetType", setup=setup, eax_mask=0xFFFF)


@pytest.mark.parametrize("flags,mask", [(0, 1), (5, 4), (5, 2), (0x80000000, 0x80000000)])
def test_has_criteria(flags, mask):
    def setup(em):
        n = em.alloc(0x40)
        em.write(n + 0x30, u32(flags))
        return n, mask
    check(e, fk, 0x509890, "GraphNode_HasCriteria", setup=setup, eax_mask=0xFF)


def node(em, pos, radius, flags=0):
    n = em.alloc(0x40)
    em.write(n, u32(0) + u32(0) + f64(*pos) + f32(0.0, radius, 0.0) + u32(0) + u32(flags))
    return n


def nearest_case(seed):
    rng = random.Random(seed)
    cap = rng.choice([0, 1, 2, 4, 8, 12])
    nodes = []
    for i in range(cap):
        if rng.random() < 0.15:
            nodes.append(None)
            continue
        pos = tuple(rng.uniform(-50000, 50000) for _ in range(3))
        if rng.random() < 0.3:
            pos = (rng.uniform(-3000, 3000), rng.uniform(-3000, 3000), rng.uniform(-100, 100))
        nodes.append((pos, rng.choice([0.0, 0.25, 0.5, 1.0, rng.uniform(0, 3)]),
                      rng.choice([0, 0, 1, 2, 3, 4])))
    return cap, nodes


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(30))
def test_find_nearest(seed, cw):
    cap, nodes = nearest_case(seed)

    def setup(em):
        em.write(VIEW, u32(2))
        ST["nodes"] = {i: node(em, *n) for i, n in enumerate(nodes) if n}
        pos = em.alloc(24)
        em.write(pos, f64(0.0, 0.0, 0.0))
        out = em.alloc(4)
        em.write(out, u32(0xDEAD))
        return out, graph(em, cap), pos
    check(e, fk, 0x5096F0, "AIGraph_FindNearest", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_find_nearest_order(cw):
    """Distances given in an order that exercises every insert position."""
    dists = [500.0, 400.0, 600.0, 300.0, 450.0, 700.0, 100.0, 100.0, 50.0]

    def setup(em):
        em.write(VIEW, u32(2))
        ST["nodes"] = {i: node(em, (d, 0.0, 0.0), 0.0) for i, d in enumerate(dists)}
        pos = em.alloc(24)
        em.write(pos, f64(0.0, 0.0, 0.0))
        return em.alloc(4), graph(em, len(dists)), pos
    check(e, fk, 0x5096F0, "AIGraph_FindNearest", setup=setup, cw=cw)


def test_shutdown_modules():
    def setup(em):
        em.write(EVENT, bytes([3]))
    check(e, fk, 0x508F90, "AllTaskTypes_Close", setup=setup, check_eax=False)


@pytest.mark.parametrize("dists", [[5e7, 6e7], [5e7, 100.0, 4.1e7]])
def test_find_nearest_far(dists):
    def setup(em):
        em.write(VIEW, u32(2))
        ST["nodes"] = {i: node(em, (d, 0.0, 0.0), 0.0) for i, d in enumerate(dists)}
        pos = em.alloc(24)
        em.write(pos, f64(0.0, 0.0, 0.0))
        return em.alloc(4), graph(em, len(dists)), pos
    check(e, fk, 0x5096F0, "AIGraph_FindNearest", setup=setup)
