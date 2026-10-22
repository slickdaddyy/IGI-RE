import math
import random
import struct

import pytest

import test_viewcam_util
from emu import Emu
from test_viewcam_util import (RNG, WORLD, Fakes, f32, f64, i32, log, new_task, ref, rf32, set_handler, u8, words)

emu = Emu()
fk = Fakes(emu)
CWS = [0x37F, 0x7F]

TASK_NEW, MEM_MODE, GAME, COMPUTER, CAMLIST, RFLOAT, BONE = (
    0x4012A0, 0x4B0DB0, 0x416D40, 0x46C060, 0x4E81F0, 0x4B4770, 0x4D3210)
PARENT, HUMAN_EV, RIGID, HUMAN_TYPE = 0x88, 0x24, 0x77, 0x31
rng = random.Random(5)
COLL = {}


def coll(e, a, addr, extra):
    log(e, [addr, a[0]] + words(e.read(a[2], 24)) + words(e.read(a[3], 24)) + [a[4], a[5] & 0xFF]
        + words(e.read(a[6], 8)) + a[7:7 + extra])
    hit = bytearray(0x70)
    h = COLL.get(addr)
    if h:
        struct.pack_into("<3d", hit, 8, *h[0])
        struct.pack_into("<I", hit, 0x58, 1)
        struct.pack_into("<d", hit, 0x60, h[1])
    e.write(a[1], bytes(hit))
    return 0


def set_bone(e, a):
    i32(e, a[0] + 4, COLL.get("bone", -1))
    return 0


def weapon_pos(e, a):
    for i, x in enumerate(COLL.get("wpos", (0.0, 0.0, 0.0))):
        f32(e, a[0] + 4 * i, x)
    return 0


fk.add(TASK_NEW, 3, 0x7A5C, masks=(~0, 0xFFFF))
fk.add(0x401400, 3, masks=(0xFFFF, 0xFF))
fk.add(0x401530, 3, masks=(0xFFFF,))
fk.add(0x401900, 5, 0x1234, masks=(~0, ~0, 0xFFFF), deref={0: "s"})
fk.add(0x401A20, 1, masks=(0xFFFF,))
for addr, ev in ((0x416830, 0x17), (0x416850, 0x18), (0x416880, 0x1D), (0x4CEA10, 0x20), (0x4CEA20, 0x21),
                 (0x489610, 0x22), (0x4E6610, 0x23), (0x477C20, HUMAN_EV), (0x4C48C0, RIGID), (0x4D9610, PARENT)):
    fk.add(addr, 0, ev)
fk.add(0x416920, 0, WORLD)
fk.add(0x416D20, 0, RNG)
fk.add(GAME, 0, 0)
fk.add(COMPUTER, 0, 0)
fk.add(0x4B0D40)
fk.add(0x4B0D50)
fk.add(MEM_MODE, 0, 0)
fk.add(RFLOAT, 1, 0.25, st0="d")
fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4C7560, 2)
fk.add(0x4CC2A0, 8, fn=lambda e, a: coll(e, a, 0x4CC2A0, 1), logged=False)
fk.add(0x4CDB20, 9, fn=lambda e, a: coll(e, a, 0x4CDB20, 2), logged=False)
fk.add(0x4CEC10, 1, 0x7E7E, deref={0: "s"})
fk.add(0x4D2FB0, 3, fn=weapon_pos)
fk.add(BONE, 2, fn=set_bone)
fk.add(0x4D38C0, 2)
fk.add(0x4D6110, 4)
fk.add(0x4D9620, 4)
fk.add(0x4D96F0, 1)
fk.add(CAMLIST, 0, 0)
fk.add(0x4EC0E0, 3, deref={2: 24})
fk.add(0x4F2060, 2, fn=lambda e, a: e.u32(a[0] + 4 * a[1]))
fk.add(0x4F2310, 3, fn=lambda e, a: log(e, [0x4F2310, a[0]]), logged=False)
HANDLER = fk.add(None, 2, deref={1: 4})
HANDLER0 = fk.add(None, 2)


def check(orig, ours, setup=None, **kw):
    def s(e):
        rng.seed(77)
        COLL.clear()
        return setup(e) if setup else None
    return test_viewcam_util.check(emu, fk, orig, ours, s, **kw)


def rnd(n, lo=-1.0, hi=1.0):
    return [rng.uniform(lo, hi) for _ in range(n)]


def human(e):
    h = new_task(e, 0xE00, HUMAN_TYPE)
    f64(e, h + 0x20, *rnd(3, -50000, 50000))
    return h


