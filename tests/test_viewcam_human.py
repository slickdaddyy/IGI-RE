import random
import struct

import pytest

from emu import Emu
import test_viewcam_util
from test_viewcam_util import (RNG, ROOT, WORLD, Fakes, f32, f64, i32, log, new_task, ref, set_handler, u8,
                               u16, words)

emu = Emu()
fk = Fakes(emu)
CWS = [0x37F, 0x7F]

TICKS, PROFILE, WEAPON_INFO, TASKLIST1, HUMANLIST, PLAYER, RAIN, FRAME, SOUND, PS = (
    0x4028B0, 0x406220, 0x413C80, 0x4168B0, 0x460C60, 0x416A70, 0x416FB0, 0x4D64C0, 0x4E6B00, 0x4FDF00)
RFLOAT, RINT = 0x4B4770, 0x4B47C0
EV0, EV8, EVW, EVD = 0x05, 0x08, 0x0B, 0x0C

HITS = {}


def coll_lines(e, a):
    log(e, [0x4CC2A0, a[0]] + words(e.read(a[2], 24)) + words(e.read(a[3], 24)) + [a[4], a[5] & 0xFF,
            e.u32(a[6] + 4), a[7]])
    hit = HITS.get("hit")
    if hit:
        e.write(a[1], hit)
    else:
        e.write(a[1] + 0x58, b"\0" * 4)
    return 0


def taskref_add(e, a):
    p = e.alloc(4)
    i32(e, p, a[0])
    return p


fk.add(0x401AE0, 1, fn=taskref_add)
fk.add(0x401B20, 1)
fk.add(TICKS, 0, 100)
fk.add(PROFILE, 0, 0)
fk.add(WEAPON_INFO, 1, 0)
fk.add(0x4167D0, 0, EV0)
fk.add(0x416850, 0, EV8)
fk.add(TASKLIST1, 0, 0)
fk.add(0x416920, 0, WORLD)
fk.add(PLAYER, 1, 0)
fk.add(0x416D20, 0, RNG)
fk.add(RAIN, 0, 0)
fk.add(0x450CE0, 1, deref={0: 0x1C})
fk.add(0x460BF0, 0, EVW)
fk.add(HUMANLIST, 0, 0)
fk.add(0x462390, 6, deref={1: 24, 2: 24, 3: 12})
fk.add(0x47E6F0, 0, EVD)
fk.add(RFLOAT, 1, 0.25, st0="d")
fk.add(RINT, 2, 0)
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4CC2A0, 8, fn=coll_lines, logged=False)
fk.add(0x4D38C0, 2)
fk.add(FRAME, 2, 0)
fk.add(SOUND, 2, 0, deref={1: "s"})
fk.add(0x4E6C00, 1)
fk.add(0x4E6C30, 2, deref={1: 24})
fk.add(0x4E6C60, 3)
fk.add(0x4E6CA0, 2)
fk.add(0x4F0EE0, 0, ROOT)
fk.add(0x4F2060, 2, fn=lambda e, a: e.u32(a[0] + 4 * a[1]))
fk.add(PS, 13, 0, deref={3: 12, 4: 12, 5: 8})
fk.add(0x4FE120, 9, deref={1: 24, 2: 12})
fk.add(0x4FE210, 12, deref={2: 12})
def decal_handler(e, a):
    m = e.read(a[1], 16)
    log(e, [0xF3, a[0]] + words(e.read(e.u32(a[1]), 0x70)) + words(m[4:]))
    return 0


HANDLER = fk.add(None, 2, fn=decal_handler, logged=False)

rng = random.Random(7)


def check(e, fk_, orig, ours, setup=None, **kw):
    def s(em):
        rng.seed(1234)
        return setup(em) if setup else None
    return test_viewcam_util.check(e, fk_, orig, ours, s, **kw)


def rnd(n, lo=-1.0, hi=1.0):
    return [rng.uniform(lo, hi) for _ in range(n)]


def human(e, type_=3):
    h = new_task(e, 0xE00, type_)
    for off in range(0x20, 0xE00, 4):
        i32(e, h + off, 0)
    f64(e, h + 0x20, *rnd(3, -50000, 50000))
    f32(e, h + 0xA0, 300.0)
    return h


def view(e):
    v = new_task(e, 0x328, 9)
    f64(e, v + 0xE0, *rnd(3, -9000, 9000))
    for i, x in enumerate(rnd(9)):
        f32(e, v + 0xF8 + 4 * i, x)
    i32(e, v + 0x11C, 5)
    for i, x in enumerate(rnd(9)):
        f32(e, v + 0x78 + 4 * i, x)
    for i, x in enumerate(rnd(3, -500, 500)):
        f32(e, v + 0x1F0 + 4 * i, x)
    f32(e, v + 0xA0, 200.0)
    return v


