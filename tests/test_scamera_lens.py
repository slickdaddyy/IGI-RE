import math
import random
import struct

import pytest

from emu import Emu, compare
from test_scamera_fakes import (Fakes, add_child, check, common, cstr, event, f32, f64, i32, masked, new_task,
                                rmatrix, rvec, set_events, set_handler, u8, with_children)

CT, LT, VT, HT = 0x60, 0x61, 0x62, 0x63  # SCamera, SCameraLens, view cone, Human
GAME = 0
V = {}  # values the fakes return, set by each setup

e = Emu()
fk = Fakes(e)
common(e, fk, {b"SCamera": CT, b"SCameraLens": LT, b"SCameraLensViewCone": VT})
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)


def vec_out(key):
    def run(em, a):
        em.write(a[0], f64(*V[key]))
    return run


def project(em, a):
    """sub_467620: a made up projection of the point, so every vertex differs."""
    x, y, z = struct.unpack("<3d", em.read(a[2], 24))
    em.write(a[0], f32(x * 0.01 + 3, y * 0.02 - z * 0.001))
    if a[1]:
        em.write(a[1], f32(z * 0.5))


def project_local(em, a):
    x, y, z = struct.unpack("<3f", em.read(a[1], 12))
    em.write(a[0], f32(x * 0.3 + 1, y * 0.2 + z))


def screen(em, a):
    x, y, z = struct.unpack("<3d", em.read(a[2], 24))
    em.write(a[1], f32(V.get("depth", 1.5)))
    em.write(a[0], f32(x * 0.01, y * 0.01))


def scale_size(em, a):
    w, h = struct.unpack("<2f", em.read(a[1], 8))
    d = struct.unpack("<f", struct.pack("<I", a[2]))[0]
    em.write(a[0], f32(w / d, h * 2 / d))


def set_euler(em, a):
    ax, ay, az = struct.unpack("<3f", struct.pack("<3I", *a[1:4]))
    ca, sa, cb, sb, cc, sc = math.cos(ax), math.sin(ax), math.cos(ay), math.sin(ay), math.cos(az), math.sin(az)
    em.write(a[0], f32(cb * cc, -cb * sc, sb, sa * sb * cc + ca * sc, -sa * sb * sc + ca * cc, -sa * cb,
                       -ca * sb * cc + sa * sc, ca * sb * sc + sa * cc, ca * cb))


def get_euler(em, a):
    em.write(a[1], f32(V.get("euler", (0.1, 0.2, 0.3))[0]))
    em.write(a[2], f32(V.get("euler", (0.1, 0.2, 0.3))[1]))
    em.write(a[3], f32(V.get("euler", (0.1, 0.2, 0.3))[2]))


def coll_lines(em, a):
    """Coll_Lines: hits come from V['hits'], one per call: None (no hit) or (pos, unk54, material)."""
    hits = V["hits"]
    h = hits.pop(0) if hits else None
    res = bytearray(0x70)
    if h:
        pos, unk54, mat = h
        res[8:0x20] = f64(*pos)
        p = 0
        if mat is not None:
            p = em.alloc(4)
            em.write(p, f32(mat))
        res[0x4C:0x50] = struct.pack("<I", p)
        res[0x54:0x58] = struct.pack("<I", unk54)
        res[0x58:0x5C] = struct.pack("<I", 1)
    em.write(a[1], bytes(res))


def strip_log(em, a):
    s = em.read(a[0], 20)
    n = struct.unpack("<I", s[12:16])[0]
    return (s[:16], em.read(struct.unpack("<I", s[16:20])[0], 0x24 * n))


