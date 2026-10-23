"""Human damage, animation and look helpers, AI detection lookups (src/loadscreen/human.c)."""
import struct

import pytest

from emu import Emu
from test_loadscreen_util import CWS, Fakes, check, list_new, w32, wd, wf

e = Emu()
fk = Fakes(e)
LOS_RET = [1]
VIS = [0.5]
HEAD = [(10.0, 20.0, 30.0)]
fk.add(0x401B20, 1)
fk.add(0x489610, 0, ret=0x33)
fk.add(0x416920, 0, ret=0x7777)
fk.add(0x4CE410, 8, ret=lambda em, a: LOS_RET[0],
       fmt=lambda em, a: (a[0], em.read(a[1], 24), em.read(a[2], 24), a[3], em.read(a[4] + 4, 12), a[5], a[6]),
       effect=lambda em, a: wf(em, a[7], VIS[0]))
fk.add(0x414F40, 4, fmt=lambda em, a: (em.normalize(a[1]), a[2], em.read_cstr(a[3])),
       effect=lambda em, a: wd(em, a[0], *HEAD[0]))
LEN = [100.0]
fk.add(0x4B3130, 1, st0="d", ret=lambda em, a: LEN[0], deref={0: 24})
fk.add(0x4D61D0, 5)
fk.add(0x4D6130, 7)
fk.add(0x4D62B0, 3)
fk.add(0x4B4660, 3, fmt=lambda em, a: (em.read(a[1], 12), a[2]), effect=lambda em, a: em.write(a[0], em.read(a[1], 12) + struct.pack("<I", a[2])))
fk.add(0x4D7070, 5, deref={1: 16})
e.baseline = e.snapshot()


def human(em):
    h = em.alloc(0x700)
    model = em.alloc(0x400)
    view = em.alloc(0x200)
    vmodel = em.alloc(0x20)
    w32(em, h + 0x70, model)
    w32(em, h + 0x4EC, view)
    w32(em, view + 0x70, vmodel)
    wd(em, h + 0x20, 100.0, 200.0, 300.0)
    wf(em, h + 0x78, 0.6, 0.0, -0.8, 0.0, 1.0, 0.0, 0.8, 0.0, 0.6)
    return h, model, view, vmodel


@pytest.mark.parametrize("los,vis", [(1, 0.0), (0, 0.79), (0, 0.789), (0, 0.0)])
def test_is_visible(los, vis):
    def setup(em):
        LOS_RET[0], VIS[0] = los, vis
        a, b = em.alloc(24), em.alloc(24)
        wd(em, a, 1.0, 2.0, 3.0)
        wd(em, b, 4.0, 5.0, 6.0)
        return (0x1000, a, 0x2000, b)
    check(e, fk, 0x489B20, "AIDetection_IsVisible", setup=setup, eax_mask=0xFF)


def detection(em, enemies, friends):
    d = em.alloc(0x40)
    for off, items in ((0x18, enemies), (0x24, friends)):
        nodes = []
        for task, valid in items:
            n = em.alloc(0x30)
            ref = em.alloc(4)
            w32(em, ref, task)
            w32(em, n + 8, ref)
            em.write(n + 0x2A, bytes([valid]))
            nodes.append(n)
        list_new(em, nodes, d + off)
    return d


@pytest.mark.parametrize("en,fr", [([], []), ([(1, 1), (2, 1)], [(3, 0)]), ([(5, 1)], [])])
def test_remove_refs(en, fr):
    check(e, fk, 0x489B90, "AIDetection_RemoveRefs", setup=lambda em: (detection(em, en, fr),), check_eax=False)


@pytest.mark.parametrize("t", [1, 2, 3, 4, 9])
def test_find_seen(t):
    en, fr = [(1, 1), (2, 0), (3, 1)], [(2, 1), (4, 0)]
    check(e, fk, 0x489BE0, "AIDetection_FindSeen", setup=lambda em: (detection(em, en, fr), t))


def zones(em):
    z = em.alloc(17 * 16)
    for i in range(17):
        wf(em, z + 16 * i, 0.5 + i)
        w32(em, z + 16 * i + 4, 100 + i, 200 + i, 300 + i)
    return z


def test_init_damage():
    def setup(em):
        h = human(em)[0]
        wf(em, h + 0x260, *[9.0] * 34)
        wf(em, h + 0x254, 5.0)
        return (h, zones(em))
    check(e, fk, 0x489C30, "Human_InitDamage", setup=setup, check_eax=False)


def test_reset_damage():
    def setup(em):
        h = human(em)[0]
        w32(em, h + 0x25C, zones(em))
        wf(em, h + 0x254, 5.0)
        return (h,)
    check(e, fk, 0x489E30, "Human_ResetDamage", setup=setup, check_eax=False)


