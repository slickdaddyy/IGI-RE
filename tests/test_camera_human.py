"""HumanCamera (0x482580 - 0x4849B0)."""
import math
import random
import struct

import pytest

from emu import ErrorShown
from test_camera_util import (LOG, RET, d64, f32, make_emu, pyfake, run, set_handler, task, u16, u32)
from test_menu_util import trap

emu = make_emu()
CAM, HUMAN = 0x62, 0x63
EVENT = 0x36
CWS = [0x37F, 0x7F]
LIST = 0x5C1188
INFO = {}


def info_handler(e):
    h, out = e.arg(0), e.arg(1)
    if LOG:
        LOG[-1].append(("info", e.normalize(h)))
    i = INFO
    d64(e, out, *i["pos"])
    f32(e, out + 0x18, *i["matrix"])
    u32(e, out + 0x3C, 5)
    f32(e, out + 0x40, i["fov"])
    f32(e, out + 0x44, *i["body"])
    u32(e, out + 0x68, 9)
    f32(e, out + 0x6C, i["unk6C"])
    u32(e, out + 0x70, i["mode"])
    e.ret(0)


INFO_FN = trap(emu, "info", info_handler)
emu.baseline = emu.snapshot()


def rot(rng):
    """A random rotation matrix (rows)."""
    a, b, c = (rng.uniform(-math.pi, math.pi) for _ in range(3))
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    return [cb * cc, -cb * sc, sb, sa * sb * cc + ca * sc, -sa * sb * sc + ca * cc, -sa * cb,
            -ca * sb * cc + sa * sc, ca * sb * sc + sa * cc, ca * cb]


def set_info(rng, mode=1):
    INFO.clear()
    INFO.update(pos=[rng.uniform(-90000, 90000) if rng.random() < 0.3 else rng.uniform(-5, 5) for _ in range(3)], matrix=rot(rng), fov=rng.uniform(0.3, 1.2),
                body=rot(rng), unk6C=rng.uniform(0.3, 1.0), mode=mode)


def human(e, killer=0):
    h = task(e, 0xE00, HUMAN)
    d64(e, h + 0x20, 1500.25, -3200.5, 400.125)
    if killer is not None:
        r = e.alloc(4)
        u32(e, r, killer)
        u32(e, h + 0xDE8, r)
    return h


def camera(e, rng, h=None, mode=1, prev=1, dist=2000.0, target=8000.0, flags=0, pitch=0.3, yaw=-1.2):
    set_handler(e, EVENT, HUMAN, INFO_FN)
    e.write(0x540992, bytes([EVENT]))
    c = task(e, 0x128, CAM)
    d64(e, c + 0x20, *[rng.uniform(-9000, 9000) for _ in range(3)])
    f32(e, c + 0x38, *rot(rng))
    u32(e, c + 0x5C, 3)
    f32(e, c + 0x60, 0.7)
    u32(e, c + 0x64, 0x6464)
    if h is not None:
        r = e.alloc(4)
        u32(e, r, h)
        u32(e, c + 0x68, r)
    d64(e, c + 0x70, *INFO.get("pos", [1.0, 2.0, 3.0]))
    f32(e, c + 0x88, *INFO.get("matrix", rot(rng)))
    f32(e, c + 0xB0, INFO.get("fov", 0.8))
    f32(e, c + 0xB4, *INFO.get("body", rot(rng)))
    u32(e, c + 0xE0, mode, 0, prev)
    d64(e, c + 0xF0, *[rng.uniform(-9000, 9000) for _ in range(3)])
    f32(e, c + 0x108, pitch, yaw, dist, target)
    u32(e, c + 0x118, 5, 9, 0x1200, flags)
    return c


def fill_list(e, used, null_at=None):
    for i in range(128):
        u32(e, LIST + 8 * i, 0)
        e.write(LIST + 8 * i + 4, b"\0\0")
    for i in used:
        r = e.alloc(4)
        u32(e, r, 0 if i == null_at else 0x7000 + i)
        u32(e, LIST + 8 * i, r)
        e.write(LIST + 8 * i + 5, b"\1")


def test_open():
    def setup(e):
        for i in range(128):
            u32(e, LIST + 8 * i, 0x11111111, 0x22222222)
    run(emu, 0x4825C0, "HumanCamera_Open", setup)


def test_create():
    def setup(e):
        c = task(e, 0x128, CAM)
        e.write(c + 0x20, b"\xab" * 0x108)
        return (c,)
    run(emu, 0x482650, "HumanCamera_CreateHandler", setup)


