"""AnimObj_Control (0x52A230): AnimRigidDynCubeObj driven by its AnimTask channels."""
import random

import pytest

from emu import Emu
from test_animcam_util import FISTP_CWS, NAN, RET, d64, f32, find_by_id_fake, floatfake, logfake, run, task, u32
from test_menu_util import pyfake

AOBJ, OTHER = 0x81, 0x83
TASKS = {}
ISA = {}

e = Emu()
floatfake(e, 0x4D0950, 1)  # VirModel_GetRadius
logfake(e, 0x4C6940, 4, derefs={2: 24})  # DynCube_Move
find_by_id_fake(e, TASKS)


def is_a(em):
    em.ret(ISA.get(em.arg(0) & 0xFFFF, 0))


pyfake(e, 0x401CF0, is_a)  # TaskType_IsA
e.baseline = e.snapshot()


def matrix(rng):
    return [rng.uniform(-1, 1) for _ in range(9)]


def anim_obj(em, rng, scale):
    o = task(em, 0x158, AOBJ)
    em.write(o + 0x20, bytes(rng.randrange(256) for _ in range(0x138)))
    d64(em, o + 0x20, *[rng.uniform(-500, 500) for _ in range(3)])
    u32(em, o + 0x68, 0x6868, 0x6C6C)
    f32(em, o + 0x70, *matrix(rng))
    u32(em, o + 0x94, rng.randrange(0, 2000))
    f32(em, o + 0x98, *scale)
    d64(em, o + 0xF0, *[rng.uniform(-500, 500) for _ in range(3)])
    f32(em, o + 0x108, *matrix(rng))
    u32(em, o + 0x12C, rng.randrange(0, 2000))
    f32(em, o + 0x130, *matrix(rng))
    u32(em, o + 0x154, rng.randrange(0, 2000))
    return o


def parent(em, rng, typ, off):
    t = task(em, 0x100, typ)
    d64(em, t + 0x20, *[rng.uniform(-500, 500) for _ in range(3)])
    f32(em, t + off, *matrix(rng))
    u32(em, t + off + 0x24, rng.randrange(0, 2000))
    return t


# parent id key -> which task Task_FindById returns
PARENTS = [(-3.0, None), (0.0, None), (0.4, None), (1.0, "rigid"), (1.5, "other"), (2.0, "other"),
           (2.5, "other"), (3.0, "missing"), (NAN, None)]


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("pk", range(len(PARENTS)))
@pytest.mark.parametrize("seed", range(4))
def test_obj_control(seed, pk, mode, cw):
    key, kind = PARENTS[pk]

    def setup(em):
        rng = random.Random(seed * 101 + pk * 7 + mode)
        RET[0x4D0950] = rng.choice([1.0, 12.345678901, 0.25])
        scale = rng.choice([(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 1.5, 2.0), (NAN, 1.0, 2.0)])
        o = anim_obj(em, rng, scale)
        TASKS.clear()
        ISA.clear()
        rigid = parent(em, rng, 0x90, 0x70)
        other = parent(em, rng, OTHER, 0x78)
        ISA[0x90] = 1
        if kind == "rigid":
            TASKS[1] = rigid
        elif kind == "other":
            # the original rounds the key with fistp, so 1.5 and 2.5 go to 2
            TASKS[2] = other
        keys = em.alloc(22 * 4)
        vals = [rng.uniform(-300, 300) for _ in range(21)] + [key]
        if seed == 3:
            vals[rng.randrange(21)] = rng.choice([0.0, -0.0, 1e-30, 4096.0])
        f32(em, keys, *vals)
        msg = em.alloc(8)
        u32(em, msg, mode, keys)
        return o, msg
    run(e, 0x52A230, "AnimObj_Control", setup, cw=cw)
