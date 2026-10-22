"""ComputerObject and ComputerHilight: what the map computer draws (0x4675B0 - 0x4688A0)."""
import random
import struct

import pytest

from emu import compare
from test_computer_util import (LOG, RET, check, cstr, d64, f32, floatfake, logfake, make_emu, mat, obj, pyfake,
                                run, set_handler, task, task_array, u16, u32, vfake, with_cw)
from test_menu_util import trap

EV0, EV1, EV2 = 0x3A, 0x3B, 0x3C
T1, T2, T3 = 0x60, 0x61, 0x62
MODE = 0xC28B40
MAPMODE = 0x5BDDA0
RES = 0x5BDDB0
GAME = 0xBC2360

emu = make_emu()


def qhandler(e, tag, nargs=2):
    """A map handler that logs the task and the query (its matrix field is never set by the caller)."""
    def fn(e):
        t, q = e.arg(0), e.arg(1)
        if LOG:
            data = e.read(q, 44) if nargs == 2 and q else None
            if data:
                data = data[:16] + b"...." + data[20:]
            LOG[-1].append((tag, e.normalize(t), data))
        e.ret(0)
    return trap(e, "qhandler_%s" % tag, fn)


def euler(e, a):
    for i in range(3):
        f32(e, a[1 + i], 0.25 * (i + 1) + RET.get("euler", 0.0))
    return 0


def size_out(e, a):
    w, h = struct.unpack("<2f", e.read(a[1], 8))
    f32(e, a[0], w * 0.01, h * 0.02)
    return 0


logfake(emu, 0x46C070, 0, ret=0xC070)  # Computer_GetView
logfake(emu, 0x4E8210, 1, effect=lambda e, a: RET.get("cam", 0))
logfake(emu, 0x4D2B80, 1)
logfake(emu, 0x4D2BD0, 1)
logfake(emu, 0x4D0D20, 1, effect=lambda e, a: RET.get("npts", 0))
vfake(emu, 0x4D0A50, 3, show=lambda e, a: (a[1], a[2]),
      effect=lambda e, a: d64(e, a[0], 10.5 * a[2], -3.25 * a[2], 7.0 + a[2]))
logfake(emu, 0x4E1130, 6, derefs={0: 24, 1: 24, 2: 12, 4: 12})
floatfake(emu, 0x4D0950, 1)  # VirModel_GetRadius
logfake(emu, 0x4B6CE0, 1, ret=24)  # Sprite_Width
logfake(emu, 0x4B6D00, 1, ret=13)  # Sprite_Height
logfake(emu, 0x4B50B0, 11)  # QVertex_DrawFrame
logfake(emu, 0x4B5860, 14)  # QVertex_DrawPolyline
vfake(emu, 0x498040, 3, show=lambda e, a: (e.read(a[1], 8), a[2]), effect=size_out)
vfake(emu, 0x4B3C50, 4, show=lambda e, a: (e.read(a[0], 40),), effect=euler)  # Matrix_GetEuler
logfake(emu, 0x4B64E0, 1, strings=(0,), effect=lambda e, a: RET.get("found", 0))  # Resource_Find
logfake(emu, 0x4B5F00, 2, strings=(0,), effect=lambda e, a: RET.get("res", 0))  # Resource_Load
logfake(emu, 0x418E40, 2, strings=(1,), ret=0xF0F0)  # MenuManager_FindFont
logfake(emu, 0x469450, 8, strings=(5,))  # Computer_DrawWrapped
logfake(emu, 0x4F1320, 1)  # VarString_Init
logfake(emu, 0x4F1340, 1)  # VarString_Free
logfake(emu, 0x4F1400, 2)  # VarString_Bind
floatfake(emu, 0x4F16E0, 2, effect=lambda e, a: RET.get("val", 0.0))
logfake(emu, 0x4C1790, 2)  # QTaskList_Add
logfake(emu, 0x4C17C0, 2)  # QTaskList_Remove
logfake(emu, 0x4B0D10, 1)  # Mem_Free
logfake(emu, 0x4B0D60, 1)  # Mem_PushMode
logfake(emu, 0x4B0D90, 0)  # Mem_PopMode
logfake(emu, 0x4B8770, 1, strings=(0,), effect=lambda e, a: len(e.read_cstr(a[0])) * 3 - 40)  # Script_EvalExpr
logfake(emu, 0x4F1030, 1, effect=lambda e, a: RET.get(("id", a[0]), 0))  # Task_FindById
logfake(emu, 0x4017C0, 1, ret=0x2E)  # TaskEvent_Alloc
for t in (T1, T2, T3):
    set_handler(emu, EV0, t, qhandler(emu, "e0_%x" % t))
    set_handler(emu, EV1, t, qhandler(emu, "e1_%x" % t))
    set_handler(emu, EV2, t, qhandler(emu, "e2_%x" % t, 1))
