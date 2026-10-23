import random
import struct

import pytest

from emu import compare
from test_d3d_util import D3DEmu, check, logged, pyfake, trap, u32
from test_dxinit_util import LISTS, check_cw, device, f32, f64, noise, set_view, vertex_buffer

e = D3DEmu()

POLY = [0]
pyfake(e, 0x4012A0, "Task_New", 3, before=lambda e: POLY[0])
pyfake(e, 0x401AE0, "TaskRef_Add", 1, ret=0x5555)
pyfake(e, 0x401B20, "TaskRef_Remove", 1)
pyfake(e, 0x4C6890, "DynCube_ListAdd", 3)
pyfake(e, 0x4C6840, "DynCube_ListRemove", 3)
pyfake(e, 0x401900, "TaskType_New", 2, ret=0x7FFF0077,
       before=lambda e: (None, (e.arg(2) & 0xFFFF, e.arg(3), e.arg(4))))
pyfake(e, 0x401400, "TaskType_SetHandler", 0,
       before=lambda e: (None, (e.arg(0) & 0xFFFF, e.arg(1) & 0xFF, e.normalize(e.arg(2)))))
pyfake(e, 0x401530, "TaskType_SetHandler2", 0,
       before=lambda e: (None, (e.arg(0) & 0xFFFF, e.arg(1), e.normalize(e.arg(2)))))
pyfake(e, 0x401A20, "TaskType_Free", 0, before=lambda e: (None, e.arg(0) & 0xFFFF))
pyfake(e, 0x4978D0, "Primitive_Register", 1, ret=0x21)
pyfake(e, 0x4978E0, "Primitive_Unregister", 1)
pyfake(e, 0x497860, "Primitive_Draw", 0, before=lambda e: (None, (e.read(e.arg(0) + 4, 4), e.read(e.arg(0) + 12, 4))))
pyfake(e, 0x4B0EC0, "RenderMode_SetMethod", 3)
pyfake(e, 0x4B7450, "PtrList_Clear", 2)
pyfake(e, 0x4B72E0, "SortRender_Add", 3, before=lambda e: e.alloc(e.arg(1)))
PTRLIST_N = [0]


def ptrlist_add(e):
    PTRLIST_N[0] += 1
    return 0x40 + PTRLIST_N[0]


pyfake(e, 0x4B7430, "PtrList_Add", 2, before=ptrlist_add)

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]


def fbits(v):
    return struct.unpack("<I", f32(v))[0]


def poly_list(e, rng, count=4, texture_runs=(1,), lit=1, flags=0, owner=0):
    p = e.alloc(0xB8)
    e.write(p, noise(0xB8, rng.randrange(30)))
    e.write(p + 0x20, f64(*[rng.uniform(-2e5, 2e5) for _ in range(3)]))
    e.write(p + 0x70, f32(*[rng.uniform(-1, 1) for _ in range(9)]) + struct.pack("<i", 77))
    tris = e.alloc(0x50 * max(count, 1))
    texs = [0] + [e.alloc(0x24) for _ in range(3)]
    for t in texs[1:]:
        u32(e, t + 0x20, t + 0x1000)
    i = 0
    run = 0
    while i < max(count, 1):
        tex = texs[rng.choice(texture_runs)] if texture_runs else 0
        n = rng.choice([1, 2, 5, 70])
        for _ in range(n):
            if i >= max(count, 1):
                break
            t = tris + 0x50 * i
            vs = [e.alloc(12) for _ in range(3)]
            for v in vs:
                e.write(v, f32(*[rng.uniform(-500, 500) for _ in range(3)]))
            cs = [e.alloc(12) for _ in range(3)]
            for c in cs:
                e.write(c, f32(*[rng.uniform(0, 1.2) for _ in range(3)]))
            e.write(t, struct.pack("<6I", *vs, *cs) + f32(*[rng.uniform(0, 1) for _ in range(6)]) +
                    f32(*[rng.uniform(0, 1) for _ in range(3)]) + f32(0, 0, 1) + struct.pack("<II", tex, 0))
            i += 1
        run += 1
    u32(e, p + 0x6C, tris)
    u32(e, p + 0x9C, count)
    u32(e, p + 0xA0, flags)
    u32(e, p + 0xA4, lit)
    u32(e, p + 0xB4, owner)
    return p


