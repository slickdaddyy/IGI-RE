"""BoneMagicObj (0x531620 - 0x531D30)."""
import random
import struct

import pytest

from test_freecam_util import (CWS, FISTP_CWS, RET, add_child, d64, event, f32, handler, logfake, new_emu, run,
                               run_error, set_events, set_handler, task, u16, u32)

NAN = float("nan")
e = new_emu()
CHILD = 0x52


def root_motion(em, a):
    f32(em, a[1], *RET.get("motion", (0.5, -1.25, 2.0)))


def arg_int(em, a):
    return 1000 + a[1] * 7


for addr, n, kw in [
    (0x401900, 5, dict(strings=(0,), masks={2: 0xFFFF}, ret=0x51)),  # TaskType_New
    (0x401400, 3, dict(masks={0: 0xFFFF, 1: 0xFF})),  # TaskType_SetHandler
    (0x401530, 3, dict(masks={0: 0xFFFF})),  # TaskType_SetHandler2
    (0x401A20, 1, dict(masks={0: 0xFFFF})),  # TaskType_Free
    (0x4B8810, 2, dict(strings=(0,))),  # Script_RegisterConstant
    (0x4B8A20, 2, dict(effect=arg_int)),  # Script_GetArgInt
    (0x4D9610, 0, dict(ret=0x50)),  # BoneDynCubeObj_GetType
    (0x4D9620, 4, dict(masks={2: 0xFFFF})),  # BoneDynCubeObj_Init
    (0x4D96F0, 1, {}),  # BoneDynCubeObj_Deinit
    (0x5124C0, 2, {}),  # BoneDynCubeObj_EnterHandler
    (0x4CEC10, 1, dict(strings=(0,))),  # VirModel_Find
    (0x4C6940, 4, dict(derefs={2: 24})),  # DynCube_Move
    (0x4C7140, 5, dict(derefs={2: 24})),  # DynCube_Add
    (0x4C7560, 2, {}),  # DynCube_Remove
    (0x4D6110, 4, {}),  # Anim_PlayNow
    (0x4D38C0, 2, dict(derefs={0: 40})),  # BoneObj_Update
    (0x4D2FB0, 3, dict(masks={1: 0}, effect=root_motion)),  # BoneObj_GetRootMotion
    (0x4D3210, 2, {}),  # BoneObj_GetEvents
]:
    logfake(e, addr, n, **kw)
set_events(e)
for slot, n, derefs in [(1, 1, None), (event(0x4C56F0), 2, {1: 64}), (event(0x4D9FB0), 2, {1: 8})]:
    set_handler(e, slot, CHILD, handler(e, "child%d" % slot, n, derefs=derefs))
e.baseline = e.snapshot()


def rnd_bytes(em, p, n, rng):
    em.write(p, bytes(rng.randrange(256) for _ in range(n)))


def matrix(rng):
    return [rng.uniform(-1, 1) for _ in range(9)]


def bone(em, rng, children=2):
    o = task(em, 0x130, 0x51)
    rnd_bytes(em, o + 0x20, 0x110, rng)
    d64(em, o + 0x20, *[rng.uniform(-1e3, 1e3) for _ in range(3)])
    f32(em, o + 0x78, *matrix(rng))
    u32(em, o + 0x9C, rng.randrange(0, 2000))
    f32(em, o + 0xA0, rng.uniform(1, 30))
    d64(em, o + 0xD8, *[rng.uniform(-5, 5) for _ in range(3)])
    f32(em, o + 0xF0, *matrix(rng))
    u32(em, o + 0x114, rng.randrange(0, 2000))
    f32(em, o + 0x120, *[rng.uniform(-3, 3) for _ in range(3)])
    kids = [task(em, 0x40, CHILD if i != 1 else 0x53) for i in range(abs(children))]
    for k in kids:
        add_child(em, o, k)
    if children < 0:
        u32(em, kids[-1], 0)  # a broken list: the last child has no next
    return o


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("children", [0, 1, 3, -2])
def test_run(children, cw):
    def setup(em):
        rng = random.Random(children)
        RET["motion"] = (rng.uniform(-2, 2), rng.uniform(-2, 2), rng.uniform(-2, 2))
        set_events(em)
        return (bone(em, rng, children),)
    run(e, 0x531620, "BoneMagicObj_Run", setup, cw=cw)


