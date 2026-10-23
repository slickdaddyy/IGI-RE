import math
import random
import struct

import pytest

import test_boneobj_anim as A
from emu import ErrorShown
from test_boneobj_util import (ALLOC_NODE, ALLOC_SOURCE, BONE, FADE_WEIGHT, GET_ORIENT_CB, NODE_POOL, PARENT_ORIENT_CB,
                               ROOT_QUAT, check, f32, fade, fade_node, i32, node, orient_source, put, quat, rquat,
                               set_blend, u32)

e = A.e
CWS = [0x37F, 0x7F]


def fb(v):
    return struct.unpack("<I", f32(v))[0]


def oriented(em, o, track, mode, aid, blend, seed=1):
    q = put(em, quat(rquat(random.Random(seed), 600)))
    em.call(0x4D6130, o, track, mode, q, aid & 0xFFFFFFFF, 0, fb(blend))


def fade_out(em, o, track, t):
    em.call(0x4D62B0, o, track, fb(t))


SCENES = {
    "play": lambda em, o: A.play(em, o, 0, 0),
    "fade": lambda em, o: (A.play(em, o, 0, 0), A.play(em, o, 0, 1, blend=0.5)),
    "fade2": lambda em, o: (A.play(em, o, 0, 0), A.play(em, o, 0, 1, blend=0.5), A.play(em, o, 0, 3, blend=0.3)),
    "track": lambda em, o: (A.play(em, o, 0, 0), A.play(em, o, 2, 1, 2), A.play(em, o, 3, 3, 1, blend=0.2)),
    "orient": lambda em, o: (A.play(em, o, 0, 0), oriented(em, o, 3, 4, -1, 0.0)),
    "orient_anim": lambda em, o: (A.play(em, o, 0, 0), oriented(em, o, 2, 0, 1, 0.5)),
    "orient_root": lambda em, o: (A.play(em, o, 0, 4), oriented(em, o, 1, 5, -1, 0.2, 3), oriented(em, o, 4, 2, 3, 0.0)),
    "fade_out": lambda em, o: (A.play(em, o, 0, 0), A.play(em, o, 2, 1), fade_out(em, o, 2, 0.4)),
    "fade_out2": lambda em, o: (A.play(em, o, 0, 0), A.play(em, o, 2, 1), A.play(em, o, 2, 3, blend=0.3),
                                fade_out(em, o, 2, 0.4)),
}


def scene(name, frames=0):
    def setup(em):
        o = A.load(em, caches=name.startswith("fade"))
        SCENES[name](em, o)
        ev = em.alloc(0x1C * 64)
        for _ in range(frames):
            em.call(0x4D3210, ev, o)
        return o
    return setup


@pytest.mark.parametrize("name", SCENES)
@pytest.mark.parametrize("track,aid,blend", [(0, 0, 0.0), (0, 1, 0.5), (2, 3, 0.0), (2, 0, 0.01), (4, 1, 0.3)])
def test_anim_play(name, track, aid, blend):
    s = scene(name)
    check(e, 0x4D61D0, "Anim_Play", lambda em: (s(em), track, aid, 1, fb(blend)), check_eax=False)
    if blend == 0.0:
        check(e, 0x4D6110, "Anim_PlayNow", lambda em: (s(em), track, aid, 2), check_eax=False)


@pytest.mark.parametrize("name", ["play", "fade", "orient", "fade_out"])
@pytest.mark.parametrize("track,mode,aid,blend", [(0, 1, 0, 0.0), (2, 4, -1, 0.5), (3, 0, 1, 0.0), (4, 3, 3, 0.2)])
def test_anim_play_oriented(name, track, mode, aid, blend):
    s = scene(name)

    def setup(em):
        o = s(em)
        return o, track, mode, put(em, quat(rquat(random.Random(4), 1))), aid & 0xFFFFFFFF, 0, fb(blend)
    check(e, 0x4D6130, "Anim_PlayOriented", setup, check_eax=False)


@pytest.mark.parametrize("name", ["play", "fade", "track", "orient", "fade_out"])
@pytest.mark.parametrize("track", [0, 2, 3])
@pytest.mark.parametrize("t", [0.0, 0.006, 0.0066, 0.4])
def test_anim_fade_out(name, track, t):
    s = scene(name)
    check(e, 0x4D62B0, "Anim_FadeOut", lambda em: (s(em), track, fb(t)), check_eax=False)