emu.e0 = qhandler(emu, "e0_none")
emu.baseline = emu.snapshot()


def events(e):
    e.write(0x53F582, bytes([EV0, EV1, EV2]))


def display(e, w=640, h=480):
    u32(e, MODE + 4, w, h)


def view(e, rng):
    """center, view matrix and zoom, like the map camera block."""
    v = e.alloc(0x60)
    mat(e, v, rng)
    d64(e, v + 0x28, *[rng.uniform(-20000, 20000) for _ in range(3)])
    f32(e, v + 0x40, rng.uniform(0.2, 3.0))
    return v


def query(e, rng, marker=-1, color=-1, sprite=0, text=0, text2=0, radius=1000.0):
    v = view(e, rng)
    q = e.alloc(44)
    pos = e.alloc(24)
    d64(e, pos, *[rng.uniform(-20000, 20000) for _ in range(3)])
    m = e.alloc(40)
    mat(e, m, rng)
    u32(e, q, v + 0x28, v)
    e.write(q + 8, e.read(v + 0x40, 4))
    u32(e, q + 12, pos, m)
    f32(e, q + 20, radius)
    u32(e, q + 24, sprite, marker, color, text, text2)
    return q


# --- projection ----------------------------------------------------------------

@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_project(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        display(e, rng.choice([640, 800, 1024, 333]), rng.choice([480, 600, 768, 257]))
        q = query(e, rng)
        v = e.alloc(12)
        f32(e, v, rng.uniform(-9000, 9000), rng.uniform(-9000, 9000), rng.uniform(-9000, 9000))
        return e.alloc(12), v, q
    run(emu, with_cw(emu, 0x4675B0, cw), with_cw(emu, "ComputerObject_Project", cw), setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("depth", [0, 1])
@pytest.mark.parametrize("seed", range(12))
def test_project_pos(seed, depth, cw):
    def setup(e):
        rng = random.Random(seed)
        display(e, rng.choice([640, 800, 1024, 333]), rng.choice([480, 600, 768, 257]))
        q = query(e, rng)
        return e.alloc(8), e.alloc(4) if depth else 0, e.u32(q + 12), q
    run(emu, with_cw(emu, 0x467620, cw), with_cw(emu, "ComputerObject_ProjectPos", cw), setup)


# --- ComputerObject --------------------------------------------------------------

@pytest.mark.parametrize("model,npts,door", [(0, 20, 0), (1, 10, 0), (1, 11, 0), (1, 11, 1), (1, 40, 1)])
def test_draw_line(model, npts, door):
    def setup(e):
        RET["npts"] = npts
        RET[0x447D20] = T1 if door else 0x77
        o = obj(e, 0xF0, T1)
        m = e.alloc(4)
        u32(e, m, 0x4D4D)
        u32(e, o + 0x6C, m if model else 0)
        return (o,)
    run(emu, 0x467800, "ComputerObject_DrawLine", setup)


def test_create():
    run(emu, 0x4678F0, "ComputerObject_Create", lambda e: (task(e, 0x24),))


def test_delete():
    run(emu, 0x467960, "ComputerObject_Delete", lambda e: (task(e, 0x24),))


@pytest.mark.parametrize("kinds", [[], [4], [1, 4, 4, 0, 4], [3, 3]])
def test_notify(kinds):
    def setup(e):
        events(e)
        types = [T1, T2, 0x70, T3, T1]
        items = e.alloc(8 * len(kinds) + 8)
        for i, k in enumerate(kinds):
            u32(e, items + 8 * i, k | 0xABCD00, task(e, 0x40, types[i]))
        lst = e.alloc(8)
        u32(e, lst, items, len(kinds))
        return (lst,)
    run(emu, 0x467900, "ComputerObject_Notify", setup)


def lists(e, n):
    out = []
    types = [T1, T2, T3, T1, T2]
    for i in range(5):
        out.append(task_array(e, [task(e, 0x40, types[(i + j) % 3]) for j in range(n[i])]))
    return out


@pytest.mark.parametrize("flags,mode,cam,n", [
    (0, 1, 0, (1, 1, 1, 1, 1)),
    (0x1F, 1, 1, (2, 1, 3, 1, 2)),
    (0x1F, 0, 1, (2, 1, 3, 1, 2)),
    (0x1F, -1, 1, (2, 1, 3, 1, 2)),
    (0x08, 2, 0, (0, 0, 0, 2, 0)),
    (0x15, 1, 1, (0, 2, 0, 0, 3)),
    (0x12, 1, 1, (1, 0, 0, 0, 0)),
    (0xFFFFFFE0, 1, 1, (1, 1, 1, 1, 1)),
])
def test_draw(flags, mode, cam, n):
    def setup(e):
        rng = random.Random(flags)
        events(e)
        RET["cam"] = view(e, rng)
        humans, tl4, hil, cams, objs = lists(e, n)
        u32(e, 0x5BDC24, humans)
        u32(e, 0x57BAAC, tl4)
        u32(e, 0x5BDC80, hil)
        u32(e, 0x57C18C, cams)
        u32(e, 0x57BAA0, objs)
        u32(e, 0x5BE330, task_array(e, [task(e, 0x40, T2), task(e, 0x40, T3)]))
        # one object type without a map handler
        u32(e, objs, n[4] + 1)
        e.write(objs + 8 + 4 * n[4], struct.pack("<I", task(e, 0x40, 0x75)))
        u16(e, MAPMODE, mode & 0xFFFF)
        m = e.alloc(0xA8)
        u32(e, 0x5BDD98, m)
        u32(e, m + 0xA4, task(e, 0x40, T3) if cam else 0)
        o = task(e, 0x24)
        u32(e, o + 0x20, flags)
        return (o,)
    run(emu, 0x467970, "ComputerObject_Draw", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("mode,flag,scale", [
    (0, 1, (1.0, 2.0, 3.0)), (3, 1, (1.0, 2.0, 3.0)), (1, 0, (1.0, 2.0, 3.0)),
    (1, 1, (1.0, 2.0, 3.0)), (2, 1, (5.0, 2.0, 3.0)), (-2, 1, (1.0, 4.0, 3.0)), (1, 1, (0.1, 0.2, 0.1)),
])
def test_draw_vehicle(mode, flag, scale, cw):
    def setup(e):
        rng = random.Random(7)
        display(e)
        RET[0x4D0950] = 1500.0
        u16(e, MAPMODE, mode & 0xFFFF)
        v = obj(e, 0x800, T1, scale=scale)
        e.write(v + 0x745, bytes([flag]))
        return v, query(e, rng)
    run(emu, with_cw(emu, 0x467C60, cw), with_cw(emu, "ComputerObject_DrawVehicle", cw), setup)


def test_new():
    def setup(e):
        u16(e, 0x53F580, 0x55)
        logfake(e, 0x4012A0, 3, masks={1: 0xFFFF}, effect=lambda e, a: RET.get("new", 0))
        RET["new"] = task(e, 0x24)
        u32(e, RET["new"] + 0x20, 0x5A5A5A5A)
        return (0x1234,)
    em = make_emu()
    run(em, 0x467D70, "ComputerObject_New", setup, check_eax=True)


@pytest.mark.parametrize("orig,ours,g", [(0x467D90, "ComputerObject_GetEvent0", 0x53F582),
                                         (0x467DB0, "ComputerObject_GetEvent1", 0x53F583),
                                         (0x467DD0, "ComputerObject_GetEvent2", 0x53F584)])
@pytest.mark.parametrize("v", [0xFF, 0x12])
def test_get_event(orig, ours, g, v):
    def setup(e):
        e.write(0x53F582, bytes([0x40, 0x41, 0x42]))
        e.write(g, bytes([v]))
    run(emu, orig, ours, setup, check_eax=True, eax_mask=0xFF)


def sprite(e):
    return e.alloc(0x40)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("has", [0, 1])
def test_draw_sprite(seed, has, cw):
    def setup(e):
        rng = random.Random(seed)
        display(e)
        return (query(e, rng, sprite=sprite(e) if has else 0),)
    run(emu, with_cw(emu, 0x467DF0, cw), with_cw(emu, "ComputerObject_DrawSprite", cw), setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("marker,color,radius", [(-1, 0, 5000.0), (0, 0, 5000.0), (1, 2, 100.0), (2, 4, 3072.0),
                                                 (0, 1, float("nan")), (0, -1, 4000.0)])
def test_draw_marker(seed, marker, color, radius, cw):
    def setup(e):
        rng = random.Random(seed)
        display(e)
        RET["euler"] = seed * 0.1
        return (query(e, rng, marker=marker, color=color, radius=radius),)
    run(emu, with_cw(emu, 0x467F30, cw), with_cw(emu, "ComputerObject_DrawMarker", cw), setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("mode,text,text2,found", [
    (0, b"a", b"b", 1), (1, b"label", b"other", 1), (1, b"label", b"other", 0), (1, b"", b"other", 1),
    (2, b"label", b"other", 1), (5, b"label", b"x", 0), (2, b"label", b"", 0), (-1, b"a", b"b", 1),
])
@pytest.mark.parametrize("seed", range(3))
def test_draw_text(mode, text, text2, found, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        display(e)
        u16(e, MAPMODE, mode & 0xFFFF)
        RET["found"] = found
        RET["res"] = cstr(e, b"resolved text")
        g = e.alloc(0x200)
        u32(e, GAME, g)
        u32(e, g + 0x120, 0x1201)
        return (query(e, rng, text=cstr(e, text), text2=cstr(e, text2)),)
    run(emu, with_cw(emu, 0x4680B0, cw), with_cw(emu, "ComputerObject_DrawText", cw), setup)


def test_set_flags():
    compare(emu, 0x4682E0, "ComputerObject_SetFlags", setup=lambda e: (task(e, 0x24), 0x1F), check_eax=False)


# --- ComputerHilight -------------------------------------------------------------

def hilight(e, ids=b"", icon=None, marker=b"MARKER_BOX", color=b"MARKER_COLOR_RED"):
    h = task(e, 0x4E0)
    e.write(h + 0x20, bytes((i * 29 + 1) & 0xFF for i in range(0x4C0)))
    e.write(h + 0x8C, ids + b"\0")
    u32(e, h + 0x88, cstr(e, icon) if icon is not None else 0)
    e.write(h + 0x294, marker + b"\0")
    e.write(h + 0x2B8, color + b"\0")
    return h


def icons(e):
    for i in range(19):
        u32(e, RES + 0x4B0 + 4 * i, cstr(e, b"icon_%02d" % i))


def test_hilight_create():
    run(emu, 0x4684F0, "ComputerHilight_Create", lambda e: (hilight(e),))


@pytest.mark.parametrize("icon", [0, 1])
def test_hilight_delete(icon):
    run(emu, 0x468570, "ComputerHilight_Delete", lambda e: (hilight(e, icon=b"x" if icon else None),))


def test_hilight_get_params():
    def setup(e):
        u32(e, 0x5BDC84, 0x8484)
        return 0, e.alloc(4)
    run(emu, 0x4685B0, "ComputerHilight_GetParams", setup)


@pytest.mark.parametrize("icon", [None, b"icon_00", b"icon_07", b"icon_18", b"icon_19", b"", b"icon_0"])
def test_hilight_changed(icon):
    def setup(e):
        icons(e)
        return (hilight(e, b"3 5-7", icon=icon, marker=b"MARKER_BOX + 1"),)
    run(emu, 0x4685C0, "ComputerHilight_Changed", setup)


IDS = [b"", b"1", b"12 34 5", b"3-7", b"3 - 7", b"-5 2", b"9-3", b"1-100", b"0", b"05 6", b"a1b2", b"7--9",
       b"2-a-6", b"  42  ", b"1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 "
       b"31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65",
       b"10-80 3", b"5 1-70", b"99999999999 4", b"1-", b"-"]


@pytest.mark.parametrize("ids", IDS)
def test_hilight_parse(ids):
    run(emu, 0x468690, "ComputerHilight_ParseIds", lambda e: (hilight(e, ids),))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("val", [0.0, 1.0, 0.4, -2.0, 4294967296.0])
@pytest.mark.parametrize("count", [0, 3])
def test_hilight_draw(val, count, cw):
    def setup(e):
        events(e)
        RET["val"] = val
        h = hilight(e)
        u32(e, h + 0x18C, count, 11, 12, 13, 14)
        RET[("id", 11)] = task(e, 0x40, T1)
        RET[("id", 13)] = task(e, 0x40, 0x75)
        RET[("id", 14)] = task(e, 0x40, T2)
        return h, query(e, random.Random(1))
    run(emu, with_cw(emu, 0x4687A0, cw), with_cw(emu, "ComputerHilight_DrawOnMap", cw), setup)


def test_hilight_get_list():
    compare(emu, 0x4688A0, "ComputerHilight_GetList", setup=lambda e: u32(e, 0x5BDC80, 0x12345678))
