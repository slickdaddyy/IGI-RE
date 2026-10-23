"""Vehicle_ParseConfig, EditorRigidObj, SimpointObj and SimpointObjContainer (0x5031F0 - 0x503D50)."""
import struct

import pytest

from emu import ErrorShown, compare
from test_editobj_util import (CWS, RET, add_child, cstr, d64, f32, floatfake, handler, init_emu, logfake, mat, obj,
                               rnd, run, set_handler, setup_types, strz, task, u16, u32)

CONT, SIMP, OTHER = 0x64, 0x65, 0x66

emu = init_emu()
logfake(emu, 0x4B8A20, 2, effect=lambda e, a: RET.get("count", 0) if a[1] == RET.get("count_at") else a[1] * 7 - 3)
floatfake(emu, 0x4B8A50, 2, effect=lambda e, a: RET.get(("real", a[1]), a[1] * 1.37 - 0.11))
logfake(emu, 0x4EC4D0, 8)  # AeroPart_Init
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4015F0, 1)  # Task_Delete
logfake(emu, 0x4C5710, 3)  # RigidDynCubeObj_Init
logfake(emu, 0x4C6890, 3)  # DynCube_ListAdd
logfake(emu, 0x4C6840, 3)  # DynCube_ListRemove
logfake(emu, 0x4F0EE0, 0, ret=0x0EE0)  # Level_GetDynamic
logfake(emu, 0x4012A0, 3, masks={1: 0xFFFF}, effect=lambda e, a: RET.get(0x4012A0, 0))  # Task_New
logfake(emu, 0x4CEC10, 1, strings=(0,))  # VirModel_Find
logfake(emu, 0x4C48D0, 2)  # RigidDynCubeObj_SetModel
u32(emu, 0xA44344, 0x4444)
u16(emu, 0xA7A628, CONT)
u16(emu, 0xA7A62A, SIMP)
for t in (SIMP, OTHER):
    set_handler(emu, 1, t, handler(emu, "run_%x" % t, 1))
emu.baseline = emu.snapshot()


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("parts", [0, 1, 3, -1])
def test_parse_config(parts, cw):
    def setup(e):
        RET["count_at"] = 12
        RET["count"] = parts
        RET[("real", 2)] = float("nan")
        RET[("real", 15)] = 1e30
        n = e.alloc(4)
        u32(e, n, 2)
        return e.alloc(0x118), n, 0x4321
    run(emu, 0x5031F0, "Vehicle_ParseConfig", setup, cw=cw)


def test_editor_close():
    run(emu, 0x5034C0, "EditorRigidObj_Close", lambda e: u16(e, 0x54E634, 0x55))


def test_editor_get_type():
    run(emu, 0x5034D0, "EditorRigidObj_GetType", lambda e: u16(e, 0x54E634, 0x55), check_eax=True, eax_mask=0xFFFF)


def simp(e, rng, life=3):
    o = obj(e, 0x100, SIMP, rng)
    f32(e, o + 0xF0, rng.uniform(-100, 100), rng.uniform(-100, 100), rng.uniform(-100, 100))
    u32(e, o + 0xFC, life)
    return o


def container(e, rng, n=0, auto=0, kids=()):
    c = task(e, 0x64, CONT)
    u32(e, c + 0x20, 45)
    strz(e, c + 0x24, b"debris")
    f32(e, c + 0x44, 0.97, 1.0, 44.6, 10.0, 0.3, -0.2, 0.05)
    e.write(c + 0x60, bytes([auto]))
    for i in range(n):
        o = simp(e, rng)
        if i == 1:
            u32(e, o + 0x94, 1000)
        add_child(e, c, o)
    for k in kids:
        add_child(e, c, task(e, 0x40, k))
    return c


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("n", [0, 1, 3])
@pytest.mark.parametrize("auto", [0, 1])
@pytest.mark.parametrize("seed", [1, 2])
def test_container_run(n, auto, seed, cw):
    def setup(e):
        return (container(e, rnd(seed), n, auto),)
    run(emu, 0x5035B0, "SimpointObjContainer_Run", setup, cw=cw)


def test_container_run_other_child():
    def setup(e):
        return (container(e, rnd(3), 2, 0, kids=(OTHER,)),)
    run(emu, 0x5035B0, "SimpointObjContainer_Run", setup)


@pytest.mark.parametrize("orig,ours", [(0x503B60, "SimpointObj_Create"), (0x503B90, "SimpointObj_Delete")])
def test_simpoint_create_delete(orig, ours):
    run(emu, orig, ours, lambda e: (simp(e, rnd(4)),))


@pytest.mark.parametrize("life", [-2, 0, 1, 7])
def test_simpoint_run(life):
    run(emu, 0x503BB0, "SimpointObj_Run", lambda e: (simp(e, rnd(5), life),))


def test_simpoint_close():
    run(emu, 0x503BD0, "SimpointObj_Close", None)


def test_simpoint_get_type():
    run(emu, 0x503BF0, "SimpointObj_GetType", None, check_eax=True, eax_mask=0xFFFF)


@pytest.mark.parametrize("name2", [None, b"", b"smoke", b"fifteen_chars__"])
@pytest.mark.parametrize("model", [b"m", b"debris_long_nam"])
@pytest.mark.parametrize("auto", [0, 1, 0x101])
def test_container_new(name2, model, auto):
    def setup(e):
        c = e.alloc(0x64)
        e.write(c, b"\x77" * 0x64)
        RET[0x4012A0] = c
        spin = e.alloc(12)
        f32(e, spin, 0.5, -0.25, 3.0)
        fl = [struct.unpack("<I", struct.pack("<f", v))[0] for v in (0.98, 1.0, 44.6, 10.0)]
        return [33, cstr(e, model), cstr(e, name2) if name2 is not None else 0] + fl + [spin, auto]
    run(emu, 0x503C00, "SimpointObjContainer_New", setup, check_eax=True)


@pytest.mark.parametrize("found", [0, 0x3333])
def test_container_add(found):
    def setup(e):
        RET[0x4CEC10] = found
        rng = rnd(6)
        c = container(e, rng)
        o = e.alloc(0x100)
        RET[0x4012A0] = o
        pos, m, v = e.alloc(24), e.alloc(40), e.alloc(12)
        d64(e, pos, 1.5, 2.5, 3.5)
        mat(e, m, rng)
        f32(e, v, 7.0, 8.0, 9.0)
        return c, SIMP | 0xABCD0000, pos, m, v
    if found:
        run(emu, 0x503CC0, "SimpointObjContainer_Add", setup, check_eax=True)
    else:
        with pytest.raises(ErrorShown):
            run(emu, 0x503CC0, "SimpointObjContainer_Add", setup)


@pytest.mark.parametrize("v", [0, 1, 0x1FF])
def test_set_auto_delete(v):
    run(emu, 0x503D50, "SimpointObjContainer_SetAutoDelete", lambda e: (container(e, rnd(7)), v))


init = init_emu()


@pytest.mark.parametrize("orig,ours", [(0x5033F0, "EditorRigidObj_Open"), (0x5034E0, "SimpointObj_Open")])
def test_open(orig, ours):
    compare(init, orig, ours, setup=setup_types, check_eax=False, normalized=True)
