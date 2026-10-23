import random
import struct

import pytest

from test_boneobj_util import (ANIM_EVENTS_CB, ANIM_ORIENT_CB, ANIM_POS_CB, BONE, SETS, SKELS, SMOOTH, anim, bobj,
                               check, f32, i32, new_emu, put, u32)

PARENTS = [-1, 0, 1, 0, 3]
SKEL = 3
LOOPS = 0x80000000
EVENTS = [(11, 0.0, 1, 2, 3), (12, 250.0, 0.5, 0, 0), (13, 250.0, 0, 1, 0), (14, 600.0, 0, 0, 2), (15, 1000.0, 1, 1, 1)]


def build_file():
    r = random.Random(5)
    offsets = [[r.uniform(-1, 1) for _ in range(3)] for _ in PARENTS]
    anims = [anim(r, 0, 5, 1000.0, LOOPS, pos=4, rots=[4] * 5, events=EVENTS),
             anim(r, 1, 5, 600.0, 0, pos=1, rots=[1, 2, 5, 3, 1]),
             anim(r, 3, 5, 800.0, 0, pos=2, rots=[2] * 5, events=EVENTS[:2]),
             anim(r, 4, 5, 1000.0, LOOPS, pos=6, rots=[6] * 5, near=True)]
    return bobj(SKEL, PARENTS, offsets, anims, total=5)


FILE = build_file()
NAME = b"anims\\soldier.bobj\0"

e = new_emu()
e.fake(0x4B6380, nargs=1)  # Resource_Unload
e.fake(0x4AF810, nargs=2)  # Warning_Show
FILE_P = put(e, FILE)
NAME_P = put(e, NAME)
e.fake(0x4B5F00, ret=FILE_P, nargs=2)  # Resource_Load
e.baseline = e.snapshot()
CWS = [0x37F, 0x7F]


def load(em, caches=False):
    em.call(0x4D67A0, NAME_P)
    if caches:
        em.call(0x4B0B00, SETS + SKEL * 0x28 + 8, 0x10 + 4 * len(PARENTS), 16, 4)
        em.write(SETS + SKEL * 0x28 + 0x20, i32(1))
    o = em.alloc(12)
    em.call(0x4D2D60, o, SKEL)  # BoneObj_Create
    return o


def bones(em, o):
    return u32(em, o + 8)


def play(em, o, track, aid, flags=0, blend=0.0):
    em.call(0x4D61D0, o, track, aid, flags, struct.unpack("<I", f32(blend))[0])


def inst(em, o, track):
    return em.call(0x4D6460, o, track)[0]


def set_time(em, i, t):
    em.write(i + 0x2C, f32(t))


def test_load():
    check(e, 0x4D67A0, "BoneObj_Load", lambda em: (NAME_P,), check_eax=False)


@pytest.mark.parametrize("data", [b"JUNK\0\0\0\0", b"FORM\0\0\0\x04XXXX"])
def test_load_bad(data):
    def setup(em):
        em.write(FILE_P, data)
        return (NAME_P,)
    check(e, 0x4D67A0, "BoneObj_Load", setup, check_eax=False)


def test_load_no_pos_keys():
    r = random.Random(9)
    f = bobj(SKEL, PARENTS, [[0, 0, 1]] * 5, [anim(r, 2, 5, 800.0, 0, pos=0, rots=[2, 1, 2, 1, 2])], total=3)

    def setup(em):
        em.write(FILE_P, f)
        return (NAME_P,)
    check(e, 0x4D67A0, "BoneObj_Load", setup, check_eax=False)


def test_load_no_bones():
    f = bobj(SKEL, [], [], [])

    def setup(em):
        em.write(FILE_P, f)
        return (NAME_P,)
    check(e, 0x4D67A0, "BoneObj_Load", setup, check_eax=False)


@pytest.mark.parametrize("caches", [False, True])
def test_free_all(caches):
    check(e, 0x4D65D0, "BoneObj_FreeAll", lambda em: load(em, caches) and (), check_eax=False)


@pytest.mark.parametrize("caches", [False, True])
def test_free_anims(caches):
    check(e, 0x4D6640, "BoneObj_FreeAnims", lambda em: load(em, caches) and (SKEL,), check_eax=False)


def test_free_skeleton():
    check(e, 0x4D65F0, "BoneObj_FreeSkeleton", lambda em: load(em) and (SKEL,), check_eax=False)
    check(e, 0x4D65F0, "BoneObj_FreeSkeleton", lambda em: (SKEL,), check_eax=False)


def odd_angles(em, i):
    """Small and wrapped slerp angles, which the loader's acos table never makes."""
    rot = u32(em, u32(em, i + 0xC) + 0xC)
    angles = [0.005, 6.2831855 + 0.004, 0.0, 7.0, 6.2831855, 0.3]
    for b in range(len(PARENTS)):
        for t in (8, 12):
            sl = u32(em, rot + 16 * b + t)
            for k in range(6):
                em.write(sl + 8 * k, f32(angles[(k + b + t) % 6], 1.0 + k))