fk.add(0x4012A0, 3, ret=0x7E57, fmt=masked(~0, 0xFFFF))
fk.add(0x401AE0, 1, ret=0xAE00)
fk.add(0x401B20, 1)
fk.add(0x401CF0, 2, ret=lambda em, a: V.get("isa", 0), fmt=masked(0xFFFF, 0xFFFF))
fk.add(0x414950, 6, ret=lambda em, a: V["yaw"], st0="d")
fk.add(0x414E20, 3, ret=vec_out("point"), fmt=lambda em, a: a[1:])
fk.add(0x416C90, 5, ret=lambda em, a: V.get("blast", 0.0), st0="d")
fk.add(0x465820, 2, ret=vec_out("hpos"), fmt=lambda em, a: a[1:])
fk.add(0x4675B0, 3, ret=project_local, fmt=lambda em, a: (em.read(a[1], 12), a[2]))
fk.add(0x467620, 4, ret=project, fmt=lambda em, a: (em.read(a[2], 24), a[3]))
fk.add(0x480FE0, 16)
fk.add(0x4896C0, 6)
fk.add(0x489750, 4)
fk.add(0x489B90, 1)
fk.add(0x497F20, 3, ret=screen, deref={2: 24}, fmt=lambda em, a: (em.read(a[2], 24),))
fk.add(0x498040, 3, ret=scale_size, fmt=lambda em, a: (em.read(a[1], 8), a[2]))
fk.add(0x4B38E0, 4, ret=set_euler, fmt=lambda em, a: tuple(a[1:]))
fk.add(0x4B3C50, 4, ret=get_euler, fmt=lambda em, a: (em.read(a[0], 36),))
fk.add(0x4B3BE0, 2)
fk.add(0x4B50B0, 11)
fk.add(0x4B53B0, 17)
fk.add(0x4B6CE0, 1, ret=33)
fk.add(0x4B6D00, 1, ret=-17)
fk.add(0x4C1790, 2)
fk.add(0x4C17C0, 2)
fk.add(0x4C1800, 1, ret=0x1800)
fk.add(0x4C3440, 1, ret=0x3440)
fk.add(0x4C48D0, 2)
fk.add(0x4C5710, 3)
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C7560, 2)
fk.add(0x4CC2A0, 8, ret=coll_lines,
       fmt=lambda em, a: (a[0], em.read(a[2], 24), em.read(a[3], 24), a[4], a[5] & 0xFF, em.u32(a[6] + 4), a[7]))
fk.add(0x4CEC10, 1, ret=lambda em, a: V.get("model", 0), fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4D0950, 1, ret=lambda em, a: V.get("radius", 100.0), st0="d")
fk.add(0x4DCCB0, 3)
fk.add(0x4DCDD0, 1)
fk.add(0x4DCF00, 3)
fk.add(0x4E6B00, 2, ret=lambda em, a: V.get("sound", 0), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4E6C00, 1)
fk.add(0x4E6C30, 2)
fk.add(0x4F0EE0, 0, ret=0xEE0)
fk.add(0x489640, 1, ret=0x40, fmt=masked(0xFFFF))
fk.add(0x489680, 1)
fk.add(0x4896A0, 1)
fk.add(0x4169D0, 1, ret=0x5100, fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4F1320, 1)
fk.add(0x4F1340, 1)
fk.add(0x4F1400, 2)
fk.add(0x4F1050, 1, ret=lambda em, a: V.get("id", 0))
fk.add(0x4F16C0, 1, ret=lambda em, a: V.get("expr", 0))
fk.add(0x4F16E0, 2, ret=lambda em, a: V.get("eval", 0.0), st0="d")
fk.add(0x4F1270, 2, ret=lambda em, a: em.write(a[0], b"cam_3\0"), fmt=lambda em, a: (a[1],))
fk.add(0x4B7EE0, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))
fk.add(0x4B7FA0, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))
fk.add(0x4B80A0, 1, fmt=lambda em, a: (em.read_cstr(a[0]),))
STRIP = fk.add(fk.code(b"\xc3"), 1, fmt=strip_log)
e.baseline = e.snapshot()
GAME = e.alloc(0x40)
e.baseline = e.snapshot()