@pytest.mark.parametrize("ref,isa,killer", [(None, 0, 0), (1, 0, 0), (1, 1, 0), (1, 1, None), (1, 1, 0x4444)])
def test_has_killer(ref, isa, killer):
    def setup(e):
        RET[0x401CF0] = isa
        c = camera(e, random.Random(1), human(e, killer) if ref else None)
        if killer is None:
            u32(e, e.u32(c + 0x68) and e.u32(e.u32(c + 0x68)) + 0xDE8, 0)
        return (c,)
    run(emu, 0x482580, "HumanCamera_HasKiller", setup, check_eax=True)


@pytest.mark.parametrize("used,null_at,index", [((), None, 0), ((3, 40, 127), None, 5), ((0,), None, 0),
                                                ((40,), 40, 1), ((127,), None, -1), ((5,), None, 300)])
@pytest.mark.parametrize("fn", ["find", "next", "prev"])
def test_list(used, null_at, index, fn):
    addr, name = {"find": (0x482880, "HumanCamera_FindHuman"), "next": (0x482930, "HumanCamera_NextHuman"),
                  "prev": (0x4829A0, "HumanCamera_PrevHuman")}[fn]

    def setup(e):
        fill_list(e, used, null_at)
        c = camera(e, random.Random(2))
        u32(e, c + 0x118, index)
        return (c,)
    if null_at is not None:
        for target in (addr, name):
            with pytest.raises(ErrorShown):
                emu.reset()
                emu.call(target, *setup(emu))
        return
    run(emu, addr, name, setup)


@pytest.mark.parametrize("key", [None, 0x10, 0x1E, 0x12])
def test_control_list(key):
    def setup(e):
        if key:
            RET[("pressed", key)] = 1
        fill_list(e, (7, 20))
        return (camera(e, random.Random(3)),)
    run(emu, 0x4828D0, "HumanCamera_ControlListInput", setup)


@pytest.mark.parametrize("addr,name", [(0x482A30, "HumanCamera_FirstPersonEnter"),
                                       (0x482C10, "HumanCamera_ThirdPersonEnter"),
                                       (0x483360, "HumanCamera_MovementEnter"),
                                       (0x483D40, "HumanCamera_DeathCameraEnter")])
def test_enter(addr, name):
    run(emu, addr, name, lambda e: (camera(e, random.Random(4), flags=6),))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", [1, 2, 3])
def test_first_person(cw, seed):
    def setup(e):
        rng = random.Random(seed)
        set_info(rng)
        return (camera(e, rng),)
    run(emu, 0x482A40, "HumanCamera_FirstPersonUpdate", setup, cw=cw)


@pytest.mark.parametrize("keys", [(), (0x49, 0x48, 0x4B), (0x51, 0x50, 0x4D), (0x49, 0x51)])
@pytest.mark.parametrize("dist,pitch", [(2000.0, 0.3), (1500.0, -1.5), (409000.0, 1.5), (1024.0, 1.6),
                                        (math.nan, math.nan), (1e6, -2.0)])
def test_third_person_input(keys, dist, pitch):
    def setup(e):
        for k in keys:
            RET[("down", k)] = 1
        return (camera(e, random.Random(5), dist=dist, pitch=pitch),)
    run(emu, 0x482C30, "HumanCamera_ThirdPersonControlInput", setup)


