"""TerrainLightMap (0x4F8190 - 0x4F90B0) except the Changed handler."""
import random

import pytest

from emu import compare
from test_graphnode_util import CWS, RET, d64, handler, logfake, new_emu, run, set_handler, u16, u32
from test_vehicle_util import init_emu, setup_init

TYPE = 0x55
WORLD_COUNT = 0xDEC
TASKS = 0xBC00A0
LIGHTMAPS = 0xB81B20
VISIBLE, VISIBLE_COUNT = 0xB97B20, 0xB97D24

e = new_emu()
logfake(e, 0x4B47E0, 2)  # Random_Seed
logfake(e, 0x4F1160, 0)
logfake(e, 0x4B22F0, 1)  # Texture_Free
logfake(e, 0x4C7560, 2)  # DynCube_Remove
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free


def new_task(e, a):
    t = e.alloc(0x110)
    u16(e, t + 0x1C, a[1] & 0xFFFF)
    return t


logfake(e, 0x4012A0, 3, masks={1: 0xFFFF}, effect=new_task)  # Task_New
e.write(0x54E0E5, bytes([0x31]))
set_handler(e, 0x31, TYPE, handler(e, "changed", 2))
e.baseline = e.snapshot()


def lightmap(e, rng=None):
    t = e.alloc(0x110)
    rng = rng or random.Random(1)
    e.write(t, bytes(rng.randrange(256) for _ in range(0x110)))
    u16(e, t + 0x1C, TYPE)
    return t


def world(e, count=0):
    w = e.alloc(0xF00)
    u32(e, w + WORLD_COUNT, count)
    return w


def test_create():
    def setup(e):
        u32(e, 0xA44344, 0x4444)
        return (lightmap(e),)
    run(e, 0x4F8190, "TerrainLightMap_Create", setup)


@pytest.mark.parametrize("ret,tex", [(0, 0), (1, 0), (0, 0x1234), (1, 0x1234)])
def test_show(ret, tex):
    def setup(e):
        RET[0x4F1160] = ret
        t = lightmap(e)
        u32(e, t + 0x108, tex)
        u32(e, t + 0xD8, 33)
        u32(e, VISIBLE_COUNT, 5)
        return (t,)
    run(e, 0x4F82B0, "TerrainLightMap_Show", setup)


@pytest.mark.parametrize("visible", [0, 1, 7])
def test_hide(visible):
    def setup(e):
        t = lightmap(e)
        u32(e, t + 0xE0, visible)
        u32(e, VISIBLE_COUNT, 5)
        return (t,)
    run(e, 0x4F8320, "TerrainLightMap_Hide", setup)


# active, child, slot of count 4, texture, lightmap record has data
@pytest.mark.parametrize("active,child,slot,tex,data", [
    (0, 0, 1, 1, 0), (1, 1, 1, 1, 0), (1, 0, 1, 1, 0), (1, 0, 3, 1, 0), (1, 0, 1, 0, 0), (1, 0, 1, 1, 1)])
def test_delete(active, child, slot, tex, data):
    def setup(e):
        w = world(e, 4)
        tasks = [lightmap(e, random.Random(i)) for i in range(4)]
        for i, t in enumerate(tasks):
            u32(e, t + 0x68, w)
            u32(e, t + 0xD8, i)
        u32(e, TASKS, *tasks)
        t = tasks[slot]
        e.write(t + 0xCA, bytes([child]))
        e.write(t + 0xCD, bytes([active]))
        u32(e, t + 0x108, 0x7E7 if tex else 0)
        u32(e, t + 0xDC, 9)
        u32(e, LIGHTMAPS + 9 * 0x2C + 4, data)
        return (t,)
    run(e, 0x4F86E0, "TerrainLightMap_Delete", setup)


def test_get_params():
    def setup(e):
        u32(e, 0xA76D04, 0x5151)
        out = e.alloc(4)
        return lightmap(e), out
    run(e, 0x4F8770, "TerrainLightMap_GetParams", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("shift,size", [(10, 0x40), (0, 8), (23, 0x100), (5, 0x30), (17, 1), (3, 0x7FFFFFFF)])
def test_set_draw_info(shift, size, cw):
    def setup(e):
        t = lightmap(e)
        u32(e, t + 0xB8, shift)
        u32(e, t + 0xD4, size)
        u32(e, t + 0xD8, 0x31)
        d64(e, t + 0x20, 123456.789, -98765.4321, 5.0)
        return (t,)
    run(e, 0x4F8EF0, "TerrainLightMap_SetDrawInfo", setup, cw=cw)


def test_shutdown():
    def setup(e):
        u32(e, 0xA76D04, 0x5151)
        u16(e, 0xA76CDC, 0x66)
    run(e, 0x4F8FB0, "TerrainLightMap_Shutdown", setup)


def test_get_type():
    def setup(e):
        u32(e, 0xA76CDC, 0xABCD0066)
    run(e, 0x4F8FD0, "TerrainLightMap_GetType", setup, check_eax=True, eax_mask=0xFFFF)


@pytest.mark.parametrize("mode,handled", [(0, 1), (1, 1), (0, 0)])
def test_new_child(mode, handled):
    def setup(e):
        e.write(0x9422FD, bytes([mode]))
        u16(e, 0xA76CDC, TYPE if handled else TYPE + 1)
        parent = e.alloc(0x40)
        lm = lightmap(e)
        pos = e.alloc(24)
        d64(e, pos, 1.5, -2.5, 3.25)
        return parent, lm, pos
    run(e, 0x4F8FE0, "TerrainLightMap_NewChild", setup, check_eax=True)


def test_init():
    ie = init_emu()

    def setup(e):
        setup_init(e)
        u16(e, 0x54875C, e.u32(0x548648) & 0xFFFF)
    compare(ie, 0x4F8340, "TerrainLightMap_Init", setup=setup, check_eax=False, normalized=True)