def base(em):
    V.clear()
    set_events(em)
    em.write(0x57C180, struct.pack("<H", CT))
    em.write(0x57C190, struct.pack("<H", LT))
    i32(em, 0x57C18C, 0x18C)
    i32(em, 0x57C198, 0x198)
    i32(em, 0x57C17C, 0x17C00)
    i32(em, 0x57BABC, GAME)
    i32(em, GAME + 0x20, 0x3333)
    i32(em, 0xA44344, 0x4444)
    em.write(0x5BDDB0 + 0x498 + 0x14, struct.pack("<H", 6))
    i32(em, 0xA84A54, 2)
    i32(em, 0xA94E84 + 8, STRIP)


def camera(em, rng):
    c = new_task(em, CT, 0x1C8)
    em.write(c + 0x20, rvec(rng, 1e5))
    em.write(c + 0x68, struct.pack("<II", 0x6868, 0x6C6C))
    em.write(c + 0x70, rmatrix(rng))
    em.write(c + 0x98, f32(*[rng.choice([0.5, 1.0, 2.0]) for _ in range(3)]))
    em.write(c + 0xF0, rvec(rng, 1e5))
    em.write(c + 0x108, f32(rng.uniform(-3, 3)))
    em.write(c + 0x10C, b"cam_model\0")
    em.write(c + 0x120, f32(rng.uniform(-1, 0.5), rng.uniform(-1, 1)))
    em.write(c + 0x128, b"lens_model\0")
    em.write(c + 0x138, b"broken_model\0")
    em.write(c + 0x148, struct.pack("<3h", rng.randint(0, 90), rng.randint(0, 90), rng.randint(1, 90)))
    em.write(c + 0x150, f32(rng.uniform(0, 3)))
    em.write(c + 0x154, struct.pack("<2h", rng.randint(1, 120), rng.randint(-5, 120)))
    em.write(c + 0x158, f32(rng.uniform(1, 50)))
    return c


def lens(em, rng, c):
    l = new_task(em, LT, 0x25E8)
    add_child(em, c, l)
    i32(em, c + 0x15C, l)
    i32(em, l + 0x25D0, c)
    em.write(l + 0x20, rvec(rng, 1e5))
    em.write(l + 0x68, struct.pack("<II", 0x6868, 0x6C6C))
    em.write(l + 0x70, rmatrix(rng))
    em.write(l + 0x98, f32(*[rng.choice([0.5, 1.0, 2.0]) for _ in range(3)]))
    em.write(l + 0xF0, f32(rng.uniform(-1.5, 1), rng.uniform(-1.5, 1.5), rng.uniform(-1.5, 1.5)))
    em.write(l + 0x104, f32(rng.uniform(0, 0.6), 0.5))
    V["point"] = [rng.uniform(-200, 200) for _ in range(3)]
    V["radius"] = rng.uniform(100, 900)
    return l


def detect_list(em, entries):
    """A detection list: entries are (target, visible); returns the head node."""
    nodes = [em.alloc(0x30) for _ in entries] + [em.alloc(0x30)]
    for i, (target, visible) in enumerate(entries):
        i32(em, nodes[i], nodes[i + 1])
        ref = em.alloc(4)
        i32(em, ref, target)
        i32(em, nodes[i] + 8, ref)
        u8(em, nodes[i] + 0x29, visible)
    return nodes[0]


# --- SCamera ----------------------------------------------------------------------

def test_camera_open():
    _, log = check(e, fk, 0x440FD0, "SCamera_Open", base)
    assert len(log) == 57


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(3))
def test_camera_create(seed, cw):
    def setup(em):
        base(em)
        c = camera(em, random.Random(seed))
        em.write(c + 0xF0, b"\x55" * 0xD8)
        V["radius"] = 321.5
        return (c,)
    check(e, fk, 0x4423F0, "SCamera_Create", setup, cw=cw)


