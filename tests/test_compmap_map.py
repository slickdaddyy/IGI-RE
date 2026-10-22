import struct

import pytest

from test_compmap_util import (COMPUTER, MAP, MAP_POS, MOUSE, ZOOM, add_child, check, d64, f32, make_emu, make_menu,
                               set_handler, task, u16, u32)

GAME_OBJ = 0x57BABC
LIMIT = 0x5BDC68
SELECT = 0x53F760
START = 0x5BDD9C
NAN = float("nan")

e, fk = make_emu()
PARAMS = e.alloc(0x40)  # profile, panel busy, select answer
e.baseline = e.snapshot()
fk.add(0x401A20, 1, masks={0: 0xFFFF})                           # TaskType_Free
fk.add(0x401900, 4, ret=0x42, strings=(0,), masks={2: 0xFFFF})   # TaskType_New
fk.add(0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})                  # TaskType_SetHandler
fk.add(0x401530, 3, masks={0: 0xFFFF})                           # TaskType_SetHandler2
fk.add(0x4017C0, 1, ret=0x33, masks={0: 0xFFFF})                 # TaskEvent_Alloc
fk.add(0x4018C0, 1, masks={0: 0xFF})                             # TaskEvent_Free
fk.add(0x4B7F40, 2, strings=(0,))                                # Script_RegisterInt16
fk.add(0x401B20, 1)                                              # TaskRef_Remove
fk.add(0x401AE0, 1, ret=lambda e, a: a[0] + 0x10000)             # TaskRef_Add
fk.add(0x489470, 0, ret=12.375, st0=True)                        # game time
fk.add(0x4D9870, 10, deref={1: 24, 2: 40})                       # Camera_Set
fk.add(0x4A23E0, 6, ret=lambda e, a: e.alloc(0x40))              # noise
fk.add(0x4A4FF0, 2)                                              # set noise level
fk.add(0x406220, 0, ret=lambda e, a: e.u32(PARAMS))              # Profile_GetActive
fk.add(0x505A30, 1, ret=lambda e, a: e.u32(PARAMS + 4))              # panel busy
fk.add(0x491D40, 0, ret=1.3125, st0=True)                        # Display_GetAspect


def startup_fmt(e, a):
    names = [e.read_cstr(e.u32(a[3] + 4 * i)) for i in range(3)]
    return (e.normalize(a[0]), a[1], e.read(a[2], 12), names, e.read(a[4], 36), e.read(a[5], 36), a[6])


fk.add(0x466240, 7, ret=lambda e, a: e.alloc(0x100), fmt=startup_fmt)  # ComputerStartup_New

hits = {}


def coll_lines(e, a):
    hits["lines"] = e.read(a[2], 24) + e.read(a[3], 24)
    h = hits.get("hit")
    if h:
        found, task_, pos = h
        u32(e, a[1], task_)
        d64(e, a[1] + 8, *pos)
        u32(e, a[1] + 0x58, found)
    else:
        u32(e, a[1] + 0x58, 0)
        u32(e, a[1], 0)
    return 0


fk.add(0x4CC2A0, 8, ret=coll_lines,
       fmt=lambda e, a: (a[0], e.read(a[2], 24), e.read(a[3], 24)) + tuple(a[4:]))  # Coll_Lines


def coll_drop(e, a):
    h = hits.get("drop")
    u32(e, a[1] + 0x58, 1 if h else 0)
    if h:
        d64(e, a[1] + 8, *h)
    return 0


fk.add(0x4CB9F0, 8, ret=coll_drop,
       fmt=lambda e, a: (a[0], e.read(a[2], 24), a[3], e.read(a[4], 8)) + tuple(a[5:]))  # Coll_DropPointsMat


def pos_handler(e, a):
    d64(e, a[1], 1000.5, -2000.25, 333.0)
    return 0


POS = fk.add(fk.code(b"\xc3"), 2, ret=pos_handler, fmt=lambda e, a: (e.normalize(a[0]), e.u32(a[1] + 0x50)))


def select_handler(e, a):
    v = e.u32(PARAMS + 8)
    if v != 0xFFFFFFFF:
        e.write(a[1], bytes([v]))
    return 0


