import random

import pytest

from emu import Emu, compare
from test_comprun_util import RET, check_init, d64, f32, floatfake, init_emu, logfake, obj, run, task, u32, vfake

CUR = 0x5BE2CC
MAP = 0x53FD98

emu = Emu()
GAME = emu.alloc(0x100)
u32(emu, GAME + 0x44, 0x7100)
logfake(emu, 0x416D40, 0, ret=GAME)  # Game_Get
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
logfake(emu, 0x4CEC10, 1, strings=(0,), ret=0x7600)  # VirModel_Find
for a, n in ((0x4C5710, 3), (0x4C6890, 3), (0x4C6840, 3), (0x4D9950, 1), (0x401BB0, 2), (0x4E8370, 2),
             (0x4E8220, 3), (0x4E8250, 2), (0x4E8270, 2), (0x499FD0, 1), (0x46AB40, 0), (0x46AB50, 1),
             (0x46AFB0, 1), (0x4015F0, 1)):
    logfake(emu, a, n)
floatfake(emu, 0x489470, 0)
logfake(emu, 0x4E75A0, 0, ret=0x55)
logfake(emu, 0x4D9870, 10, derefs={1: 24, 2: 40})  # Camera_Set
logfake(emu, 0x4E8110, 12, ret=0x7200)  # View_New
logfake(emu, 0x485320, 1, ret=0x7300)
logfake(emu, 0x499FE0, 0, ret=0x77)
logfake(emu, 0x46B030, 0)  # ComputerMenu_Get
logfake(emu, 0x4012A0, 3, ret=0x7400, masks={1: 0xFFFF})  # Task_New
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free


def eye_fake(e, args):
    d64(e, args[0], *RET["eye"])


def matrix_fake(e, args):
    f32(e, args[0], *RET["look"])
    u32(e, args[0] + 36, 41)


vfake(emu, 0x465800, 2, effect=eye_fake)
vfake(emu, 0x465880, 2, effect=matrix_fake)
emu.baseline = emu.snapshot()

init = init_emu()


def computer(e, view=0):
    owner = task(e, 0x500)
    u32(e, owner + 0x4EC, 0x6600)
    t = obj(e, 0x198)
    u32(e, t + 0x14, owner)
    u32(e, t + 0x130, view)
    return t


def test_init():
    check_init(init, 0x46BA60, "Computer_Init")


@pytest.mark.parametrize("map_on", [0, 1])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_create(map_on, cw):
    def setup(e):
        RET[0x489470] = 0.75
        u32(e, MAP, map_on)
        return (computer(e),)
    run(emu, 0x46BB10, "Computer_Create", setup, cw=cw)


@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_run(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        RET["eye"] = [rng.uniform(-1e5, 1e5) for _ in range(3)]
        RET["look"] = [rng.uniform(-1, 1) for _ in range(9)]
        return (computer(e),)
    run(emu, 0x46BD50, "Computer_RunHandler", setup, cw=cw)


@pytest.mark.parametrize("menu", [0, 0x7500])
@pytest.mark.parametrize("view", [0, 0x7600])
def test_destroy(menu, view):
    def setup(e):
        RET[0x46B030] = menu
        u32(e, 0x53FD9C, 0x1234)
        t = computer(e, view)
        u32(e, CUR, t)
        return (t,)
    run(emu, 0x46BF80, "Computer_Destroy", setup)


def test_close():
    run(emu, 0x46C010, "Computer_Close", lambda e: u32(e, 0x5BE2C4, 0xAB0042))


def test_new():
    def setup(e):
        u32(e, 0x5BE2C4, 0xAB0042)
        return (0x6000,)
    run(emu, 0x46C020, "Computer_New", setup, check_eax=True)


def test_delete():
    run(emu, 0x46C050, "Computer_Delete", lambda e: u32(e, CUR, 0x6100))


@pytest.mark.parametrize("cur", [0, 1])
@pytest.mark.parametrize("addr,name", [(0x46C060, "Computer_Get"), (0x46C070, "Computer_GetView"),
                                       (0x46C090, "Computer_GetCamera"), (0x46C0B0, "Computer_IsMapEnabled")])
def test_getters(cur, addr, name):
    def setup(e):
        t = 0
        if cur:
            t = computer(e, 0x7700)
        u32(e, CUR, t)
        u32(e, MAP, 0x31)
    compare(emu, addr, name, setup=setup)


def test_set_map_enabled():
    compare(emu, 0x46C0A0, "Computer_SetMapEnabled", args=(0x2A,), check_eax=False)