@pytest.mark.parametrize("exploded,expr,value", [(1, 1, 1.0), (0, 0, 1.0), (0, 1, 0.4), (0, 1, 1.6), (0, 1, 256.0)])
@pytest.mark.parametrize("kids", [0, 2])
def test_camera_run(exploded, expr, value, kids):
    def setup(em):
        base(em)
        c = camera(em, random.Random(1))
        with_children(em, c, 1, LOG1, kids)
        u8(em, c + 0x170, exploded)
        u8(em, c + 0x168, 0x33)
        V.update(expr=expr, eval=value)
        return (c,)
    check(e, fk, 0x442610, "SCamera_Run", setup)


@pytest.mark.parametrize("id_,handler", [(-1, True), (0, True), (3, False)])
def test_camera_delete(id_, handler):
    def setup(em):
        base(em)
        c = camera(em, random.Random(1))
        set_handler(em, event(0x4F1220), CT, LOG1 if handler else 0)
        V.update(id=id_)
        return (c,)
    check(e, fk, 0x4426D0, "SCamera_Delete", setup)


def test_camera_params():
    def setup(em):
        base(em)
        i32(em, 0x57C184, 0xABCD)
        return 0, em.alloc(4)
    check(e, fk, 0x442730, "SCamera_GetParams", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("arg", [None, b"\0", b"x"])
@pytest.mark.parametrize("kids", [0, 2])
def test_camera_params_changed(arg, kids, cw):
    def setup(em):
        base(em)
        c = camera(em, random.Random(kids))
        with_children(em, c, event(0x4F1A70), LOG2, kids)
        V.update(model=0x7700, radius=444.4)
        return c, cstr(em, arg) if arg is not None else 0
    check(e, fk, 0x442740, "SCamera_ParamsChanged", setup, cw=cw)


@pytest.mark.parametrize("registered", [0, 1])
def test_camera_symbols(registered):
    def setup(em):
        base(em)
        c = camera(em, random.Random(1))
        u8(em, c + 0x171, registered)
        return (c,)
    check(e, fk, 0x442890, "SCamera_AddSymbols", setup)
    check(e, fk, 0x442970, "SCamera_RemoveSymbols", setup)


@pytest.mark.parametrize("exploded", [0, 1, 2])
def test_camera_detect_info(exploded):
    def setup(em):
        base(em)
        c = camera(em, random.Random(1))
        u8(em, c + 0x170, exploded)
        out = em.alloc(0x30)
        em.write(out, b"\x77" * 0x30)
        return c, out
    check(e, fk, 0x442A30, "SCamera_GetDetectInfo", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("kids", [0, 1, 3])
def test_camera_world(kids, cw):
    def setup(em):
        base(em)
        c = camera(em, random.Random(kids))
        with_children(em, c, event(0x4CEA10), LOG2, kids)
        with_children(em, c, event(0x4CEA20), LOG2, kids, type0=0x48)
        arg = em.alloc(8)
        em.write(arg, struct.pack("<II", 0xAAAA, 0xBBBB))
        V["radius"] = 123.25
        return c, arg
    check(e, fk, 0x4434F0, "SCamera_WorldAdd", setup, cw=cw)
    check(e, fk, 0x443450, "SCamera_WorldRemove", setup, cw=cw)


def test_camera_small():
    def setup(em):
        base(em)
        i32(em, 0x57C184, 0x184)
        em.write(0x57C188, struct.pack("<H", VT))
    check(e, fk, 0x443610, "SCamera_Close", setup)
    compare(e, 0x443660, "SCameraLens_GetList", setup=setup)
    compare(e, 0x443670, "SCamera_GetDetectMask", setup=setup)


# --- SCameraLens --------------------------------------------------------------------

@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_lens_create(cw):
    def setup(em):
        base(em)
        rng = random.Random(5)
        c = camera(em, rng)
        l = lens(em, rng, c)
        em.write(l + 0xF0, b"\x55" * 0x20)
        em.write(l + 0x25B8, b"\x55" * 0x30)
        return (l,)
    check(e, fk, 0x4415C0, "SCameraLens_Create", setup, cw=cw)


def track_state(em, rng, c, l, other_ok=True):
    """Detection lists and sweep state. A seen target that is neither a human nor a
    camera leaves the target position uninitialised, so only direct calls use one."""
    human, cam2, other = new_task(em, HT, 0x40), camera(em, rng), new_task(em, 0x70, 0x40)
    pick = rng.choice([human, cam2, other] if other_ok else [human, cam2])
    V["isa"] = int(pick == human)
    V["hpos"] = [rng.uniform(-1e5, 1e5) for _ in range(3)]
    V["yaw"] = rng.choice([rng.uniform(-0.3, 0.3), rng.uniform(-0.1, 0.1), 0.0872664675116539, 0.05])
    lists = [[], [(pick, 1)], [(other, 0), (pick, 1)], [(other, 0)]]
    i32(em, l + 0x1A0, detect_list(em, rng.choice(lists)))
    i32(em, l + 0x1AC, detect_list(em, rng.choice(lists)))
    i32(em, l + 0x25B8, rng.choice([0, pick, other]))
    i32(em, l + 0x25BC, rng.choice([0, 1, 5]))
    i32(em, c + 0x16C, rng.choice([0, 1, 5]))
    i32(em, l + 0x25C0, rng.choice([0, 0x25C0]))
    i32(em, l + 0xFC, rng.choice([0, 0, 3]))
    i32(em, l + 0x100, rng.choice([0, 1, -1]))
    yaw_max = struct.unpack("<h", em.read(c + 0x14A, 2))[0] * 0.01745329238474369
    yaw = rng.choice([rng.uniform(-1.5, 1.5), yaw_max, yaw_max - 0.001, yaw_max + 0.01])
    target = rng.choice([yaw, yaw_max, rng.uniform(-1.5, 1.5)])
    em.write(l + 0xF4, f32(yaw, target))
    V["sound"] = rng.choice([0, 0x50D])


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(200))
def test_lens_track(seed, cw):
    def setup(em):
        base(em)
        rng = random.Random(seed)
        c = camera(em, rng)
        l = lens(em, rng, c)
        track_state(em, rng, c, l)
        return (l,)
    check(e, fk, 0x441BF0, "SCameraLens_Track", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("dir_", [-1, 0, 1])
@pytest.mark.parametrize("seed", range(3))
def test_lens_sweep_end(dir_, seed, cw):
    def setup(em):
        base(em)
        rng = random.Random(seed)
        c = camera(em, rng)
        l = lens(em, rng, c)
        i32(em, l + 0x1A0, detect_list(em, []))
        i32(em, l + 0x1AC, detect_list(em, []))
        yaw = rng.uniform(-1, 1)
        em.write(l + 0xF4, f32(yaw, yaw) + struct.pack("<ii", 0, dir_))
        return (l,)
    check(e, fk, 0x441BF0, "SCameraLens_Track", setup, cw=cw)


@pytest.mark.parametrize("seed", range(40))
def test_lens_sounds(seed):
    def setup(em):
        base(em)
        rng = random.Random(seed)
        c = camera(em, rng)
        l = lens(em, rng, c)
        ref = em.alloc(4)
        i32(em, ref, rng.choice([0, 0xBEE9]))
        i32(em, l + 0x25C4, rng.choice([0, ref]))
        i32(em, l + 0x25C8, rng.choice([0, 0x100F]))
        i32(em, c + 0x16C, rng.choice([0, 3]))
        u8(em, c + 0x169, rng.choice([0, 1]))
        i32(em, c + 0x164, rng.choice([0, 1]))
        V["sound"] = rng.choice([0, 0x50D])
        return (l,)
    check(e, fk, 0x441AE0, "SCameraLens_UpdateSounds", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(120))
def test_lens_run(seed, cw):
    def setup(em):
        base(em)
        rng = random.Random(seed)
        c = camera(em, rng)
        l = lens(em, rng, c)
        track_state(em, rng, c, l, False)
        u8(em, c + 0x168, rng.choice([0, 1]))
        em.write(c + 0x169, bytes([rng.choice([0, 1]), 0x55]))
        pitch = struct.unpack("<f", em.read(c + 0x120, 4))[0]
        rest = -1.3089969158172607
        p = rng.choice([pitch, pitch + 0.01, pitch - 0.01, pitch - 0.1, rest, rest + 0.01, rest - 0.03,
                        rng.uniform(-1.5, 1)])
        em.write(l + 0xF0, f32(p))
        ref = em.alloc(4)
        i32(em, ref, rng.choice([0, 0xBEE9]))
        i32(em, l + 0x25C4, rng.choice([0, ref]))
        i32(em, l + 0x25C8, rng.choice([0, 0x100F]))
        u8(em, l + 0x25CC, rng.choice([0, 0, 1]))
        em.write(l + 0x25D4, struct.pack("<4i", rng.choice([0, 3]), rng.choice([0, 1]), rng.choice([0, 3]),
                                         rng.choice([0, 4, 5, 6])))
        with_children(em, l, 1, LOG1, rng.choice([0, 2]))
        V["model"] = 0x7700
        return (l,)
    check(e, fk, 0x441770, "SCameraLens_Run", setup, cw=cw)


@pytest.mark.parametrize("kind", ["human", "camera", "other"])
def test_lens_target_pos(kind):
    def setup(em):
        base(em)
        rng = random.Random(2)
        t = {"human": new_task(em, HT, 0x40), "camera": camera(em, rng), "other": new_task(em, 0x70, 0x40)}[kind]
        V.update(isa=int(kind == "human"), hpos=(1.5, 2.5, 3.5))
        out = em.alloc(24)
        em.write(out, b"\x66" * 24)
        return out, t
    check(e, fk, 0x442020, "SCameraLens_GetTargetPos", setup)


@pytest.mark.parametrize("beep", [0, 1])
def test_lens_delete(beep):
    def setup(em):
        base(em)
        rng = random.Random(2)
        l = lens(em, rng, camera(em, rng))
        i32(em, l + 0x25C4, 0x25C4 * beep)
        return (l,)
    check(e, fk, 0x442070, "SCameraLens_Delete", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
def test_lens_params_changed(seed, cw):
    def setup(em):
        base(em)
        rng = random.Random(seed)
        l = lens(em, rng, camera(em, rng))
        V["model"] = rng.choice([0, 0x7700])
        return (l,)
    check(e, fk, 0x4420D0, "SCameraLens_ParamsChanged", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("exploded", [0, 1])
@pytest.mark.parametrize("damage,hit", [(0.1, 0.2), (0.3, 0.2), (0.4, 0.1), (0.0, 0.5), (0.2, 0.299999)])
def test_lens_damage(exploded, damage, hit, cw):
    def setup(em):
        base(em)
        rng = random.Random(2)
        c = camera(em, rng)
        l = lens(em, rng, c)
        u8(em, c + 0x170, exploded)
        em.write(l + 0x104, f32(damage, 0.5))
        V["blast"] = hit
        arg = em.alloc(8)
        em.write(arg, f32(9, hit))
        return l, arg
    check(e, fk, 0x4422B0, "SCameraLens_Hit", setup, cw=cw)
    check(e, fk, 0x4422F0, "SCameraLens_Blast", setup, cw=cw)


@pytest.mark.parametrize("which", [0, 1, 2])
def test_lens_is_other(which):
    def setup(em):
        base(em)
        rng = random.Random(2)
        c = camera(em, rng)
        l = lens(em, rng, c)
        arg = em.alloc(0x10)
        em.write(arg, struct.pack("<4I", 0x99, 0, c if which == 1 else 0x111, c if which == 2 else 0x222))
        return l, arg
    check(e, fk, 0x4423C0, "SCameraLens_IsOther", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("on,detect,time,blink,depth", [(0, 0, 0, 0, 1.0), (1, 0, 0, 0, 2.5), (1, 0, 0, 1, 0.75),
                                                        (1, 1, 0, 0, 1.5), (1, 0, 3, 1, 3.0), (1, 1, 2, 1, 1.0),
                                                        (1, 0, 0, 0, 0.0), (1, 1, 0, 0, -1.0)])
def test_lens_draw(on, detect, time, blink, depth, cw):
    def setup(em):
        base(em)
        rng = random.Random(2)
        c = camera(em, rng)
        l = lens(em, rng, c)
        set_handler(em, 4, event(0x4C48C0), LOG2)
        u8(em, c + 0x168, on)
        u8(em, c + 0x169, detect)
        i32(em, c + 0x16C, time)
        i32(em, l + 0x25D8, blink)
        V["depth"] = depth
        return (l,)
    check(e, fk, 0x442A70, "SCameraLens_Draw", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
def test_lens_draw_on_map(seed, cw):
    def setup(em):
        base(em)
        rng = random.Random(seed)
        c = camera(em, rng)
        l = lens(em, rng, c)
        u8(em, c + 0x170, seed == 0)
        u8(em, c + 0x168, seed & 1)
        i32(em, l + 0x25E0, 4)
        center = em.alloc(24)
        em.write(center, rvec(rng, 1e5))
        m = em.alloc(40)
        em.write(m, rmatrix(rng))
        arg = em.alloc(12)
        em.write(arg, struct.pack("<IIf", center, m, 1.5))
        return l, arg
    check(e, fk, 0x442C60, "SCameraLens_DrawOnMap", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_lens_view_cone(seed, cw):
    def setup(em):
        base(em)
        rng = random.Random(seed)
        c = camera(em, rng)
        l = lens(em, rng, c)
        u8(em, c + 0x170, seed == 0)
        V["euler"] = (rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-3, 3))
        hits = []
        for _ in range(40):
            pos = [rng.uniform(-1e5, 1e5) for _ in range(3)]
            hits.append(rng.choice([None, (pos, 0, None), (pos, 1, None), (pos, 1, 0.0), (pos, 1, 0.5),
                                    (pos, 7, 1.0)]))
        V["hits"] = hits
        return l, 0xA59
    check(e, fk, 0x442DD0, "SCameraLens_DrawViewCone", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("kids", [0, 2])
def test_lens_world_add(kids, cw):
    def setup(em):
        base(em)
        rng = random.Random(kids)
        l = lens(em, rng, camera(em, rng))
        with_children(em, l, event(0x4CEA10), LOG2, kids)
        arg = em.alloc(8)
        em.write(arg, struct.pack("<II", 0xAAAA, 0xBBBB))
        return l, arg
    check(e, fk, 0x4432F0, "SCameraLens_WorldAdd", setup, cw=cw)


SCALES = [(1, 2, 3), (3, 2, 1), (2, 3, 1), (1, 1, 1), (2, 1, 2), (1.5, 2.5, 2.5)]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("scale", SCALES)
@pytest.mark.parametrize("addr,name", [(0x4415C0, "SCameraLens_Create"), (0x4422F0, "SCameraLens_Blast"),
                                       (0x4432F0, "SCameraLens_WorldAdd"), (0x4423F0, "SCamera_Create"),
                                       (0x442740, "SCamera_ParamsChanged"), (0x4434F0, "SCamera_WorldAdd")])
def test_scale_patterns(addr, name, scale, cw):
    def setup(em):
        base(em)
        rng = random.Random(7)
        c = camera(em, rng)
        l = lens(em, rng, c)
        t = l if "Lens" in name else c
        em.write(t + 0x98, f32(*scale))
        arg = em.alloc(8)
        em.write(arg, struct.pack("<II", 0xAAAA, 0xBBBB))
        V["blast"] = 0.25
        return t, arg
    check(e, fk, addr, name, setup, cw=cw)
