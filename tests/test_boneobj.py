import math
import random
import struct

import pytest

from emu import ErrorShown
from test_boneobj_util import (BONE, FADE_WEIGHT, GET_ORIENT_CB, ROOT_QUAT, SMOOTH, check, f32, fade, i32,
                               make_bones, new_emu, node, orient_source, put, quat, rquat, set_blend)

e = new_emu()
CWS = [0x37F, 0x7F]
N = 12


@pytest.mark.parametrize("cw", CWS)
def test_lerp_vec(cw):
    r = random.Random(1)

    def setup(em):
        a = put(em, f32(*[r.uniform(-9, 9) for _ in range(3)]))
        b = put(em, f32(*[r.uniform(-9, 9) for _ in range(3)]))
        return em.alloc(12), a, b, fbits(r.uniform(-0.5, 1.5))
    for _ in range(N):
        check(e, 0x4D3160, "BoneObj_LerpVec", seeded(r, setup), cw, check_eax=False)


def fbits(v):
    return struct.unpack("<I", f32(v))[0]


def seeded(r, fn):
    st = r.getstate()

    def s(em):
        r.setstate(st)
        return fn(em)
    return s


def unit(q):
    n = math.sqrt(sum(x * x for x in q[:4]))
    return [x / n for x in q[:4]] + [q[4]]


SLERP_T = [0.0005, 0.001, 0.3, 0.5, 0.75, 0.9989, 0.999, 1.2]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("t", SLERP_T)
@pytest.mark.parametrize("kind", ["random", "near", "tiny", "same", "opposite"])
def test_slerp(kind, t, cw):
    r = random.Random("%s%s" % (kind, t))

    def setup(em):
        a = rquat(r)
        if kind == "random":
            b = rquat(r)
        elif kind == "near":
            b = [x + r.uniform(-0.003, 0.003) for x in a[:4]] + [7]
        elif kind == "tiny":
            a = unit(a)
            b = unit([x + r.uniform(-0.002, 0.002) for x in a[:4]] + [3])
        elif kind == "same":
            b = list(a)
        else:
            b = [-x + r.uniform(-0.01, 0.01) for x in a[:4]] + [9]
        return em.alloc(20), put(em, quat(a)), put(em, quat(b)), fbits(t)
    for _ in range(N):
        check(e, 0x4D4500, "BoneObj_Slerp", seeded(r, setup), cw, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("t0,t1", [(-1, -1), (-1, 0), (-1, 1), (0, -1), (1, -1), (0, 0), (0, 1), (1, 0), (1, 1)])
def test_blend_orientations(t0, t1, cw):
    r = random.Random(t0 * 3 + t1)

    def setup(em):
        a, b = rquat(r, r.randrange(400, 600)), rquat(r, r.randrange(400, 600))
        return em.alloc(20), t0 & 0xFFFFFFFF, put(em, quat(a)), t1 & 0xFFFFFFFF, put(em, quat(b)), fbits(r.random())
    for _ in range(N):
        check(e, 0x4D41C0, "BoneObj_BlendOrientations", seeded(r, setup), cw)


def root_quat(em, r):
    p = put(em, quat(rquat(r, r.randrange(400, 700))))
    em.write(ROOT_QUAT, struct.pack("<I", p))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("mode", range(6))
@pytest.mark.parametrize("parent", [False, True])
def test_get_orientation_cb(mode, parent, cw):
    r = random.Random(mode * 2 + parent)

    def setup(em):
        root_quat(em, r)
        bones = make_bones(em, [-1, 0], seed=r.randrange(99))
        b = bones + BONE if parent else bones
        em.write(bones + 0x5C, i32(r.randrange(300, 700)))
        d = put(em, i32(1 if parent else 0, mode) + quat(rquat(r, r.randrange(300, 700))))
        return em.alloc(20), b, d
    for seed in range(N):
        check(e, GET_ORIENT_CB, "BoneObj_GetOrientationCB", seeded(r, setup), cw)


def test_get_orientation_cb_other_bone():
    def setup(em):
        bones = make_bones(em, [-1, 0])
        return em.alloc(20), bones, put(em, i32(1, 0) + quat([0, 0, 0, 1, 0]))
    check(e, GET_ORIENT_CB, "BoneObj_GetOrientationCB", setup)


def test_get_orientation_cb_bad_mode():
    msgs = []
    for target in (GET_ORIENT_CB, "BoneObj_GetOrientationCB"):
        e.reset()
        root_quat(e, random.Random(1))
        bones = make_bones(e, [-1, 0])
        d = put(e, i32(0, 6) + quat([0, 0, 0, 1, 0]))
        with pytest.raises(ErrorShown) as err:
            e.call(target, e.alloc(20), bones, d)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]
