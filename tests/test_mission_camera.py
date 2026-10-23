"""The end of HumanCamera (0x4849B0 - 0x484E00)."""
import random

import pytest

from emu import ErrorShown
from test_mission_util import RET, d64, make_emu, run, task, u16, u32

LIST = 0x5C1188

emu = make_emu()


def human(e, pos, killer_pos=None):
    h = task(e, 0xE00, 0x40)
    d64(e, h + 0x20, *pos)
    if killer_pos is not None:
        k = task(e, 0x40, 0x41)
        d64(e, k + 0x20, *killer_pos)
        ref = e.alloc(4)
        u32(e, ref, k)
        u32(e, h + 0xDE8, ref)
    return h


def camera(e, h):
    c = task(e, 0x128, 0x42)
    ref = e.alloc(4)
    u32(e, ref, h)
    u32(e, c + 0x68, ref)
    e.write(c + 0x38, b"\x77" * 0x28)
    return c


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_watch_killer(seed, cw):
    rng = random.Random(seed)
    pos = tuple(rng.uniform(-50000, 50000) for _ in range(3))
    kpos = tuple(rng.uniform(-50000, 50000) for _ in range(3))
    if seed == 0:
        kpos = (pos[0], pos[1], pos[2] - 4096.0)  # killer at the same place: no collision line
    hit = (seed % 3, tuple(rng.uniform(-1000, 1000) for _ in range(3)))

    def setup(e):
        RET["coll"] = hit
        return (camera(e, human(e, pos, kpos)),)
    run(emu, 0x4849B0, "HumanCamera_WatchKiller", setup, cw=cw)


def test_delete_handler():
    run(emu, 0x484CE0, "HumanCamera_DeleteHandler", lambda e: (camera(e, 0),))


def fill_list(e, used, humans):
    """used: indices of used entries; humans: {index: task or 0}."""
    e.write(LIST, b"\xAB" * 0x400)
    for i in range(128):
        e.write(LIST + 8 * i + 5, bytes([1 if i in used else 0]))
        if i in used:
            ref = e.alloc(4)
            u32(e, ref, humans.get(i, 0x7000 + i))
            u32(e, LIST + 8 * i, ref)


@pytest.mark.parametrize("used", [(), (5,), (127,)])
def test_close(used):
    def setup(e):
        u16(e, 0x540990, 0x1234)
        e.write(0x540992, b"\x2A")
        fill_list(e, used, {})
    if used:
        for fn in (0x484CF0, "HumanCamera_Close"):
            emu.reset()
            setup(emu)
            with pytest.raises(ErrorShown, match="humanlist not empty"):
                emu.call(fn)
    else:
        run(emu, 0x484CF0, "HumanCamera_Close", setup)


def test_get_event():
    run(emu, 0x484D30, "HumanCamera_GetEvent", lambda e: e.write(0x540992, b"\x5A"), check_eax=True, eax_mask=0xFF)


def test_new():
    def setup(e):
        u16(e, 0x540990, 0x66)
        return 0x4321, 0xCAFE
    run(emu, 0x484D40, "HumanCamera_New", setup, check_eax=True)


@pytest.mark.parametrize("used,humans,target", [
    ((0, 3, 7), {3: 0x5000}, 0x5000),
    ((0, 3, 127), {3: 0x5001, 127: 0x5000}, 0x5000),
    ((2,), {2: 0x5000}, 0x5000),
])
def test_set_human(used, humans, target):
    def setup(e):
        fill_list(e, used, humans)
        return camera(e, 0), target
    run(emu, 0x484D60, "HumanCamera_SetHuman", setup)


def test_set_human_missing():
    for fn in (0x484D60, "HumanCamera_SetHuman"):
        emu.reset()
        fill_list(emu, (1, 4), {})
        with pytest.raises(ErrorShown, match="ERROR IN HUMANCAMERA HUMANLIST"):
            emu.call(fn, camera(emu, 0), 0x5000)


@pytest.mark.parametrize("used", [(), (0, 1, 2), tuple(range(127)), tuple(range(128))])
def test_add_human(used):
    def setup(e):
        fill_list(e, used, {})
        return (0x5000,)
    run(emu, 0x484DC0, "HumanCamera_AddHuman", setup)


@pytest.mark.parametrize("used,humans", [((), {}), ((1, 5), {5: 0x5000}), ((1, 5), {}), ((127,), {127: 0x5000})])
def test_remove_human(used, humans):
    def setup(e):
        fill_list(e, used, humans)
        return (0x5000,)
    run(emu, 0x484E00, "HumanCamera_RemoveHuman", setup)


def test_remove_human_dead_entry():
    for fn in (0x484E00, "HumanCamera_RemoveHuman"):
        emu.reset()
        fill_list(emu, (2,), {2: 0})
        with pytest.raises(ErrorShown, match="ERROR IN HUMANCAMERA HUMANLIST"):
            emu.call(fn, 0x5000)
