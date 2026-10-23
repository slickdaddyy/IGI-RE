"""AnimCam (0x5284A0 - 0x52A210) except the Aim mode (test_animcam_aim.py)."""
import random
import struct

import pytest

from emu import Emu
from test_animcam_util import (CWS, FISTP_CWS, KEYS, KEYS_PREV, NAN, NO_KEY, SNAN, bits, compare_open, d64, f32,
                               find_by_id_fake, fval, info_handler, logfake, open_emu, printf_fake, run, set_handler,
                               task, u16, u32)

CAM, OBJ, PLAIN = 0x71, 0x72, 0x73
TASKS = {}
INFOS = {}

e = Emu()
find_by_id_fake(e, TASKS)
set_handler(e, 7, OBJ, info_handler(e, INFOS))
logfake(e, 0x4D9870, 10, derefs={1: 24, 2: 40})  # Camera_Set
logfake(e, 0x4E75A0, 0, ret=77)  # Screen_GetFrame
logfake(e, 0x4E7B80, 2)  # DebugText_SetCursor
printf_fake(e, 0x4E7BD0)  # Debug_printf
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
u16(e, 0x54F198, CAM)
e.baseline = e.snapshot()

RELATIVE, DISABLE, NOTHING = 0x528930, 0x528FF0, 0x4D3710


def camera(e, rng):
    c = task(e, 0x220, CAM)
    e.write(c + 0x20, bytes(rng.randrange(256) for _ in range(0x200)))
    u32(e, c + 0x88, 0x8888)
    f32(e, c + 0x8C, *[rng.uniform(-1, 1) for _ in range(9)])
    d64(e, c + 0xB8, *[rng.uniform(-1e4, 1e4) for _ in range(3)])
    f32(e, c + 0xD0, 0.75)
    u32(e, c + 0xD8, *range(100, 110))
    u32(e, c + 0x100, NOTHING)
    u32(e, c + 0x108, 1, 2)
    d64(e, c + 0x148, *[rng.uniform(-500, 500) for _ in range(6)])
    f32(e, c + 0x178, *[rng.uniform(-3, 3) for _ in range(9)])
    e.write(c + 0x19C, bytes([0, 1, 0, 1]))
    u32(e, c + 0x1B8, 3, 4)
    d64(e, c + 0x1C0, *[rng.uniform(-1e5, 1e5) for _ in range(6)])
    f32(e, c + 0x1F0, rng.uniform(0, 10))
    e.write(c + 0x214, bytes([1, 0, 0xFF, 0]))
    u32(e, c + 0x218, 0)
    return c


def obj(e, rng, id_, type_=OBJ):
    t = task(e, 0x40, type_)
    TASKS[id_] = t
    INFOS[t] = ([rng.uniform(-1e5, 1e5) for _ in range(3)], [rng.uniform(-1, 1) for _ in range(9)], 5)
    return t


def keys(e, down=(), prev=()):
    for k in down:
        e.write(KEYS + k, b"\x80")
    for k in prev:
        e.write(KEYS_PREV + k, b"\x80")


def reset():
    TASKS.clear()
    INFOS.clear()


def test_create():
    run(e, 0x528760, "AnimCam_Create", lambda e: (camera(e, random.Random(1)),))


def test_reset():
    run(e, 0x528850, "AnimCam_Reset", lambda e: (camera(e, random.Random(2)),))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("found", ["none", "missing", "plain", "obj"])
@pytest.mark.parametrize("seed", [1, 2])
def test_relative(found, seed, cw):
    def setup(e):
        reset()
        rng = random.Random(seed)
        c = camera(e, rng)
        if found == "none":
            u32(e, c + 0x108, -1)
        elif found == "plain":
            obj(e, rng, 1, PLAIN)
        elif found == "obj":
            obj(e, rng, 1)
        return (c,)
    run(e, 0x528930, "AnimCam_Relative", setup, cw=cw)


def test_get_params():
    def setup(e):
        u32(e, 0xA84388, 0x8383)
        return 0x1234, e.alloc(4)
    run(e, 0x528CF0, "AnimCam_GetParams", setup)


def control_keys(rng, mode, missing):
    k = [float(mode), rng.uniform(-3, 9), 2.5, rng.uniform(-1e3, 1e3), rng.uniform(-1e3, 1e3), -7.25,
         rng.uniform(-9, 9), 0.125, NAN, rng.uniform(0, 1)] + [rng.uniform(-360, 360) for _ in range(8)] + \
        [1.0, 257.6, -0.5, 2.5]
    for i in missing:
        k[i] = NO_KEY
    return k


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("mode", [0, 1, 2, 3, 4, 5, -1, 1.5, 3.5, 1e10, NO_KEY])
@pytest.mark.parametrize("msg_mode", [-1, 0, 1])
def test_control(mode, msg_mode, cw):
    def setup(e):
        reset()
        rng = random.Random(3)
        c = camera(e, rng)
        e.write(c + 0x19F, bytes([int(mode) & 1 if mode != NO_KEY else 0]))
        obj(e, rng, 1)
        k = e.alloc(4 * 22)
        missing = [] if mode in (0, 2) else [1, 5, 9, 12, 18, 21] if mode in (1, 3) else range(1, 22)
        f32(e, k, *control_keys(rng, mode, missing))
        if mode == 4:
            u32(e, k + 4 * 3, SNAN)
        msg = e.alloc(8)
        u32(e, msg, msg_mode, k)
        return c, msg
    run(e, 0x528D00, "AnimCam_Control", setup, cw=cw)


def test_disable():
    run(e, 0x528FF0, "AnimCam_Disable", lambda e: (camera(e, random.Random(4)),))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("fn", [NOTHING, RELATIVE, DISABLE, 0x529010])
