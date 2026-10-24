"""HumanAI targets, view checks, turning and walking along the AI graph
(src/humanai/target.c, src/humanai/move.c)."""
import math
import random
import struct

import pytest

from emu import Emu
from test_humanai import (Fakes, add_tail, check, f32, f64, fake_common, new_ai, new_human, new_list, p32, ref,
                          set_handler)

TARGETS = 0x57D838
M = Emu()
MF = Fakes(M)
fake_common(MF)
MF.add(0x489BE0, 2, ret=0)
MF.add(0x48EF70, 3, fmt=lambda e, a: (e.normalize(a[0]), a[1], struct.unpack("<f", p32(a[2]))[0]))
MF.add(0x447D70, 1, ret=1)
MF.add(0x447DA0, 1)
MF.add(0x452C10, 3)
MF.add(0x452D20, 6, ret=0x8181, fmt=lambda e, a: (e.normalize(a[0]), a[1], a[2], a[3], a[4], e.read(a[5], 24)))
MF.add(0x509860, 0, ret=4)

NODES = {}
STEPS = {}
INSIDE = set()


def node_at(e, a):
    return NODES.get(a[1], 0)


def node_pos(e, a):
    e.write(a[0], e.read(a[1] + 0x40, 24))


def is_in(e, a):
    return e.read(a[0], 24) in INSIDE or e.u32(a[1]) in INSIDE


def next_step(e, a):
    return STEPS.get((a[1], a[2]), 0)


MF.add(0x4F9720, 2, ret=node_at)
MF.add(0x4F9770, 2, ret=node_pos, fmt=lambda e, a: (a[1],))
MF.add(0x4F9A40, 5, ret=is_in, fmt=lambda e, a: (e.read(a[0], 24), a[1], a[2], a[3], a[4]))
MF.add(0x5096C0, 3, ret=next_step)


def human_at(e, pos, mat=None, dead=0):
    h = new_human(e, dead=dead)
    e.write(h + 0x20, f64(*pos))
    if mat:
        e.write(h + 0x78, f32(*mat))
    return h


# --- targets -----------------------------------------------------------------------

def target(e, slot, task, humans, states=None, used=1):
    t = TARGETS + 0x130 * slot
    e.write(t + 8, bytes([used]))
    e.write(t + 0xC, p32(ref(e, task)))
    e.write(t + 0x10, p32(*humans))
    e.write(t + 0x90, p32(*(states or [7] * len(humans))))
    e.write(t + 0x128, p32(len(humans), 2))
    return t


def clear_targets(e):
    e.write(TARGETS, b"\0" * 0x130 * 8)
    new_list(e, 0x58A9D8)


@pytest.mark.parametrize("case", ["new", "join", "other", "full", "null"])
def test_target_add(case):
    def setup(e):
        clear_targets(e)
        ai = new_ai(e)
        enemy = human_at(e, (5.0, 6.0, 7.0))
        other = human_at(e, (1.0, 1.0, 1.0))
        if case in ("join", "other"):
            add_tail(e, 0x58A9D8, target(e, 2, other, [0x10]))
            add_tail(e, 0x58A9D8, target(e, 0, enemy if case == "join" else other, [0x11, 0x12]))
        if case in ("full", "null"):
            for i in range(8):
                add_tail(e, 0x58A9D8, target(e, i, other, [0x10]))
        return (ai, 0 if case == "null" else enemy)
    check(M, MF, 0x451080, "AITarget_Add", setup=setup, check_eax=True)


def test_target_free():
    def setup(e):
        clear_targets(e)
        t = target(e, 3, 0x1234, [1])
        add_tail(e, 0x58A9D8, t)
        return (t,)
    check(M, MF, 0x4511D0, "AITarget_Free", setup=setup)


