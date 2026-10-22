import pytest

from test_plane_util import (LOG, RET, VEHICLE, handler, logfake, plane_emu, run, set_handler, task, u32,  # noqa: F401
                             vfake)

HELI = 0x75
emu = plane_emu()
set_handler(emu, 0x26, VEHICLE, handler(emu, "vehicle_remove", 2))
logfake(emu, 0x4B8A20, 2, effect=lambda e, a: RET["ints"][a[1]] if a[1] < len(RET["ints"]) else 0x55)


def parse_vehicle(e, a):
    LOG[-1].append(("vehicle_parse", e.normalize(a[0]), e.u32(a[1]), a[2]))
    u32(e, a[1], e.u32(a[1]) + 3)


vfake(emu, 0x5031F0, 3, show=lambda e, a: (), effect=parse_vehicle)
emu.baseline = emu.snapshot()


def heli(e, sound=None):
    h = task(e, 0x2D00, HELI)
    if sound is not None:
        ref = e.alloc(4)
        u32(e, ref, sound)
        u32(e, h + 0x74C, ref)
    return h


def state(e):
    s = e.alloc(0x80)
    e.write(s, bytes(range(1, 0x81)))
    m = e.alloc(12)
    u32(e, m, s, e.alloc(24), 0)
    return m


def test_load_state():
    run(emu, 0x432F20, "Heli_LoadState", lambda e: (heli(e), state(e)))


def test_save_state():
    run(emu, 0x432F40, "Heli_SaveState", lambda e: (heli(e), state(e)))


def test_config_size():
    run(emu, 0x432F80, "Heli_GetConfigSize", lambda e: (0, e.alloc(4)))


@pytest.mark.parametrize("sound", [None, 0, 0x6000])
def test_remove_from_world(sound):
    run(emu, 0x432F90, "Heli_RemoveFromWorld", lambda e: (heli(e, sound), 0x4321))


def test_sound_info():
    run(emu, 0x433000, "Heli_GetSoundInfo", lambda e: (heli(e), e.alloc(4)))


def test_close():
    def setup(e):
        u32(e, 0x57BD40, 0x4455)
        u32(e, 0x53BE58, 0x12345678)
    run(emu, 0x433010, "Heli_Close", setup)


LISTS = [[0, 0], [1, 7, 2, 8, 9], [3, 1, 2, 3, 2, 4, 5], [5, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6], [0, 10] + list(range(10))]


@pytest.mark.parametrize("ints", LISTS)
def test_parse_config(ints):
    def setup(e):
        RET["ints"] = [0x99] * 4 + ints
        cfg = e.alloc(0x154)
        e.write(cfg, b"\xab" * 0x154)
        n = e.alloc(4)
        u32(e, n, 1)
        return cfg, n, 0x7777
    run(emu, 0x433030, "Heli_ParseConfig", setup)


@pytest.mark.parametrize("ints", LISTS[:3])
def test_parse_args(ints):
    def setup(e):
        RET["ints"] = [0x99] * 4 + ints
        cfg = e.alloc(0x154)
        m = e.alloc(12)
        u32(e, m, cfg, 0x7777, 1)
        return 0, m
    run(emu, 0x432F60, "Heli_ParseArgs", setup)
