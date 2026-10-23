"""LevelFlow."""
import struct

import pytest

from test_levelflow_util import (RET, check_init, f32, handler, kids, logfake, make_emu, make_init_emu, run,
                                 set_handler, task, u32)

LF = 0xBC2360
KEYS = 0xC28C62

emu = make_emu()
logfake(emu, 0x416D90, 0)  # Game_ResetState
logfake(emu, 0x416F40, 0)  # Game_ClearPlayers
logfake(emu, 0x406220, 0, effect=lambda e, a: RET.get("profile", 0))  # IsPlayerProfileActive
logfake(emu, 0x416D40, 0, effect=lambda e, a: RET["game"])  # Game_Get
logfake(emu, 0x418B00, 5, strings=(1,), ret=0x7700)  # MenuManager_New
logfake(emu, 0x418D70, 1)  # MenuManager_Free
logfake(emu, 0x48F2D0, 0, effect=lambda e, a: RET.get("usa", 0))  # AppContext_IsUSA
logfake(emu, 0x4F1160, 0, effect=lambda e, a: RET.get("running", 1))
logfake(emu, 0x402870, 0, effect=lambda e, a: RET.get("freq", 30))  # Timer_GetFrequency
RUN = handler(emu, "run", 1)
DAC = handler(emu, "dac", 1)
emu.baseline = emu.snapshot()
init = make_init_emu()


def test_init():
    check_init(init, 0x488BB0, "LevelFlow_Init")


def flow(e, registered=0, complete=0, failed=0, tick=100, start=6.5, limit=60.0, set_global=True):
    t = task(e, 0x128, 0x55)
    e.write(t + 0x20, bytes((i * 7 + 3) & 0xFF for i in range(0x108)))
    e.write(t + 0x60, bytes([complete, failed]))
    u32(e, t + 0x68, tick)
    f32(e, t + 0x6C, start)
    f32(e, t + 0x74, limit)
    e.write(t + 0x78, bytes([registered]))
    if set_global:
        u32(e, LF, t)
    return t


def script_value(fn, setup_globals):
    ign = []

    def setup(e):
        setup_globals(e)
        ret = e.alloc(24)
        ign[:] = [(ret + 4, ret + 8), (ret + 20, ret + 24)]
        return ret, 0, 0
    return setup, ign


@pytest.mark.parametrize("key", [0, 0x80, 0x81, 0x7F, 0xFF])
def test_get_break_cut_scene_key(key):
    setup, ign = script_value(None, lambda e: e.write(KEYS + 1, bytes([key])))
    run(emu, 0x488E00, "LevelFlow_GetBreakCutSceneKey", setup, ignore=ign)


@pytest.mark.parametrize("failed", [0, 1, 0x80, 0xFF])
def test_level_failed(failed):
    setup, ign = script_value(None, lambda e: flow(e, failed=failed))
    run(emu, 0x488E50, "LevelFlow_LevelFailed", setup, ignore=ign)


@pytest.mark.parametrize("usa", [0, 1])
def test_is_country_usa(usa):
    setup, ign = script_value(None, lambda e: RET.update(usa=usa))
    run(emu, 0x488E90, "LevelFlow_IsCountryUSA", setup, ignore=ign)


@pytest.mark.parametrize("profile", [0, 2])
def test_create(profile):
    def setup(e):
        game = e.alloc(0x50)
        u32(e, game + 0x44, 0x7800)
        RET["game"] = game
        if profile:
            p = e.alloc(4)
            u32(e, p, profile)
            RET["profile"] = p
        return (flow(e, set_global=False),)
    run(emu, 0x488ED0, "LevelFlow_Create", setup)


@pytest.mark.parametrize("registered", [0, 1])
def test_activate(registered):
    run(emu, 0x488F90, "LevelFlow_Activate", lambda e: (flow(e, registered=registered),))