def view(e, h=0):
    v = new_task(e, 0x328, 9, h)
    f64(e, v + 0x20, *rnd(3, -50000, 50000))
    f64(e, v + 0xE0, *rnd(3, -9000, 9000))
    for i, x in enumerate(rnd(9)):
        f32(e, v + 0x78 + 4 * i, x)
        f32(e, v + 0xF8 + 4 * i, -x * 0.5)
    i32(e, v + 0x9C, 3)
    i32(e, v + 0x11C, 5)
    f32(e, v + 0xA0, 200.0)
    for i, x in enumerate(rnd(3, -500, 500)):
        f32(e, v + 0x1F0 + 4 * i, x)
    return v


# --- small functions -------------------------------------------------------------

def test_get_unk():
    def setup(e):
        i32(e, 0x5BDC3C, 0x1357)
    check(0x464250, "Human_GetMaskType", setup)


def test_init():
    check(0x464260, "HumanView_Init", check_eax=False)


def test_free():
    check(0x464600, "HumanView_Free", lambda e: u8(e, 0x5BDC40, 0x42), check_eax=False)


@pytest.mark.parametrize("mode", [0, 1])
def test_new(mode):
    def setup(e):
        fk.rets[MEM_MODE] = mode
        return (0x4545,)
    check(0x464610, "HumanView_New", setup)


def test_create():
    def setup(e):
        v = view(e)
        e.write(v + 0x20, bytes([0xAB]) * 0x300)
        return (v,)
    check(0x4643A0, "HumanView_CreateHandler", setup, check_eax=False)


@pytest.mark.parametrize("in_world", [0, 1])
def test_delete(in_world):
    def setup(e):
        v = view(e)
        u8(e, v + 0xD9, in_world)
        return (v,)
    check(0x464470, "HumanView_DeleteHandler", setup, check_eax=False)


@pytest.mark.parametrize("in_world", [0, 1])
def test_leave_world(in_world):
    def setup(e):
        v = view(e)
        u8(e, v + 0xD9, in_world)
        w = e.alloc(4)
        i32(e, w, 0x3434)
        return v, w
    check(0x464580, "HumanView_LeaveWorldHandler", setup, check_eax=False)


@pytest.mark.parametrize("flag", [0, 1])
def test_join_world(flag):
    def setup(e):
        v = view(e)
        u8(e, v + 0xD8, flag)
        return (v,)
    check(0x4645B0, "HumanView_JoinWorldHandler", setup, check_eax=False)


def test_add_to_world():
    check(0x464660, "HumanView_AddToWorld", lambda e: (view(e),), check_eax=False)


def test_sound_info():
    check(0x4645D0, "HumanView_SoundInfoHandler", lambda e: (view(e), e.alloc(12)), check_eax=False)


@pytest.mark.parametrize("addr,name", [
    (0x464700, "HumanView_LockPitch"), (0x464710, "HumanView_UnlockPitch"), (0x464720, "HumanView_ClearAngles"),
    (0x464750, "HumanView_ClearWeaponAngles")])
def test_setters(addr, name):
    def setup(e):
        v = view(e)
        e.write(v + 0x110, bytes(range(1, 0xF1)))
        return (v,)
    check(addr, name, setup, check_eax=False)


def fbits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def test_set_weapon_target():
    check(0x464770, "HumanView_SetWeaponTarget",
          lambda e: (view(e), fbits(0.5), fbits(-0.25), fbits(0.01), fbits(0.02)), check_eax=False)


def test_set_weapon_angles():
    check(0x4647A0, "HumanView_SetWeaponAngles", lambda e: (view(e), fbits(0.5), fbits(-0.25)), check_eax=False)


@pytest.mark.parametrize("vals", [(1, 1, 2, 2), (1, 2, 2, 2), (1, 1, 2, 3), (float("nan"), 1, 2, 2), (0.0, -0.0, 5, 5)])
def test_weapon_at_target(vals):
    def setup(e):
        v = view(e)
        f32(e, v + 0x1CC, vals[0])
        f32(e, v + 0x1D0, vals[1])
        f32(e, v + 0x1D8, vals[2])
        f32(e, v + 0x1DC, vals[3])
        return (v,)
    check(0x4647D0, "HumanView_WeaponAtTarget", setup, eax_mask=0xFF)


