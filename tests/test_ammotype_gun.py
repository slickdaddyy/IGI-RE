"""Gun task handlers and the weapon HUD (0x479780 - 0x47ABD0)."""
import struct

import pytest

from emu import Emu
from test_comprun_util import run as run_cw
from test_menu_util import LOG, RET, add_child, cstr, f32, handler, logfake, set_handler, strip_logger, task, u16, u32
from test_vehicle_util import d64, floatfake

EVENT = 0x5C10E8  # AmmoList_GetEvent
GUN_T, OWNER_T, KID_T = 0x60, 0x61, 0x62
FIRE_EV, AMMO_EV, EV1, WEV0, WEV1, RDC_T = 0x40, 0x41, 0x42, 0x43, 0x44, 0x63

emu = Emu()
strip_logger(emu)


def floats(e, a):
    return tuple(struct.unpack("<f", struct.pack("<I", v & 0xFFFFFFFF))[0] for v in a)


def fired(e, a):
    e.write(a[1] + 1, bytes([RET.get("fired", 1)]))


def give_list(e, a):
    u32(e, a[1], RET.get("list", 0))


def p414ed0(e, a):
    f32(e, a[0], *([1.5, -2.25, 3.125] if a[2] == 0 else [-7.75, 8.5, 0.0625]))