ORIENT_TIMES = [-10.0, 0.0, 100.0, 250.0, 260.0, 600.0, 749.0, 900.0, 999.0, 1200.0]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("smooth", [0, 1])
@pytest.mark.parametrize("aid", [0, 1, 4])
@pytest.mark.parametrize("caches", [False, True])
def test_anim_orientation_cb(aid, smooth, caches, cw):
    for t in ORIENT_TIMES:
        for b in range(len(PARENTS)):
            def setup(em, t=t, b=b):
                o = load(em, caches)
                play(em, o, 0, aid)
                i = inst(em, o, 0)
                em.write(SMOOTH, i32(smooth))
                if aid == 4:
                    odd_angles(em, i)
                if caches:
                    c = u32(em, i + 0x30)
                    em.write(c + 4, f32(t - 50))
                    em.write(u32(em, c + 0xC) + 4 * b, i32(1))
                set_time(em, i, t)
                return em.alloc(20), bones(em, o) + BONE * b, i
            check(e, ANIM_ORIENT_CB, "BoneObj_AnimOrientationCB", setup, cw)


POS_TIMES = [0.0, 100.0, 333.33334, 500.0, 999.0, 1000.0, 1100.0] + [random.Random(i).uniform(0, 1000) for i in range(12)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("smooth", [0, 1])
@pytest.mark.parametrize("aid", [0, 1, 4])
@pytest.mark.parametrize("out", [False, True])
def test_anim_position_cb(aid, smooth, out, cw):
    for t in POS_TIMES:
        def setup(em, t=t):
            o = load(em)
            play(em, o, 0, aid)
            i = inst(em, o, 0)
            em.write(SMOOTH, i32(smooth))
            set_time(em, i, t)
            em.write(i + 0x10, f32(0.25, -0.5, 1.0))
            return ((em.alloc(12), em.alloc(12)) if out else (0, 0)) + (i,)
        check(e, ANIM_POS_CB, "BoneObj_AnimPositionCB", setup, cw)


# time, speed, flags
EVENT_CASES = [(0.0, 160.0, 0), (240.0, 20.0, 0), (250.0, 0.0, 0x8000), (900.0, 160.0, 1), (900.0, 160.0, 0x8001),
               (900.0, 160.0, 0), (100.0, -160.0, 3), (100.0, -160.0, 2), (300.0, -60.0, 0x8002), (980.0, 30.0, 1),
               (0.0, 160.0, 0x8000)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", EVENT_CASES)
@pytest.mark.parametrize("track", [0, 2])
@pytest.mark.parametrize("caches", [False, True])
@pytest.mark.parametrize("events", [False, True])
def test_anim_events_cb(case, track, caches, events, cw):
    t, speed, flags = case

    def setup(em):
        o = load(em, caches)
        play(em, o, track, 0)
        i = inst(em, o, track)
        em.write(i + 8, i32(flags))
        em.write(i + 0x28, f32(speed, t))
        ev = em.alloc(0x1C * 16) if events else 0
        return ev, 2, bones(em, o) + BONE * track, i
    check(e, ANIM_EVENTS_CB, "BoneObj_AnimEventsCB", setup, cw)


@pytest.mark.parametrize("frm,to", [(0.0, 0.0), (250.0, 250.0), (-1.0, 300.0), (240.0, 260.0), (260.0, 240.0),
                                    (1001.0, 590.0), (0.0, 1000.0), (600.0, 600.0), (700.0, 1500.0)])
def test_anim_collect_events(frm, to):
    def setup(em):
        o = load(em)
        play(em, o, 1, 0)
        return (em.alloc(0x1C * 16), 3, bones(em, o) + BONE, inst(em, o, 1),
                struct.unpack("<I", f32(frm))[0], struct.unpack("<I", f32(to))[0])
    check(e, 0x4D4DC0, "BoneObj_AnimCollectEvents", setup)
    check(e, 0x4D4DC0, "BoneObj_AnimCollectEvents", lambda em: (0, 3, 0, 0, 0, 0))


@pytest.mark.parametrize("flags", [0, 2, 0x8001])
@pytest.mark.parametrize("caches", [False, True])
def test_new_anim_source(flags, caches):
    check(e, 0x4D47D0, "BoneObj_NewAnimSource", lambda em: load(em, caches) and (SKEL, 2, 1, flags))


@pytest.mark.parametrize("caches", [False, True])
def test_free_anim_inst(caches):
    def setup(em):
        o = load(em, caches)
        play(em, o, 0, 0)
        play(em, o, 2, 1)
        return (inst(em, o, 0),)
    check(e, 0x4D5090, "BoneObj_FreeAnimInst", setup, check_eax=False)
    check(e, 0x4D5080, "BoneObj_AnimFreeCB", setup, check_eax=False)
