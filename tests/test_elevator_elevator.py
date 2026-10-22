"""Elevator and ElevatorFloorIndicator."""
import random

import pytest

from emu import compare
from test_elevator_util import (RET, add_child, d64, f32, init_emu, make_emu, run, setup_types, strz, task, u16,
                                u32, obj)

ELEV, IND, CHILD = 0x6B, 0x6C, 0x6D

emu = make_emu((ELEV, IND, CHILD))
init = init_emu()

T = 2.0 ** -70  # lost when added to 1 in 80 bits, so sums show their order
VALUES = [1.0, -1.0, T, -T, 3.0, 0.5, 2.0 ** 30]


@pytest.mark.parametrize("orig,ours", [(0x43C6D0, "ElevatorFloorIndicator_Init"), (0x43CC70, "Elevator_Init")])
def test_init(orig, ours):
    compare(init, orig, ours, setup=setup_types, check_eax=False, normalized=True)


def children(e, t, n):
    for i in range(n):
        add_child(e, t, task(e, 0x40, CHILD if i else IND))


def indicator(e, seed=1, n=0):
    rng = random.Random(seed)
    t = obj(e, 0x248, IND, seed=seed)
    d64(e, t + 0xF0, 11.0, 12.0, 13.0)
    f32(e, t + 0x108, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, t + 0x12C, 6)
    if seed >= 46:
        # parent matrix of 1s (see move_arg): the sums run over these
        d64(e, t + 0x130, *PERMS[seed % 6])
        cols = [PERMS[(seed + k) % 6] for k in (0, 1, 3)]
        f32(e, t + 0x148, *[cols[c][r] for r in range(3) for c in range(3)])
    elif seed >= 40:
        d64(e, t + 0x130, 1.0, 1.0, 1.0)
        f32(e, t + 0x148, *[1.0] * 9)
    else:
        d64(e, t + 0x130, *[rng.choice(VALUES) for _ in range(3)])
        f32(e, t + 0x148, *[rng.choice(VALUES) for _ in range(9)])
    u32(e, t + 0x16C, 8)
    strz(e, t + 0x170, b"lamp_lit")
    strz(e, t + 0x180, b"lamp")
    strz(e, t + 0x190, b"ding")
    e.write(t + 0x1A0, b"\x07")
    children(e, t, n)
    return t


# rows holding 1, -1 and T in every order, so a wrong sum order loses T
PERMS = [(1.0, -1.0, T), (1.0, T, -1.0), (-1.0, 1.0, T), (-1.0, T, 1.0), (T, 1.0, -1.0), (T, -1.0, 1.0)]


def move_arg(e, seed):
    rng = random.Random(seed * 7 + 1)
    a = e.alloc(8)
    pos = e.alloc(24)
    d64(e, pos, *[rng.choice(VALUES) * 100 for _ in range(3)])
    m = e.alloc(0x28)
    if seed >= 46:
        f32(e, m, *[1.0] * 9)
    elif seed >= 40:
        f32(e, m, *(PERMS[seed % 6] + PERMS[(seed + 2) % 6] + PERMS[(seed + 4) % 6]))
    else:
        f32(e, m, *[rng.choice(VALUES) for _ in range(9)])
    u32(e, m + 0x24, 9)
    u32(e, a, pos, m)
    return a


def test_indicator_create():
    def setup(e):
        RET[0x4D0950] = 1.75
        return (indicator(e),)
    run(emu, 0x43C8D0, "ElevatorFloorIndicator_Create", setup)


@pytest.mark.parametrize("seed", range(52))
def test_indicator_move(seed):
    def setup(e):
        RET[0x4D0950] = 1.75
        return indicator(e, seed=seed, n=seed % 3), move_arg(e, seed)
    run(emu, 0x43C390, "ElevatorFloorIndicator_Move", setup)


