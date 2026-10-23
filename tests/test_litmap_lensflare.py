"""Lensflare and LensflareItem (0x520BB0 - 0x521040)."""
import random

import pytest

from emu import compare
from test_litmap_util import RET, f32, init_emu, logfake, new_emu, rnd_task, run, setup_init, u16, u32

TYPE = 0x52
DIRLIGHT = 0x61

e = new_emu()
logfake(e, 0x4C6890, 3)  # DynCube_ListAdd
logfake(e, 0x4C6840, 3)  # DynCube_ListRemove
logfake(e, 0x4F1030, 1)  # Task_FindById
logfake(e, 0x4F1A40, 1)  # ParamType_Free
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
u16(e, 0xA83D34, DIRLIGHT)
e.baseline = e.snapshot()


def test_item_create():
    def setup(e):
        u32(e, 0xA44344, 0x4444)
        return (rnd_task(e, 0x40, TYPE, random.Random(1)),)
    run(e, 0x520E40, "LensflareItem_Create", setup)


def test_item_get_params():
    def setup(e):
        u32(e, 0xA83970, 0x5151)
        return rnd_task(e, 0x40, TYPE, random.Random(2)), e.alloc(4)
    run(e, 0x520E70, "LensflareItem_GetParams", setup)


@pytest.mark.parametrize("index", [-5, -1, 0, 3, 9, 10, 100])
@pytest.mark.parametrize("used", [0, 1])
def test_item_changed(index, used):
    def setup(e):
        u32(e, 0xBA1E20, *[used if i % 2 == 0 else 1 - used for i in range(10)])
        it = rnd_task(e, 0x40, TYPE, random.Random(3))
        u32(e, it + 0x3C, index)
        return (it,)
    run(e, 0x520E80, "LensflareItem_Changed", setup)


def test_create():
    def setup(e):
        u32(e, 0xA44344, 0x4444)
        return (rnd_task(e, 0x98, TYPE, random.Random(4)),)
    run(e, 0x520EC0, "Lensflare_Create", setup)


def test_delete():
    run(e, 0x520F20, "Lensflare_Delete", lambda e: (rnd_task(e, 0x98, TYPE, random.Random(5)),))


def test_get_params():
    def setup(e):
        u32(e, 0xA8389C, 0x5151)
        return rnd_task(e, 0x98, TYPE, random.Random(6)), e.alloc(4)
    run(e, 0x520F40, "Lensflare_GetParams", setup)


# light id, what Task_FindById / the parent is
@pytest.mark.parametrize("id_", [-1, 7])
@pytest.mark.parametrize("light", [None, DIRLIGHT, DIRLIGHT + 1])
def test_changed(id_, light):
    def setup(e):
        rng = random.Random(7)
        lf = rnd_task(e, 0x98, TYPE, rng)
        u32(e, lf + 0x84, id_)
        d = 0
        if light is not None:
            d = rnd_task(e, 0x40, light, rng)
            f32(e, d + 0x24, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6)
        u32(e, lf + 0x14, d if id_ == -1 else 0x5555)
        RET[0x4F1030] = d
        return (lf,)
    run(e, 0x520F50, "Lensflare_Changed", setup)


def test_shutdown():
    def setup(e):
        u32(e, 0xA83898, 0x11)
        u32(e, 0xA83970, 0x1234)
        u32(e, 0xA8389C, 0x5678)
        u16(e, 0xA83968, 0x77)
        u16(e, 0xA83894, 0x78)
    run(e, 0x520FF0, "Lensflare_Shutdown", setup)


def test_get_type():
    def setup(e):
        u32(e, 0xA83894, 0xABCD0066)
    run(e, 0x521030, "Lensflare_GetType", setup, check_eax=True, eax_mask=0xFFFF)


def test_init():
    ie = init_emu()
    compare(ie, 0x520BB0, "Lensflare_Init", setup=setup_init, check_eax=False, normalized=True)
