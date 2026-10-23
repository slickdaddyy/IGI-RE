"""NoiseQTask (0x51FEF0 - 0x520730)."""
import random

import pytest

from emu import compare
from test_patrol_util import fake
from test_litmap_util import (FISTP_CWS, RET, add_child, cstr, d64, floatfake, handler, init_emu, logfake, new_emu,
                              rnd_task, run, set_handler, setup_init, u16, u32)

TYPE = 0x52
CHILD = 0x53
DAC = 0x25

e = new_emu()
logfake(e, 0x4F1320, 1)  # VarString_Init
logfake(e, 0x4F1340, 1)  # VarString_Free
logfake(e, 0x4F1400, 2)  # VarString_Bind
logfake(e, 0x4F16C0, 1)  # VarString_HasExpr
floatfake(e, 0x4F16E0, 2)  # VarString_EvalReal
logfake(e, 0x4F1050, 1)  # Task_GetId
fake(e, 0x4F1270, 2, skip=(0,), effect=lambda e, a: e.write(a[0], b"noise_1\0"))  # Task_GetName
logfake(e, 0x4B7FC0, 2, strings=(0,))  # Script_RegisterReal
logfake(e, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
logfake(e, 0x4B47C0, 2)  # Random_Int
floatfake(e, 0x4B4770, 1)  # Random_Float
logfake(e, 0x4EE940, 0)  # ScreenGrab_IsActive
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
e.write(0x54E0E5, bytes([0x31]))
u32(e, 0x54DDAC, DAC)
set_handler(e, 0x31, CHILD, handler(e, "changed", 2))
set_handler(e, 1, CHILD, handler(e, "update", 1))
set_handler(e, DAC, TYPE, handler(e, "unregister", 1))
e.baseline = e.snapshot()


def noise(e, rng, children=0):
    n = rnd_task(e, 0x130, TYPE, rng)
    for i in range(children):
        add_child(e, n, rnd_task(e, 0x40, CHILD if i != 1 else CHILD + 1, rng))
    return n


def test_create():
    run(e, 0x520200, "NoiseQTask_Create", lambda e: (noise(e, random.Random(1)),))


@pytest.mark.parametrize("registered", [0, 1, 0x80])
def test_register(registered):
    def setup(e):
        n = noise(e, random.Random(2))
        e.write(n + 0x128, bytes([registered]))
        return (n,)
    run(e, 0x5202F0, "NoiseQTask_Register", setup)


def test_unregister():
    run(e, 0x520350, "NoiseQTask_Unregister", lambda e: (noise(e, random.Random(3)),))


@pytest.mark.parametrize("id_,type_", [(5, TYPE), (0, TYPE), (-1, TYPE), (5, TYPE + 1)])
def test_delete(id_, type_):
    def setup(e):
        RET[0x4F1050] = id_
        n = noise(e, random.Random(4))
        u16(e, n + 0x1C, type_)
        return (n,)
    run(e, 0x5203A0, "NoiseQTask_Delete", setup)


def test_get_params():
    def setup(e):
        u32(e, 0xA8383C, 0x5151)
        return noise(e, random.Random(5)), e.alloc(4)
    run(e, 0x5203F0, "NoiseQTask_GetParams", setup)


@pytest.mark.parametrize("children,arg", [(0, None), (3, None), (3, b""), (3, b"x")])
@pytest.mark.parametrize("lo,hi", [(1.0, 2.0), (2.0, 1.0), (1.0, 1.0)])
def test_changed(children, arg, lo, hi):
    def setup(e):
        n = noise(e, random.Random(6), children)
        d64(e, n + 0xA8, lo, hi)
        d64(e, n + 0x90, 0.25)
        return n, cstr(e, arg) if arg is not None else 0
    run(e, 0x520630, "NoiseQTask_Changed", setup)


def update_case(seed):
    rng = random.Random(seed)

    def r():
        return rng.choice([0.0, 1.0, -1.0, 0.5, rng.uniform(-10, 10), rng.uniform(-1e3, 1e3), 1 / 3.0])
    c = dict(expr=rng.randrange(2), eval=r(), base=r(), current=r(), rise=abs(r()), fall=abs(r()),
             time=rng.choice([-2, 0, 0, 1, 5]), tmin=rng.randrange(5), tmax=rng.randrange(5),
             nmin=r(), nmax=r(), rint=rng.randrange(9), rfloat=rng.random(), grab=rng.randrange(2),
             step=rng.choice([0.0, 1.0, 0.25, 0.1, 3.0, float("nan"), r()]), min=r(), max=r(),
             children=rng.choice([0, 3]))
    if seed % 5 == 0:
        c["base"] = c["current"]
    return c


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("seed", range(40))
def test_update(seed, cw):
    c = update_case(seed)

    def setup(e):
        RET[0x4F16C0] = c["expr"]
        RET[0x4F16E0] = c["eval"]
        RET[0x4B47C0] = c["rint"]
        RET[0x4B4770] = c["rfloat"]
        RET[0x4EE940] = c["grab"]
        n = noise(e, random.Random(seed), c["children"])
        d64(e, n + 0x70, c["current"])
        u32(e, n + 0x80, c["time"])
        d64(e, n + 0x88, c["base"], 0.0, c["rise"], c["fall"], c["nmin"], c["nmax"])
        u32(e, n + 0xB8, c["tmin"], c["tmax"])
        d64(e, n + 0xC0, c["min"], c["max"], c["step"])
        return (n,)
    run(e, 0x520400, "NoiseQTask_Update", setup, cw=cw)


def test_shutdown():
    def setup(e):
        u32(e, 0xA8383C, 0x1234)
        u16(e, 0xA83838, 0x77)
    run(e, 0x520710, "NoiseQTask_Shutdown", setup)


def test_init():
    ie = init_emu()
    compare(ie, 0x51FEF0, "NoiseQTask_Init", setup=setup_init, check_eax=False, normalized=True)
