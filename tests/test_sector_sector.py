import struct

import pytest

from test_sector_util import (LOG, MAGIC, RET, RIGID, SECTOROBJ, add_child, check, d64, f32, handler, logfake, logger,
                              mesh, model, new_emu, obj, pool, sector_object, set_handler, task, trap, u16, u32,
                              with_cw)

E = None
SECTOR = 0x53
OTHER = 0x60
LINKS = 0xA5E318
FREE_EV = 0x30


def emu():
    global E
    if E is None:
        E = new_emu()
        e = E

        def slot9(e):
            o, q = e.arg(0), e.arg(1)
            frm, to = struct.unpack("<2I", e.read(q, 8))
            hit = e.u32(q + 0x20)
            if LOG:
                LOG[-1].append(("slot9", o, e.read(frm, 24), e.read(to, 24), e.read(q + 8, 5),
                                e.read(q + 0x10, 16), e.read(q + 0x24, 0), e.read(hit, 0x20), e.u32(hit + 0x58)))
            if RET.get(("hit", o)):
                u32(e, hit, RET[("hit", o)])
                u32(e, hit + 0x58, 1)
            e.ret(0)
        e.slot9 = trap(e, "slot9", slot9)
        e.freeh = handler(e, "free", 2, derefs={1: 4})

        def coll(e, a):
            q = a[2]
            u32(e, q + 8, RET.get("q8", 0), RET.get("qC", 0))
        logfake(e, 0x4CE090, 6, derefs={2: 8}, masks={1: 0xFF}, effect=coll)
        e.draw = trap(e, "drawrigid", logger(e, "drawrigid", 1, derefs={0: 20}))
        e.place = handler(e, "place", 2, derefs={1: 0x40})
        e.objdraw = handler(e, "objdraw", 2)
        e.baseline = e.snapshot()
    return E


def magic(e, n=2, fmt=3, center=(0.0, 0.0, 0.0), radius=10.0):
    return obj(e, 0xF0, MAGIC, model(e, [mesh(e, fmt, center=center, radius=radius) for _ in range(n)]))


def build(e, sectors=True):
    """A sector object with two sectors, attached magic objects and children of several kinds."""
    att = []
    for k in range(3):
        a = task(e, 0x20)
        m1 = magic(e, 2, center=(10.0 * k, 5.0, 1.0), radius=20.0 + k)
        m2 = magic(e, 1, fmt=0, center=(-30.0, 2.0 * k, 4.0), radius=5.0)
        add_child(e, m1, magic(e, 3, center=(100.0, 50.0, 0.0), radius=30.0))
        add_child(e, a, m1)
        add_child(e, a, m2)
        att.append(a)
    s1 = obj(e, 0x148, SECTOR, model(e, [mesh(e, center=(500.0, 0.0, 0.0), radius=200.0)] * 2), attached=att[1])
    s2 = obj(e, 0x148, SECTOR, model(e, [mesh(e, center=(-300.0, 100.0, 0.0), radius=150.0)]), attached=att[2])
    so = sector_object(e, model(e, [mesh(e, center=(0.0, 0.0, 0.0), radius=400.0)] * 3),
                       [s1, s2] if sectors else [])
    u32(e, so + 0xE8, att[0])
    r1 = obj(e, 0xF0, RIGID, model(e, [mesh(e)] * 2), attached=task(e, 0x20))
    add_child(e, e.u32(r1 + 0xE8), magic(e, 1))
    add_child(e, e.u32(r1 + 0xE8), obj(e, 0xF0, RIGID, model(e, [mesh(e)])))
    r2 = obj(e, 0xF0, RIGID, model(e, [mesh(e, fmt=0)]))
    r3 = obj(e, 0xF0, RIGID, 0)
    other = task(e, 0x20, OTHER)
    add_child(e, other, obj(e, 0xF0, RIGID, model(e, [mesh(e)] * 3)))
    inner = sector_object(e, model(e, [mesh(e)]))
    for c in (r1, r2, r3, other, inner):
        add_child(e, so, c)
    return so, [s1, s2], att