@pytest.mark.parametrize("who", [0, 1, 2, 3])
def test_target_states(who):
    def setup(e):
        clear_targets(e)
        ai = new_ai(e)
        h = e.u32(ai + 0x14)
        humans = [0x10, 0x11, 0x12][:who] + ([h] if who < 3 else [])
        t = target(e, 1, 0x1234, humans, states=[5, 6, 5, 9][:len(humans)])
        return (t, ai, 77)
    check(M, MF, 0x4511F0, "AITarget_SetState", setup=setup)
    check(M, MF, 0x451230, "AITarget_GetState", setup=setup, check_eax=True)

    def setup2(e):
        t, ai, _ = setup(e)
        return (t, 5)
    check(M, MF, 0x451270, "AITarget_CountState", setup=setup2, check_eax=True)


@pytest.mark.parametrize("n,pos,flag", [(1, 0, 1), (3, 0, 1), (3, 2, 0), (3, 9, 1), (0, 0, 0)])
def test_target_remove(n, pos, flag):
    def setup(e):
        clear_targets(e)
        ai = new_ai(e)
        h = e.u32(ai + 0x14)
        humans = [0x10 + i for i in range(n)]
        if pos < n:
            humans[pos] = h
        t = target(e, 4, 0x1234, humans, states=list(range(n)))
        add_tail(e, 0x58A9D8, t)
        return (t, ai, flag | 0x100)
    check(M, MF, 0x4512A0, "AITarget_Remove", setup=setup)


# --- small getters -----------------------------------------------------------------

@pytest.mark.parametrize("dead,crouch,seen", [(0, 0, 0), (1, 1, 0x4444)])
def test_target_getters(dead, crouch, seen):
    MF.rets[0x489BE0] = seen

    def setup(e):
        clear_targets(e)
        ai = new_ai(e, human_at(e, (0, 0, 0), dead=dead))
        enemy = human_at(e, (5.5, 6.25, 7.125))
        e.write(enemy + 0x33D, bytes([crouch]))
        e.write(ai + 0x3AA8, p32(target(e, 0, enemy, [1])))
        e.write(ai + 0x1514, p32(9))
        return (ai,)
    check(M, MF, 0x451320, "HumanAI_PopRoute", setup=setup)
    check(M, MF, 0x451330, "HumanAI_IsDead", setup=setup, check_eax=True, eax_mask=0xFF)
    check(M, MF, 0x451350, "HumanAI_CanSeeTarget", setup=setup, check_eax=True)
    check(M, MF, 0x4513E0, "HumanAI_GetTarget", setup=setup, check_eax=True)

    def setup2(e):
        ai, = setup(e)
        return (e.alloc(24), ai)
    check(M, MF, 0x451380, "HumanAI_GetTargetPos", setup=setup2)


def test_can_see_target_none():
    def setup(e):
        ai = new_ai(e)
        e.write(ai + 0x3AA8, p32(target(e, 0, 0, [1])))
        return (ai,)
    check(M, MF, 0x451350, "HumanAI_CanSeeTarget", setup=setup, check_eax=True)


# --- view math ---------------------------------------------------------------------

def rot(yaw, pitch=0.0):
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    return [cy, sy, 0.0, -sy * cp, cy * cp, sp, sy * sp, -cy * sp, cp]


