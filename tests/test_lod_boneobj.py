import random
import struct

import pytest

from emu import Emu
from test_lod_util import Fakes, check, check_error, f32, put, u32

e = Emu()
fk = Fakes(e)
fk.add(0x4D38A0, 2, name="4d38a0")
fk.add(0x4D65D0, 0, name="4d65d0")
fk.add(0x4B0B00, 4, name="pool_init")
fk.add(0x4B0B70, 1, name="pool_free")
fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(a[0]), name="alloc")
fk.add(0x4B0D10, 1, name="free")
fk.add(0x4B0DB0, 0, ret=0, name="get_mode")
fk.add(0x4B0D40, 0, name="default_mode")
fk.add(0x4B0D50, 0, name="clear_mode")
fk.add(0x4D34A0, 1, name="blend_free")

HIER = 0xA542F0

# parent index per bone; -1 is a root
PARENTS = [-1, 0, 1, 1, 0, 4, -1, 6, 2]


def hierarchy(em, type_, parents, rng):
    n = len(parents)
    p = put(em, struct.pack("<%di" % n, *parents))
    o = put(em, struct.pack("<%df" % (3 * n), *[rng.uniform(-30, 30) for _ in range(3 * n)]))
    em.write(HIER + 12 * type_, struct.pack("<iII", n, p, o))


def test_init():
    check(e, fk, 0x4D2C30, "BoneObj_Init", check_eax=False)


def test_shutdown():
    check(e, fk, 0x4D2CF0, "BoneObj_Shutdown", check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("fps", [30.0, 25.0, 7.3])
def test_frame_rate(fps, cw):
    check(e, fk, 0x4D2D30, "BoneObj_SetFrameRate", args=(struct.unpack("<I", struct.pack("<f", fps))[0],),
          check_eax=False, cw=cw)
    check(e, fk, 0x4D2D50, "BoneObj_GetTicksPerFrame", setup=lambda em: f32(em, 0xA54658, 4800 / fps),
          check_eax=False, check_st0=True)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("seed", range(3))
def test_create(seed, mode, cw):
    def setup(em):
        fk.rets[0x4B0DB0] = mode
        hierarchy(em, 5, PARENTS, random.Random(seed))
        return em.alloc(12), 5
    check(e, fk, 0x4D2D60, "BoneObj_Create", setup=setup, check_eax=False, cw=cw)


def test_create_no_hierarchy():
    check_error(e, fk, 0x4D2D60, "BoneObj_Create", lambda em: (em.alloc(12), 3))


@pytest.mark.parametrize("bones", [True, False])
def test_free(bones):
    def setup(em):
        n = len(PARENTS)
        hierarchy(em, 2, PARENTS if bones else [], random.Random(1))
        o = em.alloc(12)
        if bones:
            b = em.alloc(0x84 * n)
            for i in (1, 4, 8):
                u32(em, b + 0x84 * i + 0x80, 0x9000 + i)
            em.write(o, struct.pack("<iiI", 2, 7, b))
        else:
            em.write(o, struct.pack("<iiI", 2, 7, 0))
        return (o,)
    check(e, fk, 0x4D2E40, "BoneObj_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("index", [-1, 0, 1, 6])
def test_build(index, cw):
    def setup(em):
        hierarchy(em, 0, PARENTS, random.Random(4))
        bones = em.alloc(0x84 * len(PARENTS))
        u32(em, 0xA54638, HIER)
        u32(em, 0xA5467C, bones)
        root = em.alloc(0x84)
        return root, index
    check(e, fk, 0x4D2EC0, "BoneObj_Build", setup=setup, check_eax=False, cw=cw)


def test_bone_init():
    def setup(em):
        parent = em.alloc(0x84)
        u32(em, parent + 0x78, 0x4444)
        b = put(em, b"\xAB" * 0x84)
        return b, parent
    check(e, fk, 0x4D2F80, "Bone_Init", setup=setup, check_eax=False)


TRACKS = {}


def get_pose(em, a):
    r = TRACKS.get(a[2])
    if r is None:
        return 0
    f32(em, a[0], *r[0])
    f32(em, a[1], *r[1])
    return 1


fk.add(0x4D3720, 2, ret=lambda em, a: 0x7700 + a[1], fmt=lambda em, a: (a[1],), name="track")
fk.add(0x4D31C0, 3, ret=get_pose, fmt=lambda em, a: (a[2],), name="pose")
fk.add(0x4D3160, 4, deref={1: 12, 2: 12}, name="lerp")
WEIGHT = fk.func(1, ret=0.375, st0="f", name="weight")


def blend(em):
    b = em.alloc(0x1C)
    u32(em, b + 0xC, WEIGHT)
    u32(em, b + 0x18, 0x5151)
    return b


CASES = {"none": {}, "a": {0x7700: ((1, 2, 3), (4, 5, 6))}, "b": {0x7701: ((7, 8, 9), (10, 11, 12))},
         "ab": {0x7700: ((1, 2, 3), (4, 5, 6)), 0x7701: ((7, 8, 9), (10, 11, 12))}}


@pytest.mark.parametrize("outs", [(True, True), (True, False), (False, True), (False, False)])
@pytest.mark.parametrize("case", list(CASES))
def test_blend_motion(case, outs):
    def setup(em):
        TRACKS.clear()
        TRACKS.update(CASES[case])
        return (em.alloc(12) if outs[0] else 0), (em.alloc(12) if outs[1] else 0), blend(em)
    check(e, fk, 0x4D3020, "BoneBlend_GetMotion", setup=setup)


@pytest.mark.parametrize("outs", [(True, True), (True, False), (False, True)])
@pytest.mark.parametrize("case", ["noblend"] + list(CASES))
def test_root_motion(case, outs):
    def setup(em):
        TRACKS.clear()
        TRACKS.update(CASES.get(case, {}))
        bones = em.alloc(0x84)
        if case != "noblend":
            u32(em, bones + 0x80, blend(em))
        o = em.alloc(12)
        u32(em, o + 8, bones)
        return (put(em, b"\xEE" * 12) if outs[0] else 0), (put(em, b"\xEE" * 12) if outs[1] else 0), o
    check(e, fk, 0x4D2FB0, "BoneObj_GetRootMotion", setup=setup, check_eax=False)