def lights(e, rng, n):
    base = e.alloc(0x40 * max(n, 1))
    order = e.alloc(4 * max(n, 1))
    idx = list(range(n))
    rng.shuffle(idx)
    for i in range(n):
        l = base + 0x30 * i
        e.write(l, f32(*[rng.uniform(-1, 1) for _ in range(3)], *[rng.uniform(0, 1) for _ in range(3)]) +
                noise(0x18, i))
        e.write(l + 0x25, bytes([rng.randrange(2)]))
    e.write(order, struct.pack("<%di" % n, *idx))
    e.write(0xB041E0, struct.pack("<III", base, order, n))
    u32(e, 0xB041F0, 0x30)
    e.write(0xB818AC, f32(*[rng.uniform(0, 0.6) for _ in range(3)]))
    e.write(0xB818EC, f32(*[rng.uniform(0, 1) for _ in range(3)]))
    e.write(0xB46D04, f32(rng.uniform(0, 0.5)))


# --- small ones -------------------------------------------------------------------------

@pytest.mark.parametrize("in_world", [0, 1])
@pytest.mark.parametrize("owner", [0, 0x3333])
def test_polylist_delete(in_world, owner):
    def setup(e):
        p = poly_list(e, random.Random(1), owner=owner)
        u32(e, p + 0xA8, in_world)
        return (p,)
    check(e, 0x49A350, "PolyList_Delete", setup=setup, check_eax=False)


