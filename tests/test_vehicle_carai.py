import math
import random
import struct

import pytest

from emu import compare
from test_vehicle_util import (LOG, RET, d64, f32, init_emu, logfake, run, set_events, set_handler, setup_init, task,
                               trap, u16, u32, vfake)

CAR, GRAPH, AI = 0x60, 0x61, 0x62
NODES = [(0.0, 0.0, 0.0), (10000.0, 0.0, 0.0), (10000.0, 10000.0, 0.0), (-8000.0, 30000.0, 500.0),
         (60000.0, -50000.0, 100.0), (1234.5, -9876.25, 33.0), (0.0, 12288.0, 0.0), (-45000.0, -45000.0, 0.0)]

emu = init_emu()
set_events(emu)
u16(emu, 0x53BDA8, CAR)
u16(emu, 0x54E83C, GRAPH)


def node(graph, i):
    return graph + 0x100 + 0x40 * i


def get_node(e, a):
    return node(a[0], a[1])


def find_nearest(e, a):
    i = RET.get(0x5096F0)
    u32(e, a[0], node(a[1], i) if i is not None else 0)


def next_step(e, a):
    i = RET.get(0x5096C0, {}).get((a[1], a[2]))
    if i is None:
        return 0
    p = e.alloc(4)
    u32(e, p, i)
    return p


vfake(emu, 0x4F9720, 2, effect=get_node)
vfake(emu, 0x4F9770, 2, show=lambda e, a: (a[1],), effect=lambda e, a: e.write(a[0], e.read(a[1] + 8, 24)))
vfake(emu, 0x5096F0, 3, show=lambda e, a: (a[1], e.read(a[2], 24)), effect=find_nearest)
vfake(emu, 0x5096C0, 3, effect=next_step)
logfake(emu, 0x453730, 1)  # AICmdList_Next
logfake(emu, 0x453760, 1)  # AICmdList_Advance
logfake(emu, 0x4537E0, 3)  # AICmdList_Init
vfake(emu, 0x4AF810, 4, show=lambda e, a: [e.read_cstr(a[0])] + a[1:1 + e.read_cstr(a[0]).count(b"%")])
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4C1830, 1)  # QTaskList_Free


def control(e):
    car, ctl = e.arg(0), e.arg(1)
    tick, axes = struct.unpack("<II", e.read(ctl, 8))
    LOG[-1].append(("control", e.normalize(car), tick, e.read(axes, 8), e.read(axes + 12, 4)))
    e.ret(0)


set_handler(emu, 0x33, CAR, trap(emu, "control", control))
emu.baseline = emu.snapshot()

init = init_emu()


def rot(yaw):
    c, s = math.cos(yaw), math.sin(yaw)
    return (c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0)


def graph(e):
    g = task(e, 0x400, GRAPH)
    for i, p in enumerate(NODES):
        u32(e, node(g, i), i)
        d64(e, node(g, i) + 8, *p)
    return g


def car(e, pos=(100.0, 200.0, 0.0), yaw=0.3, speed=500.0, unk720=0.25, hunting=0, attacker=0, passengers=()):
    c = task(e, 0x3000, CAR)
    d64(e, c + 0x20, *pos)
    f32(e, c + 0x70, *rot(yaw))
    f32(e, c + 0xF8, speed)
    f32(e, c + 0x720, unk720)
    e.write(c + 0x91D, bytes([hunting]))
    u32(e, c + 0x2ED0, attacker)
    nodes = [e.alloc(0x30) for _ in range(len(passengers) + 1)]
    for n, nxt in zip(nodes, nodes[1:] + [0]):
        u32(e, n, nxt)
    for n, (active, who) in zip(nodes, passengers):
        ref = e.alloc(4)
        u32(e, ref, who)
        u32(e, n + 8, ref)
        e.write(n + 0x29, bytes([active]))
    u32(e, c + 0xAA8, nodes[0])
    return c


def enemy(e, pos=(5000.0, 5000.0, 0.0), dead=0):
    t = task(e, 0x1000, 0x63)
    d64(e, t + 0x20, *pos)
    e.write(t + 0xCF5, bytes([dead]))
    return t


def cmd(e, type_, arg):
    c = e.alloc(0x30)
    u32(e, c + 0x20, type_, arg)
    return c