def test_update(fn, cw):
    def setup(e):
        reset()
        rng = random.Random(5)
        c = camera(e, rng)
        obj(e, rng, 1)
        u32(e, c + 0x100, fn)
        return (c,)
    run(e, 0x529A10, "AnimCam_Update", setup, cw=cw)


@pytest.mark.parametrize("index", [-1, 0, 7, 21, 22, 0x7FFFFFFF])
def test_get_input_info(index):
    def setup(e):
        msg = e.alloc(8)
        u32(e, msg, index, 0x5555)
        return 0x1234, msg
    run(e, 0x529A70, "AnimCam_GetInputInfo", setup)


SELECT_KEYS = [
    ((), ()),
    ((0x1D, 0x0B), ()),
    ((0x9D, 0x2A, 0x05), ()),
    ((0x1D, 0x36, 0x02, 0x0A), ()),
    ((0x1D,) + tuple(range(2, 12)), ()),
    ((0x0A, 0x2F, 0x30, 0x31), ()),
    ((0x2F, 0x30, 0x31), (0x30,)),
    ((0x9D, 0x08), (0x2F,)),
]


@pytest.mark.parametrize("case", range(len(SELECT_KEYS)))
def test_read_select_keys(case):
    def setup(e):
        keys(e, *SELECT_KEYS[case])
        return 0x1234, camera(e, random.Random(6))
    run(e, 0x529AE0, "AnimCam_ReadSelectKeys", setup)


MODE_KEYS = [
    ((), ()),
    ((0x21,), ()),
    ((0x21,), (0x21,)),
    ((0x2E,), ()),
    ((0x2E,), (0x2E,)),
    ((0x1E,), ()),
    ((0x13,), ()),
    ((0x12,), (0x12,)),
    ((0x21, 0x2E, 0x1E, 0x13, 0x12), ()),
]


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("case", range(len(MODE_KEYS)))
@pytest.mark.parametrize("found", ["none", "missing", "plain", "obj"])
def test_read_mode_keys(case, found, cw):
    out = {}

    def setup(e):
        reset()
        rng = random.Random(7)
        keys(e, *MODE_KEYS[case])
        c = camera(e, rng)
        u32(e, c + 0x1BC, -1 if found == "none" else 0x40000001)
        if found == "plain":
            obj(e, rng, 0x40000001, PLAIN)
        elif found == "obj":
            obj(e, rng, 0x40000001)
        k = e.alloc(4 * 22)
        f32(e, k, *[NO_KEY] * 22)
        msg = e.alloc(4)
        u32(e, msg, k)
        out["k"] = k
        return c, msg
    ignore = ()
    if found != "obj" and 0x21 in MODE_KEYS[case][0] and 0x21 not in MODE_KEYS[case][1]:
        # the original leaves these unset
        e.reset()
        setup(e)
        ignore = [(out["k"] + 12, out["k"] + 24)]
        if 0x2E in MODE_KEYS[case][0]:
            ignore = ()
    run(e, 0x529C20, "AnimCam_ReadModeKeys", setup, cw=cw, ignore=ignore)


@pytest.mark.parametrize("view", [0, 1, 2, 3, 4, -1])
@pytest.mark.parametrize("locks", [(0, 0, 0), (1, 0, 1), (0xFF, 1, 0)])
def test_draw_info(view, locks):
    def setup(e):
        c = camera(e, random.Random(8))
        e.write(c + 0x1A8, b"Relative\0")
        e.write(c + 0x214, bytes(locks))
        u32(e, c + 0x218, view)
        return (c,)
    run(e, 0x52A010, "AnimCam_DrawInfo", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("down", [(), (0x21, 0x1D, 0x2A, 0x04, 0x2F)])
def test_read_keys(down, cw):
    def setup(e):
        reset()
        rng = random.Random(9)
        keys(e, down)
        c = camera(e, rng)
        obj(e, rng, 103)
        k = e.alloc(4 * 24)
        f32(e, k, *[rng.uniform(-1, 1) for _ in range(24)])
        msg = e.alloc(4)
        u32(e, msg, k)
        return c, msg
    run(e, 0x529AA0, "AnimCam_ReadKeys", setup, cw=cw)


@pytest.mark.parametrize("space", [False, True])
@pytest.mark.parametrize("enabled", [0, 1, 0x80])
def test_run(space, enabled):
    def setup(e):
        if space:
            keys(e, (0x39,))
        c = camera(e, random.Random(10))
        e.write(c + 0x19F, bytes([enabled]))
        return (c,)
    run(e, 0x52A180, "AnimCam_Run", setup)


def test_get_state_size():
    run(e, 0x52A1C0, "AnimCam_GetStateSize", lambda e: (0x1234, e.alloc(4)))


@pytest.mark.parametrize("orig,ours", [(0x52A1D0, "AnimCam_SaveState"), (0x52A1F0, "AnimCam_LoadState")])
def test_save_load(orig, ours):
    def setup(e):
        rng = random.Random(11)
        c = camera(e, rng)
        buf = e.alloc(0xB0)
        e.write(buf, bytes(rng.randrange(256) for _ in range(0xB0)))
        msg = e.alloc(8)
        u32(e, msg, buf, 0x99)
        return c, msg
    run(e, orig, ours, setup)


def test_close():
    def setup(e):
        u32(e, 0xA84388, 0x8383)
        u32(e, 0x54F198, 0x12340071)
    run(e, 0x52A210, "AnimCam_Close", setup)


def test_open():
    compare_open(open_emu(), 0x5284A0, "AnimCam_Open")