SELECT_FN = fk.add(fk.code(b"\xc3"), 2, ret=select_handler, fmt=lambda e, a: (e.normalize(a[0]),))


def world(e, profile=1, busy=0, select=1):
    """Computer, view, camera and game blocks."""
    comp = e.alloc(0x200)
    u32(e, COMPUTER, comp)
    inp = e.alloc(0x200)
    u32(e, comp + 0x14, inp)
    f32(e, inp + 0x1B0, 0.0125, -0.03)
    view = e.alloc(0x100)
    u32(e, comp + 0x130, view)
    f32(e, view + 0x34, 100.0, 50.0, 600.0, 450.0)
    cam = e.alloc(0x60)
    u32(e, view + 0x50, cam)
    f32(e, cam, 0.8, -0.6, 0.1, 0.6, 0.8, 0.2, -0.1, 0.05, 0.99)
    d64(e, cam + 0x28, 12000.0, -34000.5, 7000.25)
    f32(e, cam + 0x40, 0.7)
    game = e.alloc(0x100)
    u32(e, GAME_OBJ, game)
    u32(e, game + 0x2C, 0x12345678, 0x9ABCDEF0, 0x0F1E2D3C)
    u32(e, game + 0x20, 0x7777)
    p = e.alloc(0x600)
    u32(e, p + 0x538, profile)
    u32(e, PARAMS, p, busy, select)
    d64(e, MAP_POS, 5000.0, -6000.0, 7000.0)
    return comp, inp, view, cam


def new_map(e):
    m = task(e, 0xAC, 0x40)
    f32(e, m + 0x20, *[0.1 * i for i in range(9)])
    f32(e, m + 0x48, 1.0, 0.25, 0.0, 0.0, -0.5, 0.75, 0.0, 0.875, -0.125)
    f32(e, m + 0x70, 0.4142)
    u32(e, m + 0x9C, e.alloc(0x100))
    u32(e, m + 0xA0, e.alloc(0x40))
    u32(e, MAP, m)
    return m


@pytest.mark.parametrize("state", [0, 1, 2, 3])
def test_on_status(state):
    def setup(e):
        return new_map(e), state
    check(e, fk, 0x469660, "ComputerMap_OnStatus", setup, check_eax=False)


def test_init():
    check(e, fk, 0x4696B0, "ComputerMap_Init", check_eax=False)


