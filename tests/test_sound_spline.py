import random
import struct

import pytest

from emu import Emu
from test_menu_util import RET, add_child, check, cstr, handler, logfake, set_handler, task, task_system, u16, u32, f32
from test_vehicle_util import floatfake

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]
rng = random.Random(9)

# a fresh Emu with the original task system for the type registration
ei = Emu()
logfake(ei, 0x4C48C0, 0, ret=0)  # RigidDynCubeObj_GetType: the root type

e = Emu()
logfake(e, 0x4C7560, 2)  # DynCube_Remove
logfake(e, 0x4B0D10, 1)  # Mem_Free
logfake(e, 0x4DAA20, 1, masks={0: 0}, effect=lambda e, a: (u32(e, a[0], 3), 0x7000)[1])  # Light_GetActive
logfake(e, 0x4DA180, 2, masks={0: 0, 1: 0}, effect=lambda e, a: u32(e, a[0], 0x7100) or u32(e, a[1], 2))
logfake(e, 0x4CFC50, 0, ret=5)  # Mesh3D_GetDrawSplineMethod
logfake(e, 0x4C56F0, 0, ret=0x30)  # RigidDynCubeObj_GetEvent2
logfake(e, 0x4E5360, 0, ret=0x31)  # DynCubeObj_GetMaskVisibleEvent
logfake(e, 0x4E5610, 7)  # CellMask_FillTriangle
logfake(e, 0x4012A0, 3, masks={1: 0xFFFF}, effect=lambda e, a: e.alloc(0x5B8))
logfake(e, 0x4CEC10, 1, strings=(0,), ret=0x6100)  # VirModel_Find
logfake(e, 0x4C5710, 3)  # RigidDynCubeObj_Init
logfake(e, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
logfake(e, 0x4E4530, 10, strings=(9,), derefs={1: 24, 2: 24, 3: 40, 4: 40}, masks={5: 0xFF, 6: 0xFF})
floatfake(e, 0x4B32B0, 2, derefs={0: 24, 1: 24})  # Vec3d_Distance
VERTS = e.alloc(12 * 64)
logfake(e, 0x49A050, 6, ret=VERTS)  # SplineMesh_GetVerts
DRAW = handler(e, "draw", 1, derefs={0: 20})
MOVE = handler(e, "move", 2, derefs={1: 0x40})
VISIBLE = [1]
from test_menu_util import trap, LOG  # noqa: E402


def _visible(e):
    LOG[-1].append(("visible", e.arg(0)))
    u32(e, e.arg(1), VISIBLE[0])
    e.ret(0)


VIS = trap(e, "visible", _visible)
_tramps = {}


def with_cw(target, cw):
    addr = e.symbols[target] if isinstance(target, str) else target
    if (addr, cw) not in _tramps:
        p = e.fake_top
        e.fake_top += 32
        e.write(p + 16, struct.pack("<H", cw))
        e.write(p, b"\xd9\x2d" + struct.pack("<I", p + 16) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        _tramps[(addr, cw)] = p
        e.baseline = e.snapshot()
    return _tramps[(addr, cw)]


def run(orig, ours, setup=None, cw=None, em=None, **kw):
    em = em or e
    if cw is not None:
        orig, ours = with_cw(orig, cw), with_cw(ours, cw)
    RET.clear()
    kw.setdefault("check_eax", False)
    return check(em, orig, ours, setup=setup, normalized=True, **kw)


def init(e):
    task_system(e)
    e.write(0xA5E408, b"\x40")


def test_init_shutdown():
    run(0x4E41B0, "SplineObjSegment_Init", init, em=ei)
    run(0x4E43B0, "SplineObjSegment_Shutdown", lambda e: init(e) or e.call(0x4E41B0) and None, em=ei)
    run(0x4E5350, "SplineObjSegment_GetType", lambda e: u16(e, 0xA5E3F4, 0x77), check_eax=True, eax_mask=0xFFFF)
    run(0x4E4280, "Task_YesHandler", lambda e: (0, e.alloc(4)))


@pytest.mark.parametrize("bufs", [(0, 0), (0x5000, 0), (0x5000, 0x6000)])
def test_delete(bufs):
    def setup(e):
        o = e.alloc(0x5B8)
        u32(e, o + 0xF0, 0x4444)
        u32(e, o + 0x5A0, bufs[0], 7, bufs[1], 9)
        return (o,)
    run(0x4E3F80, "SplineObjSegment_Delete", setup)


@pytest.mark.parametrize("n", [0, 1, 3])
def test_render(n):
    def setup(e):
        o = task(e, 0x5B8, 0x50)
        e.write(o + 0x20, struct.pack("<3d", 1.5, -2.25, 100.125))
        for i in range(n):
            add_child(e, o, task(e, 0x40, 0x51 + (i & 1)))
        set_handler(e, 0x30, 0x51, MOVE)
        set_handler(e, 0x30, 0x52, 0)
        u32(e, 0xA94E84 + 4 * 5, DRAW)
        return (o,)
    run(0x4E4290, "SplineObjSegment_Render", setup)


@pytest.mark.parametrize("rigid", [0, 1])
@pytest.mark.parametrize("magic", [False, True])
def test_new(rigid, magic):
    def setup(e):
        u16(e, 0xA5E3F4, 0x55)
        a, b = e.alloc(24), e.alloc(24)
        e.write(a, struct.pack("<3d", 10.5, 20.25, 30.0))
        if magic:
            u32(e, a, 0, 0xC113F2DE)
        e.write(b, struct.pack("<3d", -4.0, 7.5, 1.0))
        ra, rb = e.alloc(40), e.alloc(40)
        f32(e, ra, *range(10))
        f32(e, rb, *range(20, 30))
        RET[0x4B32B0] = 12.345678
        return 0x1000, 0x2000, a, b, ra, rb, 0x100 | rigid, 0x201, 6, 0x33, cstr(e, b"spline_1")
    run(0x4E43D0, "SplineObjSegment_New", setup, check_eax=True)


def model(e, parts):
    """model[lod] -> +0xC -> +0xC -> +0x2C -> MeshPart[2] (tri count, tris at +0x10)."""
    arr = e.alloc(0x40)
    for j, tris in enumerate(parts):
        t = e.alloc(8 * max(len(tris), 1))
        for i, (a, b, c) in enumerate(tris):
            u16(e, t + 8 * i, a, b, c, 0)
        u32(e, arr + 0x20 * j, len(tris))
        u32(e, arr + 0x20 * j + 0x10, t)
    m3 = e.alloc(0x30)
    u32(e, m3 + 0x2C, arr)
    m2 = e.alloc(0x10)
    u32(e, m2 + 0xC, m3)
    m1 = e.alloc(0x10)
    u32(e, m1 + 0xC, m2)
    models = e.alloc(8)
    u32(e, models, 0, m1)
    return models


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", ["ok", "nomodel", "hidden", "nohandler"])
def test_draw_mask(cw, case):
    verts = [rng.uniform(-30, 30) for _ in range(3 * 64)]
    parts = [[tuple(rng.randrange(64) for _ in range(3)) for _ in range(5)],
             [tuple(rng.randrange(64) for _ in range(3)) for _ in range(3)]]

    def setup(e):
        f32(e, VERTS, *verts)
        o = task(e, 0x5B8, 0x50)
        e.write(o + 0x20, struct.pack("<3d", 1001.5, -2002.25, 3.0))
        u32(e, o + 0xA4, 1)
        if case != "nomodel":
            u32(e, o + 0x6C, model(e, parts))
        set_handler(e, 0x31, 0x50, 0 if case == "nohandler" else VIS)
        VISIBLE[0] = 0 if case == "hidden" else 1
        ctr = e.alloc(24)
        e.write(ctr, struct.pack("<3d", 990.25, -2010.75, 0))
        view = e.alloc(16)
        u32(e, view, 0x9000, 256, ctr)
        f32(e, view + 12, 40.0)
        return o, view
    run(0x4E3FC0, "SplineObjSegment_DrawMask", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_grow_radius(cw):
    nverts = 40
    verts = [rng.uniform(-50, 50) for _ in range(3 * nverts)]
    tris = [tuple(rng.randrange(nverts) for _ in range(3)) for _ in range(12)]
    # node 0 root (tris 0-1), children 1, 2 (2 has child 3)
    nodes = [(0, 2, 1, 0), (2, 3, 0, 2), (5, 0, 3, 0), (5, 7, 0, 0)]

    def setup(e):
        vp = e.alloc(12 * nverts)
        f32(e, vp, *verts)
        tp = e.alloc(8 * len(tris))
        for i, t in enumerate(tris):
            u16(e, tp + 8 * i, *t, 0)
        mesh = e.alloc(0x14)
        u32(e, mesh + 0x10, tp)
        arr = e.alloc(0x18 * len(nodes))
        for i, (first, count, child, nxt) in enumerate(nodes):
            f32(e, arr + 0x18 * i, *[rng.uniform(-5, 5) for _ in range(3)], 99.0)
            u16(e, arr + 0x18 * i + 0x10, first, count, child, nxt)
        pp = e.alloc(4)
        u32(e, pp, arr)
        u32(e, 0xA5E3EC, vp)
        u32(e, 0xA5E3F0, pp)
        u32(e, 0xA5E3F8, mesh)
        u32(e, 0xA5E400, 1)
        u32(e, 0xA5E374, arr + 0x18 * 3, arr)
    st = rng.getstate()

    def s(e):
        rng.setstate(st)
        return setup(e)
    run(0x4E51C0, "SplineObjSegment_GrowRadius", s, cw=cw)