@pytest.mark.parametrize("lit", [None, 0.0, 1.0, 0.4])
@pytest.mark.parametrize("ring", [None, 0.0, 2.0])
@pytest.mark.parametrize("sound", [0, 0x6100])
@pytest.mark.parametrize("n", [0, 2])
def test_indicator_run(lit, ring, sound, n):
    def setup(e):
        t = indicator(e, n=n)
        for off, v in ((0x1A4, lit), (0x1F4, ring)):
            RET[("has", t + off)] = v is not None
            RET[("val", t + off)] = v or 0.0
        RET[0x4E6B00] = sound
        return (t,)
    run(emu, 0x43CA10, "ElevatorFloorIndicator_Run", setup)


@pytest.mark.parametrize("orig,ours", [(0x43CB30, "ElevatorFloorIndicator_Delete"),
                                       (0x43CB70, "ElevatorFloorIndicator_Changed")])
def test_indicator_misc(orig, ours):
    def setup(e):
        RET[0x4D0950] = 1.75
        return (indicator(e),)
    run(emu, orig, ours, setup)


def test_indicator_get_params():
    def setup(e):
        u32(e, 0x57BF88, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x43CB60, "ElevatorFloorIndicator_GetParams", setup)


def test_indicator_close():
    def setup(e):
        u32(e, 0x57BF88, 0x9C9C)
        u16(e, 0x57BF84, 0x33)
    run(emu, 0x43CC50, "ElevatorFloorIndicator_Close", setup)


HEIGHTS = [0.0, 100.0, 250.0, 400.0, 600.0]


def elevator(e, height=0.0, wanted=0, speed=0.0, max_speed=5.0, accel=0.99, graph=7, n=0, tick=-1, last=(0, 0),
             last_move=1, requests=(), moving=0, floor_pos=1.5):
    t = obj(e, 0x648, ELEV)
    d64(e, t + 0xF0, 1.0, 2.0, 3.0)
    u32(e, t + 0x180, graph)
    f32(e, t + 0x184, height)
    f32(e, t + 0x190, speed, max_speed, accel)
    u32(e, t + 0x19C, wanted)
    d64(e, t + 0x1A0, floor_pos, 0.25)
    for r in requests:
        e.write(t + 0x1B0 + r, b"\x01")
    u32(e, t + 0x1BC, 5)
    e.write(t + 0x5D4, bytes(last))
    strz(e, t + 0x5D9, b"start")
    strz(e, t + 0x5F9, b"stop")
    strz(e, t + 0x619, b"hum")
    u32(e, t + 0x63C, moving)
    e.write(t + 0x640, bytes([last_move]))
    u32(e, t + 0x644, tick)
    children(e, t, n)
    return t


def graph(count, heights=HEIGHTS):
    RET[0x502130] = count
    for i, h in enumerate(heights):
        RET[(0x502180, i)] = h


@pytest.mark.parametrize("scale", [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5)])
def test_elevator_create(scale):
    def setup(e):
        RET[0x4D0950] = 1.75
        t = elevator(e, height=3.0, tick=8, moving=4)
        f32(e, t + 0x98, *scale)
        return (t,)
    run(emu, 0x43D010, "Elevator_CreateHandler", setup)


def test_move_children():
    run(emu, 0x43D8C0, "Elevator_MoveChildren", lambda e: (elevator(e, n=3),))


MOVES = [dict(graph=-1), dict(count=0)]
for h, w in ((0.0, 4), (0.0, 1), (100.0, 1), (100.0, 0), (99.999, 1), (100.0005, 1), (600.0, 0), (250.0, 3),
             (float("nan"), 2), (599.0, 4), (1.0, 0), (0.0, 0)):
    for speed, max_speed, accel in ((0.0, 5.0, 0.99), (4.5, 5.0, 0.99), (5.0, 5.0, 0.99), (6.5, 5.0, 0.99),
                                    (5.5, 5.0, 0.99), (2.0, 120.0, 30.0), (float("nan"), 5.0, 0.99),
                                    (3.0, float("nan"), 1.0)):
        MOVES.append(dict(height=h, wanted=w, speed=speed, max_speed=max_speed, accel=accel))