def test_small():
    def setup(e):
        u16(e, 0x5BDD90, 0x66)
        e.write(SELECT, b"\x29")
        u16(e, ZOOM, 0x8002)
        u32(e, MAP, 0x1234)
    check(e, fk, 0x46AAD0, "ComputerMap_Shutdown", setup, check_eax=False)
    check(e, fk, 0x46AAF0, "ComputerMap_New", setup, args=(0x4444,))
    check(e, fk, 0x46AB10, "ComputerMap_Get", setup)
    check(e, fk, 0x46AB20, "ComputerMap_GetSelectEvent", setup, eax_mask=0xFF)
    check(e, fk, 0x46AB30, "ComputerMap_GetZoom", setup, eax_mask=0xFFFF)
    check(e, fk, 0x46AB40, "ComputerMap_ResetZoom", setup, check_eax=False)
    check(e, fk, 0x46AB50, "ComputerMap_SetStartTask", setup, check_eax=False, args=(0x5678,))
    check(e, fk, 0x46A080, "ComputerMap_SetNoise", lambda e: (new_map(e), 0x3F123456, 77), check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("start", [False, True])
@pytest.mark.parametrize("zoom", [0, 1, 2, 3])
@pytest.mark.parametrize("size", [(800, 600), (1023, 767)])
def test_create(start, zoom, size, cw):
    def setup(e):
        world(e)
        make_menu(e)
        u32(e, 0xC28B44, *size)
        u16(e, ZOOM, zoom)
        if start:
            t = task(e, 0x40, 0x51)
            set_handler(e, 7, 0x51, POS)
            u32(e, START, t)
        return (task(e, 0xAC),)
    check(e, fk, 0x469740, "ComputerMap_Create", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_look_at_task(cw):
    def setup(e):
        t = task(e, 0x40, 0x51)
        set_handler(e, 7, 0x51, POS)
        return new_map(e), t
    check(e, fk, 0x469CC0, "ComputerMap_LookAtTask", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("p", [(0.0, 0.0, 0.0), (123456.789, -98765.4321, 1e-3), (1e15, 3.3, -7.7)])
@pytest.mark.parametrize("tilt", [(0.25, -0.5, 0.875), (1 / 3, 2 / 7, -5 / 9)])
def test_look_at(p, tilt, cw):
    def setup(e):
        m = new_map(e)
        f32(e, m + 0x4C, tilt[0])
        f32(e, m + 0x58, tilt[1])
        f32(e, m + 0x64, tilt[2])
        v = e.alloc(24)
        d64(e, v, *p)
        return m, v
    check(e, fk, 0x469D00, "ComputerMap_LookAt", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("pressed", [0, 0x10, 0x20])
@pytest.mark.parametrize("id_", [0, 1, 2, 3])
def test_on_tab(pressed, id_):
    def setup(e):
        m = make_menu(e)
        u32(e, m + 0x34, pressed)
        p = e.alloc(4)
        u32(e, p, id_)
        return task(e, 0x90), p
    check(e, fk, 0x469D70, "ComputerMap_OnTab", setup, check_eax=False)


def button_setup(id_, down, was, zoom, profile):
    def setup(e):
        world(e, profile)
        menu = make_menu(e)
        u32(e, menu + 0x30, down | 0x20)
        m = new_map(e)
        u32(e, m + 0x90, 0x7FF0)
        u16(e, ZOOM, zoom)
        b = task(e, 0x94)
        u32(e, b + 0x38, id_)
        e.write(b + 0x80, bytes([was, 0x77]))
        p = e.alloc(4)
        u32(e, p, id_)
        return b, p
    return setup


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("id_", [2, 3, 4, 5, 6, 7, 8])
@pytest.mark.parametrize("down,was", [(0, 0), (0x10, 0), (0x10, 0x10)])
@pytest.mark.parametrize("zoom", [0, 2, 3])
def test_on_button(id_, down, was, zoom, cw):
    check(e, fk, 0x469DA0, "ComputerMap_OnButton", button_setup(id_, down, was, zoom, 1), check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("down", [0, 0x10])
@pytest.mark.parametrize("profile", [0, 1])
@pytest.mark.parametrize("zoom", [0, 3])
def test_on_button_drag(down, profile, zoom, cw):
    check(e, fk, 0x469DA0, "ComputerMap_OnButton", button_setup(9, down, 0, zoom, profile), check_eax=False, cw=cw)


def test_on_button_other_id():
    def setup(e):
        world(e)
        make_menu(e)
        new_map(e)
        b = task(e, 0x94)
        u32(e, b + 0x38, 3)
        p = e.alloc(4)
        u32(e, p, 4)
        return b, p
    check(e, fk, 0x469DA0, "ComputerMap_OnButton", setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("fov", [0.4142, 0.5, NAN, -0.0, 0.0])
@pytest.mark.parametrize("old", [0.4142, NAN, 0.0])
def test_set_fov(fov, old, cw):
    def setup(e):
        world(e)
        m = new_map(e)
        f32(e, m + 0x70, old)
        return m, struct.unpack("<I", struct.pack("<f", fov))[0]
    check(e, fk, 0x469FE0, "ComputerMap_SetFov", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("ref", [0, 0x7FF0])
def test_clear_target(ref):
    def setup(e):
        m = new_map(e)
        u32(e, m + 0x90, ref)
        return (m,)
    check(e, fk, 0x46A0A0, "ComputerMap_ClearTarget", setup, check_eax=False)


def limit(e, pos, size):
    if pos is None:
        u32(e, LIMIT, 0)
        return
    b = e.alloc(0x40)
    u32(e, LIMIT, b)
    d64(e, b + 0x20, *pos)
    f32(e, b + 0x38, size)


LIMITS = [None, ((0.0, 0.0, 0.0), 100.0), ((5100.3, -6100.7, 20.0), 0.01), ((4000.0, -7000.0, 0.0), 0.3)]
POSITIONS = [(5000.0, -6000.0, 7000.0), (5100.29, -6100.71, -1.0), (1e9, -1e9, 0.0), (NAN, NAN, NAN),
             (5000.1234567, -6000.987654, 1.5)]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("lim", LIMITS)
@pytest.mark.parametrize("pos", POSITIONS)
def test_clamp(lim, pos, cw):
    def setup(e):
        limit(e, *(lim or (None, 0)))
        d64(e, MAP_POS, *pos)
        return (new_map(e),)
    check(e, fk, 0x46A200, "ComputerMap_ClampCamera", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("lim", LIMITS)
@pytest.mark.parametrize("pos", POSITIONS)
def test_in_bounds(lim, pos, cw):
    def setup(e):
        limit(e, *(lim or (None, 0)))
        v = e.alloc(24)
        d64(e, v, *pos)
        return new_map(e), v
    check(e, fk, 0x46A970, "ComputerMap_InBounds", setup, eax_mask=0xFF, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("drop", [None, (100.5, 200.25, -300.125)])
def test_drop(drop, cw):
    def setup(e):
        world(e)
        hits["drop"] = drop
        return (new_map(e),)
    check(e, fk, 0x46A2D0, "ComputerMap_DropCamera", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("pos7", [False, True])
@pytest.mark.parametrize("sel", [None, 0, 1, 0xFFFFFFFF])
@pytest.mark.parametrize("ref", [0, 0x7FF0])
def test_set_target(pos7, sel, ref):
    def setup(e):
        world(e, select=0 if sel is None else sel)
        e.write(SELECT, b"\x29")
        m = new_map(e)
        u32(e, m + 0x90, ref)
        t = task(e, 0x40, 0x52)
        set_handler(e, 7, 0x52, POS if pos7 else 0)
        set_handler(e, 0x29, 0x52, 0 if sel is None else SELECT_FN)
        return m, t
    check(e, fk, 0x46A9F0, "ComputerMap_SetTarget", setup, check_eax=False)


@pytest.mark.parametrize("view", [0, 1])
@pytest.mark.parametrize("ref", [0, 0x7FF0])
def test_delete(view, ref):
    def setup(e):
        comp, _, v, _ = world(e)
        if not view:
            u32(e, comp + 0x130, 0)
        m = new_map(e)
        u32(e, m + 0x90, ref)
        return (m,)
    check(e, fk, 0x46AA80, "ComputerMap_Delete", setup, check_eax=False)


HUMANS, OBJS, CAMS = 0x5BDC24, 0x5BE3A8, 0x57C18C
HIT = (20000.0, 30000.0, 15.0)


def task_list(e, addr, offsets, size=0x260, type_=0x53):
    """A TaskArray at *addr of tasks placed at HIT + offset (x, y, z)."""
    arr = e.alloc(8 + 4 * len(offsets))
    u32(e, addr, arr)
    u32(e, arr, len(offsets), 0)
    out = []
    for i, (dx, dy, z) in enumerate(offsets):
        t = task(e, size, type_)
        d64(e, t + 0x20, HIT[0] + dx, HIT[1] + dy, z)
        u32(e, arr + 8 + 4 * i, t)
        out.append(t)
    return out


TARGET = 0x54
NEAR, FAR = (5000.0, -3000.0, 999.0), (9000.0, 2000.0, 5.0)

SCENES = {
    "none": dict(),
    "human": dict(humans=[FAR, NEAR]),
    "obj": dict(humans=[FAR], objs=[FAR, NEAR, NEAR]),
    "cam": dict(cams=[NEAR], objs=[FAR]),
    "all": dict(humans=[NEAR], objs=[NEAR], cams=[NEAR]),
    "hittask": dict(hittask=True, humans=[FAR]),
    "miss": dict(found=0, humans=[NEAR]),
    "outside": dict(limit=((0.0, 0.0, 0.0), 0.1)),
}


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("scene", sorted(SCENES))
@pytest.mark.parametrize("click", [(400.0, 300.0), (100.0, 50.0), (99.5, 300.0), (400.0, 499.9), (700.0, 300.0),
                                   (123.25, 456.75), (400.0, 49.5), (699.9, 500.0)])
def test_trace_click(scene, click, cw):
    s = SCENES[scene]

    def setup(e):
        world(e)
        menu = make_menu(e)
        u32(e, menu + 0x34, 0x10)
        f32(e, menu + 0x24, *click)
        m = new_map(e)
        u32(e, m + 0x90, 0x7FF0)
        task_list(e, HUMANS, s.get("humans", []))
        task_list(e, OBJS, s.get("objs", []))
        task_list(e, CAMS, s.get("cams", []))
        for t in (0x53, TARGET):
            set_handler(e, 7, t, POS)
            set_handler(e, 0x29, t, SELECT_FN)
        e.write(SELECT, b"\x29")
        ht = task(e, 0x40, TARGET) if s.get("hittask") else 0
        hits["hit"] = (s.get("found", 1), ht, HIT)
        lim = s.get("limit", ((19000.0, 29000.0, 0.0), 1.0))
        limit(e, *lim)
        return (m,)
    check(e, fk, 0x46A330, "ComputerMap_Trace", setup, check_eax=False, cw=cw)


BUTTONS = [  # pressed, click times, mouse, last mouse, zoom, panel busy
    (0, (0, 0), 0, 0, 1, 0), (0x10, (0, 0), 0, 0, 1, 0), (0x10, (5, 0), 0, 0, 1, 0), (0x10, (5, 0), 0, 0, 3, 0),
    (0x20, (0, 0), 0, 0, 1, 0), (0x20, (0, 7), 0, 0, 1, 0), (0x20, (0, 7), 0, 0, 0, 0), (0x30, (1, 13), 0, 0, 2, 0),
    (0, (1, 13), 0x100, 0, 1, 0), (0, (0, 0), 0x100, 0x100, 1, 0), (0, (0, 0), 0x100, 0, 3, 0),
    (0, (0, 0), 0x200, 0, 1, 0), (0, (0, 0), 0x200, 0x200, 1, 0), (0, (0, 0), 0x200, 0, 0, 0),
    (0, (0, 0), 0x300, 0x200, 2, 0), (0x30, (5, 5), 0x300, 0, 1, 1), (0, (2, 2), 0x300, 0, 1, 1),
]


@pytest.mark.parametrize("pressed,times,mouse,last,zoom,busy", BUTTONS)
def test_trace_buttons(pressed, times, mouse, last, zoom, busy):
    def setup(e):
        world(e, busy=busy)
        menu = make_menu(e)
        u32(e, menu + 0x34, pressed)
        f32(e, menu + 0x24, 50.0, 30.0)
        m = new_map(e)
        u32(e, m + 0x80, *times)
        u32(e, m + 0xA8, last)
        u32(e, MOUSE, mouse)
        u16(e, ZOOM, zoom)
        return (m,)
    check(e, fk, 0x46A330, "ComputerMap_Trace", setup, check_eax=False)


@pytest.mark.parametrize("ref", ["none", "dead", "alive"])
def test_trace_follow(ref):
    def setup(e):
        world(e)
        make_menu(e)
        m = new_map(e)
        if ref != "none":
            r = e.alloc(4)
            u32(e, m + 0x90, r)
            if ref == "alive":
                t = task(e, 0x40, 0x51)
                set_handler(e, 7, 0x51, POS)
                u32(e, r, t)
        return (m,)
    check(e, fk, 0x46A330, "ComputerMap_Trace", setup, check_eax=False)


RUN = fk.handler(1)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("noise", [0, 7])
@pytest.mark.parametrize("drop", [None, (100.5, 200.25, -300.125)])
def test_run(noise, drop, cw):
    def setup(e):
        world(e)
        make_menu(e)
        m = new_map(e)
        u32(e, m + 0x74, e.alloc(0x40))
        f32(e, m + 0x78, 0.875)
        u32(e, m + 0x7C, noise)
        for i in range(2):
            add_child(e, m, task(e, 0x40, 0x61 + i))
        set_handler(e, 1, 0x61, RUN)
        limit(e, (5000.0, -6000.0, 0.0), 0.05)
        hits["drop"] = drop
        return (m,)
    check(e, fk, 0x46A0D0, "ComputerMap_Run", setup, check_eax=False, cw=cw)