logfake(emu, 0x4F1FF0, 0, ret=FIRE_EV)
FIRE = handler(emu, "fire", 2, effect=lambda e, a: fired(e, a))
AMMO = handler(emu, "ammo", 2, effect=lambda e, a: give_list(e, a), masks={1: 0})
KID = handler(emu, "kid", 2)
WORLD = handler(emu, "world", 2)
logfake(emu, 0x416D20, 0, ret=0x1234)  # Game_GetRandom
floatfake(emu, 0x4B4770, 1, effect=lambda e, a: RET["rand"].pop(0))  # Random_Float
logfake(emu, 0x463130, 1, ret=3)  # Human_GetTeam
logfake(emu, 0x450CE0, 1, derefs={0: 0x1C})  # HumanAI_Broadcast
logfake(emu, 0x4F2060, 2, ret=0x7100)  # WeaponSlots_Get
logfake(emu, 0x413C80, 1, ret=0x7200)  # Weapon_GetType
logfake(emu, 0x463150, 1, effect=lambda e, a: RET.get("player", 0))  # Human_IsPlayer
logfake(emu, 0x4137E0, 2)  # HumanPlayer_SetWeaponAnim
logfake(emu, 0x489F50, 1, effect=lambda e, a: RET.get("animdone", 0))
logfake(emu, 0x4F2350, 2, masks={1: 0xFF})  # Weapon_UseAmmo
logfake(emu, 0x460C10, 0, ret=EV1)  # Human_GetEvent1
logfake(emu, 0x4785F0, 1)
logfake(emu, 0x4788D0, 1)
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
floatfake(emu, 0x4D0950, 2, effect=lambda e, a: RET.get("radius", 123.456))  # VirModel_GetRadius
logfake(emu, 0x4C7140, 5, derefs={2: 24})  # DynCube_Add
logfake(emu, 0x4168C0, 0, effect=lambda e, a: RET["tasklist"])  # Game_GetTaskList2
logfake(emu, 0x4F0EE0, 0, ret=0x4321)
logfake(emu, 0x46C980, 5, derefs={1: 24, 2: 36}, ret=0x7300)  # GunPickup_New
logfake(emu, 0x414ED0, 3, effect=lambda e, a: p414ed0(e, a), masks={0: 0})
logfake(emu, 0x491CF0, 0, effect=lambda e, a: RET["display"])  # Display_Get
logfake(emu, 0x4B64E0, 1, strings=(0,), effect=lambda e, a: RET.get("found", 0))  # Resource_Find
logfake(emu, 0x4B5F00, 2, strings=(0,), effect=lambda e, a: RET["res"])  # Resource_Load
logfake(emu, 0x4B6E90, 2, strings=(1,))  # Text_SetString
logfake(emu, 0x4B6F50, 1)  # Text_DrawAlt
logfake(emu, 0x4B7DF0, 1, effect=lambda e, a: RET.get("tw", 37))  # Text_GetWidth
logfake(emu, 0x4B7E00, 1, ret=11)  # Text_GetHeight
logfake(emu, 0x4B6CE0, 1, effect=lambda e, a: (a[0] & 0xFFF) // 4 + RET.get("sw", 5))  # Sprite_Width
logfake(emu, 0x4B6D00, 1, effect=lambda e, a: (a[0] & 0xFFF) // 8 + RET.get("sh", 3))  # Sprite_Height
logfake(emu, 0x4B4E00, 9)  # Draw_Sprite
logfake(emu, 0x4B4F40, 10)  # Draw_SpriteZ
logfake(emu, 0x4B53B0, 17)  # QVertex_DrawQuadEx
logfake(emu, 0x4015F0, 1)  # Task_Delete
logfake(emu, 0x401B20, 1)  # TaskRef_Remove
logfake(emu, 0x401CF0, 2, masks={0: 0xFFFF, 1: 0xFFFF}, effect=lambda e, a: RET.get("isa", 0))  # TaskType_IsA
logfake(emu, 0x4D9FA0, 0, ret=0x50)  # MagicObj_GetType
logfake(emu, 0x4CEA10, 0, ret=WEV0)
logfake(emu, 0x4CEA20, 0, ret=WEV1)
logfake(emu, 0x4C48C0, 0, ret=RDC_T)
logfake(emu, 0x4C1830, 1)  # QTaskList_Free
logfake(emu, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free


def run(orig, ours, setup=None, cw=None, **kw):
    return run_cw(emu, orig, ours, setup, cw=cw, **kw)


def world(e, wid=7, clip=30, ammo_id=50, carried=(50, 40), display=1, sight=1, rounds=12, ammo=True):
    """A gun owned by a human that carries one ammo entry."""
    e.write(EVENT, bytes([AMMO_EV]))
    owner = task(e, 0x600, OWNER_T)
    g = task(e, 0x1B0, GUN_T, owner)
    w = e.alloc(0x6E8)
    u32(e, w, wid)
    e.write(w + 0x110, b"m16a2\0")
    u32(e, w + 0x610, 0, sight, display)
    u32(e, w + 0x648, clip)
    f32(e, w + 0x66C, 1.5, -2.5, 3.25, 0.75)
    f32(e, w + 0x6D0, 2048.5)
    at = e.alloc(0x168)
    u32(e, at, ammo_id)
    lst = e.alloc(4 + 12 * 32)
    if carried is not None:
        u32(e, lst, 1, *carried, -1)
        RET["list"] = lst
    u32(e, g + 0x128, w, at if ammo else 0)
    u32(e, g + 0x144, rounds)
    set_handler(e, AMMO_EV, OWNER_T, AMMO)
    disp = e.alloc(12)
    u32(e, disp, 0, 640, 480)
    RET["display"] = disp
    return g, w, lst


def hud(e):
    h = e.alloc(0x500)
    t = e.alloc(0x40)
    u32(e, h + 0x4E0, t)
    return h


# --- fire, reload and ammo -------------------------------------------------

@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("fire,human", [(0, 0), (1, 0), (1, 1)])
def test_fire(fire, human, cw):
    def setup(e):
        g, w, _ = world(e)
        RET["fired"] = fire
        RET["rand"] = [0.99, 0.95, 0.1, 0.9, 0.3127, 0.8813]
        set_handler(e, FIRE_EV, GUN_T, FIRE)
        msg = e.alloc(0x30)
        if human:
            u32(e, msg + 0x2C, task(e, 0x40, 0x70))
        return g, msg
    run(0x479780, "Gun_ShootHandler", setup, cw)


@pytest.mark.parametrize("unlimited,carried,rounds", [(1, (50, 40), 3), (0, (50, 40), 3), (0, (50, 10), 3),
                                                       (0, (50, 27), 3), (0, (50, 0), 3), (0, (50, 40), 30),
                                                       (0, (51, 40), 3)])
def test_fill_clip(unlimited, carried, rounds):
    def setup(e):
        g, _, _ = world(e, carried=carried, rounds=rounds)
        p = e.alloc(4)
        e.write(p, bytes([unlimited]))
        return g, p
    run(0x4798C0, "Gun_FillClipHandler", setup)


@pytest.mark.parametrize("state,player,done,kids", [(0, 0, 0, 0), (0, 1, 0, 2), (1, 0, 0, 3), (1, 1, 1, 3),
                                                    (2, 0, 0, 1)])
def test_reload(state, player, done, kids):
    def setup(e):
        g, _, _ = world(e)
        u32(e, g + 0x1AC, state)
        RET["player"], RET["animdone"] = player, done
        child = task(e, 0x20, 0x64)
        u32(e, g + 0xE8, child)
        for i in range(kids):
            k = task(e, 0x20, KID_T if i != 1 else 0x65)
            add_child(e, child, k)
        set_handler(e, EV1, KID_T, KID)
        msg = e.alloc(0x24)
        u32(e, msg, task(e, 0x600, 0x70))
        e.write(msg + 0x21, b"\x01")
        return g, msg
    run(0x479990, "Gun_ReloadHandler", setup)


@pytest.mark.parametrize("ammo,carried,lst", [(0, (50, 40), 1), (1, (50, 40), 1), (1, (51, 40), 1), (1, (50, 4), 0)])
def test_get_ammo(ammo, carried, lst):
    def setup(e):
        g, _, _ = world(e, carried=carried, ammo=ammo)
        if not lst:
            RET["list"] = 0
        return g, e.alloc(8)
    run(0x479AA0, "Gun_GetAmmoHandler", setup)


def test_get_ammo_no_handler():
    def setup(e):
        g, _, _ = world(e)
        set_handler(e, AMMO_EV, OWNER_T, 0)
        return g, e.alloc(8)
    run(0x479AA0, "Gun_GetAmmoHandler", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("f1,scale", [(1, (1, 2, 3)), (0, (1, 2, 3)), (0, (3, 2, 1)), (0, (1, 3, 2)),
                                      (0, (2, 2, 2)), (0, (0.3, 1.7, 1.7))])
def test_479b20(f1, scale, cw):
    def setup(e):
        g, _, _ = world(e)
        e.write(g + 0xF1, bytes([f1]))
        u32(e, g + 0x6C, 0x7400)
        f32(e, g + 0x98, *scale)
        RET["radius"] = 1234.567
        return (g,)
    run(0x479B20, "Gun_ResetHandler", setup, cw)


def test_479b20_args():
    def setup(e):
        g, _, _ = world(e)
        u32(e, g + 0x6C, 0x7400)
        f32(e, g + 0x98, 1.1, 2.2, 0.7)
        return (g,)
    run(0x479B20, "Gun_ResetHandler", setup)
    run(0x479C10, "Gun_RemoveFromWorldHandler", setup)


@pytest.mark.parametrize("count,mx", [(3, 10), (10, 10), (11, 10)])
def test_drop(count, mx):
    def setup(e):
        g, _, _ = world(e)
        d64(e, g + 0x20, 1.5, 2.5, 3.5)
        f32(e, g + 0x70, *[0.25 * i for i in range(9)])
        u32(e, g + 0x100, 9)
        lst = e.alloc(8)
        u32(e, lst, count, mx)
        RET["tasklist"] = lst
        return g, e.alloc(4)
    run(0x479C20, "Gun_DropHandler", setup)


def test_get_type():
    run(0x479C80, "Gun_GetTypeHandler", lambda e: (world(e)[0], e.alloc(4)))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_479ca0(cw):
    run(0x479CA0, "Gun_UnusedPointsHandler", lambda e: (0x5555, e.alloc(24)), cw)


# --- HUD ------------------------------------------------------------------

@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("wid,display,sight,found,tw", [(7, 1, 1, 0, 37), (1, 2, 0, 1, 37), (12, 3, 1, 0, 2),
                                                        (42, 4, 0, 0, 80), (2, 5, 1, 0, 37), (13, 0, 1, 0, 37),
                                                        (17, 0, 0, 0, 37), (21, 2, 1, 1, 5), (99, 1, 2, 0, 37)])
def test_hud(wid, display, sight, found, tw, cw):
    def setup(e):
        g, _, _ = world(e, wid=wid, display=display, sight=sight)
        u32(e, g + 0x19C, 4)
        RET["found"], RET["tw"] = found, tw
        RET["res"] = cstr(e, b"M16A2 Rifle")
        msg = e.alloc(4)
        u32(e, msg, hud(e))
        return g, msg
    run(0x479D20, "Gun_HudHandler", setup, cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("addr,name", [(0x47A260, "Gun_DrawAmmoCount"), (0x47A360, "Gun_DrawAmmoClip"),
                                       (0x47A540, "Gun_DrawAmmoBar"), (0x47A750, "Gun_DrawAmmoItems")])
@pytest.mark.parametrize("wid,rounds,carried", [(7, 12, (50, 40)), (12, 1, (50, 3)), (12, 0, (50, 3)),
                                                (7, 0, (51, 9)), (7, 45, (50, 9)), (7, 9, (50, 9))])
def test_draw_ammo(addr, name, wid, rounds, carried, cw):
    def setup(e):
        g, _, _ = world(e, wid=wid, rounds=rounds, carried=carried)
        u32(e, g + 0x19C, 4)
        RET["sw"] = 6
        return g, hud(e)
    run(addr, name, setup, cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("wid", [7, 12])
def test_ammo_text_pos(wid, cw):
    def setup(e):
        g, _, _ = world(e, wid=wid)
        return e.alloc(4), e.alloc(4), g, hud(e)
    run(0x47AAB0, "Gun_GetAmmoTextPos", setup, cw)


# --- other handlers -------------------------------------------------------

@pytest.mark.parametrize("case", range(9))
def test_coll_filter(case):
    def setup(e):
        g, _, _ = world(e)
        owner = e.u32(g + 0x14)
        other, other2 = task(e, 0x20, 0x70), task(e, 0x20, 0x71)
        a, b, own, isa = [(other, other2, 1, 0), (g, other2, 1, 0), (owner, other2, 1, 0), (other, g, 1, 0),
                          (other, owner, 1, 0), (other, other2, 0, 0), (other, other2, 1, 1),
                          (other2, other, 1, 1), (other2, other, 1, 0)][case]
        if not own:
            u32(e, g + 0x14, 0)
        u32(e, owner + 0x130, other2)
        RET["isa"] = isa
        msg = e.alloc(16)
        e.write(msg, b"\x77")
        u32(e, msg + 8, a, b)
        return g, msg
    run(0x47A8E0, "Gun_CollFilter", setup)


def test_shot_source():
    def setup(e):
        g, _, _ = world(e)
        ref = e.alloc(4)
        u32(e, ref, 0x7500)
        u32(e, g + 0x130, ref)
        return g, e.alloc(8)
    run(0x47A940, "Gun_GetShotSourceHandler", setup)


@pytest.mark.parametrize("ref,inworld", [(0, 0), (1, 0), (2, 1), (2, 0), (0, 1)])
def test_remove(ref, inworld):
    def setup(e):
        g, _, _ = world(e)
        if ref:
            r = e.alloc(4)
            u32(e, r, 0x7600 if ref == 2 else 0)
            u32(e, g + 0x194, r)
        e.write(g + 0xF0, bytes([inworld]))
        set_handler(e, WEV1, RDC_T, WORLD)
        return g, 0xAB
    run(0x47A960, "Gun_RemoveHandler", setup)


@pytest.mark.parametrize("f1,f4,f0", [(0, 1, 0), (1, 1, 0), (0, 0, 0), (0, 1, 1)])
def test_add(f1, f4, f0):
    def setup(e):
        g, _, _ = world(e)
        e.write(g + 0xF0, bytes([f0, f1, 0, 0, f4]))
        set_handler(e, WEV0, RDC_T, WORLD)
        return g, 0xAB
    run(0x47A9E0, "Gun_AddHandler", setup)


def test_close():
    def setup(e):
        u32(e, 0x5BE3C0, 0x7700)
        e.write(0x5407B8, bytes([0x31, 0x32, 0x33, 0x34, 0x35]))
        u16(e, 0x5BE3C4, 0x56)
        u16(e, 0x5BE3C6, 0x57)
    run(0x47AA40, "Gun_Close", setup)