def carai(e, cmds=((2, 2),), target=1, maxSpeed=25.0, stop=0.0, **f):
    ai = task(e, 0x84, AI)
    g = graph(e)
    c = car(e, **f.get("car", {}))
    u32(e, ai + 0x20, 7, 3, 9, g)
    cur = cmd(e, *cmds[0]) if cmds else 0
    u32(e, ai + 0x30, 0x4444 if cmds else 0, cur)
    if len(cmds) > 1:
        RET[0x453730] = cmd(e, *cmds[1])
    u32(e, ai + 0x4C, c, f.get("tick", 5))
    e.write(ai + 0x54, bytes([f.get("started", 1)]))
    f32(e, ai + 0x58, f.get("throttle", 0.5), f.get("steer", 0.1), f.get("brake", 0.0), f.get("speed", 20.0))
    u32(e, ai + 0x68, node(g, target))
    f32(e, ai + 0x6C, maxSpeed, stop)
    u32(e, ai + 0x74, f.get("enemy", 0), f.get("chase", 0), f.get("wait", 0))
    f32(e, ai + 0x80, f.get("cmdSpeed", 30.0))
    return ai


def test_init():
    compare(init, 0x425C80, "CarAI_Init", setup=setup_init, check_eax=False, normalized=True)


def test_create():
    run(emu, 0x425D90, "CarAI_Create", lambda e: (carai(e),))


@pytest.mark.parametrize("c", [
    dict(), dict(target=2), dict(target=3), dict(target=4), dict(target=7), dict(maxSpeed=3.0), dict(maxSpeed=5.0),
    dict(cmds=((2, 1),), target=1), dict(cmds=((2, 1),), target=1, near=3),
    dict(cmds=((2, 1), (3, 2)), target=1), dict(cmds=((2, 1), (3, 5)), target=1, car=dict(pos=(9000.0, 100.0, 0.0))),
    dict(cmds=((2, 1), (6, 0)), target=1), dict(cmds=((2, 1), (7, 0)), target=1, car=dict(pos=(10000.0, 0.0, 0.0))),
    dict(cmds=((2, 1), (1, 9)), target=1), dict(cmds=((3, 1), None), target=1),
    dict(cmds=((2, 2),), target=1, step=None),
])
def test_get_speed(c):
    def setup(e):
        cmds = c.get("cmds", ((2, 2),))
        if cmds[-1] is None:
            cmds = cmds[:-1]
        RET[0x5096F0] = c.get("near", 2)
        RET[0x5096C0] = {} if "step" in c else {(i, j): (i + 1) % 8 for i in range(8) for j in range(8)}
        f = {k: v for k, v in c.items() if k in ("target", "maxSpeed", "car")}
        return (carai(e, cmds=cmds, **f),)
    run(emu, 0x4255A0, "CarAI_GetSpeed", setup, check_st0=True)


@pytest.mark.parametrize("t", [0, 1, 2, 3, 6, 7, 8, 9, -1])
def test_cmd_kinds(t):
    compare(emu, 0x4258C0, "CarAI_IsMoveCmd", args=(t,))
    compare(emu, 0x4258E0, "CarAI_IsStopCmd", args=(t,))


@pytest.mark.parametrize("c", [
    dict(), dict(target=3), dict(target=4), dict(target=7), dict(maxSpeed=4.0), dict(near=1),
    dict(near=2, target=6), dict(near=5, step=None), dict(enemy=(10500.0, 300.0, 0.0), near=0),
    dict(near=3, enemy=(-8000.0, 30000.0, 500.0)), dict(target=0, car=(57344.0, 0.0, 0.0)),
    dict(target=0, car=(57344.000001, 0.0, 0.0)),
])
def test_get_chase_speed(c):
    def setup(e):
        RET[0x5096F0] = c.get("near", 2)
        RET[0x5096C0] = {} if "step" in c else {(i, j): (i + 3) % 8 for i in range(8) for j in range(8)}
        ai = carai(e, target=c.get("target", 1), maxSpeed=c.get("maxSpeed", 25.0), car=dict(pos=c.get("car", (100.0, 200.0, 0.0))))
        u32(e, ai + 0x74, enemy(e, c.get("enemy", (5000.0, 5000.0, 0.0))))
        return (ai,)
    run(emu, 0x425900, "CarAI_GetChaseSpeed", setup, check_st0=True)


def test_get_car_pos():
    run(emu, 0x425B70, "CarAI_GetCarPos", lambda e: (carai(e),), check_eax=True)