# --- 0x462BA0 ------------------------------------------------------------------

@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(6))
def test_avoid_others(case, cw):
    def setup(e):
        to = e.alloc(24)
        frm = e.alloc(24)
        out = e.alloc(12)
        f64(e, frm, 1000.0, 2000.0, 300.0)
        f64(e, to, 1000.0 + 900.0 * (case - 2), 2100.0, 310.0)
        lst = e.alloc(8 + 4 * 4)
        hs = []
        for k in range(4):
            o = human(e)
            u8(e, o + 0xCF6, 0 if (case == 3 and k == 1) else 1)
            u8(e, o + 0xCF5, 1 if (case == 4 and k == 2) else 0)
            f64(e, o + 0x20, 1000.0 + 1500.0 * (k - 1) + 30 * case, 2050.0 + 400 * k, 300.0 + (9000 if case == 5 and k == 0 else 0))
            hs.append(o)
        i32(e, lst, 4 if case else 0, 0, *hs)
        fk.rets[HUMANLIST] = lst
        return human(e), frm, to, out
    check(emu, fk, 0x462BA0, "Human_AvoidHumans", setup, cw=cw, check_eax=False)


# --- platform ------------------------------------------------------------------

def platform(e):
    p = new_task(e, 0xA0, 4)
    f64(e, p + 0x20, *rnd(3, -30000, 30000))
    for i, x in enumerate(rnd(9)):
        f32(e, p + 0x70 + 4 * i, x)
    i32(e, p + 0x94, 11)
    return p


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(3))
def test_move_on_platform(seed, cw):
    def setup(e):
        rng.seed(seed)
        h = human(e)
        i32(e, h + 0x76C, platform(e))
        f64(e, h + 0x770, *rnd(3, -3000, 3000))
        for i, x in enumerate(rnd(3, -40, 40)):
            f32(e, h + 0x520 + 4 * i, x)
        return (h,)
    check(emu, fk, 0x462CF0, "Human_MoveOnPlatform", setup, cw=cw, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(3))
def test_set_platform(seed, cw):
    def setup(e):
        rng.seed(seed)
        h = human(e)
        for i, x in enumerate(rnd(9)):
            f32(e, h + 0x78 + 4 * i, x)
        i32(e, h + 0x9C, 3)
        return h, platform(e)
    check(emu, fk, 0x463310, "Human_SetPlatform", setup, cw=cw, check_eax=False)


def test_clear_platform():
    def setup(e):
        h = human(e)
        i32(e, h + 0x76C, 0x1234)
        return (h,)
    check(emu, fk, 0x463550, "Human_ClearPlatform", setup, check_eax=False)


# --- weapon use ----------------------------------------------------------------

def weapon_handler(e, a):
    m = a[1]
    data = e.read(m, 0x30)
    eye, cam, vel = struct.unpack("<3I", data[4:16])
    log(e, [0xF1, a[0], data[0], data[1]] + words(e.read(eye, 24)) + words(e.read(cam, 40)) + words(e.read(vel, 12))
        + words(data[0x10:0x30]))
    e.write(m + 1, b"\x01")
    f32(e, m + 0x20, 0.125)
    f32(e, m + 0x24, -0.375)
    return 0