def view_ai(e, rng, alarm=0):
    ai = new_ai(e, human_at(e, (rng.uniform(-9e4, 9e4), rng.uniform(-9e4, 9e4), rng.uniform(0, 4000))))
    eye = [rng.uniform(-9e4, 9e4), rng.uniform(-9e4, 9e4), rng.uniform(0, 4000)]
    e.write(ai + 0x20, f64(eye[0] + 3, eye[1] - 2, eye[2] - 900) + f64(*eye))
    e.write(ai + 0x50, f32(*rot(rng.uniform(-3.2, 3.2))) + p32(0))
    e.write(ai + 0x78, f32(*rot(rng.uniform(-3.2, 3.2), rng.uniform(-1.2, 1.2))) + p32(0))
    e.write(ai + 0xA0, f64(rng.uniform(0.2, 1.5), rng.uniform(0.2, 1.5), rng.uniform(1e3, 2e5),
                           rng.uniform(1e3, 2e5), rng.uniform(0.2, 1.5), rng.uniform(0.2, 1.5),
                           rng.uniform(1e3, 2e5), rng.uniform(1e3, 2e5)))
    e.write(ai + 0x3EC8, f32(rng.choice([0.0259, 3.14159, 0.5]), rng.choice([0.0259, 3.14159, 0.5])))
    e.write(ai + 0x3ABB, bytes([alarm]))
    return ai, eye


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(24))
def test_can_see(cw, seed):
    def setup(e):
        rng = random.Random(seed * 3)
        ai, eye = view_ai(e, random.Random(seed))
        pos = e.alloc(24)
        e.write(pos, f64(*[eye[i] + rng.uniform(-6e4, 6e4) for i in range(3)]))
        return (ai + 0x20, pos, (seed & 1) | 0x500)
    check(M, MF, 0x4502F0, "AIView_CanSee", setup=setup, check_eax=True, eax_mask=0xFF, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_pitch(cw, seed):
    def setup(e):
        ai, _ = view_ai(e, random.Random(seed))
        if seed == 0:
            e.write(ai + 0x50, f32(*[0.0] * 9))
        return (ai,)

    def setup2(e):
        return (setup(e)[0] + 0x20,)
    check(M, MF, 0x451470, "AIView_GetPitch", setup=setup2, check_st0=True, cw=cw)
    check(M, MF, 0x4513F0, "HumanAI_LevelAim", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(16))
@pytest.mark.parametrize("addr,name", [(0x4522D0, "HumanAI_TurnTo"), (0x452580, "HumanAI_AimAt"),
                                       (0x4525B0, "HumanAI_FaceTo")])
def test_turn(cw, seed, addr, name):
    def setup(e):
        rng = random.Random(seed * 7 + 1)
        ai, eye = view_ai(e, random.Random(seed))
        pos = e.alloc(24)
        e.write(pos, f64(*[eye[i] + rng.uniform(-6e4, 6e4) for i in range(3)]))
        tol = rng.choice([0.035, 0.5, 3.0])
        return (ai, pos, struct.unpack("<I", f32(tol))[0], (seed >> 1) & 1)
    check(M, MF, addr, name, setup=setup, check_eax=True, eax_mask=0xFF, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(30))
@pytest.mark.parametrize("add", [0, 1])
def test_edge_point(cw, seed, add):
    def setup(e):
        rng = random.Random(seed)
        node = e.alloc(0x50)
        e.write(node + 0x24, f32(rng.choice([0.5, 1.0, 2.75, 0.0])))
        c = e.alloc(24)
        e.write(c, f64(rng.uniform(-9e4, 9e4), rng.uniform(-9e4, 9e4), rng.uniform(-9e3, 9e3)))
        d = e.alloc(24)
        k = rng.choice([1.0, 100.0, 1e4, 1e5])
        e.write(d, f64(rng.uniform(-k, k) if seed % 5 else 0.0, rng.uniform(-k, k) if seed % 7 else 0.0,
                       0.0 if seed % 3 else 5.0))
        return (e.alloc(24), node, c, d, add | 0x300)
    check(M, MF, 0x4520C0, "Graph_EdgePoint", setup=setup, cw=cw)


# --- channels and sides ------------------------------------------------------------

TYPES = [0x45D5A0, 0x45A7E0, 0x455980, 0x457800]
TYPE_VARS = [0x5BDABC, 0x5BDAA8, 0x5BDA88, 0x5BDA94]


@pytest.mark.parametrize("which", range(5))
@pytest.mark.parametrize("flag", [0, 1, 0xFE])
def test_channel(which, flag):
    def setup(e):
        for i, v in enumerate(TYPE_VARS):
            e.write(v, struct.pack("<H", 0x90 + i))
        h = new_human(e, type_=0x90 + which if which < 4 else 0x61)
        e.write(h + 0xD00, bytes([flag]))
        return (new_ai(e, h),)
    check(M, MF, 0x4525E0, "HumanAI_SetWalk", setup=setup)
    check(M, MF, 0x452660, "HumanAI_SetRun", setup=setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("inside", [0, 1, 2, 3, None])
def test_check_sides(cw, inside):
    def setup(e):
        rng = random.Random(inside or 0)
        h = human_at(e, (rng.uniform(-9e4, 9e4), rng.uniform(-9e4, 9e4), 100.0), rot(rng.uniform(-3, 3), 0.3))
        ai = new_ai(e, h)
        node = e.alloc(0x50)
        e.write(node, p32(55))
        NODES.clear()
        if inside is not None:
            NODES[9] = node
        e.write(ai + 0x3B6C, p32(0x7070))
        e.write(ai + 0x3E2C, p32(9))
        INSIDE.clear()
        # remember which side points count as inside, by their coordinates
        pos = struct.unpack("<3d", e.read(h + 0x20, 24))
        m = struct.unpack("<9f", e.read(h + 0x78, 36))
        x, y, z = (m[0] * -12288.0, m[3] * -12288.0, m[6] * -12288.0)
        if inside and inside & 1:
            INSIDE.add(f64(x + pos[0], y + pos[1], z + pos[2]))
        if inside and inside & 2:
            INSIDE.add(f64(-x + pos[0], -y + pos[1], z + pos[2]))
        return (ai,)
    check(M, MF, 0x4526E0, "HumanAI_CheckSides", setup=setup, check_eax=True, cw=cw)


# --- walking -----------------------------------------------------------------------

DOOR_SLOT = 0x34
DOOR = MF.handler(2, deref={1: 0x24})
TASKS = 0x20F00000


def door_search(e, a):
    q = a[1]
    if e.u32(q + 0x20) == 0:
        e.write(q + 0x20, p32(a[0]))


MF.rets[DOOR] = door_search


def move_ai(e, rng, items=(), door_state=None):
    ai, eye = view_ai(e, rng)
    new_list(e, 0x58A9C8)
    pool = e.alloc(0x28 * 16)
    for i in range(16):
        add_tail(e, 0x58A9C8, pool + 0x28 * i)
    for pos, door, id_ in items:
        it = e.u32(0x58A9C8)
        e.call(0x4AF960, it)
        e.write(it + 8, f64(*pos) + p32(door, id_))
        add_tail(e, ai + 0xE0, it)
    e.write(ai + 0x3B6C, p32(0x7070))
    e.write(ai + 0x3E2C, p32(1))
    return ai, eye


def door_world(e, state, n=2):
    MF.rets[0x447D70] = state
    e.write(0x53C923, bytes([DOOR_SLOT]))
    arr = e.alloc(8 + 4 * n)
    e.write(arr, p32(n, 0))
    for i in range(n):
        t = e.alloc(0x40)
        e.write(t + 0x1C, struct.pack("<H", 0x48 + i))
        if i:
            set_handler(e, DOOR_SLOT, 0x48 + i, DOOR)
        e.write(arr + 8 + 4 * i, p32(t))
    e.write(0x57BA9C, p32(arr))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", ["empty", "far", "near", "near_door", "door1", "door2", "door3", "door0",
                                  "door_waiting"])