@pytest.mark.parametrize("near,target,linked", [(None, 3, 1), (3, 3, 1), (2, 3, 1), (2, 3, 0)])
@pytest.mark.parametrize("which", ["path", "next"])
def test_path(near, target, linked, which):
    def setup(e):
        RET[0x5096F0] = near
        RET[0x5096C0] = {(2, 3): 5} if linked else {}
        ai = carai(e)
        g = e.u32(ai + 0x2C)
        pos = e.alloc(24)
        d64(e, pos, 1.0, 2.0, 3.0)
        if which == "next":
            return pos, g, target, ai
        out = e.alloc(24)
        d64(e, out, 7.0, 8.0, 9.0)
        return out, g, pos, target, ai
    if which == "next":
        run(emu, 0x425C10, "CarAI_NextNode", setup, check_eax=True)
    else:
        run(emu, 0x425B80, "CarAI_GetPathPos", setup)


def run_setup(c):
    def setup(e):
        RET[0x5096F0] = c.get("near", 2)
        RET[0x5096C0] = {(i, j): (i + 1) % 8 for i in range(8) for j in range(8)}
        f = dict(c.get("f", {}))
        if "enemy" in c:
            f["enemy"] = enemy(e, c["enemy"])
        ai = carai(e, **f)
        if "attacker" in c:
            u32(e, e.u32(ai + 0x4C) + 0x2ED0, enemy(e, c["attacker"]))
        if c.get("nocar"):
            u32(e, ai + 0x4C, 0)
        if c.get("nograph"):
            u32(e, ai + 0x2C, 0)
        return (ai,)
    return setup


RUNS = [
    dict(), dict(nocar=1), dict(nograph=1), dict(f=dict(cmds=())), dict(f=dict(cmds=(), started=0)),
    dict(f=dict(started=0)), dict(f=dict(cmds=((3, 4),))), dict(f=dict(cmds=((2, 0),), car=dict(pos=(5.0, 6.0, 7.0)))),
    dict(f=dict(cmds=((1, 3),), wait=2)), dict(f=dict(cmds=((1, 3),), wait=3)), dict(f=dict(cmds=((1, 3),), wait=4)),
    dict(f=dict(cmds=((6, 0),))), dict(f=dict(cmds=((7, 0),))), dict(f=dict(cmds=((8, 45),))),
    dict(f=dict(cmds=((5, 1),))), dict(f=dict(cmds=((0, 1),))),
    dict(f=dict(car=dict(hunting=1)), attacker=(5000.0, 5000.0, 0.0)),
    dict(f=dict(car=dict(hunting=1), chase=3), enemy=(5000.0, 5000.0, 0.0)),
    dict(f=dict(car=dict(hunting=1), chase=3), enemy=(100.0, 200.0, 0.0), near=0),
    dict(f=dict(car=dict(hunting=1), chase=0)),
]


@pytest.mark.parametrize("c", RUNS)
def test_run(c):
    run(emu, 0x425DE0, "CarAI_Run", run_setup(c))


@pytest.mark.parametrize("c", [
    dict(), dict(hunting=1), dict(hunting=1, enemy=1), dict(hunting=1, enemy=1, dead=1), dict(hunting=1, attacker=1),
    dict(hunting=1, chase=5), dict(hunting=1, passengers=((0, 0x4100), (1, 0), (1, 0x4200))),
    dict(hunting=1, passengers=((0, 0x4100),), chase=1), dict(hunting=1, passengers=((1, 0x4100),)),
])
def test_update_chase(c):
    def setup(e):
        ai = carai(e, car=dict(hunting=c.get("hunting", 0), attacker=0x4300 if c.get("attacker") else 0,
                               passengers=c.get("passengers", ())), chase=c.get("chase", 0))
        if c.get("enemy"):
            u32(e, ai + 0x74, enemy(e, dead=c.get("dead", 0)))
        return (ai,)
    run(emu, 0x426000, "CarAI_UpdateChase", setup, check_eax=True)


@pytest.mark.parametrize("i", range(len(NODES)))
@pytest.mark.parametrize("pos", [(100.0, 200.0, 0.0), (0.0, 12288.0, 0.0), (0.0, 12287.0, 0.0), (5.5, 12288.0, 1e-9)])
def test_is_near(i, pos):
    run(emu, 0x4260C0, "CarAI_IsNear", lambda e: (carai(e, car=dict(pos=pos)), i), check_eax=True)


DRIVES = [dict(target=t, yaw=y, maxSpeed=m) for t in (1, 2, 3, 6) for y in (0.0, 0.3, -2.0, 3.1) for m in (0.0, 25.0)]


