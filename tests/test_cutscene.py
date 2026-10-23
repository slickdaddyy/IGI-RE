"""CutScene (0x4F4860 - 0x4F65E0)."""
import math
import random
import struct

import pytest

from test_cutscene_util import (CUT, CWS, FISTP_CWS, KEY, OTHER, RET, add_child, d64, f32, handler, make_emu, run,
                                set_handler, task, u16, u32)

emu = make_emu()
SLOTS = {1: 1, 0x34: 2, 0x32: 1}
HANDLERS = {s: handler(emu, "cut%x" % s, n) for s, n in SLOTS.items()}


def types(e, ticks=30):
    u16(e, 0xA7B260, KEY)
    u32(e, 0xA76C54, ticks)
    for s in SLOTS:
        for t in (CUT, KEY, OTHER):
            set_handler(e, s, t, HANDLERS[s])


def cutscene(e, seed=1, **kw):
    rng = random.Random(seed)
    types(e)
    c = task(e, 0x278, CUT)
    RET["base"] = c
    d64(e, c + 0x20, *[rng.uniform(-9000, 9000) for _ in range(3)])
    f32(e, c + 0x38, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, c + 0x5C, 5)
    f = dict(isRun=1, isFinished=0, registered=0, tick=12, start=0.5, scale=1.0, screen=0.8, fadeIn=0.0,
             fadeOut=0.0, key=-1, unk210=0.75, filters=(0, 0, 0, 0), noFilters=0, wasRunning=0, frame=7)
    f.update(kw)
    e.write(c + 0x60, bytes([f["isRun"], f["isFinished"], f["registered"]]))
    u32(e, c + 0x64, f["tick"])
    f32(e, c + 0x68, f["start"], f["scale"], f["screen"], f["fadeIn"], f["fadeOut"])
    u32(e, c + 0x20C, f["key"])
    f32(e, c + 0x210, f["unk210"])
    d64(e, c + 0x218, 100.5, 200.25, -300.125)
    f32(e, c + 0x230, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, c + 0x254, 9)
    u32(e, c + 0x258, 0xCA3E)
    u32(e, c + 0x25C, *f["filters"])
    e.write(c + 0x26C, bytes([f["noFilters"], f["wasRunning"]]))
    u32(e, c + 0x270, f["frame"])
    return c


def key(e, parent, seed=1, dur=1.0, smooth=1, child=False, **kw):
    rng = random.Random(seed)
    k = task(e, 0xE4, KEY)
    add_child(e, parent, k)
    f = dict(pos_attach=-1, rot_attach=-1, follow=(0, 0), filter=0, level=0.0, shake=0.0, unk98=0.5,
             angles=[rng.uniform(-7, 7) for _ in range(3)])
    f.update(kw)
    d64(e, k + 0x20, *[rng.uniform(-9000, 9000) for _ in range(3)])
    f32(e, k + 0x38, dur, *f["angles"])
    e.write(k + 0x48, bytes(rng.randrange(256) for _ in range(0x40)))
    f32(e, k + 0x88, rng.uniform(0.5, 1.5))
    u32(e, k + 0x8C, f["pos_attach"], f["rot_attach"])
    e.write(k + 0x94, bytes([smooth, f["follow"][0], f["follow"][1]]))
    f32(e, k + 0x98, f["unk98"])
    u32(e, k + 0xCC, f["filter"])
    f32(e, k + 0xD0, f["level"], *[rng.uniform(0, 1) for _ in range(3)])
    f32(e, k + 0xE0, f["shake"])
    return k


def test_small():
    def setup(e):
        u32(e, 0x54E200, 0x7A6B5C4D)
        u32(e, 0xA76C58, 0x5858)
        u32(e, 0xA76C68, 0x6868)
        return 0xABCD, e.alloc(8)
    run(emu, 0x4F4860, "Vehicle_GetClearHitEvent", setup, check_eax=True, eax_mask=0xFF)
    run(emu, 0x4F4870, "Vehicle_SetTaskList", setup)
    run(emu, 0x4F65B0, "CutScene_SetCamera", setup)
    run(emu, 0x4F65C0, "CutScene_SetFilterParent", setup)
    run(emu, 0x4F65D0, "CutScene_SetTicksPerSecond", setup)
    run(emu, 0x4F51C0, "CutScene_GetParamTableHandler", setup)
    run(emu, 0x4F64E0, "Task_ReturnZero", setup)
    run(emu, 0x4F64F0, "CutScene_InputInfoHandler", setup)
    run(emu, 0x4F6590, "CutScene_Close", setup)


def test_open():
    run(emu, 0x4F4B00, "CutScene_Open")


def test_create():
    def setup(e):
        u32(e, 0xA76C7C, 0x7C7C)
        return (cutscene(e, tick=99, key=3, filters=(1, 2, 3, 4)),)
    run(emu, 0x4F4EE0, "CutScene_CreateHandler", setup)


@pytest.mark.parametrize("registered", [0, 1])
def test_register_vars(registered):
    def setup(e):
        return (cutscene(e, registered=registered),)
    run(emu, 0x4F4FB0, "CutScene_RegisterVarsHandler", setup)
    run(emu, 0x4F5050, "CutScene_UnregisterVarsHandler", setup)


