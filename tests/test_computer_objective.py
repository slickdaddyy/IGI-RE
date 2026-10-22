"""DefineComputerLimit and DefineComputerObjective (0x466F10 - 0x467590)."""
import random

import pytest

from emu import compare
from test_computer_util import RET, floatfake, logfake, make_emu, run, task, u32

emu = make_emu()
logfake(emu, 0x4F1320, 1)  # VarString_Init
logfake(emu, 0x4F1340, 1)  # VarString_Free
logfake(emu, 0x4F1400, 2)  # VarString_Bind
logfake(emu, 0x4F16C0, 1, effect=lambda e, a: RET.get(("has", a[0]), 0))
floatfake(emu, 0x4F16E0, 2, effect=lambda e, a: RET.get(("val", a[0]), 0.0))
emu.baseline = emu.snapshot()

LIMIT, CUR_LIMIT = 0x5BDC5C, 0x5BDC68
OBJ_PARAMS, CUR_OBJ = 0x5BDC6C, 0x5BDC78


def noisy(e, size, type_=0x50):
    t = task(e, size, type_)
    e.write(t + 0x20, bytes((i * 13 + 5) & 0xFF for i in range(size - 0x20)))
    return t


# --- DefineComputerLimit -------------------------------------------------------

def test_limit_create():
    run(emu, 0x466F10, "ComputerLimit_Create", lambda e: (noisy(e, 0x40),))


def test_limit_delete():
    run(emu, 0x466F20, "ComputerLimit_Delete", lambda e: u32(e, CUR_LIMIT, 0x1234))


def test_limit_get_params():
    def setup(e):
        u32(e, LIMIT, 0x5C5C)
        return 0, e.alloc(4)
    run(emu, 0x466F30, "ComputerLimit_GetParams", setup)


@pytest.mark.parametrize("cur", [0, 0x20001000])
def test_limit_get(cur):
    compare(emu, 0x466F60, "ComputerLimit_Get", setup=lambda e: u32(e, CUR_LIMIT, cur))


# --- DefineComputerObjective ---------------------------------------------------

def test_obj_create():
    run(emu, 0x4673B0, "ComputerObjective_Create", lambda e: (noisy(e, 0x538),))


VALUES = [1.0, 0.0, 255.0, 256.0, 0.5, 1.5, -3.0, 2.5, 4294967297.0]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(10))
def test_obj_run(seed, cw):
    from test_computer_util import with_cw

    def setup(e):
        rng = random.Random(seed)
        c = noisy(e, 0x538)
        u32(e, CUR_OBJ, 0x7777)
        RET[("has", c + 0x20)] = rng.choice([0, 1, 1])
        RET[("val", c + 0x20)] = rng.choice(VALUES)
        for i in range(6):
            for off in (0x94, 0xE4):
                v = c + off + 0xCC * i
                RET[("has", v)] = rng.choice([0, 1, 0x100])
                RET[("val", v)] = rng.choice(VALUES)
        return (c,)
    run(emu, with_cw(emu, 0x467410, cw), with_cw(emu, "ComputerObjective_Run", cw), setup)


def test_obj_delete():
    def setup(e):
        u32(e, CUR_OBJ, 0x7777)
        return (noisy(e, 0x538),)
    run(emu, 0x4674D0, "ComputerObjective_Delete", setup)


def test_obj_get_params():
    def setup(e):
        u32(e, OBJ_PARAMS, 0x6C6C)
        return 0, e.alloc(4)
    run(emu, 0x467520, "ComputerObjective_GetParams", setup)


def test_obj_changed():
    run(emu, 0x467530, "ComputerObjective_Changed", lambda e: (noisy(e, 0x538),))


@pytest.mark.parametrize("cur,i", [(0, 0), (0x20001000, 0), (0x20001000, 3), (0x20001000, -1)])
def test_obj_get(cur, i):
    compare(emu, 0x467590, "ComputerObjective_Get", args=(i,), setup=lambda e: u32(e, CUR_OBJ, cur))