@pytest.mark.parametrize("c", DRIVES)
def test_drive(c):
    def setup(e):
        RET[0x5096F0] = 2
        RET[0x5096C0] = {(i, j): (i + 1) % 8 for i in range(8) for j in range(8)}
        return (carai(e, target=c["target"], maxSpeed=c["maxSpeed"], car=dict(yaw=c["yaw"])),)
    run(emu, 0x426140, "CarAI_Drive", setup)


@pytest.mark.parametrize("speed", [0.0, 10.8203125, 11.3, 20.0, 37.9, 100.0])
@pytest.mark.parametrize("carspeed", [0.0, 600.0, 1000.0, 2000.0])
@pytest.mark.parametrize("unk720", [0.0, 0.25, 0.99])
@pytest.mark.parametrize("stop", [0.0, 1.0])
def test_get_pedals(speed, carspeed, unk720, stop):
    def setup(e):
        ai = carai(e, stop=stop, speed=speed, car=dict(speed=carspeed, unk720=unk720))
        return ai + 0x58, ai + 0x60, ai
    run(emu, 0x426260, "CarAI_GetPedals", setup)


CHASES = [dict(target=t, yaw=y, maxSpeed=m, enemy=p) for t in (1, 3, 4) for y in (0.0, 0.1, 1.5, -0.12, 0.17)
          for m in (0.0, 20.0) for p in ((5000.0, 5000.0, 0.0), (100.0, 9000.0, 0.0))]


@pytest.mark.parametrize("c", CHASES)
def test_chase(c):
    def setup(e):
        RET[0x5096F0] = 2
        RET[0x5096C0] = {(i, j): (i + 3) % 8 for i in range(8) for j in range(8)}
        ai = carai(e, target=c["target"], maxSpeed=c["maxSpeed"], stop=0.5, car=dict(yaw=c["yaw"]))
        u32(e, ai + 0x74, enemy(e, c["enemy"]))
        return (ai,)
    run(emu, 0x4262F0, "CarAI_Chase", setup)


@pytest.mark.parametrize("brake", [0.0, 0.5, -0.0])
def test_send_control(brake):
    def setup(e):
        ai = carai(e)
        return ai, 77, struct.unpack("<I", struct.pack("<f", 0.75))[0], struct.unpack("<I", struct.pack("<f", -0.5))[0], \
            struct.unpack("<I", struct.pack("<f", brake))[0]
    run(emu, 0x426510, "CarAI_SendControl", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57BC74, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x426590, "CarAI_GetParams", setup)


@pytest.mark.parametrize("cartype", [CAR, 0x55, None])
@pytest.mark.parametrize("graphtype", [GRAPH, 0x55, None])
def test_changed(cartype, graphtype):
    def setup(e):
        ai = carai(e)
        u32(e, ai + 0x4C, 0, 0)
        for id_, kind in ((9, cartype), (7, graphtype)):
            if kind is not None:
                ref = e.alloc(4)
                u32(e, ref, task(e, 0x3000, kind))
                u32(e, 0xA71890 + 4 * id_, ref)
        return (ai,)
    run(emu, 0x4265A0, "CarAI_Changed", setup)


def test_close():
    def setup(e):
        u32(e, 0x53BA50, 0x44)
        u32(e, 0x57BC74, 0x5151)
    run(emu, 0x426610, "CarAI_Close", setup)


@pytest.mark.parametrize("fn", [(0x426140, "CarAI_Drive"), (0x4262F0, "CarAI_Chase"), (0x4255A0, "CarAI_GetSpeed"),
                                (0x425900, "CarAI_GetChaseSpeed")])
@pytest.mark.parametrize("perm", range(3))
@pytest.mark.parametrize("scale", [1.0, 1e5])
def test_sum_order(fn, perm, scale):
    """Car matrix of ones and a target where one term is lost when added to the
    others first, so the result shows the order of the sums."""
    def setup(e):
        RET[0x5096F0] = 2
        RET[0x5096C0] = {}
        ai = carai(e, target=0, maxSpeed=20.0, car=dict(pos=(0.0, 0.0, 0.0)))
        f32(e, e.u32(ai + 0x4C) + 0x70, *[1.0] * 9)
        g = e.u32(ai + 0x2C)
        v = [scale, -scale, scale * 1e-20]
        d64(e, node(g, 0) + 8, *[v[(i + perm) % 3] for i in range(3)])
        u32(e, ai + 0x74, enemy(e))
        return (ai,)
    run(emu, fn[0], fn[1], setup, check_st0=fn[0] in (0x4255A0, 0x425900))