@pytest.mark.parametrize("id_", [-1, 0, 2])
@pytest.mark.parametrize("handled", [0, 1])
@pytest.mark.parametrize("filters", [(0, 0, 0, 0), (0x25C, 0x260, 0x264, 0x268), (0, 0x260, 0, 0x268)])
def test_delete_leave(id_, handled, filters):
    def setup(e):
        RET[0x4F1050] = id_
        c = cutscene(e, filters=filters)
        if not handled:
            set_handler(e, 0x32, CUT, 0)
        return (c,)
    run(emu, 0x4F50E0, "CutScene_DeleteHandler", setup)
    run(emu, 0x4F6520, "CutScene_LeaveWorldHandler", setup)


def key_tree(e, c, shape):
    """shape: list of (depth, type) in depth-first order."""
    stack = [c]
    first = None
    for i, (depth, type_) in enumerate(shape):
        del stack[depth + 1:]
        k = key(e, stack[depth], seed=i, dur=0.25 * (i + 1))
        u16(e, k + 0x1C, type_)
        stack.append(k)
        first = first or k
    return first


SHAPES = [
    [(0, KEY)],
    [(0, KEY), (0, KEY), (0, KEY)],
    [(0, KEY), (1, KEY), (1, KEY), (2, KEY), (0, KEY), (1, OTHER), (0, KEY)],
    [(0, KEY), (0, OTHER), (0, KEY)],
    [(0, OTHER), (0, KEY)],
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("shape", range(len(SHAPES)))
@pytest.mark.parametrize("scale", [1.0, 0.3, -2.5])
def test_collect(cw, shape, scale):
    def setup(e):
        c = cutscene(e)
        first = key_tree(e, c, SHAPES[shape])
        keys, count, time = e.alloc(800), e.alloc(4), e.alloc(4)
        u32(e, count, 1)
        f32(e, time, 0.125)
        return keys, count, first, time, struct_f(scale)
    run(emu, 0x4F4880, "CutScene_CollectKeys", setup, cw=cw)


def struct_f(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


@pytest.mark.parametrize("count,i", [(1, 0), (3, -2), (3, 0), (3, 2), (3, 3), (3, 7), (0, 0)])
def test_get_key(count, i):
    run(emu, 0x4F4910, "CutScene_GetKey", lambda e: (0x10000, count, i), check_eax=True)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("count", [0, 1, 2, 5])
def test_unwrap(cw, seed, count):
    def setup(e):
        rng = random.Random(seed)
        c = cutscene(e)
        keys = e.alloc(8 * 6)
        for i in range(max(count, 1)):
            k = key(e, c, seed=i, angles=[rng.uniform(-40, 40) for _ in range(3)])
            u32(e, keys + 8 * i, k)
        return keys, count
    run(emu, 0x4F4940, "CutScene_UnwrapAngles", setup, cw=cw)


def dbits(v):
    return struct.unpack("<II", struct.pack("<d", v))


SPLINES = [
    ((1.0, 2.0, 3.0, 5.0), (0.0, 1.0, 2.0, 3.0), 1.5),
    ((10.5, -20.25, 7.0, 1e5), (0.0, 0.4, 1.7, 1.9), 1.0),
    ((10.5, -20.25, 7.0, 1e5), (0.4, 0.4, 0.4, 0.4), 0.4),
    ((10.5, -20.25, 7.0, 1e5), (0.0, 0.0, 1.0, 1.0), 0.3),
    ((3.0, 3.0, 9.0, 9.0), (1.0, 2.0, 2.0, 5.0), 2.0),
    ((0.1, 0.2, 0.3, 0.7), (-1.0, 0.3333, 1.1, 1.25), 0.9),
    ((0.1, 0.2, 0.3, 0.7), (-1.0, 0.3333, 1.1, 1.25), math.nan),
    ((0.1, math.nan, 0.3, 0.7), (-1.0, 0.3333, 1.1, 1.25), 0.5),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(SPLINES)))
def test_spline(cw, case):
    p, t, x = SPLINES[case]
    args = [w for v in p + t + (x,) for w in dbits(v)]
    run(emu, 0x4F6290, "CutScene_Spline", lambda e: args, cw=cw, check_st0=True)


def interp_setup(e, seed, changed):
    rng = random.Random(seed)
    filters = tuple(rng.choice([0, 0x25C + 4 * i]) for i in range(4))
    c = cutscene(e, seed=seed, filters=filters, noFilters=rng.choice([0, 0, 1]),
                 fadeIn=rng.choice([0.0, 0.5, 3.0]), fadeOut=rng.choice([0.0, 0.5, 4.0]),
                 screen=rng.choice([1.0, 0.75, 0.0]), unk210=rng.uniform(0, 2))
    u32(e, 0xA76C80, rng.choice([0, 0xF117]))
    u32(e, 0xA76C7C, 0xCA3E)
    mode = e.alloc(12)
    u32(e, mode, 0, 640, 480)
    RET["mode"] = mode
    RET["rf"] = [rng.random() for _ in range(6)]
    times = sorted(rng.uniform(0, 6) for _ in range(4))
    if rng.random() < 0.2:
        times[2] = times[1]
    if rng.random() < 0.2:
        times[1] = times[0]
    entries = e.alloc(32)
    for i in range(4):
        k = key(e, c, seed=seed * 10 + i, pos_attach=rng.choice([-1, 2]), rot_attach=rng.choice([-1, 3]),
                follow=(rng.choice([0, 1]), rng.choice([0, 1])), filter=rng.randrange(5),
                level=rng.choice([0.0, 0.3]), shake=rng.choice([0.0, 0.02]), unk98=rng.choice([-1.0, 0.5, 2.0]))
        u32(e, entries + 8 * i, k)
        f32(e, entries + 8 * i + 4, times[i])
    ptrs = e.alloc(16)
    u32(e, ptrs, *[entries + 8 * i for i in range(4)])
    t = rng.uniform(times[1] - 0.5, times[2] + 0.5)
    end = times[3] + rng.choice([0.0, 0.5, 2.0])
    return [c, ptrs, *dbits(t), *dbits(end), changed | 0x5A5A00]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(60))