def test_polylist_draw_item():
    def setup(e):
        u32(e, 0xC311BC, 0x29)
        item = e.alloc(4)
        u32(e, item, 0x4444)
        return (item,)
    check(e, 0x49A390, "PolyList_DrawItem", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("count", [0, 3])
def test_polylist_render(seed, count, cw):
    def setup(e):
        rng = random.Random(seed)
        set_view(e, rng)
        return (poly_list(e, rng, count=count),)
    check_cw(e, 0x49A3C0, "PolyList_Render", cw, setup=setup, check_eax=False)


def test_polylist_set_pos():
    def setup(e):
        p = poly_list(e, random.Random(2))
        v = e.alloc(24)
        e.write(v, f64(1.5, -2.25, 3e9))
        return p, v
    compare(e, 0x49A430, "PolyList_SetPos", setup=setup, check_eax=False)


@pytest.mark.parametrize("owner", [0, 0x2222])
@pytest.mark.parametrize("in_world", [0, 1])
def test_polylist_new(owner, in_world):
    def setup(e):
        POLY[0] = e.alloc(0xB8)
        e.write(POLY[0], noise(0xB8, 5))
        u32(e, 0x543C64, 0x0123)
        pos = e.alloc(24)
        e.write(pos, f64(1.0, 2.0, 3.0))
        rot = e.alloc(40)
        e.write(rot, noise(40, 7))
        return 0x9999, owner, pos, rot, 0x7777, 12, 0x18, in_world, 1, 0x55, 0x66
    check(e, 0x49A450, "PolyList_New", setup=setup)


def test_polylist_set_unk98():
    def setup(e):
        return poly_list(e, random.Random(3)), 0xABCDEF
    compare(e, 0x49A510, "PolyList_SetUnk98", setup=setup, check_eax=False)


def test_polylist_init():
    def setup(e):
        PTRLIST_N[0] = 0
        u32(e, 0x5CA0E0, 0x3)
        device(e)
    check(e, 0x49A520, "PolyList_Init", setup=setup, check_eax=False)


def test_polylist_close():
    def setup(e):
        device(e)
        e.write(0x7205A0, struct.pack("<4I", 4, 5, 6, 7))
        e.write(0x72057C, struct.pack("<I", 9))
        u32(e, 0x543C64, 0x7FFF0011)
        u32(e, 0xC311BC, 0x33)
    check(e, 0x49B260, "PolyList_Close", setup=setup, check_eax=False)


def test_polylist_set_stages():
    check(e, 0x49A5E0, "PolyList_SetStagesCB", setup=lambda e: (device(e), None)[1], check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("n", [0, 1, 5])
def test_polylist_set_lights(seed, n, cw):
    def setup(e):
        rng = random.Random(seed)
        device(e)
        lights(e, rng, n)
        e.write(0xBCABEC, f32(rng.uniform(0.5, 2)))
        u32(e, 0x5470B4, rng.randrange(2))
        u32(e, 0xBCAD78, 1)
        if seed == 3:
            e.write(0xB818AC, f32(1.5, -0.5, 0.5019608))
            e.write(0xB818EC, f32(-0.25, 1.75, 0.0019608))
    check_cw(e, 0x49A6E0, "PolyList_SetLightsCB", cw, setup=setup, check_eax=False)


# --- PolyList_Draw --------------------------------------------------------------------------

def draw_scene(e, rng, count, runs, lit, flags, used, owner_kind, lock_ret=0, same_state=False):
    set_view(e, rng)
    device(e)
    lights(e, rng, rng.randrange(4))
    u32(e, 0xBCADC0, fbits(rng.uniform(50, 300)))
    nearfn = trap(e, "near", logged("near", 1, pop=0, before=lambda e: (None, e.read(e.arg(0), 4))))
    u32(e, 0x6E5BA8, 5)
    u32(e, 0xA94E84 + 5 * 4, nearfn)
    for k, (g, glob) in enumerate(zip(LISTS[:3], (0x72057C, 0x7205A4, 0x7205A8))):
        lst = e.u32(g)
        idx = 2 + k
        u32(e, glob, idx)
        u32(e, lst + 4, idx if same_state else 0)
        u32(e, lst + 0x14 + 4 * idx, trap(e, "state%d" % k, logged("state%d" % k, 1, pop=0)))
    owner = 0
    if owner_kind:
        task = e.alloc(0x20)
        e.write(task + 0x1C, struct.pack("<H", 0x41))
        owner = e.alloc(4)
        u32(e, owner, task if owner_kind > 1 else 0)
        e.write(0x54863A, b"\x02")
        h = trap(e, "handler", logged("handler", 2, pop=0, before=lambda e: (None, e.read(e.arg(1), 24))))
        u32(e, 0xA96AE0 + 4 * (2 * 0x180 + 0x41), h if owner_kind == 3 else 0)
    p = poly_list(e, rng, count, runs, lit, flags, owner)
    vb, buf = vertex_buffer(e, 0x18 * 1000, lock_ret=lock_ret)
    e.write(0xBCAD80, struct.pack("<Iii", vb, 1000, used))
    msg = e.alloc(16)
    u32(e, msg + 12, p)
    return (msg,)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("count,runs", [(0, (1,)), (1, (0,)), (6, (0, 1, 2)), (70, (1,)), (140, (0, 3))])
@pytest.mark.parametrize("lit,flags", [(0, 0), (1, 8), (1, 0x10), (1, 0x18)])
def test_polylist_draw(seed, count, runs, lit, flags, cw):
    def setup(e):
        rng = random.Random(seed)
        return draw_scene(e, rng, count, runs, lit, flags, rng.choice([0, 500, 900]), seed)
    check_cw(e, 0x49A9B0, "PolyList_Draw", cw, setup=setup, check_eax=False)


@pytest.mark.parametrize("used", [0, 900])
@pytest.mark.parametrize("same", [False, True])
def test_polylist_draw_lock_fails(used, same):
    def setup(e):
        return draw_scene(e, random.Random(5), 4, (1,), 1, 0, used, 3, lock_ret=0x80004005, same_state=same)
    check(e, 0x49A9B0, "PolyList_Draw", setup=setup, check_eax=False)