@pytest.mark.parametrize("name", SCENES)
@pytest.mark.parametrize("frames", [0, 1, 7, 40, 70])
def test_get_events(name, frames):
    s = scene(name, frames)
    check(e, 0x4D3210, "BoneObj_GetEvents", lambda em: (em.alloc(0x1C * 64), s(em)))


def rot_matrix(axis, angle):
    c, s = math.cos(angle), math.sin(angle)
    x, y, z = axis
    n = math.sqrt(x * x + y * y + z * z)
    x, y, z = x / n, y / n, z / n
    m = [c + x * x * (1 - c), x * y * (1 - c) - z * s, x * z * (1 - c) + y * s,
         y * x * (1 - c) + z * s, c + y * y * (1 - c), y * z * (1 - c) - x * s,
         z * x * (1 - c) - y * s, z * y * (1 - c) + x * s, c + z * z * (1 - c)]
    return f32(*m) + i32(700)


MATRICES = [((0, 0, 1), 0.3), ((1, 0, 0), math.pi), ((0, 1, 0), math.pi), ((0, 0, 1), math.pi),
            ((1, 1, 0), 2.9), ((0, 1, 1), 3.0), ((1, 0, 1), 3.1), ((0.3, -0.5, 0.8), 1.7)]
_mr = random.Random(8)
MATRICES += [((_mr.uniform(-1, 1), _mr.uniform(-1, 1), _mr.uniform(-1, 1)), _mr.uniform(2.1, 3.1)) for _ in range(16)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("name", SCENES)
@pytest.mark.parametrize("mat", range(len(MATRICES)))
@pytest.mark.parametrize("frames", [0, 5])
def test_update(name, mat, frames, cw):
    if mat >= 8 and (name != "play" or frames):
        return
    s = scene(name, frames)

    def setup(em):
        o = s(em)
        return put(em, rot_matrix(*MATRICES[mat])), o
    check(e, 0x4D38C0, "BoneObj_Update", setup, cw, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("name", ["play", "fade", "orient_root"])
def test_update_counts(name, cw):
    """Bone and root counts high enough that the quaternions get normalised."""
    s = scene(name)

    def setup(em):
        o = s(em)
        b = u32(em, o + 8)
        for i in range(len(A.PARENTS)):
            em.write(b + BONE * i + 0x5C, i32(990))
        return put(em, rot_matrix((0.2, 0.4, 0.1), 0.8)[:-4] + i32(995)), o
    check(e, 0x4D38C0, "BoneObj_Update", setup, cw, check_eax=False)


GETTERS = [(0x4D6460, "Anim_GetInst", {}), (0x4D64C0, "Anim_GetId", {}),
           (0x4D64E0, "Anim_GetLength", {"check_st0": True, "check_eax": False}),
           (0x4D6500, "Anim_GetTime", {"check_st0": True, "check_eax": False}),
           (0x4D6520, "Anim_GetPhase", {"check_st0": True, "check_eax": False})]


@pytest.mark.parametrize("name", SCENES)
@pytest.mark.parametrize("track", [0, 2, 3])
@pytest.mark.parametrize("getter", GETTERS, ids=[g[1] for g in GETTERS])
def test_anim_getters(name, track, getter):
    s = scene(name, 3)
    orig, ours, kw = getter
    if orig != 0x4D6460:
        e.reset()
        try:
            e.reset()
            e.call(orig, s(e), track)
        except Exception:
            return  # no animation instance there
    check(e, orig, ours, lambda em: (s(em), track), **kw)


@pytest.mark.parametrize("speed", [1.5, -0.5, 0.0])
def test_anim_set_speed_time(speed):
    s = scene("fade", 2)
    check(e, 0x4D6580, "Anim_SetSpeed", lambda em: (s(em), 0, fb(speed)), check_eax=False)
    check(e, 0x4D65B0, "Anim_SetTime", lambda em: (s(em), 0, fb(speed * 100)), check_eax=False)


# --- blend trees built by hand -------------------------------------------------

def bones2(em):
    return A.bones(em, A.load(em))


def src(em):
    return orient_source(em, 0, 1, rquat(random.Random(2)))


def tree(em, shape):
    """shape: 'S' source, None empty, or (left, right) node."""
    if shape == "S":
        return 1, src(em)
    if shape is None:
        return -1, 0
    l, r = tree(em, shape[0]), tree(em, shape[1])
    n = node(em, l[0] & 0xFFFFFFFF, l[1], r[0] & 0xFFFFFFFF, r[1])
    for t, p in (l, r):
        if t == 0:
            em.write(p, struct.pack("<I", n))
    return 0, n


SHAPES = [(None, None), ("S", None), (None, "S"), ("S", "S"), ((None, None), "S"), (("S", None), None),
          ((("S", None), None), None), (None, ("S", "S")), (("S", "S"), (None, "S")), ((None, None), (None, None)),
          (((None, "S"), "S"), ("S", ("S", None)))]


@pytest.mark.parametrize("shape", SHAPES, ids=[str(s) for s in SHAPES])
@pytest.mark.parametrize("parented", [False, True])
def test_prune(shape, parented):
    def setup(em):
        t, n = tree(em, shape)
        if parented:
            top = node(em, 0, n, 1, src(em))
            em.write(n, struct.pack("<I", top))
        return (n,)
    check(e, 0x4D3750, "BoneObj_PruneBlendNode", setup)

    def setup_bone(em):
        b = bones2(em)
        set_blend(em, b, tree(em, shape)[1])
        return (b,)
    check(e, 0x4D3730, "BoneObj_PruneBone", setup_bone, check_eax=False)


@pytest.mark.parametrize("shape", SHAPES[3:], ids=[str(s) for s in SHAPES[3:]])
@pytest.mark.parametrize("faded", [False, True])
def test_delete_blend_node(shape, faded):
    def setup(em):
        n = tree(em, shape)[1]
        if faded:
            em.write(n + 0xC, struct.pack("<III", FADE_WEIGHT, 0x4D63F0, 0x4D63B0) + struct.pack("<I", fade(em, 9, 1)))
        return (n,)
    check(e, 0x4D34A0, "BoneObj_DeleteBlendNode", setup, check_eax=False)
    check(e, 0x4D36E0, "BoneObj_ReleaseBlendNodeData", setup, check_eax=False)
    for i in (0, 1):
        check(e, 0x4D3440, "BoneObj_DeleteBlendNodeBranch", lambda em: setup(em) + (i,), check_eax=False)


def errors(orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        args = setup(e)
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]


def test_illegal_branches():
    def bad(em):
        return (node(em, 7, 0, -1 & 0xFFFFFFFF, 0),)
    errors(0x4D3440, "BoneObj_DeleteBlendNodeBranch", lambda em: bad(em) + (0,))
    errors(0x4D33D0, "BoneObj_GetBranchEvents", lambda em: (0, 0, 0, put(em, i32(5, 0))))


@pytest.mark.parametrize("sel", [-1, 0, 1, 2, 3])
@pytest.mark.parametrize("events", [False, True])
def test_get_node_events(sel, events):
    s = scene("fade")

    def setup(em):
        o = s(em)
        b = A.bones(em, o)
        n = u32(em, b + 0x80)
        if sel >= 0:
            f = fade(em, 5.0, sel, 4.5)
            em.write(n + 0xC, struct.pack("<4I", FADE_WEIGHT, 0x4D63F0, 0x4D63B0, f))
        else:
            em.write(n + 0xC, struct.pack("<4I", 0, 0, 0, 0))
        return em.alloc(0x1C * 32) if events else 0, 1, b, n
    check(e, 0x4D32A0, "BoneObj_GetNodeEvents", setup)


@pytest.mark.parametrize("kind", ["anim", "node", "none", "orient"])
def test_branch_position(kind):
    s = scene("fade", 2)

    def setup(em):
        o = s(em)
        n = u32(em, A.bones(em, o) + 0x80)
        br = {"anim": put(em, i32(1, u32(em, n + 0x28))), "node": put(em, i32(0, n)), "none": put(em, i32(-1, 0)),
              "orient": put(em, i32(1, src(em)))}[kind]
        return em.alloc(12), em.alloc(12), br
    check(e, 0x4D31C0, "BoneObj_GetBranchPosition", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("kind", ["anim", "node", "none", "orient", "parent"])
def test_branch_orientation(kind, cw):
    s = scene("fade", 2)

    def setup(em):
        o = s(em)
        b = A.bones(em, o)
        n = u32(em, b + 0x80)
        em.write(b + 4, i32(0x20000))
        p = em.call(0x4D46E0, b + BONE * 3)[0]
        br = {"anim": put(em, i32(1, u32(em, n + 0x28))), "node": put(em, i32(0, n)), "none": put(em, i32(-1, 0)),
              "orient": put(em, i32(1, src(em))), "parent": put(em, i32(1, p))}[kind]
        return em.alloc(20), b + BONE * 3, br
    check(e, 0x4D4690, "BoneObj_GetBranchOrientation", setup, cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("parent", [False, True])
@pytest.mark.parametrize("custom", [False, True])
@pytest.mark.parametrize("types", [(0, 1), (1, 1), (4, 0), (5, 3), (3, 2)])
def test_node_orientation(parent, custom, types, cw):
    r = random.Random(str((parent, custom, types)))

    def setup(em):
        em.write(ROOT_QUAT, struct.pack("<I", put(em, quat(rquat(r, r.randrange(400, 600))))))
        b = bones2(em)
        bone = b + BONE if parent else b
        em.write(b + 0x5C, i32(r.randrange(400, 600)))
        s0 = orient_source(em, 1 if parent else 0, types[0], rquat(r, r.randrange(400, 600)))
        s1 = orient_source(em, 1 if parent else 0, types[1], rquat(r, r.randrange(400, 600)))
        n = node(em, 1, s0, 1, s1, 0x4D41C0 if custom else 0, FADE_WEIGHT if r.random() < 0.7 else 0, 0, 0,
                 fade(em, 5.0, 0, 1.0, r.random()))
        return em.alloc(20), bone, n
    for _ in range(6):
        st = r.getstate()

        def s(em):
            r.setstate(st)
            return setup(em)
        check(e, 0x4D3F50, "BoneObj_GetNodeOrientation", s, cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("animated", [0, 1, 2])
def test_parent_orientation_cb(animated, cw):
    s = scene("track")

    def setup(em):
        o = s(em)
        b = A.bones(em, o)
        em.write(b + 4, i32(0x20000))
        if animated:
            em.write(b + BONE * 3 + 4, i32(0x20000 if animated == 1 else 0))
            set_blend(em, b + BONE * 3, u32(em, b + BONE * 2 + 0x80) if animated == 1 else 0)
        return em.alloc(20), b + BONE * 4, b + BONE * 4
    check(e, PARENT_ORIENT_CB, "BoneObj_ParentOrientationCB", setup, cw)
    check(e, 0x4D47B0, "BoneObj_FindBlendParent", lambda em: setup(em)[2:])


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("t", [0.0, 3.0, 4.0, 4.5, 9.0])
def test_fade_callbacks(t, cw):
    def setup(em):
        return (fade(em, 5.0, 2, t, 0.25),)
    check(e, 0x4D63B0, "BoneObj_FadeSelectCB", setup, cw)
    check(e, 0x4D63A0, "BoneObj_FadeWeightCB", setup, cw, check_st0=True, check_eax=False)
    check(e, 0x4D63F0, "BoneObj_FadeFreeCB", setup, check_eax=False)
    check(e, 0x4D6400, "BoneObj_FreeFade", setup, check_eax=False)
    check(e, 0x4D5FD0, "BoneObj_InitFade", lambda em: setup(em) + (fb(t), 7), check_eax=False)


@pytest.mark.parametrize("speed", [1.0, -1.0, 0.0])
def test_update_anim_direction(speed):
    def setup(em):
        return (put(em, i32(0, 0, 0x8003, 0) + f32(0, 0, 0, 0, 0, 0, speed, 0) + i32(0)),)
    check(e, 0x4D6550, "BoneObj_UpdateAnimDirection", setup, check_eax=False)


def test_small_functions():
    q = lambda em: put(em, quat(rquat(random.Random(1))))
    check(e, 0x4D35B0, "BoneObj_AllocBlendNode", lambda em: ())
    check(e, 0x4D4710, "BoneObj_AllocBlendSource", lambda em: ())
    check(e, 0x4D4F20, "BoneObj_AllocAnimInst", lambda em: ())
    check(e, 0x4D5750, "BoneObj_AllocOrientation", lambda em: ())
    check(e, 0x4D5F90, "BoneObj_AllocFade", lambda em: ())
    check(e, 0x4D3560, "BoneObj_NewBlendNode", lambda em: tuple(range(1, 11)))
    check(e, 0x4D3600, "BoneObj_SetBlendNode", lambda em: (em.alloc(0x2C),) + tuple(range(1, 12)), check_eax=False)
    check(e, 0x4D4750, "BoneObj_SetBlendSource", lambda em: (em.alloc(0x14), 1, 2, 3, 4, 5), check_eax=False)
    check(e, 0x4D5790, "BoneObj_SetOrientation", lambda em: (em.alloc(0x1C), 3, 4, q(em)), check_eax=False)
    check(e, 0x4D5700, "BoneObj_NewOrientationSource", lambda em: (2, 5, q(em)))
    check(e, 0x4D46E0, "BoneObj_NewParentSource", lambda em: (0x1234,))
    check(e, 0x4D3720, "BoneObj_GetBlendBranch", lambda em: (0x1000, 1))
    check(e, 0x4D38A0, "BoneObj_SetRoot", lambda em: (0x111, 0x222), check_eax=False)

    def root(em):
        em.write(0xA538E8, struct.pack("<II", 0x333, 0x444))
        return em.alloc(4), em.alloc(4)
    check(e, 0x4D4190, "BoneObj_GetRoot", root, check_eax=False)
    check(e, 0x4D4190, "BoneObj_GetRoot", lambda em: root(em) and (0, 0), check_eax=False)

    def source(em):
        return (em.call(0x4D5700, 2, 5, q(em))[0],)
    check(e, 0x4D3660, "BoneObj_DeleteBlendSource", source, check_eax=False)
    check(e, 0x4D3660, "BoneObj_DeleteBlendSource", lambda em: (em.call(0x4D46E0, 0)[0],), check_eax=False)
    check(e, 0x4D57C0, "BoneObj_FreeOrientation", lambda em: (u32(em, source(em)[0] + 0x10),), check_eax=False)


@pytest.mark.parametrize("name", ["play", "fade", "orient", "fade_out"])
@pytest.mark.parametrize("track", [0, 1, 2])
@pytest.mark.parametrize("t", [0.5, 1.0, 30.0])
def test_fade_in_blend_to(name, track, t):
    s = scene(name)

    def setup(em):
        o = s(em)
        return A.bones(em, o) + BONE * track, src(em), fb(t), 1
    check(e, 0x4D5EC0, "BoneObj_FadeIn", setup, check_eax=False)

    def setup_node(em):
        b, so, tt, res = setup(em)
        return b, node(em, 1, so, -1 & 0xFFFFFFFF, 0), tt, res
    check(e, 0x4D5FF0, "BoneObj_BlendTo", setup_node, check_eax=False)


def test_blend_to_root_without_blend():
    def setup(em):
        b = bones2(em)
        return b, node(em, 1, src(em), -1 & 0xFFFFFFFF, 0), fb(5.0), 0
    check(e, 0x4D5FF0, "BoneObj_BlendTo", setup, check_eax=False)


# blend callback: out = q0, returns 2 so the bone takes it as absolute
ABS_BLEND = (b"\x56\x57\x8b\x7c\x24\x0c\x8b\x74\x24\x14\xb9\x05\x00\x00\x00\xf3\xa5"
             b"\x5f\x5e\xb8\x02\x00\x00\x00\xc3")


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(8))
def test_update_bone_absolute(seed, cw):
    def setup(em):
        r = random.Random(seed)
        b = bones2(em)
        stub = put(em, ABS_BLEND)
        so = orient_source(em, 2, 1, rquat(r, r.randrange(400, 600)))
        n = node(em, 1, so, -1 & 0xFFFFFFFF, 0, stub)
        set_blend(em, b + BONE * 2, n)
        m = put(em, rot_matrix((r.random(), r.random(), 1), r.uniform(0, 3)))
        pq = put(em, quat(rquat(r, r.randrange(400, 600))))
        pos = put(em, f32(1, 2, 3))
        return m, pq, pos, b + BONE * 2, 0
    check(e, 0x4D3A90, "BoneObj_UpdateBone", setup, cw, check_eax=False)