@pytest.mark.parametrize("changed", [0, 1])
def test_interpolate(cw, seed, changed):
    run(emu, 0x4F55D0, "CutScene_Interpolate", lambda e: interp_setup(e, seed, changed), cw=cw)


def update_setup(e, seed, tick, cur):
    rng = random.Random(seed)
    c = cutscene(e, seed=seed, tick=tick, key=cur, scale=rng.choice([1.0, 0.5]), screen=1.0)
    u32(e, 0xA76C80, 0)
    RET["rf"] = [0.5] * 6
    if seed % 5 == 4:
        k = key(e, c, seed=seed)
        u16(e, k + 0x1C, OTHER)
        return (c,)
    for i in range(rng.randrange(1, 5)):
        key(e, c, seed=seed * 7 + i, dur=rng.choice([0.5, 1.0, 1.5]), smooth=rng.choice([0, 1, 1]))
    return (c,)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("seed", range(10))
@pytest.mark.parametrize("tick", [0, 14, 15, 31, 75, 1000])
@pytest.mark.parametrize("cur", [-1, 0, 1])
def test_update(cw, seed, tick, cur):
    run(emu, 0x4F5400, "CutScene_Update", lambda e: update_setup(e, seed, tick, cur), cw=cw)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("has", [(0, 0, 0, 0, 0), (1, 1, 1, 1, 1), (1, 0, 1, 0, 1), (0, 1, 0, 1, 0)])
@pytest.mark.parametrize("vals", [(1.0, 0.0, 2.5), (0.0, 1.0, -30.4), (0.4, 0.6, 0.5), (300.0, -0.5, -0.45)])
@pytest.mark.parametrize("state", [(0, 0), (1, 0), (0, 1), (1, 1)])
@pytest.mark.parametrize("frame", [7, 0x75A0])
@pytest.mark.parametrize("keys", [0, 2])
def test_run(cw, has, vals, state, frame, keys):
    def setup(e):
        c = cutscene(e, isRun=state[0], wasRunning=state[1], frame=frame, filters=(5, 6, 7, 8), screen=1.0)
        u32(e, 0xA76C80, 0)
        for off, h in zip((0x7C, 0xCC, 0x11C, 0x16C, 0x1BC), has):
            RET[("has", off)] = h
        for off, v in zip((0x7C, 0xCC, 0x11C), vals):
            RET[("val", off)] = v
        for i in range(keys):
            key(e, c, seed=i)
        add_child(e, c, task(e, 0x40, OTHER))
        return (c,)
    run(emu, 0x4F51D0, "CutScene_RunHandler", setup, cw=cw)


@pytest.mark.parametrize("cw", FISTP_CWS)
@pytest.mark.parametrize("arg", [None, b"", b"x"])
@pytest.mark.parametrize("fades", [(0.5, 2.0), (-1.0, -0.0), (math.nan, 1.0), (0.0, -3.0)])
@pytest.mark.parametrize("start", [0.0, 1.25, 0.05, 2.5 / 30])
def test_param_changed(cw, arg, fades, start):
    def setup(e):
        c = cutscene(e, fadeIn=fades[0], fadeOut=fades[1], start=start)
        key(e, c)
        add_child(e, c, task(e, 0x40, OTHER))
        p = 0
        if arg is not None:
            p = e.alloc(4)
            e.write(p, arg + b"\0")
        return c, p
    run(emu, 0x4F63C0, "CutScene_ParamChangedHandler", setup, cw=cw)


@pytest.mark.parametrize("tick", [0, 20, 500])
def test_control(tick):
    def setup(e):
        c = cutscene(e, filters=(1, 2, 3, 4))
        key(e, c, dur=1.0)
        key(e, c, seed=2, dur=2.0)
        RET["rf"] = [0.5] * 6
        p = e.alloc(4)
        u32(e, p, tick)
        return c, p
    run(emu, 0x4F6500, "CutScene_ControlHandler", setup)