@pytest.mark.parametrize("zone", [0, 5, 16])
def test_get_zone(zone):
    def setup(em):
        h = human(em)[0]
        w32(em, h + 0x25C, zones(em))
        return (h + 0x254, zone)
    check(e, fk, 0x489E10, "HumanDamage_GetZone", setup=setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("kind", [0, 4, 9, 29, 30, 77])
@pytest.mark.parametrize("d", [(0.0, -1.0, 0.0), (0.0, 1.0, 0.0), (1.0, 0.2, 0.1), (-0.3, -0.2, 0.7), (0.1, 0.7, 0.2),
                               (0.0, 0.0, 0.0), (0.7, 0.3, 0.3), (-0.123, 0.456, 0.789)])
@pytest.mark.parametrize("length", [10.0, 614.4, 1000.0])
def test_hit_zone(cw, kind, d, length):
    def setup(em):
        LEN[0] = length
        HEAD[0] = (12.3456789, -7.25, 1.0 / 3)
        h = human(em)[0]
        wf(em, h + 0x78, 0.36, 0.48, -0.8, -0.6, 0.8, 0.0, 0.64, 0.48, 0.6)
        shot = em.alloc(0x50)
        wd(em, shot + 8, 111.111, 222.5, 333.3)
        w32(em, shot + 0x44, kind)
        hit = em.alloc(0x20)
        wf(em, hit + 8, *d)
        w32(em, hit, shot)
        return (h, hit)
    check(e, fk, 0x489C60, "Human_GetHitZone", setup=setup, cw=cw)


def test_play_anim_slot():
    def setup(em):
        h = human(em)[0]
        slots = em.alloc(64)
        w32(em, slots, 1, 2, 3, 4, 5, 6)
        w32(em, h + 0x338, slots)
        return (h, 2, 0x3F000000)
    check(e, fk, 0x489E70, "Human_PlayAnimSlot", setup=setup, check_eax=False)


@pytest.mark.parametrize("addr,name,args", [
    (0x489EA0, "Human_PlayAnim", (7, 1, 0x3F400000)),
    (0x489ED0, "Human_PlayWeaponAnim", (7, 0, 0x3F400000)),
    (0x489F00, "Human_StopWeaponAnim", (0x3E800000,)),
    (0x489F20, "HumanPlayer_PlayAnim", (9, 1, 0x40000000)),
])
def test_anim_wrappers(addr, name, args):
    check(e, fk, addr, name, setup=lambda em: (human(em)[0],) + args, check_eax=False)


@pytest.mark.parametrize("flags", [0, 0x100, 0xFFFFFEFF, 0x300])
def test_anim_done(flags):
    def setup(em):
        h, _, _, vm = human(em)
        w32(em, vm + 4, flags)
        return (h,)
    check(e, fk, 0x489F50, "HumanPlayer_AnimDone", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("look", [0, 1])
@pytest.mark.parametrize("yaw", [0.0, 0.5, -0.5, 1.2, -2.0, 0.9599311351776123])
@pytest.mark.parametrize("pitch", [0.0, 0.3, -1.5, 1.7])
@pytest.mark.parametrize("bones", [0, 1])
def test_apply_look(cw, look, yaw, pitch, bones):
    def setup(em):
        h, m, v, _ = human(em)
        wf(em, h + 0x78, 0.36, 0.48, 0.3, -0.6, 0.8, 0.1, 0.64, 0.48, 0.7)
        if not bones:
            wf(em, h + 0x80, 0.0)
            wf(em, h + 0x8C, 0.0)
            wf(em, h + 0x98, 0.0)
        em.write(h + 0x64C, bytes([look]))
        wf(em, v + 0x120, yaw)
        wf(em, v + 0x12C, pitch)
        if bones:
            wf(em, m + 0x228, 1.5, 2.25, -3.0)
            wf(em, m + 0x2AC, 4.0, 1.0, 7.125)
        return (h,)
    check(e, fk, 0x489F70, "Human_ApplyLook", setup=setup, cw=cw, check_eax=False)


@pytest.mark.parametrize("addr,name", [(0x48A320, "Human_EnableLook"), (0x48A330, "Human_DisableLook")])
@pytest.mark.parametrize("v", [0, 1])
def test_look_flag(addr, name, v):
    def setup(em):
        h = human(em)[0]
        em.write(h + 0x64C, bytes([v]))
        return (h,)
    check(e, fk, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("bone", [0, 3])
def test_bone_pos(cw, bone):
    def setup(em):
        h, m, _, _ = human(em)
        wf(em, h + 0x78, 0.36, 0.48, 0.3, -0.6, 0.8, 0.1, 0.64, 0.48, 0.7)
        wf(em, m + 0x18 + bone * 0x84, 12.345, -6.789, 1.0 / 3)
        wf(em, h + 0x514, 0.1, 0.2, -0.3)
        out = em.alloc(12)
        return (out, h, bone)
    check(e, fk, 0x48A340, "Human_GetBonePos", setup=setup, cw=cw)