WEAPON_HANDLER = fk.add(None, 2, fn=weapon_handler, logged=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", ["none", "gun", "g0", "g10", "g100", "g3", "g60"])
def test_update_weapon(case, cw):
    def setup(e):
        rng.seed(3)
        h = human(e)
        v = view(e)
        i32(e, h + 0x4EC, v)
        f32(e, v + 0x128, 0.5)
        f32(e, v + 0x134, -0.25)
        i32(e, v + 0x1EC, 1)
        e.write(h + 0x4D0, bytes([0xFE, 1, 0x80, 1]))
        i32(e, h + 0x4D4, 77)
        for i, x in enumerate(rnd(3)):
            f32(e, h + 0x688 + 4 * i, x)
        if case != "none":
            w = new_task(e, 0x40, 0x21)
            i32(e, h + 0x4C4, w)
            info = e.alloc(0x700)
            i32(e, info + 0x610, 5 if case == "gun" else 4)
            f32(e, info + 0x660, 3.75)
            fk.rets[WEAPON_INFO] = info
            if case != "gun":
                i32(e, h + 0x4E0, int(case[1:]))
            set_handler(e, EVW, 0x21, WEAPON_HANDLER)
        return (h,)
    check(emu, fk, 0x462E50, "Human_UpdateWeapon", setup, cw=cw, check_eax=False)


@pytest.mark.parametrize("n", [0, 1, 3])
def test_send_position(n):
    def setup(e):
        h = human(e)
        f32(e, h + 0xA0, 123.5)
        lst = e.alloc(8 + 4 * n)
        ts = [new_task(e, 0x40, 0x30 + k) for k in range(n)]
        i32(e, lst, n, 0, *ts)
        fk.rets[TASKLIST1] = lst
        for k in range(n):
            set_handler(e, EV0, 0x30 + k, HANDLER_POS)
        return (h,)
    check(emu, fk, 0x463080, "Human_SendPosition", setup, check_eax=False)


def pos_handler(e, a):
    d = e.read(a[1], 0x24)
    log(e, [0xF2, a[0]] + words(d[:4]) + words(d[8:]))
    return 0


HANDLER_POS = fk.add(None, 2, fn=pos_handler, logged=False)


# --- getters and setters ----------------------------------------------------------

@pytest.mark.parametrize("addr,name,off,size", [
    (0x463110, "Human_GetState", 0x2FC, 4), (0x463120, "Human_GetIndex", 0x7C0, 4), (0x463130, "Human_GetTeam", 0xFC, 4),
    (0x463170, "Human_IsDead", 0xCF5, 1), (0x463180, "Human_IsCrouching", 0x33D, 1),
    (0x463190, "Human_IsActive", 0xCF6, 1)])
def test_getters(addr, name, off, size):
    def setup(e):
        h = human(e)
        e.write(h + off, bytes([0x5A, 0x81, 0x12, 0x9C])[:size])
        return (h,)
    check(emu, fk, addr, name, setup, eax_mask=0xFF if size == 1 else 0xFFFFFFFF)


def test_get_matrix():
    check(emu, fk, 0x463140, "Human_GetMatrix", lambda e: (human(e),))


@pytest.mark.parametrize("player", [False, True])
def test_is_player(player):
    def setup(e):
        h = human(e)
        fk.rets[PLAYER] = h if player else h + 4
        return (h,)
    check(emu, fk, 0x463150, "Human_IsPlayer", setup, eax_mask=0xFF)


@pytest.mark.parametrize("addr,name", [
    (0x463570, "Human_DisableCollision"), (0x463580, "Human_EnableCollision"), (0x463590, "Human_SetExactMotion"), (0x4635A0, "Human_ClearExactMotion"),
    (0x4635B0, "Human_LockInput"), (0x4635E0, "Human_UnlockInput"), (0x4635F0, "Human_FreezeView"),
    (0x463600, "Human_UnfreezeView"), (0x463610, "Human_BeginUse"), (0x463620, "Human_EndUse"), (0x464230, "Human_HideShadow"),
    (0x464240, "Human_ShowShadow")])
def test_flags(addr, name):
    def setup(e):
        h = human(e)
        e.write(h + 0x100, bytes(range(1, 0xFF)) * 4)
        for off in (0x694, 0x52C, 0x250, 0x510, 0x7B8, 0x7B9, 0x7E8):
            u8(e, h + off, 0x55)
        return (h,)
    check(emu, fk, addr, name, setup, check_eax=False)


@pytest.mark.parametrize("addr,name,off", [
    (0x463640, "Human_SetGround", 0x768), (0x4636A0, "Human_SetClimbObject", 0x7B4),
    (0x463700, "Human_SetAttached", 0x7B0)])
@pytest.mark.parametrize("old,new", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_set_ref(addr, name, off, old, new):
    def setup(e):
        h = human(e)
        if old:
            i32(e, h + off, ref(e, 0x4444))
        return h, 0x7777 if new else 0
    check(emu, fk, addr, name, setup, check_eax=False)


@pytest.mark.parametrize("addr,name,off", [
    (0x463680, "Human_GetGround", 0x768), (0x4636E0, "Human_GetClimbObject", 0x7B4),
    (0x463740, "Human_GetAttached", 0x7B0)])
@pytest.mark.parametrize("state", [0, 1, 2])
def test_get_ref(addr, name, off, state):
    def setup(e):
        h = human(e)
        if state:
            i32(e, h + off, ref(e, 0x4444 if state == 2 else 0))
        return (h,)
    check(emu, fk, addr, name, setup)


def test_view_cam():
    check(emu, fk, 0x463760, "HumanViewCam", lambda e: (human(e), 3), check_eax=False)


@pytest.mark.parametrize("cw", CWS)
def test_damp(cw):
    def setup(e):
        h = human(e)
        f32(e, h + 0x664, 13.7)
        f32(e, h + 0x668, -1.3)
        f32(e, h + 0x66C, 911.2)
        return (h,)
    check(emu, fk, 0x463770, "Human_ApplyGravity", setup, cw=cw, check_eax=False)


# --- position ------------------------------------------------------------------

@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("in_world", [0, 1])
def test_set_position(in_world, cw):
    def setup(e):
        rng.seed(5)
        h = human(e)
        v = view(e)
        u8(e, v + 0xD9, in_world)
        i32(e, h + 0x4EC, v)
        for off in range(0x670, 0x688, 4):
            f32(e, h + off, 1.5)
        pos = e.alloc(24)
        f64(e, pos, 1.25, -7.5, 3.0)
        return h, pos
    check(emu, fk, 0x4631A0, "Human_SetPosition", setup, cw=cw, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
def test_set_yaw(cw):
    def setup(e):
        return human(e), struct.unpack("<I", struct.pack("<f", 0.7))[0]
    check(emu, fk, 0x463220, "Human_SetYaw", setup, cw=cw, check_eax=False)


def test_snap():
    def setup(e):
        rng.seed(9)
        h = human(e)
        v = view(e)
        u8(e, v + 0xD9, 1)
        i32(e, h + 0x4EC, v)
        i32(e, h + 0x2F4, 0x1234)
        return (h,)
    check(emu, fk, 0x463260, "Human_SnapToGround", setup, check_eax=False)


@pytest.mark.parametrize("ground", [0, 1])
def test_leave_ground(ground):
    def setup(e):
        h = human(e)
        i32(e, h + 0x2F4, 0xFFFF)
        i32(e, h + 0x6F8, 3, *([0] * 11), 1, 2, 3)
        if ground:
            i32(e, h + 0x768, ref(e, 0x999))
        return (h,)
    check(emu, fk, 0x4632D0, "Human_LeaveGround", setup, check_eax=False)


# --- sounds --------------------------------------------------------------------

@pytest.mark.parametrize("case", ["early", "none", "empty", "playing"])
@pytest.mark.parametrize("r", [0.0, 0.37, 0.5, 0.99])
def test_say_timed(case, r):
    def setup(e):
        h = human(e)
        i32(e, h + 0x4EC, 0x4ECC)
        i32(e, h + 0x844, 200 if case == "early" else 50)
        if case == "empty":
            i32(e, h + 0x840, ref(e, 0))
        elif case == "playing":
            i32(e, h + 0x840, ref(e, 0x5050))
        fk.set_seq(RFLOAT, [r])
        fk.set_seq(RINT, [2])
        name = e.alloc(16)
        e.write(name, b"guard_hey_\0")
        return h, name, 3
    check(emu, fk, 0x4637C0, "Human_SayTimed", setup, check_eax=False)


@pytest.mark.parametrize("case", ["none", "empty", "playing"])
def test_say(case):
    def setup(e):
        h = human(e)
        if case == "empty":
            i32(e, h + 0x840, ref(e, 0))
        elif case == "playing":
            i32(e, h + 0x840, ref(e, 0x5050))
        fk.set_seq(RINT, [4])
        name = e.alloc(16)
        e.write(name, b"player_death_\0")
        return h, name, 5
    check(emu, fk, 0x4638A0, "Human_Say", setup, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", ["anim", "frame", "air", "noground", "walk", "crouch", "rain", "nosound",
                                  "rain_crouch", "wrap"])
def test_footstep(case, cw):
    def setup(e):
        h = human(e)
        i32(e, h + 0x538, 0x3F1 if case == "anim" else 0x3F2, 17)
        fk.rets[FRAME] = 16 if case == "frame" else 17
        i32(e, h + 0x6F8, 0 if case == "air" else 1)
        i32(e, h + 0x84C, 11 if case == "wrap" else 4)
        i32(e, h + 0xFC, 2)
        f64(e, h + 0x6A8, 1.0, 2.0, 3.0)
        if case != "noground":
            g = e.alloc(0x200)
            f32(e, g + 0x50, 0.75)
            for k in range(6):
                e.write(g + 0x54 + 0x20 * k, b"step_%d\0" % k)
            i32(e, h + 0x6EC, g)
        u8(e, h + 0x33D, 1 if "crouch" in case else 0)
        fk.rets[RAIN] = 1 if "rain" in case else 0
        fk.rets[SOUND] = 0 if case == "nosound" else 0x5D5D
        fk.set_seq(RFLOAT, [0.61])
        return (h,)
    check(emu, fk, 0x463920, "Human_Footstep", setup, cw=cw, check_eax=False)


# --- blood ---------------------------------------------------------------------

def coll_hit(e, found=1, material=1, material2=0, decals=1, task_type=0x40):
    hit = bytearray(0x70)
    task = new_task(e, 0x40, task_type)
    struct.pack_into("<I", hit, 0, task)
    struct.pack_into("<3d", hit, 8, 11.0, 22.0, 33.0)
    if material:
        mat = e.alloc(0x300)
        tab = e.alloc(0x60)
        i32(e, mat + 0x258, tab)
        i32(e, tab + 0x58, decals)
        struct.pack_into("<I", hit, 0x4C, mat)
    struct.pack_into("<I", hit, 0x50, material2)
    struct.pack_into("<I", hit, 0x58, found)
    return bytes(hit)


def sprites(e):
    for addr in (0x5BDAD4, 0x5BDAD0, 0x5BDB08, 0x5BDB10, 0x5BDB14, 0x5BDB04):
        s = e.alloc(0x40)
        hdr = e.alloc(8)
        u16(e, hdr + 4, 4)
        i32(e, s + 0x24, hdr)
        i32(e, addr, s)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", ["noblood", "miss", "hit", "hit1", "hit2", "nomat", "mat2", "nodecal", "nohandler",
                                  "refs", "deadrefs", "zerodir"])