for h, w in ((0.0, 1), (100000.0, 1), (100000.0, 0), (10.0, 4), (600000.0, 2), (99999.95, 1), (100000.0005, 1)):
    for speed, max_speed, accel in ((0.0, 5.0, 0.99), (4.5, 5.0, 0.99), (4.0, 5.0, 1.0), (5.0, 5.0, 0.99),
                                    (6.5, 5.0, 0.99), (5.5, 5.0, 0.99), (5.5, 5.0, 0.5), (float("nan"), 5.0, 0.99),
                                    (3.0, float("nan"), 1.0), (6.0, 5.0, float("nan")), (2.0, 5.0, float("nan"))):
        MOVES.append(dict(height=h, wanted=w, speed=speed, max_speed=max_speed, accel=accel, far=True))


@pytest.mark.parametrize("m", MOVES)
def test_elevator_move(m):
    def setup(e):
        m2 = dict(m)
        graph(m2.pop("count", 5), HEIGHTS + [float("nan")])
        if m2.pop("far", False):
            far_heights()
        RET[0x5020C0] = 1600.0
        return (elevator(e, n=1, **m2),)
    run(emu, 0x43D6E0, "Elevator_Move", setup)


def far_heights():
    graph(5, [0.0, 100000.0, 250000.0, 400000.0, 600000.0])


RUNS = []
for count in (0, 1, 2, 5):
    for height in (0.0, 100.0, 175.0, 700.0):
        for wanted in (0, 1, 4):
            RUNS.append(dict(count=count, height=height, wanted=wanted))
RUNS += [
    dict(tick=33),
    dict(up=1), dict(up=1, last=(1, 0)), dict(up=1, wanted=4), dict(up=1, wanted=3),
    dict(down=1, wanted=2), dict(down=1, wanted=0), dict(down=1, wanted=2, last=(0, 1)), dict(up=1, down=1, wanted=2),
    dict(requests=(1, 3)), dict(requests=(2,), floor_req=(2, 0.0)), dict(floor_req=(4, 1.0)), dict(floor_req=(9, 3.0)),
    dict(move=0.0), dict(move=0.0, last_move=0, height=0.0), dict(move=2.0, last_move=2, height=0.0),
    dict(height=100.0, wanted=1, last_move=1), dict(height=100.0, wanted=1, last_move=0, moving=0x6200),
    dict(height=99.99, wanted=1), dict(height=0.0, wanted=1, moving=0x6200), dict(height=250.0, wanted=2),
    dict(height=float("nan"), wanted=2), dict(height=0.0, wanted=0, move=0.0),
    dict(height=50.0, wanted=1, speed=4.0, far=True), dict(height=0.0, wanted=4, far=True),
]


@pytest.mark.parametrize("r", RUNS)
@pytest.mark.parametrize("sound", [0, 0x6100])
def test_elevator_run(r, sound):
    def setup(e):
        r2 = dict(r)
        graph(r2.pop("count", 5))
        if r2.pop("far", False):
            far_heights()
        RET[0x4028B0] = 33
        RET[0x4E6B00] = sound
        RET[0x5020C0] = 1600.0
        exprs = {0x4E4: r2.pop("up", None), 0x534: r2.pop("down", None), 0x584: r2.pop("move", None)}
        floor_req = r2.pop("floor_req", None)
        t = elevator(e, n=2, **r2)
        for off, v in exprs.items():
            if v is not None:
                RET[("has", t + off)] = 1
                RET[("val", t + off)] = float(v)
        if floor_req:
            RET[("has", t + 0x1C4 + 0x50 * floor_req[0])] = 1
            RET[("val", t + 0x1C4 + 0x50 * floor_req[0])] = floor_req[1]
        return (t,)
    run(emu, 0x43D280, "Elevator_RunHandler", setup)