def first(e, t):
    h = e.u32(t + 8)
    return h if e.u32(h) else 0


@pytest.mark.parametrize("fn,addr,args", [
    ("SectorObject_CountMagicLods", 0x4DBF30, "magic"),
    ("SectorObject_CountRigidLods", 0x4DBFA0, "rigid"),
    ("SectorObject_ClearMagicRefs", 0x4DC2F0, "magic"),
    ("SectorObject_ClearRigidRefs", 0x4DC340, "rigid"),
    ("SectorObject_FillMagicRefs", 0x4DC050, "magic"),
    ("SectorObject_FillRigidRefs", 0x4DC190, "rigid"),
])
def test_tree_walks(fn, addr, args):
    e = emu()

    def setup(e):
        so, secs, att = build(e)
        lst = first(e, att[1]) if args == "magic" else first(e, so)
        if "Count" in fn:
            n = e.alloc(4)
            u32(e, n, 3)
            return [n, lst]
        if "Clear" in fn:
            return [lst]
        n = e.alloc(4)
        u32(e, n, 1)
        set_ = e.alloc(0x18 + 0x24 * 40)
        return [n, set_, lst, 1]
    check(e, addr, fn, setup=setup, check_eax=False)


@pytest.mark.parametrize("has", [0, 1, 2])
def test_match_lightmaps(has):
    e = emu()

    def setup(e):
        so = obj(e, 0x1A0, SECTOROBJ, 0x1234)
        s = e.alloc(8)
        u32(e, s + 4, 0x1234 if has == 1 else 0x99)
        a = e.alloc(8)
        u32(e, a, s if has else 0, 0xAAAA)
        return [so, a]
    check(e, 0x4DBF00, "SectorObject_MatchLightmaps", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("mode", ["one", "two", "sectors", "noattached_model", "nomodel"])
def test_load_lightmaps(mode, cw):
    e = emu()

    def setup(e):
        e.write(0x548366, bytes([FREE_EV]))
        set_handler(e, FREE_EV, SECTOROBJ, e.freeh)
        if mode == "nomodel":
            so = sector_object(e, 0, None)
        elif mode in ("one", "two"):
            so, secs, att = build(e, sectors=False)
            if mode == "one":
                u32(e, so + 0x148, 0)
        else:
            so, secs, att = build(e)
        arg = e.alloc(4)
        u32(e, arg, 0x5555)
        return [so, arg]
    check(e, with_cw(e, 0x4DC3D0, cw), with_cw(e, "SectorObject_LoadLightmaps", cw), setup=setup,
          check_eax=False)


SPHERES = [
    [((0.0, 0.0, 0.0), 10.0), ((0.0, 0.0, 0.0), 20.0)],
    [((0.0, 0.0, 0.0), 100.0), ((10.0, 5.0, 0.0), 20.0)],
    [((10.0, 5.0, 0.0), 20.0), ((0.0, 0.0, 0.0), 100.0)],
    [((0.0, 0.0, 0.0), 10.0), ((100.0, 0.0, 0.0), 20.0)],
    [((1.5, -7.25, 3.0), 13.3), ((100.1, 40.7, -9.0), 21.9), ((-55.5, 12.0, 80.25), 7.7), ((3.0, 3.0, 3.0), 1.0),
     ((-200.0, 330.0, 17.0), 64.0)],
    [((float(i * 37 % 101), float(i * 53 % 89), float(i * 11 % 7)), 5.0 + i) for i in range(9)],
]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("i", range(len(SPHERES) + 2))
def test_get_bounds(i, cw):
    e = emu()

    def setup(e):
        if i >= len(SPHERES):
            so = sector_object(e, model(e, [mesh(e, center=(1.0, 2.0, 3.0), radius2=55.0)]) if i == len(SPHERES)
                               else 0, None)
        else:
            sp = SPHERES[i]
            parts = [obj(e, 0x148, SECTOR, model(e, [mesh(e, center=c, radius=r)]), attached=0)
                     for c, r in sp[1:]]
            so = sector_object(e, model(e, [mesh(e, center=sp[0][0], radius=sp[0][1])]), parts)
            u32(e, so + 0xE8, 0)
        return [so, e.alloc(12), e.alloc(4)]
    check(e, with_cw(e, 0x4DC640, cw), with_cw(e, "SectorObject_GetBounds", cw), setup=setup, check_eax=False)


def test_add_child_spheres():
    e = emu()

    def setup(e):
        so, secs, att = build(e)
        p = e.alloc(0x18)
        pool(e, p, 0x10, 50)
        return [p, att[1]]
    check(e, 0x4DCBA0, "SectorObject_AddChildSpheres", setup=setup, check_eax=False)


@pytest.mark.parametrize("mode", ["full", "nomodel", "noset"])
def test_free_lightmaps(mode):
    e = emu()

    def setup(e):
        so, secs, att = build(e)
        if mode == "nomodel":
            u32(e, so + 0x6C, 0)
        for o in [so] + secs:
            u32(e, o + 0xDC, 0x777)
        arg = e.alloc(4)
        u32(e, arg, 0 if mode == "noset" else e.call(0x4B0C60, 64, 4)[0])
        return [so, arg]
    check(e, 0x4DCC20, "SectorObject_FreeLightmaps", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("raise_,hit", [(0, 0), (1, 1), (0, 2), (1, 3)])
def test_locate(raise_, hit, cw):
    e = emu()

    def setup(e):
        so, secs, att = build(e)
        f32(e, so + 0x70, 0.6, -0.8, 0, 0.8, 0.6, 0, 0, 0, 1)
        f32(e, so + 0x98, 1.5, 2.0, 0.5)
        f32(e, so + 0x190, 11.0, -3.5, 7.25)
        u32(e, so + 0x19C, 0x4242)
        for t in (so, *secs):
            set_handler(e, 9, e.u32(t + 0x1C) & 0xFFFF, e.slot9)
        RET.clear()
        if hit:
            RET[("hit", [0, so, secs[0], secs[1]][hit])] = secs[1]
        d64(e, secs[1] + 0x20, 100.5, -200.25, 30.125)
        p = e.alloc(24)
        d64(e, p, 1.0, 2.0, 3.0)
        a = e.alloc(0x30)
        u32(e, a, p, raise_)
        return [so, a]
    check(e, with_cw(e, 0x4DBCC0, cw), with_cw(e, "SectorObject_LocateHandler", cw), setup=setup,
          check_eax=False)


def test_locate_no_sectors():
    e = emu()

    def setup(e):
        so = sector_object(e, 0, None)
        return [so, e.alloc(0x30)]
    check(e, 0x4DBCC0, "SectorObject_LocateHandler", setup=setup, check_eax=False)


def sectors_with_pools(e, cap=2):
    pool(e, LINKS, 0xC, 4)
    so, secs, att = build(e)
    for s in secs:
        pool(e, s + 0x130, 4, cap)
    return so, secs


@pytest.mark.parametrize("case", ["ok", "none", "owner", "full", "links_full"])
def test_add_object(case):
    e = emu()

    def setup(e):
        so, secs = sectors_with_pools(e)
        RET.clear()
        RET["q8"] = {"none": 0, "owner": so}.get(case, secs[0])
        RET["qC"] = so
        if case == "full":
            for _ in range(2):
                e.call(0x4DCCB0, 7, obj(e, 0xF0, RIGID), e.alloc(24))
        if case == "links_full":
            RET["q8"] = secs[1]
            for _ in range(2):
                e.call(0x4DCCB0, 7, obj(e, 0xF0, RIGID), e.alloc(24))
            u32(e, LINKS + 0xC, 2)
            RET["q8"] = secs[0]
        o = obj(e, 0xF0, RIGID)
        u32(e, o + 0x60, 0x99)
        return [7, o, e.alloc(24)]
    check(e, 0x4DCCB0, "Sector_AddObject", setup=setup)


@pytest.mark.parametrize("which", [0, 1, 2, 3, "out"])
def test_remove_object(which):
    e = emu()

    def setup(e):
        so, secs = sectors_with_pools(e)
        RET.clear()
        RET["qC"] = so
        objs = []
        for k in range(3):
            RET["q8"] = secs[k % 2]
            o = obj(e, 0xF0, RIGID)
            e.call(0x4DCCB0, 7, o, e.alloc(24))
            objs.append(o)
        if which == "out":
            o = obj(e, 0xF0, RIGID)
            u32(e, o + 0x60, 1)
            return [o]
        if which == 3:
            u32(e, objs[0] + 0x60, 0)
            return [objs[0]]
        return [objs[which]]
    check(e, 0x4DCDD0, "Sector_RemoveObject", setup=setup, check_eax=False)


@pytest.mark.parametrize("q8", [0, 1])
def test_move_object(q8):
    e = emu()

    def setup(e):
        so, secs = sectors_with_pools(e)
        RET.clear()
        RET["qC"] = so
        RET["q8"] = secs[0]
        o = obj(e, 0xF0, RIGID)
        e.call(0x4DCCB0, 7, o, e.alloc(24))
        RET["q8"] = secs[1] if q8 else 0
        return [7, o, e.alloc(24)]
    check(e, 0x4DCF00, "Sector_MoveObject", setup=setup)


@pytest.mark.parametrize("kids", [0, 1])
def test_sector_render(kids):
    e = emu()

    def setup(e):
        ev = e.call(0x4C56F0)[0] & 0xFF
        u32(e, 0xB46D14, 5)
        u32(e, 0xA94E84 + 20, e.draw)
        u32(e, 0xA5E35C, 0x111, 0)
        u32(e, 0xA5E334, 2)
        u32(e, 0xA5E330, 0x333)
        u32(e, 0xA5E310, 4)
        s = obj(e, 0x148, SECTOR, model(e, [mesh(e)]), pos=(1.0, 2.0, 3.0), attached=task(e, 0x20))
        pool(e, s + 0x130, 4, 5)
        if kids:
            for t in (MAGIC, RIGID):
                add_child(e, e.u32(s + 0xE8), task(e, 0x20, t))
            for t in (OTHER, MAGIC, RIGID):
                add_child(e, s, task(e, 0x20, t))
            for t in (RIGID, OTHER, MAGIC):
                p = e.u32(s + 0x130) + e.u32(e.u32(s + 0x134) + 4 * e.u32(s + 0x138)) * e.u32(s + 0x140)
                u32(e, p + e.u32(s + 0x144), e.u32(s + 0x138))
                u32(e, s + 0x138, e.u32(s + 0x138) + 1)
                u32(e, p, task(e, 0x20, t))
        set_handler(e, ev, RIGID, e.place)
        set_handler(e, ev, MAGIC, e.place)
        set_handler(e, ev, OTHER, 0)
        set_handler(e, 4, RIGID, e.objdraw)
        set_handler(e, 4, MAGIC, e.objdraw)
        return [s]
    check(e, 0x4DD1C0, "Sector_Render", setup=setup, check_eax=False)


def test_sector_open():
    e = new_emu()
    news = iter(range(0x61, 0x70))
    logfake(e, 0x401900, 4, masks={2: 0xFFFF}, effect=lambda e, a: next(news))
    logfake(e, 0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})
    logfake(e, 0x401530, 3, masks={0: 0xFFFF})
    for k, a in enumerate((0x4CEA10, 0x4CEA20, 0x4E55B0, 0x4D14C0, 0x4CFC70, 0x4CFC80, 0x4CFC90, 0x4D14D0)):
        logfake(e, a, 0, ret=0x20 + k)
    e.baseline = e.snapshot()

    def setup(e):
        nonlocal news
        news = iter(range(0x61, 0x70))
    check(e, 0x4DCF20, "Sector_Open", setup=setup, check_eax=False)