def test_bleed(case, cw):
    def setup(e):
        rng.seed(1)
        h = human(e)
        prof = e.alloc(16)
        u8(e, prof + 8, 0 if case == "noblood" else 1)
        fk.rets[PROFILE] = prof
        sprites(e)
        d = e.alloc(12)
        if case == "zerodir":
            f32(e, d, 0.0), f32(e, d + 4, 0.0), f32(e, d + 8, 0.0)
        else:
            for i, x in enumerate([0.3, -0.8, 0.45]):
                f32(e, d + 4 * i, x)
        src = e.alloc(0x70)
        f64(e, src + 8, 4000.5, -2000.25, 777.0)
        HITS["hit"] = {"miss": coll_hit(e, found=0), "nomat": coll_hit(e, material=0),
                       "mat2": coll_hit(e, material2=5), "nodecal": coll_hit(e, decals=0),
                       "nohandler": coll_hit(e, task_type=0x41)}.get(case, coll_hit(e))
        set_handler(e, EVD, 0x40, HANDLER)
        set_handler(e, EVD, 0x41, 0)
        if case == "refs":
            i32(e, h + 0x838, ref(e, 0x3838), ref(e, 0x3C3C))
        elif case == "deadrefs":
            i32(e, h + 0x838, ref(e, 0), ref(e, 0))
        fk.set_seq(RFLOAT, [0.1 * k + 0.05 for k in range(10)])
        fk.set_seq(RINT, [{"hit1": 1, "hit2": 2}.get(case, 0), 2, 1])
        fk.rets[PS] = 0x5050
        return h, d, src
    check(emu, fk, 0x463AD0, "Human_Bleed", setup, cw=cw, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", ["nohandler", "away", "into", "into1", "into2", "zeronormal", "nan"])
def test_blood_drop(case, cw):
    def setup(e):
        sprites(e)
        hit = e.alloc(0x70)
        task = new_task(e, 0x40, 0x42 if case == "nohandler" else 0x40)
        i32(e, hit, task)
        n = {"zeronormal": (0.0, 0.0, 0.0), "nan": (float("nan"), 0.0, 1.0)}.get(case, (0.2, -0.4, 1.3))
        for i, x in enumerate(n):
            f32(e, hit + 0x38 + 4 * i, x)
        set_handler(e, EVD, 0x40, HANDLER)
        set_handler(e, EVD, 0x42, 0)
        arg = e.alloc(0x38)
        i32(e, arg, hit)
        f64(e, arg + 8, 10.0, 20.0, 30.0 + (-5.0 if case.startswith("into") or case == "nan" else 5.0))
        f64(e, arg + 0x20, 10.5, 19.0, 30.0)
        fk.set_seq(RFLOAT, [0.33])
        fk.set_seq(RINT, [{"into1": 1, "into2": 2}.get(case, 0)])
        return (arg,)
    check(emu, fk, 0x4640B0, "Human_BloodDrop", setup, cw=cw, check_eax=False)