def test_move_step(cw, case):
    def setup(e):
        rng = random.Random(len(case))
        NODES.clear()
        node = e.alloc(0x60)
        e.write(node + 0x40, f64(1, 2, 3))
        NODES[1] = node
        ai, eye = view_ai(e, rng)
        base = struct.unpack("<3d", e.read(ai + 0x20, 24))
        d = 500.0 if case.startswith("near") else 40000.0
        items = [] if case == "empty" else [((base[0] + d, base[1] - d / 2, base[2] + 10), int(case.startswith(
            ("door", "near_door"))), 42)]
        ai2, _ = move_ai(e, rng, items)
        e.write(ai2 + 0x20, e.read(ai + 0x20, 0xC0))
        e.write(ai2 + 0x3EC8, e.read(ai + 0x3EC8, 8))
        if case.startswith(("door", "near_door")):
            door_world(e, {"door1": 1, "door2": 2, "door3": 3, "door0": 0}.get(case, 1))
        if case == "door_waiting":
            e.write(ai2 + 0x3E4A, b"\1")
            e.write(ai2 + 0x3E4C, p32(ref(e, e.alloc(0x40))))
        if case == "near_door":
            e.write(ai2 + 0x3E4A, b"\1")
        return (ai2,)
    check(M, MF, 0x451540, "HumanAI_MoveStep", setup=setup, check_eax=True, cw=cw)


