import math
import random
import struct

import pytest

from emu import Emu
from test_haze_util import CWS, ROUND_CWS, Fakes, check, f32, rbytes

e = Emu()
fk = Fakes(e)

SKELS, SETS, SMOOTH = 0xA542F0, 0xA538F0, 0xA54678
SKEL = 3


def quat(rng):
    q = [rng.uniform(-1, 1) for _ in range(4)]
    n = math.sqrt(sum(x * x for x in q))
    return [x / n for x in q]


def set_anims(e, descs, bones=4):
    """descs: list of desc pointers (0 for none) for animation set SKEL."""
    by_id = e.alloc(4 * max(len(descs), 1))
    for i, d in enumerate(descs):
        e.write(by_id + 4 * i, struct.pack("<I", d))
    e.write(SKELS + 12 * SKEL, struct.pack("<I", bones))
    e.write(SETS + 0x28 * SKEL + 4, struct.pack("<I", by_id))
    e.write(SETS + 0x28 * SKEL + 0x24, struct.pack("<I", len(descs)))


def make_desc(e, rng, bones, loops, length=1234.5, poskeys=3):
    tracks = e.alloc(0x10 * bones)
    prev = None
    for b in range(bones):
        n = rng.choice([0, 1, 2, 3, 5])
        keys = e.alloc(0x34 * max(n, 1))
        for k in range(n):
            q = quat(rng)
            if prev and rng.random() < 0.4:
                q = [-x for x in prev]
            prev = q
            e.write(keys + 0x34 * k, f32(*q, k * 100.0, *quat(rng), *quat(rng)))
        slerp = e.alloc(8) if rng.random() < 0.15 else 0
        e.write(tracks + 0x10 * b, struct.pack("<IIII", n, keys, slerp, 0))
    pos = e.alloc(8)
    pk = e.alloc(0x28 * poskeys)
    e.write(pk, f32(*[rng.uniform(-50, 50) for _ in range(10 * poskeys)]))
    e.write(pos, struct.pack("<II", poskeys, pk))
    d = e.alloc(0x14)
    e.write(d, f32(length) + struct.pack("<III", 0x80000000 if loops else 0x40, pos, tracks))
    return d


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("smooth", [0, 1])
@pytest.mark.parametrize("seed", range(6))
def test_prepare_slerps(seed, smooth, cw):
    def setup(e):
        rng = random.Random(seed)
        e.call(0x4B0BA0)
        e.write(SMOOTH, struct.pack("<I", smooth))
        descs = [make_desc(e, rng, 4, rng.random() < 0.5) if rng.random() < 0.8 else 0 for _ in range(4)]
        set_anims(e, descs, 4)
        return (SKEL,)
    check(e, fk, 0x4D6D40, "Anim_PrepareSlerps", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("bones,count", [(0, 2), (3, 0)])
def test_prepare_slerps_empty(bones, count):
    def setup(e):
        rng = random.Random(1)
        e.call(0x4B0BA0)
        set_anims(e, [make_desc(e, rng, 3, True) for _ in range(count)], bones)
        return (SKEL,)
    check(e, fk, 0x4D6D40, "Anim_PrepareSlerps", setup=setup, check_eax=False)


@pytest.mark.parametrize("pos,time", [(True, True), (True, False), (False, True), (False, False)])
def test_get_pos_key(pos, time):
    def setup(e):
        rng = random.Random(5)
        set_anims(e, [0, make_desc(e, rng, 2, True)])
        obj = e.alloc(12)
        e.write(obj, struct.pack("<I", SKEL))
        return (e.alloc(12) if pos else 0, e.alloc(4) if time else 0, obj, 1, 2)
    check(e, fk, 0x4D6FC0, "Anim_GetPosKey", setup=setup, check_eax=False)


def test_get_pos_key_count():
    def setup(e):
        set_anims(e, [make_desc(e, random.Random(2), 2, True, poskeys=7)])
        return (SKEL, 0)
    check(e, fk, 0x4D7020, "Anim_GetPosKeyCount", setup=setup)


@pytest.mark.parametrize("cw", ROUND_CWS)
@pytest.mark.parametrize("length", [1234.5, 1235.5, 0.7, -2.5, -3.7, 99999.25])
def test_get_length_ticks(length, cw):
    def setup(e):
        set_anims(e, [make_desc(e, random.Random(2), 1, True, length=length)])
        return (SKEL, 0)
    check(e, fk, 0x4D7040, "Anim_GetLengthTicks", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("length", [1234.5, 4800.0, 0.3, 77777.7])
def test_get_length_seconds(length, cw):
    def setup(e):
        set_anims(e, [make_desc(e, random.Random(2), 1, True, length=length)])
        return (SKEL, 0)
    check(e, fk, 0x4D74D0, "Anim_GetLengthSeconds", setup=setup, cw=cw, check_eax=False, check_st0=True)


@pytest.mark.parametrize("case", ["null", "nobones", "neg", "bonebig", "nocount", "idbig", "missing", "ok"])
def test_exists(case):
    def setup(e):
        rng = random.Random(3)
        set_anims(e, [] if case == "nocount" else [make_desc(e, rng, 4, True), 0, make_desc(e, rng, 4, True)])
        obj = e.alloc(12)
        e.write(obj, struct.pack("<III", SKEL, 0, 0 if case == "nobones" else 0x1234))
        bone = {"neg": -1, "bonebig": 4}.get(case, 3)
        anim = {"idbig": 3, "missing": 1}.get(case, 2)
        return (0 if case == "null" else obj, bone, anim)
    check(e, fk, 0x4D74F0, "Anim_Exists", setup=setup)


# --- Bone_Rotate -------------------------------------------------------------

def make_bone(e, rng, parent=0):
    b = e.alloc(0x84)
    e.write(b, rbytes(0x84, b))
    e.write(b + 8, f32(*[rng.uniform(-30, 30) for _ in range(3)]))
    e.write(b + 0x18, f32(*[rng.uniform(-300, 300) for _ in range(3)]))
    e.write(b + 0x24, f32(*[rng.uniform(-1, 1) for _ in range(9)]) + struct.pack("<i", 7))
    e.write(b + 0x4C, f32(*quat(rng)) + struct.pack("<i", rng.choice([0, 3, 995, 998, 999, 1000, 1200])))
    e.write(b + 0x60, f32(*quat(rng)) + struct.pack("<i", 0))
    e.write(b + 0x74, struct.pack("<III", parent, 0, 0))
    if parent:
        last = e.u32(parent + 0x78)
        if not last:
            e.write(parent + 0x78, struct.pack("<I", b))
        else:
            while e.u32(last + 0x7C):
                last = e.u32(last + 0x7C)
            e.write(last + 0x7C, struct.pack("<I", b))
    return b


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("depths", [(0, 3, 3), (0, 2, 3), (0, 3, 1), (1, 3, 3), (2, 2, 2), (0, 0, 3)])
@pytest.mark.parametrize("seed", range(5))
def test_bone_rotate(seed, depths, cw):
    def setup(e):
        rng = random.Random(seed)
        root = make_bone(e, rng)
        a = make_bone(e, rng, root)
        b = make_bone(e, rng, root)
        make_bone(e, rng, a)
        make_bone(e, rng, a)
        make_bone(e, rng, b)
        q = e.alloc(0x14)
        e.write(q, f32(*[x * rng.uniform(0.8, 1.2) for x in quat(rng)]) + struct.pack("<i", rng.choice([0, 2, 5])))
        depth, rot, pos = depths
        # the root has no parent, so start below it when positions are wanted
        start = a if depth < pos else root
        return (start, q, depth, rot, pos)
    check(e, fk, 0x4D7070, "Bone_Rotate", setup=setup, check_eax=False, cw=cw)
