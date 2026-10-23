import struct

import pytest

from test_sector_util import (RET, check, d64, f32, handler, logfake, mesh, model, new_emu, obj, pool, sector_object,
                              set_handler, set_view, u16, u32, with_cw)

E = None
SECTOR = 0x53
CUR = 0xA5E354
VIS = 0xA5E338


def emu():
    global E
    if E is None:
        E = new_emu()
        RET.clear()
        logfake(E, 0x4CFCB0, 1, effect=lambda e, a: RET.get(("lod", a[0]), 0))
        logfake(E, 0x497520, 3, derefs={1: 8, 2: 8})

        def fog(e, a):
            u32(e, a[0], 0x20000040)
            u32(e, a[1], 3)
        logfake(E, 0x4DA180, 2, derefs={0: 0, 1: 0}, effect=fog)
        E.draw = handler(E, "draw", 2)
        E.baseline = E.snapshot()
    return E


def square(y, x0, x1, z0, z1, flip=False):
    verts = [(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)]
    tris = [(0, 1, 2), (0, 2, 3)]
    if flip:
        tris = [(0, 2, 1), (0, 3, 2)]
    return verts, tris


def portal_mesh(e, specs):
    """specs: list of (id, verts, tris); builds one mesh with all the portals."""
    verts, tris, portals = [], [], []
    for pid, vs, ts in specs:
        fv, ft = len(verts), len(tris)
        verts += vs
        tris += [tuple(fv + i for i in t) for t in ts]
        portals.append((fv, len(vs), ft, len(ts), pid))
    return mesh(e, verts=verts, tris=tris, portals=portals)


def scene(e, pos=(0.0, 0.0, 0.0), matrix=(1, 0, 0, 0, 1, 0, 0, 0, 1)):
    m0 = portal_mesh(e, [(1, *square(200, -100, 100, -100, 100)),
                         (2, *square(150, 300, 500, -50, 50, True)),
                         (4, *square(600, -50, 50, -50, 50))])
    m1 = portal_mesh(e, [(1, *square(200, -100, 100, -100, 100, True)),
                         (3, *square(400, -80, 0, -60, 60))])
    m2 = portal_mesh(e, [(2, *square(150, 300, 500, -50, 50))])
    m3 = portal_mesh(e, [(3, *square(400, -80, 0, -60, 60, True))])
    s1 = obj(e, 0x148, SECTOR, model(e, [m1]), pos=pos, matrix=matrix, attached=0)
    s2 = obj(e, 0x148, SECTOR, model(e, [m2]), pos=pos, matrix=matrix, attached=0)
    s3 = obj(e, 0x148, SECTOR, model(e, [m3]), pos=pos, matrix=matrix, attached=0)
    so = sector_object(e, model(e, [m0]), [s1, s2, s3], pos=pos, matrix=matrix)
    u32(e, CUR, so)
    return so, [so, s1, s2, s3]


def rect(e, *v):
    p = e.alloc(4 * len(v))
    f32(e, p, *v)
    return p


VIEWS = [
    dict(),
    dict(pos=(30.0, -40.0, 10.0)),
    dict(pos=(0.0, 380.0, 0.0)),
    dict(pos=(20.0, 700.0, 5.0)),
    dict(pos=(-200.0, 100.0, 20.0), matrix=(0.8, 0.6, 0, 0, 0, 1, 0.6, -0.8, 0)),
]
ROT = (0.6, -0.8, 0, 0.8, 0.6, 0, 0, 0, 1)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("view", range(len(VIEWS)))
@pytest.mark.parametrize("k", [0, 1, 2])
@pytest.mark.parametrize("variant", ["plain", "rot", "clip", "closed"])
def test_portal_screen_rect(k, view, variant, cw):
    e = emu()

    def setup(e):
        set_view(e, **VIEWS[view])
        so, parts = scene(e, matrix=ROT if variant == "rot" else (1, 0, 0, 0, 1, 0, 0, 0, 1),
                          pos=(5.0, 7.0, -3.0) if variant == "rot" else (0.0, 0.0, 0.0))
        if variant == "closed":
            e.write(so + 0x14C + (1, 2, 4)[k], b"\0")
        lo, hi = ((330.0, 250.0), (340.0, 300.0)) if variant == "clip" else ((0.0, 0.0), (640.0, 480.0))
        return [so, 0, k, rect(e, *lo), rect(e, *hi), rect(e, 9, 9), rect(e, 9, 9)]
    check(e, with_cw(e, 0x4DD7F0, cw), with_cw(e, "Portal_GetScreenRect", cw), setup=setup)