def graph(e, ids, flags=None):
    NODES.clear()
    STEPS.clear()
    for i in ids:
        n = e.alloc(0x60)
        e.write(n, p32(i))
        e.write(n + 0x24, f32(0.5 + i * 0.25))
        e.write(n + 0x30, p32((flags or {}).get(i, 4)))
        e.write(n + 0x40, f64(i * 3000.0 + 7, -i * 1500.0 + 3, i * 10.0))
        NODES[i] = n


def route(path):
    for i, a in enumerate(path):
        for dest in path[i + 1:]:
            STEPS[(a, dest)] = (path[i + 1], 1000.0 * (len(path) - i))


def steps_in_memory(e):
    for k, (nxt, dist) in list(STEPS.items()):
        c = e.alloc(8)
        e.write(c, p32(nxt) + f32(dist))
        STEPS[k] = c


MOVE_CASES = ["walking", "walk_done", "same", "same_pos", "route", "route_pos", "route_pos_in", "noroute",
              "nonode", "replan", "replan_pos", "full", "off_node", "off_node_dest", "off_node_step",
              "off_node_badstep", "off_node_in", "doors", "start_in"]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", MOVE_CASES)
def test_move_to(cw, case):
    def setup(e):
        rng = random.Random(len(case) * 3)
        ids = [1, 2, 3, 4, 5]
        graph(e, ids, flags={2: 4, 3: 4, 4: 0} if case == "doors" else None)
        route([1, 2, 3, 4, 5])
        if case == "noroute":
            STEPS.pop((3, 5))
        if case == "nonode":
            NODES.pop(4)
        INSIDE.clear()
        ai, eye = move_ai(e, rng)
        e.write(ai + 0x3E30, p32(9))
        dest = 5
        pos = 0
        if case.endswith(("_pos", "_pos_in", "same_pos")) or case == "replan_pos":
            pos = e.alloc(24)
            e.write(pos, f64(16000.0, -7500.0, 77.0))
        if case.endswith("_in"):
            INSIDE.add(f64(16000.0, -7500.0, 77.0))
        if case == "start_in":
            INSIDE.add(e.read(ai + 0x20, 24))
        if case.startswith("same"):
            e.write(ai + 0x3E2C, p32(5))
        if case.startswith("walk"):
            e.write(ai + 0x3E30, p32(5))
            if case == "walking":
                item = e.u32(0x58A9C8)
                e.call(0x4AF960, item)
                e.write(item + 8, f64(9e4, 9e4, 0) + p32(0, 3))
                add_tail(e, ai + 0xE0, item)
        if case == "full":
            new_list(e, 0x58A9C8)
        if case.startswith("off_node"):
            # not inside node 1; queue holds 1, then 2 (or the destination)
            for nid in (1, 5 if case == "off_node_dest" else 2):
                item = e.u32(0x58A9C8)
                e.call(0x4AF960, item)
                e.write(item + 8, f64(0, 0, 0) + p32(0, nid))
                add_tail(e, ai + 0xE0, item)
            if case == "off_node_in":
                INSIDE.add(2)
            if case == "off_node_badstep":
                STEPS.pop((2, 5))
            if case == "off_node_step":
                STEPS[(2, 5)] = (3, 10.0)
        steps_in_memory(e)
        return (ai, dest, pos, int(case.startswith("replan")) | 0x200)
    check(M, MF, 0x4518E0, "HumanAI_MoveTo", setup=setup, check_eax=True, eax_mask=0xFF, cw=cw)
    if case in ("route", "walk_done"):
        check(M, MF, 0x4522A0, "HumanAI_RunTo", setup=setup, check_eax=True, eax_mask=0xFF, cw=cw)


@pytest.mark.parametrize("empty", [0, 1])
def test_new_move_item(empty):
    def setup(e):
        ai = new_ai(e)
        new_list(e, 0x58A9C8)
        if not empty:
            add_tail(e, 0x58A9C8, e.alloc(0x28))
        return (ai,)
    check(M, MF, 0x452080, "HumanAI_NewMoveItem", setup=setup, check_eax=True)