def far_run(addr, name, setup, cw=None, **kw):
    """run() ignoring the camera matrix serial, which the far modes take from
    uninitialised stack. A dry setup finds the camera address."""
    emu.reset()
    c = setup(emu)[0]
    run(emu, addr, name, setup, cw=cw, ignore=[(c + 0x5C, c + 0x60)], **kw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", [1, 2, 3, 4])
@pytest.mark.parametrize("dist", [0.0, 2000.0, 40000.0])
def test_third_person(cw, seed, dist):
    def setup(e):
        rng = random.Random(seed)
        set_info(rng)
        return (camera(e, rng, dist=dist, pitch=rng.uniform(-1.5, 1.5), yaw=rng.uniform(-3, 3)),)
    far_run(0x482D70, "HumanCamera_ThirdPersonUpdate", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", [1, 2, 3, 4, 5, 6])
@pytest.mark.parametrize("kind", ["random", "same", "up", "samexy"])
def test_look_at(cw, seed, kind):
    def setup(e):
        rng = random.Random(seed)
        a, b = e.alloc(24), e.alloc(24)
        pa = [rng.uniform(-9e4, 9e4) for _ in range(3)]
        pb = {"random": [rng.uniform(-9e4, 9e4) for _ in range(3)], "same": pa,
              "up": [pa[0], pa[1], pa[2] + 500], "samexy": [pa[0] + 1e-9, pa[1], pa[2] - 3]}[kind]
        d64(e, a, *pa)
        d64(e, b, *pb)
        m = e.alloc(40)
        u32(e, m + 36, 0x5E)
        return m, a, b
    run(emu, 0x483100, "HumanCamera_LookAt", setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("dist,target", [(1000.0, 8000.0), (8000.0, 1000.0), (8000.0, 8000.0), (7900.0, 8000.0),
                                         (8100.0, 8000.0), (math.nan, 1.0)])
@pytest.mark.parametrize("keys", [(), (0x49,), (0x51,)])
@pytest.mark.parametrize("invert", [0, 1])
@pytest.mark.parametrize("mouse,pitch", [((3.0, -2.0), 0.5), ((0.1, 0.7), 1.0), ((-0.4, -1.1), -0.6)])
def test_movement_input(cw, dist, target, keys, invert, mouse, pitch):
    def setup(e):
        for k in keys:
            RET[("down", k)] = 1
        p = e.alloc(0x540)
        u32(e, p + 0x538, invert)
        RET[0x406220] = p
        f32(e, 0xBC20F4, *mouse)
        return (camera(e, random.Random(6), dist=dist, target=target, pitch=pitch),)
    run(emu, 0x483390, "HumanCamera_MovementControlInput", setup, cw=cw)


def mat_hit(found=1, unk54=0, mat=None, pos=(0.0, 0.0, 0.0), normal=(0.0, 0.0, 1.0)):
    return (found, unk54, mat, pos, normal)


def near(rng):
    p = INFO["pos"]
    return [p[i] + rng.uniform(-3000, 3000) for i in range(3)]


def coll_cases(rng):
    """Coll_Lines answers: nothing; a wall then the 8 body lines; see-through glass then a wall."""
    wall = mat_hit(pos=near(rng))
    glass = mat_hit(unk54=1, mat=0.5, pos=near(rng))
    opaque = mat_hit(unk54=1, mat=0.0, pos=near(rng))
    return {
        "none": [],
        "wall_clear": [[wall], [mat_hit(0)]],
        "wall_blocked": [[wall]] + [[mat_hit()]] * 8,
        "wall_blocked_glass": [[wall], [glass]] + [[mat_hit()]] * 8,
        "glass_wall_blocked": [[glass], [opaque]] + [[mat_hit()]] * 8,
        "close_wall": [[mat_hit(pos=INFO["pos"])]] + [[mat_hit()]] * 8,
        "half": [[wall]] + [[mat_hit()]] * 4 + [[mat_hit(0)]],
    }


CASES = ["none", "wall_clear", "wall_blocked", "wall_blocked_glass", "glass_wall_blocked", "close_wall", "half"]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", [1, 2, 3, 4, 5, 6])
@pytest.mark.parametrize("case", CASES)
@pytest.mark.parametrize("addr,name", [(0x4834D0, "HumanCamera_MovementUpdate"),
                                       (0x484450, "HumanCamera_DeathCameraUpdate")])
def test_follow(cw, seed, case, addr, name):
    def setup(e):
        rng = random.Random(seed)
        set_info(rng)
        RET["coll"] = coll_cases(rng)[case]
        h = human(e)
        return (camera(e, rng, h, target=rng.uniform(2000, 20000), pitch=rng.uniform(-1.3, 1.3),
                       yaw=rng.uniform(-3, 3)),)
    far_run(addr, name, setup, cw=cw)


@pytest.mark.parametrize("isa,killer,flags", [(1, 0x4444, 1), (1, 0x4444, 0), (1, 0, 1), (0, 0x4444, 1)])
def test_death_update_killer(isa, killer, flags):
    def setup(e):
        RET[0x401CF0] = isa
        rng = random.Random(9)
        set_info(rng)
        return (camera(e, rng, human(e, killer), flags=flags),)
    far_run(0x484450, "HumanCamera_DeathCameraUpdate", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", [1, 2])
@pytest.mark.parametrize("case", ["clear", "blocked", "glass", "glass_clear"])
def test_is_blocked(cw, seed, case):
    def setup(e):
        rng = random.Random(seed)
        set_info(rng)
        RET["coll"] = {"clear": [[mat_hit()]] * 3 + [[mat_hit(0)]], "blocked": [[mat_hit()]] * 8,
                       "glass": [[mat_hit()], [mat_hit(unk54=1, mat=1.0, pos=near(rng))]] + [[mat_hit()]] * 7,
                       "glass_clear": [[mat_hit(unk54=1, mat=1.0, pos=near(rng))], [mat_hit(0)]]}[case]
        h = human(e)
        to = e.alloc(24)
        d64(e, to, *near(rng))
        return 0x1111, h, to, 0x2222
    run(emu, 0x483A00, "HumanCamera_IsBlocked", setup, cw=cw, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("dist,target", [(1000.0, 8000.0), (8000.0, 1000.0), (8100.0, 8000.0)])
@pytest.mark.parametrize("keys", [(), (0x49,), (0x51,)])
@pytest.mark.parametrize("mouse,pitch", [((3.0, -2.0), 0.5), ((0.1, 0.7), 1.0), ((-0.4, -1.1), -0.6)])
@pytest.mark.parametrize("killer,flags", [(0, 0), (0x4444, 1), (0x4444, 0)])
def test_death_input(cw, dist, target, keys, mouse, pitch, killer, flags):
    def setup(e):
        RET[0x401CF0] = 1
        for k in keys:
            RET[("down", k)] = 1
        f32(e, 0xBC20F4, *mouse)
        rng = random.Random(7)
        set_info(rng)
        return (camera(e, rng, human(e, killer), dist=dist, target=target, pitch=pitch, flags=flags),)
    run(emu, 0x483D80, "HumanCamera_DeathCameraControlInput", setup, cw=cw)


def death_hits(rng, which):
    """4 lines: the hit normal faces the line (dot < 0) or not."""
    out = []
    for i in range(4):
        if i not in which:
            out.append(mat_hit(0))
            continue
        p = INFO["pos"]
        out.append(mat_hit(pos=[p[k] + rng.uniform(-30000, 30000) for k in range(3)],
                           normal=tuple(rng.uniform(-1, 1) for _ in range(3))))
    return [out]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", [1, 2, 3, 4, 5])
@pytest.mark.parametrize("which", [(), (0,), (2, 3), (0, 1, 2, 3), (1, 3)])
@pytest.mark.parametrize("target", [3000.0, 10200.0, 12000.0])
def test_death_collide(cw, seed, which, target):
    def setup(e):
        rng = random.Random(seed)
        set_info(rng)
        RET["coll"] = death_hits(rng, which)
        return (camera(e, rng, human(e), dist=rng.uniform(1000, 9000), target=target, pitch=rng.uniform(-1, 1),
                       yaw=rng.uniform(-3, 3)), fbits(rng.uniform(-0.1, 0.1)), fbits(rng.uniform(-0.1, 0.1)))
    run(emu, 0x483EC0, "HumanCamera_DeathCameraCollide", setup, cw=cw)


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("mode", [0, 1, 2, 3, 4, 5, 6])
@pytest.mark.parametrize("prev", [0, 1])
@pytest.mark.parametrize("seed", [1, 2])
def test_run(cw, mode, prev, seed):
    def setup(e):
        RET[0x489470] = 0.625
        RET[0x401CF0] = 1
        RET[0x406220] = e.alloc(0x540)
        f32(e, 0xBC20F4, 0.3, -0.2)
        rng = random.Random(seed)
        set_info(rng, mode)
        RET["coll"] = coll_cases(rng)["wall_blocked"] if mode == 3 else death_hits(rng, (0, 2))
        return (camera(e, rng, human(e, 0x4444), prev=mode if prev else 0, flags=1),)
    far_run(0x482700, "HumanCamera_RunHandler", setup, cw=cw)


@pytest.mark.parametrize("ref", ["none", "empty"])
def test_run_no_human(ref):
    def setup(e):
        fill_list(e, (12,))
        c = camera(e, random.Random(1), human(e) if ref == "empty" else None)
        if ref == "empty":
            u32(e, e.u32(c + 0x68), 0)
        return (c,)
    run(emu, 0x482700, "HumanCamera_RunHandler", setup)


@pytest.mark.parametrize("seed", range(10, 50))
@pytest.mark.parametrize("addr,name,dist", [(0x4834D0, "HumanCamera_MovementUpdate", 0),
                                            (0x482D70, "HumanCamera_ThirdPersonUpdate", 1)])
def test_rounding(seed, addr, name, dist):
    """Many cases at single precision, where the sum orders show."""
    def setup(e):
        rng = random.Random(seed)
        set_info(rng)
        RET["coll"] = []
        return (camera(e, rng, human(e), dist=rng.uniform(1500, 30000) * dist, target=rng.uniform(2000, 20000),
                       pitch=rng.uniform(-1.3, 1.3), yaw=rng.uniform(-3, 3)),)
    far_run(addr, name, setup, cw=0x7F)


@pytest.mark.parametrize("seed", range(10, 40))
def test_death_collide_rounding(seed):
    def setup(e):
        rng = random.Random(seed)
        set_info(rng)
        RET["coll"] = death_hits(rng, (0, 1, 2, 3))
        return (camera(e, rng, human(e), dist=rng.uniform(1000, 9000), target=5000.0, pitch=rng.uniform(-1, 1),
                       yaw=rng.uniform(-3, 3)), fbits(rng.uniform(-0.1, 0.1)), fbits(rng.uniform(-0.1, 0.1)))
    run(emu, 0x483EC0, "HumanCamera_DeathCameraCollide", setup, cw=0x7F)