@pytest.mark.parametrize("registered", [0, 1])
def test_deactivate(registered):
    run(emu, 0x4890F0, "LevelFlow_Deactivate", lambda e: (flow(e, registered=registered),))


@pytest.mark.parametrize("id_", [-1, 0, 3])
@pytest.mark.parametrize("handled", [0, 1])
def test_delete(id_, handled):
    def setup(e):
        RET["id"] = id_
        set_handler(e, 0x37, 0x55, DAC if handled else 0)
        t = flow(e)
        u32(e, t + 0x120, 0x7700)
        return (t,)
    run(emu, 0x489200, "LevelFlow_Delete", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x5C8AA4, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x489280, "LevelFlow_GetParams", setup)


@pytest.mark.parametrize("running", [0, 1])
@pytest.mark.parametrize("has,evals", [((0, 0), ()), ((1, 0), (1.0,)), ((0, 1), (1.0,)), ((1, 1), (1.0, 1.0)),
                                       ((1, 1), (0.6, 0.0)), ((1, 1), (257.0, 0.2))])
@pytest.mark.parametrize("n", [0, 3])
@pytest.mark.parametrize("failed", [0, 1])
def test_run(running, has, evals, n, failed):
    def setup(e):
        RET.update(running=running, has=list(has), evals=list(evals))
        e.write(KEYS, bytes((i * 37 + 11) & 0xFF for i in range(16)))
        t = flow(e, complete=1, failed=failed)
        kids(e, t, n, 1, RUN)
        return (t,)
    run(emu, 0x489290, "LevelFlow_Run", setup)


def test_changed():
    run(emu, 0x489400, "LevelFlow_Changed", lambda e: (flow(e),))


def test_close():
    def setup(e):
        u32(e, 0x5C8AA4, 0xABCD0010)
        u32(e, 0x5C8AB8, 0xABCD0011)
    run(emu, 0x489430, "LevelFlow_Close", setup)


@pytest.mark.parametrize("complete", [0, 1, 0x80])
def test_is_complete(complete):
    run(emu, 0x489450, "LevelFlow_IsComplete", lambda e: flow(e, complete=complete) and None, check_eax=True,
        eax_mask=0xFF)


@pytest.mark.parametrize("failed", [None, 0, 1, 0x80])
def test_is_failed(failed):
    def setup(e):
        if failed is None:
            u32(e, LF, 0)
        else:
            flow(e, failed=failed)
    run(emu, 0x489460, "LevelFlow_IsFailed", setup, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("tick,freq,start", [(0, 30, 0.0), (100, 30, 6.5), (123457, 30, 23.9), (7, 60, 0.1),
                                             (-50, 25, 100.0), (999999, 33, 12.3)])
def test_get_hour(cw, tick, freq, start):
    def setup(e):
        RET["freq"] = freq
        flow(e, tick=tick, start=start)
    run(emu, 0x489470, "LevelFlow_GetHour", setup, cw=cw, check_st0=True)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("tick,limit", [(0, 0.0), (100, 60.0), (1800, 60.0), (1801, 60.0), (5, 0.1), (5, 0.15),
                                        (3, 0.05), (0, 0.0166667), (12345, 1234.567), (-10, -1.0)])
def test_get_ticks_left(cw, tick, limit):
    run(emu, 0x4894B0, "LevelFlow_GetTicksLeft", lambda e: flow(e, tick=tick, limit=limit) and None, cw=cw,
        check_eax=True)


def test_script_value_layout():
    """The script functions really fill i, d and the number type."""
    for target in (0x488E00, "LevelFlow_GetBreakCutSceneKey"):
        emu.reset()
        emu.write(KEYS + 1, b"\x80")
        ret = emu.alloc(24)
        emu.call(target, ret, 0, 0)
        assert struct.unpack("<i", emu.read(ret, 4))[0] == 0x80
        assert struct.unpack("<d", emu.read(ret + 8, 8))[0] == 128.0
        assert emu.u32(ret + 16) == 0x567C74
