"""GunShot: bullets in flight and tracer trails (0x47ABD0 - 0x47CA40)."""
import struct

import pytest

from emu import Emu
from test_comprun_util import run as run_cw
from test_menu_util import RET, cstr, f32, handler, logfake, set_handler, task, u16, u32
from test_vehicle_util import d64

SHOT_EV, FIRE_EV, GUNSHOT_EV = 0x45, 0x46, 0x47
SHOT_T, POLY_T, WEAPON_T, HUMAN_T = 0x66, 0x67, 0x68, 0x69

emu = Emu()


def project(e, a):
    depth = RET["depths"].pop(0)
    f32(e, a[0], 100.0, 200.0)
    f32(e, a[1], depth)


def sound(e, a):
    return RET.get("snd", 0x7A00)


logfake(emu, 0x497F20, 3, derefs={2: 24}, masks={0: 0, 1: 0}, effect=project)
logfake(emu, 0x4E0E20, 6, derefs={1: 24, 2: 12, 4: 12})
logfake(emu, 0x49A430, 2, derefs={1: 24})
logfake(emu, 0x49A450, 11, derefs={2: 24, 3: 36}, ret=0x7800)
logfake(emu, 0x47B750, 1)  # GunShot_MoveBullets
logfake(emu, 0x460C60, 0, effect=lambda e, a: RET["humans"])  # Human_GetList
logfake(emu, 0x460C30, 0, ret=GUNSHOT_EV)
HUMAN = handler(emu, "human", 2, derefs={1: 12})
RENDER = handler(emu, "render", 2)
SHOTSRC = handler(emu, "shotsrc", 2, masks={1: 0}, effect=lambda e, a: u32(e, a[1], RET["shot"]))
logfake(emu, 0x4012A0, 3, masks={1: 0xFFFF}, effect=lambda e, a: RET["new"])  # Task_New
logfake(emu, 0x401AE0, 1, effect=lambda e, a: a[0] + 0x10)  # TaskRef_Add
logfake(emu, 0x401B20, 1)  # TaskRef_Remove
logfake(emu, 0x4015F0, 1)  # Task_Delete
logfake(emu, 0x4B0D60, 1)  # Mem_PushMode
logfake(emu, 0x4B0D90, 0)  # Mem_PopMode
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
logfake(emu, 0x4C6890, 3)  # DynCube_ListAdd
logfake(emu, 0x4C6840, 3)  # DynCube_ListRemove
logfake(emu, 0x4028B0, 0, effect=lambda e, a: RET["ticks"].pop(0))  # Timer_GetTicks
logfake(emu, 0x416D20, 0, ret=0x1234)  # Game_GetRandom
logfake(emu, 0x4B47C0, 2, effect=lambda e, a: RET["rand"].pop(0))  # Random_Int
logfake(emu, 0x4F0EE0, 0, ret=0x4321)
logfake(emu, 0x4E6B00, 2, strings=(1,), effect=sound)  # Sound_Play
logfake(emu, 0x4E6C30, 2)  # Sound_SetPos
logfake(emu, 0x413B20, 0, ret=FIRE_EV)  # Weapon_GetFireEvent
logfake(emu, 0x4018F0, 0, ret=0x11)  # Task_GetRootType
logfake(emu, 0x401900, 4, strings=(0,), masks={2: 0xFFFF}, ret=0x77)  # TaskType_New
logfake(emu, 0x4017C0, 1, ret=0x2B)  # TaskEvent_Alloc
logfake(emu, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})  # TaskType_SetHandler
logfake(emu, 0x401530, 3, masks={0: 0xFFFF})  # TaskType_SetHandler2
logfake(emu, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant


def run(orig, ours, setup=None, cw=None, **kw):
    return run_cw(emu, orig, ours, setup, cw=cw, **kw)


def shot(e, poly_count=0):
    s = task(e, 0xC38, SHOT_T)
    p = task(e, 0xA0, POLY_T)
    u32(e, p + 0x9C, poly_count)
    u32(e, s + 0x914, p)
    d64(e, s + 0x518, 1000.5, -2000.25, 300.75)
    return s


def trail(e, s, i, a, b, frm=1.0, to=3.0, length=4.0, life=3, has_poly=0, poly=0):
    t = s + 0x518 + 0xC8 * i
    if i:
        d64(e, t, *a)
    d64(e, t + 0x18, *b)
    u32(e, t + 0x30, 1, life)
    f32(e, t + 0x38, frm, to, length)
    u32(e, t + 0xBC, poly, has_poly)
    n = e.u32(s + 0x9C)
    u32(e, s + 0x900 + 4 * n, t)
    u32(e, s + 0x9C, n + 1)
    return t


def gun_with_ammo(e, tracer=1):
    g = task(e, 0x1B0, WEAPON_T)
    at = e.alloc(0x168)
    u32(e, at + 8, 250, 200, 15, 255, 255, 15, tracer)
    u32(e, g + 0x12C, at)
    ref = e.alloc(4)
    u32(e, ref, g)
    return ref


# --- render ---------------------------------------------------------------

@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("depths,has_poly,frm", [
    ([500.0, 9000.0], 0, 1.0), ([-5.0, 9000.0], 0, 1.0), ([500.0, -9000.0], 0, 0.3), ([500.0, -9000.0], 1, 1.0),
    ([-1.0, -2.0], 0, 1.0), ([3e6, 1e6], 0, 2.0), ([1500000.0, 100.0], 0, -1.0), ([0.0, 0.0], 0, 5.0)])
def test_render(depths, has_poly, frm, cw):
    def setup(e):
        s = shot(e, 1)
        u32(e, s + 0x6C, gun_with_ammo(e))
        trail(e, s, 0, None, (1500.0, -1900.0, 320.0), frm=frm, has_poly=has_poly)
        trail(e, s, 1, (900.0, 100.0, 50.0), (700.0, 300.0, 60.0), frm=0.5, to=1.5, length=2.0)
        RET["depths"] = depths + [800.0, 1200.0]
        u32(e, 0xA982E0 + 4 * POLY_T, RENDER)
        return (s,)
    run(0x47ABD0, "GunShot_Render", setup, cw)


@pytest.mark.parametrize("case", ["nogun", "noammo", "notracer", "nopolys"])
def test_render_skips(case):
    def setup(e):
        s = shot(e, 0 if case == "nopolys" else 2)
        ref = gun_with_ammo(e, tracer=0 if case == "notracer" else 1)
        if case == "nogun":
            u32(e, ref, 0)
        if case == "noammo":
            u32(e, e.u32(ref) + 0x12C, 0)
        u32(e, s + 0x6C, ref)
        trail(e, s, 0, None, (1500.0, -1900.0, 320.0))
        RET["depths"] = [1.0, 2.0]
        u32(e, 0xA982E0 + 4 * POLY_T, RENDER)
        return (s,)
    run(0x47ABD0, "GunShot_Render", setup)


# --- small ones -----------------------------------------------------------

def test_get_event():
    run(0x47B100, "GunShot_GetEvent", lambda e: e.write(0x5BE3CC, b"\x5A"), check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("a,b", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_new(a, b):
    def setup(e):
        u16(e, 0x5BE3CE, 0x66)
        RET["new"] = e.alloc(0xC38)
        return 0x5100, 0x5200 if a else 0, 0x5300 if b else 0
    run(0x47B110, "GunShot_New", setup, check_eax=True)


@pytest.mark.parametrize("active", [0, 2])
def test_release(active):
    def setup(e):
        s = shot(e)
        u32(e, s + 0x94, active)
        return s, 7
    run(0x47B170, "GunShot_Release", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("active,used", [(0, ()), (3, (0, 1, 2)), (15, ()), (16, ()), (14, tuple(range(15)))])
def test_add_bullet(active, used, cw):
    def setup(e):
        s = shot(e)
        u32(e, s + 0x94, active)
        u32(e, s + 0x90, 40)
        for i in used:
            u32(e, s + 0xE0 + 0x48 * i, 1)
        w = e.alloc(0x6E8)
        f32(e, w + 0x650, 123.456, 7.5, 8.25)
        pos, d = e.alloc(24), e.alloc(12)
        d64(e, pos, 1.5, 2.5, 3.5)
        f32(e, d, 0.25, 0.5, -0.75)
        return s, pos, d, w, 0x55, 0x66
    run(0x47B190, "GunShot_AddBullet", setup, cw)


def test_set_unk78():
    def setup(e):
        p = e.alloc(24)
        d64(e, p, 1.0, 2.0, 3.0)
        return shot(e), p
    run(0x47B270, "GunShot_SetUnk78", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("pos,mx", [((1000.0, 0.0, 0.0), 32), ((50000.0, 3000.0, 100.0), 32),
                                    ((1000.0, 9000.0, 0.0), 32), ((1000.0, 0.0, 0.0), 1), ((1000.0, 0.0, 0.0), 0),
                                    ((-80000.0, 0.0, 0.0), 32), ((300000.0, 1000.0, -2000.0), 32)])
def test_find_near(pos, mx, cw):
    def setup(e):
        s = shot(e)
        bullets = [((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), 100.3, 1), ((0.0, 5000.0, 0.0), (0.6, 0.8, 0.0), 77.7, 1),
                   ((10.0, 20.0, 30.0), (0.0, 0.0, 1.0), 5.5, 0), ((20000.0, 2000.0, 100.0), (0.8, 0.0, 0.6), 333.3, 1)]
        for i, (bp, d, sp, used) in enumerate(bullets):
            b = s + 0xE0 + 0x48 * (3 - i)
            u32(e, b, used)
            d64(e, b + 8, *bp)
            f32(e, b + 0x20, *d)
            f32(e, b + 0x34, sp * 4096 / 10)
            u32(e, s + 0xA4 + 4 * i, b)
        u32(e, s + 0x94, len(bullets))
        p = e.alloc(24)
        d64(e, p, *pos)
        return e.alloc(24 * 32), mx, s, p
    run(0x47B290, "GunShot_FindNear", setup, cw, check_eax=True)


def test_init():
    run(0x47B4B0, "GunShot_Init")


def test_create():
    def setup(e):
        s = task(e, 0xC38, SHOT_T)
        e.write(s + 0x20, b"\x5A" * (0xC38 - 0x20))
        return (s,)
    run(0x47B560, "GunShot_Create", setup)


@pytest.mark.parametrize("active,released,trails,humans", [(0, 0, 0, 0), (2, 1, 0, 3), (0, 1, 0, 0),
                                                           (0, 1, 1, 0), (0, 0, 0, 2), (1, 0, 0, 0)])
def test_run(active, released, trails, humans):
    def setup(e):
        s = shot(e)
        u32(e, s + 0x94, active)
        u32(e, s + 0x68, released, 0x5100, 0x5200)
        if trails:
            trail(e, s, 0, None, (1.0, 2.0, 3.0), life=5)
        arr = e.alloc(8 + 4 * humans)
        u32(e, arr, humans)
        for i in range(humans):
            u32(e, arr + 8 + 4 * i, task(e, 0x20, HUMAN_T))
        RET["humans"] = arr
        set_handler(e, GUNSHOT_EV, HUMAN_T, HUMAN)
        return (s,)
    run(0x47B690, "GunShot_Run", setup)


@pytest.mark.parametrize("last,ticks,delay,snd", [(-1, [500, 500], [2, 3], 1), (-1, [500, 500], [1, 0], 0),
                                                  (100, [105, 105], [1, 2], 1), (100, [120, 120, 120], [3, 4], 1),
                                                  (100, [90, 95], [0, 0], 1), (100, [80, 80, 130], [3, 1], 1),
                                                  (0x7FFFFFF0, [-0x7FFFFFF0, -0x7FFFFFF0, 5], [2, 2], 1)])
def test_ricochet(last, ticks, delay, snd):
    def setup(e):
        u32(e, 0x540834, last)
        u32(e, 0x5BE3D4, 10)
        RET["ticks"] = list(ticks)
        RET["rand"] = list(delay)
        RET["snd"] = 0x7A00 if snd else 0
        p = e.alloc(24)
        d64(e, p, 1.0, 2.0, 3.0)
        return (p,)
    run(0x47C330, "GunShot_PlayRicochet", setup)


@pytest.mark.parametrize("a,b", [(0, 0), (1, 1), (1, 0), (0, 1)])
def test_delete(a, b):
    def setup(e):
        s = shot(e)
        u32(e, s + 0x6C, 0x5100 if a else 0, 0x5200 if b else 0)
        return (s,)
    run(0x47C3D0, "GunShot_Delete", setup)


def test_fire_handler():
    def setup(e):
        s = shot(e)
        u32(e, s + 0x94, 1)
        RET["shot"] = s
        w = task(e, 0x20, WEAPON_T)
        e.write(0x5BE3CC, bytes([SHOT_EV]))
        set_handler(e, SHOT_EV, WEAPON_T, SHOTSRC)
        wt = e.alloc(0x6E8)
        f32(e, wt + 0x650, 10.0, 1.0, 2.0)
        pos, m = e.alloc(24), e.alloc(36)
        d64(e, pos, 4.0, 5.0, 6.0)
        f32(e, m, *[0.1 * i for i in range(9)])
        msg = e.alloc(0x30)
        u32(e, msg, w, 0, pos, 0, 0, m, 0, 0, wt, 0x77, 0, 0x88)
        return 0x5555, msg
    run(0x47C420, "GunShot_FireHandler", setup)


def test_close():
    def setup(e):
        e.write(0x5BE3CC, b"\x5A")
        u16(e, 0x5BE3CE, 0x66)
    run(0x47C4A0, "GunShot_Close", setup)


@pytest.mark.parametrize("used", [(), (0, 1), (0, 1, 2, 3, 4), (1, 3)])
def test_add_trail(used):
    def setup(e):
        s = shot(e)
        for i in used:
            u32(e, s + 0x518 + 0xC8 * i + 0x30, 1)
        u32(e, s + 0x9C, len(used))
        a, b = e.alloc(24), e.alloc(24)
        d64(e, a, 1.0, 2.0, 3.0)
        d64(e, b, 4.0, 5.0, 6.0)
        e.write(s + 0x518 + 0xC0, b"\x01")
        return s, a, b, 0x3F800000, 0x40000000, 0x40400000, 9
    run(0x47C4C0, "GunShot_AddTrail", setup)


@pytest.mark.parametrize("lives,polys", [((), ()), ((3, 1, 2), ()), ((0,), ()), ((0, 2, 0), (0, 2)),
                                         ((1, 0, 0, 0), (1,)), ((0, 0, 0), (0, 1, 2)), ((5, 0), (1,))])
def test_update_trails(lives, polys):
    def setup(e):
        s = shot(e, len(polys))
        for i, life in enumerate(lives):
            hp = i in polys
            trail(e, s, i, (1.0, 2.0, 3.0), (4.0, 5.0, 6.0), life=life, has_poly=hp,
                  poly=polys.index(i) if hp else 0)
        return (s,)
    run(0x47C560, "GunShot_UpdateTrails", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("b,front,count", [((1500.0, -1900.0, 320.0), 1, 0), ((1500.0, -1900.0, 320.0), 0, 3),
                                           ((1000.5, -2000.25, 400.0), 1, 0), ((1000.5, -1500.0, 300.75), 0, 1),
                                           ((900.0, -2100.0, 300.0), 1, 10), ((-7000.1, 3333.3, 1.0), 0, 9)])
def test_add_trail_poly(b, front, count, cw):
    def setup(e):
        s = shot(e, count)
        trail(e, s, 0, None, (5.0, 5.0, 5.0))
        trail(e, s, 1, (1000.5, -2000.25, 300.75), b)
        return s, 1, 0x3F000000, 0x3E800000, 0x3F100000, 0x3F200000, 0x3F300000, 0x3F400000, 0x3F500000, \
            0x3F600000, front
    run(0x47C610, "GunShot_AddTrailPoly", setup, cw)


@pytest.mark.parametrize("i,count,match", [(0, 3, 1), (1, 3, 2), (0, 1, None), (2, 3, None)])
def test_remove_trail_poly(i, count, match):
    def setup(e):
        s = shot(e, count)
        e.write(s + 0x918, bytes(range(256)) * 3 + bytes(range(32)))
        for j in range(3):
            last = j == match
            trail(e, s, j, (1.0, 2.0, 3.0), (4.0, 5.0, 6.0), has_poly=1 if (j == i or last) else 0,
                  poly=count - 1 if last else j)
        return s, i
    run(0x47C990, "GunShot_RemoveTrailPoly", setup)