@pytest.mark.parametrize("children", [0, 1, 3, -2])
def test_enter(children):
    def setup(em):
        set_events(em)
        return bone(em, random.Random(children + 5), children), 0x7777
    run(e, 0x5316F0, "BoneMagicObj_Enter", setup)


def test_delete():
    def setup(em):
        u32(em, 0xA44344, 0x4344)
        return (bone(em, random.Random(3), 0),)
    run(e, 0x5317B0, "BoneMagicObj_Delete", setup)


@pytest.mark.parametrize("orig,ours", [(0x5317D0, "BoneMagicObj_Init"), (0x531D10, "BoneMagicObj_Shutdown")])
def test_type_setup(orig, ours):
    def setup(em):
        set_events(em)
        u32(em, 0x54F848, 0xABCD0051)
    run(e, orig, ours, setup)


def magic_args(em, rng):
    a = em.alloc(0x90)
    rnd_bytes(em, a, 0x90, rng)
    name = em.alloc(16)
    em.write(name, b"crate_bone\0")
    info = em.alloc(8)
    em.write(info, struct.pack("<hhi", rng.randrange(-5, 300), 0x2222, rng.randrange(0, 50)))
    u32(em, a, name, 0x4444)
    d64(em, a + 8, *[rng.uniform(-1e3, 1e3) for _ in range(6)])
    f32(em, a + 0x38, *matrix(rng))
    f32(em, a + 0x60, *matrix(rng))
    u32(em, a + 0x88, 0x8888, info)
    return a


@pytest.mark.parametrize("seed", range(3))
def test_create(seed):
    def setup(em):
        rng = random.Random(seed)
        RET[0x4CEC10] = 0x5678
        return bone(em, rng, 0), magic_args(em, rng)
    run(e, 0x5318D0, "BoneMagicObj_Create", setup)


def test_create_missing():
    def setup(em):
        RET[0x4CEC10] = 0
        rng = random.Random(9)
        return bone(em, rng, 0), magic_args(em, rng)
    run_error(e, 0x5318D0, "BoneMagicObj_Create", setup)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("seed", range(10))
def test_move(seed, cw):
    def setup(em):
        rng = random.Random(seed + 40)
        u32(em, 0xA44344, 0x4344)
        set_events(em)
        o = bone(em, rng, -2 if seed == 8 else seed % 3)
        if seed == 9:
            f32(em, o + 0x120, NAN, 0.0, -0.0)
        pos = em.alloc(24)
        d64(em, pos, *[rng.uniform(-1e4, 1e4) for _ in range(3)])
        m = em.alloc(40)
        f32(em, m, *matrix(rng))
        u32(em, m + 36, rng.randrange(0, 2000))
        a = em.alloc(8)
        u32(em, a, pos, m)
        return o, a
    run(e, 0x5319B0, "BoneMagicObj_Move", setup, cw=cw)


@pytest.mark.parametrize("case", ["self", "owner", "other", "null"])
def test_is_other(case):
    def setup(em):
        o = bone(em, random.Random(1), 0)
        owner = task(em, 0x40, 0x60)
        u32(em, o + 0x118, owner)
        a = em.alloc(8)
        em.write(a, b"\x77\x88\x99\xaa")
        u32(em, a + 4, {"self": o, "owner": owner, "other": task(em, 0x40, 0x61), "null": 0}[case])
        return o, a
    run(e, 0x531CA0, "BoneMagicObj_IsOther", setup, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("index", [0, 3])
def test_read_args(index):
    def setup(em):
        out = em.alloc(8)
        em.write(out, b"\xcc" * 8)
        a = em.alloc(12)
        u32(em, a, out, 0x9999, index)
        return 0x1234, a
    run(e, 0x531CD0, "BoneMagicObj_ReadArgs", setup)