def flood_setup(e, view, lods=(0, 0, 0, 0), clip=((0.0, 0.0), (640.0, 480.0))):
    set_view(e, **VIEWS[view])
    pool(e, VIS, 0x20, 20)
    so, parts = scene(e)
    for p, l in zip(parts, lods):
        RET[("lod", p)] = l
    return so, parts


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("view", range(len(VIEWS)))
@pytest.mark.parametrize("lods", [(0, 0, 0, 0), (0, -1, 0, 0), (0, 0, -1, 0)])
def test_flood_portals(view, lods, cw):
    e = emu()

    def setup(e):
        so, parts = flood_setup(e, view, lods)
        return [so, rect(e, 10.0, 20.0), rect(e, 600.0, 470.0), 0]
    check(e, with_cw(e, 0x4DD650, cw), with_cw(e, "Sector_FloodPortals", cw), setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("view", [0, 1, 4])
@pytest.mark.parametrize("pre", [0, 1, 2])
def test_add_visible(view, pre, cw):
    e = emu()

    def setup(e):
        so, parts = flood_setup(e, view)
        if pre:
            e.call(0x4DD4B0, parts[pre], rect(e, 100.0, 100.0), rect(e, 300.0, 200.0), 0)
            e.call(0x4DD4B0, parts[pre], rect(e, 150.0, 50.0), rect(e, 250.0, 300.0), 0)
        return [parts[pre], rect(e, 120.0, 90.0), rect(e, 310.0, 210.0), 0]
    check(e, with_cw(e, 0x4DD4B0, cw), with_cw(e, "Sector_AddVisible", cw), setup=setup, check_eax=False)


@pytest.mark.parametrize("same,view", [(0, 0), (1, 0), (1, 1)])
def test_draw_visible(same, view):
    e = emu()

    def setup(e):
        so, parts = flood_setup(e, 0)
        u16(e, 0x548694, SECTOR)
        set_handler(e, 4, SECTOR, e.draw)
        u32(e, 0xA538D0, so if view else 0)
        return [parts[0 if same else 2], rect(e, 1.0, 2.0), rect(e, 3.0, 4.0), 0]
    check(e, 0x4DD5D0, "Sector_DrawVisible", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("view", range(len(VIEWS)))
@pytest.mark.parametrize("mode", ["plain", "camera", "camera_skip", "nomodel", "nolod"])
def test_sector_object_render(view, mode, cw):
    e = emu()

    def setup(e):
        so, parts = flood_setup(e, view, (-1, 0, 0, 0) if mode == "nolod" else (0, 0, 0, 0))
        u16(e, 0x548694, SECTOR)
        set_handler(e, 4, SECTOR, e.draw)
        u32(e, 0xA538D0, so if mode.startswith("camera") else 0)
        u32(e, 0xA538CC, mode == "camera_skip")
        u32(e, 0xA538D4, parts[1])
        u32(e, 0xB03DF0, 2)
        if mode == "nomodel":
            u32(e, so + 0x6C, 0)
        return [so]
    check(e, with_cw(e, 0x4DD310, cw), with_cw(e, "SectorObject_Render", cw), setup=setup, check_eax=False)