@pytest.mark.parametrize("dirty", [0, 1])
def test_apply_rot(dirty):
    def setup(e):
        v = view(e)
        u8(e, v + 0xDA, dirty)
        return (v,)
    check(0x465700, "HumanView_ApplyRot", setup, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("in_world", [0, 1])
def test_update_position(in_world, cw):
    def setup(e):
        v = view(e)
        u8(e, v + 0xD9, in_world)
        return (v,)
    check(0x465730, "HumanView_UpdatePosition", setup, cw=cw, check_eax=False)


def test_get_eye():
    check(0x465800, "HumanView_GetEye", lambda e: (e.alloc(24), view(e)), check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("crouch", [0, 1])
def test_get_eye_pos(crouch, cw):
    def setup(e):
        h = human(e)
        u8(e, h + 0x33D, crouch)
        return e.alloc(24), h
    check(0x465820, "Human_GetEyePos", setup, cw=cw, check_eax=False)


def test_get_matrix():
    check(0x465880, "HumanView_GetMatrix", lambda e: (e.alloc(40), view(e)), check_eax=False)


def test_get_forward():
    check(0x4658A0, "HumanView_GetForward", lambda e: (e.alloc(12), view(e)), check_eax=False)


def test_set_camera():
    def setup(e):
        p = e.alloc(64)
        e.write(p, bytes(range(64)))
        return view(e), p, p + 24
    check(0x4658D0, "HumanView_SetCamera", setup, check_eax=False)


# --- enter handler ---------------------------------------------------------------

@pytest.mark.parametrize("case", ["ok", "otherlist", "otherhuman", "mode", "computer", "noweapon"])
def test_enter(case):
    def setup(e):
        h = human(e)
        v = view(e, h)
        cam = e.alloc(0x100)
        game = e.alloc(0x100)
        i32(e, game + 0x5C, cam)
        fk.rets[GAME] = game
        lst = e.alloc(0x100)
        i32(e, lst + 0x54, cam + (4 if case == "otherlist" else 0))
        fk.rets[CAMLIST] = lst
        i32(e, cam + 0x68, ref(e, h + (8 if case == "otherhuman" else 0)))
        i32(e, cam + 0xE8, 2 if case == "mode" else 1)
        fk.rets[COMPUTER] = 0x1111 if case == "computer" else 0
        if case != "noweapon":
            i32(e, h + 0x4C4, new_task(e, 0x40, 0x50))
        u8(e, v + 0xDA, 1)
        set_handler(e, 4, PARENT, HANDLER0)
        set_handler(e, HUMAN_EV, HUMAN_TYPE, HANDLER)
        set_handler(e, 4, RIGID, HANDLER0)
        return v, 0x6060
    check(0x4644A0, "HumanView_EnterHandler", setup, check_eax=False)


# --- per frame -------------------------------------------------------------------

def init_view(e, h):
    v = view(e, h)
    e.call(0x4643A0, v)
    i32(e, h + 0x4EC, v)
    return v


VIEW_CASES = ["plain", "zoomedfov", "locked", "slow", "fast", "neg", "clamp_hi", "clamp_lo", "zoom_in", "zoom_out",
              "zoom_reset", "target", "target_snap", "target_up", "target_down", "target_far", "nan", "target_low",
              "target_high", "target_near", "target_near2", "recoil_lo", "yaw_hi", "yaw_lo"]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", VIEW_CASES)
def test_update_view(case, cw):
    def setup(e):
        h = human(e)
        v = init_view(e, h)
        s = {"slow": 0.03, "fast": 0.3, "neg": -0.045, "nan": float("nan")}.get(case, 0.0123)
        f32(e, h + 0x1B4, s)
        f32(e, h + 0x114, s * 0.5)
        if case == "zoomedfov":
            f32(e, v + 0x1E4, 0.5)
        u8(e, v + 0x138, 1 if case == "locked" else 0)
        f32(e, v + 0x124, {"clamp_hi": 1.57, "clamp_lo": -1.57}.get(case, 0.3))
        if case == "clamp_hi":
            f32(e, h + 0x1B4, 0.05)
        if case == "clamp_lo":
            f32(e, h + 0x1B4, -0.05)
        f32(e, v + 0x128, -0.3 if case == "recoil_lo" else 0.0625)
        if case == "recoil_lo":
            f32(e, v + 0x124, -1.57)
            f32(e, h + 0x1B4, -0.05)
        f32(e, v + 0x12C, {"target_up": -1.0, "target_down": 1.2, "target_near": -0.40, "target_near2": -0.36,
                           "yaw_hi": 2.0, "yaw_lo": -2.5}.get(case, 0.1))
        f32(e, v + 0x120, {"target_up": -0.5, "target_down": 0.8, "target_near": -0.21,
                           "target_near2": -0.25}.get(case, 0.2))
        for i, k in enumerate(("zoom_in", "zoom_out", "zoom_reset")):
            f32(e, h + 0x1F0 + 4 * i, 1.0 if case == k else 0.0)
        if case.startswith("target"):
            t = {"target_far": (-5000.0, 100.0, 9000.0), "target_up": (300.0, 2000.0, 1500.0),
                 "target_low": (100.0, -3000.0, -2000.0), "target_high": (-100.0, -3000.0, 2000.0)}.get(
                case, (1200.0, 3000.0, -700.0))
            f64(e, v + 0x180, *t)
            u8(e, v + 0x198, 1 if case == "target_snap" else 0)
        return v, h
    check(0x464800, "HumanView_UpdateView", setup, cw=cw, check_eax=False)


BODY_CASES = ["plain", "shake", "shake_neg", "shake_end", "shake_end_neg", "offset", "offset_hit", "offset_close",
              "dip", "dip_end", "inworld", "inworld_hit", "duck_up", "duck_down", "duck_back", "weapon_up",
              "weapon_down", "weapon_cross", "zoomed", "bone", "all", "crouch", "duck_out_snap", "duck_out_down",
              "duck_out_down2", "duck_in_snap", "weapon_snap"]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", BODY_CASES)
def test_update_body(case, cw):
    def setup(e):
        h = human(e)
        v = init_view(e, h)
        f32(e, h + 0x4F8, 0.125)
        f32(e, h + 0x508, 1.75)
        f32(e, v + 0x120, -0.3)
        f32(e, v + 0x12C, 0.4)
        u8(e, h + 0x33D, 1 if case == "crouch" else 0)
        if "shake" in case or case == "all":
            f32(e, v + 0x1B4, {"shake_neg": -0.8, "shake_end": 0.01, "shake_end_neg": -0.01}.get(case, 0.8))
            f32(e, v + 0x1B8, 120.0)
            f32(e, v + 0x1BC, 0.07)
            f32(e, v + 0x1C0, 0.02)
            fk.set_seq(RFLOAT, [0.1, 0.9, 0.35, 0.6, 0.73])
        if "offset" in case or case == "all":
            for i, x in enumerate((30.0, -12.5, 7.25)):
                f32(e, v + 0x19C + 4 * i, x)
            if case == "offset_hit":
                eye = struct.unpack("<3d", e.read(h + 0x20, 24))
                COLL[0x4CDB20] = ((eye[0] + 300.0, eye[1] + 10.0, eye[2] + 2826.0), 0.0)
            if case == "offset_close":
                eye = struct.unpack("<3d", e.read(h + 0x20, 24))
                COLL[0x4CDB20] = ((eye[0] + 1.0, eye[1] + 1.0, eye[2] + 2826.24), 0.0)
        if "dip" in case or case == "all":
            f32(e, v + 0x1C4, 64.0)
            f32(e, v + 0x1C8, 3.0 if case == "dip_end" else 1.1)
        if case.startswith("inworld") or case.startswith("duck") or case in ("weapon_up", "zoomed", "bone", "all",
                                                                               "crouch"):
            u8(e, v + 0xD9, 1)
        if case in ("inworld_hit", "duck_up", "all", "duck_in_snap"):
            COLL[0x4CC2A0] = ((0.0, 0.0, 0.0), 1500.0)
        f32(e, v + 0x324, {"duck_out_snap": -10.0, "duck_out_down": 10.0, "duck_out_down2": 200.0,
                           "duck_in_snap": -600.0}.get(case, 0.0))
        if case == "duck_up":
            f32(e, v + 0x324, -700.0)
        if case == "duck_down":
            f32(e, v + 0x324, -100.0)
        if case == "duck_back":
            f32(e, v + 0x324, -2000.0)
            COLL[0x4CC2A0] = ((0.0, 0.0, 0.0), 1990.0)
        if case.startswith("weapon") or case == "all":
            f32(e, v + 0x1CC, 0.3)
            f32(e, v + 0x1D0, {"weapon_down": -0.4, "weapon_cross": 0.31, "weapon_snap": 0.28}.get(case, 0.9))
            f32(e, v + 0x1D4, 0.05)
            f32(e, v + 0x1D8, -0.2)
            f32(e, v + 0x1DC, {"weapon_down": -0.21, "weapon_snap": -0.19}.get(case, 0.4))
            f32(e, v + 0x1E0, 0.03)
        if case in ("zoomed", "all"):
            i32(e, v + 0x1EC, 1)
        if case in ("bone", "all"):
            COLL["bone"] = 3
            for i, x in enumerate((10.0, -20.0, 5.5)):
                f32(e, v + 0x218 + 4 * i, x)
        COLL["wpos"] = (12.0, 250.0, -40.0)
        return v, h
    check(0x464BD0, "HumanView_UpdateBody", setup, cw=cw, check_eax=False)
