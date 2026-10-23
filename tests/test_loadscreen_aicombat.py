"""AI combat actions: pose states, decisions, cover search (src/loadscreen/aicombat.c).
Every outside call is a Python fake; each test runs random worlds through the original and the rewrite."""
import math
import random
import re
import struct

import pytest

from emu import Emu
from test_loadscreen_util import CWS, Fakes, check, list_new, norm, w32, wd, wf

e = Emu()
fk = Fakes(e)
K = {}


def k(name):
    return lambda em, a: K[name]


def fmt_args(em, a, fi):
    """Log a printf-style call: the format and the words its conversions use."""
    text = em.read_cstr(a[fi])
    n = sum(2 if c[-1:] in (b"f", b"g") else 1 for c in re.findall(rb"%[-0-9.]*[a-z]", text))
    return (text,) + tuple(norm(em, x) for x in a[fi + 1:fi + 1 + n])


def rnd(em, a):
    seq = K["rand"]
    v = seq[K["ri"] % len(seq)]
    K["ri"] += 1
    return v


def vec(em, p):
    return em.read(p, 24)


fk.add(0x4C1780, 9, fmt=lambda em, a: (norm(em, a[0]), a[1]) + fmt_args(em, a, 2))
fk.add(0x4AF810, 3, fmt=lambda em, a: fmt_args(em, a, 0))
fk.add(0x451350, 1, ret=k("seen"))
fk.add(0x463170, 1, ret=k("dead"))
fk.add(0x463180, 1, ret=k("crouch"))
fk.add(0x463130, 1, ret=3)
fk.add(0x451380, 2, fmt=lambda em, a: (norm(em, a[0]), a[1]), effect=lambda em, a: wd(em, a[0], *K["tpos"]))
fk.add(0x413A70, 0, ret=k("ptype"))
fk.add(0x4028B0, 0, ret=k("ticks"))
fk.add(0x416D20, 0, ret=0x1234)
fk.add(0x4B47C0, 2, ret=2)
fk.add(0x4B4770, 1, st0="d", ret=rnd)
fk.add(0x4E6B00, 2, fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x453200, 1, ret=k("tnode"))
fk.add(0x4C3440, 1, ret=k("hpos"))
fk.add(0x4B32B0, 2, st0="d", ret=k("dist"), fmt=lambda em, a: (vec(em, a[0]), vec(em, a[1])))


def normalize(em, a):
    x, y, z = struct.unpack("<3d", em.read(a[0], 24))
    n = math.sqrt(x * x + y * y + z * z) or 1.0
    wd(em, a[0], x / n, y / n, z / n)


fk.add(0x4B3100, 1, fmt=lambda em, a: (vec(em, a[0]),), effect=normalize)
fk.add(0x4148F0, 6, st0="d", ret=k("pitch"), fmt=lambda em, a: tuple(a))
fk.add(0x414950, 6, st0="d", ret=k("yaw"), fmt=lambda em, a: tuple(a))
fk.add(0x450CE0, 1, deref={0: 28})
fk.add(0x4513F0, 1)
fk.add(0x48EFE0, 2, ret=lambda em, a: K["pose"].get(a[1], 0))
fk.add(0x48EF70, 3)
fk.add(0x451080, 2, ret=k("newtarget"))
fk.add(0x4511F0, 3)
fk.add(0x451230, 2, ret=k("tstate"))
fk.add(0x451270, 2, ret=k("tcount"))
fk.add(0x450FF0, 2)
fk.add(0x4F9720, 2, ret=lambda em, a: K["nodes"].get(a[1], K["nodes"][0]))
fk.add(0x4F9770, 2, fmt=lambda em, a: (norm(em, a[0]), a[1]),
       effect=lambda em, a: wd(em, a[0], *K["npos"][em.u32(a[1]) % len(K["npos"])]))
fk.add(0x4F98A0, 3, fmt=lambda em, a: (norm(em, a[0]), a[1], a[2]), effect=lambda em, a: wf(em, a[0], *K["nmat"]))
fk.add(0x4F9A40, 5, ret=k("innode"), fmt=lambda em, a: (vec(em, a[0]), a[1], a[2], a[3], a[4]))
fk.add(0x5096C0, 3, ret=lambda em, a: K["steps"].get((a[1], a[2]), 0))
fk.add(0x509880, 0, ret=0x10)
fk.add(0x509890, 2, ret=k("masked"))
fk.add(0x452BE0, 2, ret=lambda em, a: K["grids"].get(a[1], 0))
fk.add(0x452C10, 3)
fk.add(0x452C60, 4, ret=lambda em, a: 0x7000 + a[1])
fk.add(0x453040, 4, fmt=lambda em, a: (norm(em, a[0]), norm(em, a[1]), a[2], a[3]),
       effect=lambda em, a: w32(em, a[0], K["gw"]) or w32(em, a[1], K["gh"]))
fk.add(0x416850, 0, ret=0x21)
fk.add(0x416920, 0, ret=0x9999)
fk.add(0x4CE410, 8, ret=k("los"),
       fmt=lambda em, a: (a[0], vec(em, a[1]), vec(em, a[2]), a[3], em.read(a[4], 1), em.read(a[4] + 4, 12), a[5], a[6]),
       effect=lambda em, a: wf(em, a[7], K["vis"]))
fk.add(0x4F2060, 2, ret=k("weapon"))
fk.add(0x4F23D0, 2, fmt=lambda em, a: (a[0], norm(em, a[1])), effect=lambda em, a: w32(em, a[1], K["ammo"], 99))
fk.add(0x413C80, 1, ret=k("wtype"))
fk.add(0x475310, 3)
fk.add(0x4526E0, 1, ret=k("sides"))
fk.add(0x4522A0, 4, ret=k("arrived"))
fk.add(0x4525B0, 4, fmt=lambda em, a: (a[0], vec(em, a[1]), a[2], a[3]))
fk.add(0x452660, 1)
fk.add(0x4F1050, 1, ret=77)
fk.add(0x4F1030, 1, ret=k("gun"))
STEPS = [0x48D830, 0x48D870, 0x48D8B0, 0x48DB10, 0x48DBB0, 0x48DBF0, 0x48DC30, 0x48DC80, 0x48DCD0, 0x48DD10,
         0x48DDC0, 0x48DE00]
for s in STEPS:
    fk.add(s, 2, ret=k("stepret"))
e.baseline = e.snapshot()

AI_SIZE = 0x3ED0
FLOATS = [0.0, 0.1, 0.39, 0.4, 0.5, 0.79, 0.9, 1.0]


def world(em, seed):
    """A random AI in combat: returns (ai, action)."""
    r = random.Random(seed)
    K.clear()
    ai = em.alloc(AI_SIZE)
    human = em.alloc(0x800)
    w32(em, ai + 0x14, human)
    a = em.alloc(0x30)
    param = em.alloc(0x30)
    w32(em, a + 0x28, param)
    cd = param + 8
    # events: the first has no task, the second points at the player
    player = em.alloc(0x40)
    em.write(player + 0x1C, struct.pack("<H", 0x55))
    ref = em.alloc(4)
    w32(em, ref, player if r.random() < 0.8 else 0)
    evs = [em.alloc(0x30) for _ in range(r.choice([0, 1, 2, 3]))]
    if len(evs) > 1:
        w32(em, evs[1] + 0x2C, ref)
    if len(evs) > 2:
        w32(em, evs[2] + 0x2C, ref)
    w32(em, cd, list_new(em, evs))
    wd(em, cd + 8, r.choice([0.0, 500.0, 9000.0, 40000.0]))
    w32(em, cd + 0x14, r.choice([0, 1, 30]), r.choice([0, 3, 14, 15, 29, 44]), r.choice([0, 7, 19]))
    em.write(cd + 0x20, bytes([r.choice([0, 1]), r.choice([0, 0, 1])]))
    w32(em, a + 0x1C, 0x48C750)
    if r.random() < 0.25:
        w32(em, a + 0x24, r.choice(STEPS))
    # target and what the AI sees of it
    target = em.alloc(0x130)
    wd(em, target + 0x110, 1000.0, 2000.0, 300.0)
    w32(em, ai + 0x3AA8, target if r.random() < 0.95 else 0)
    seen = em.alloc(0x30)
    w32(em, seen + 8, ref)
    em.write(seen + 0x28, bytes([r.choice([0, 1]), r.choice([0, 1])]))
    K["seennode"] = seen
    K["seen"] = seen if r.random() < 0.95 else 0
    K["dead"] = int(r.random() < 0.05)
    K["crouch"] = r.choice([0, 1])
    K["ptype"] = r.choice([0x55, 0x56])
    K["ticks"] = r.choice([100, 1000])
    w32(em, 0x5416CC, r.choice([-1, 50, 950, 990]))
    K["rand"] = [r.choice(FLOATS) for _ in range(8)]
    K["ri"] = 0
    # graph
    graph = em.alloc(0x100)
    w32(em, graph + 0x5C, r.choice([10, 10, 10, 0x400]))
    wd(em, graph + 0x80, 1.5)
    w32(em, ai + 0x3B68, 7, graph)
    nodes = {}
    n_nodes = 6
    for i in range(n_nodes):
        n = em.alloc(0x240)
        w32(em, n, i)
        wf(em, n + 0x24, r.choice([-1.0, 0.5, 1.0, 3.0]))
        w32(em, n + 0x134, graph)
        nodes[i] = n
    for i in range(n_nodes):
        for j, other in enumerate(r.sample(range(n_nodes), 3)):
            if other == i or r.random() < 0.3:
                continue
            link = em.alloc(0x10)
            if r.random() < 0.5:
                w32(em, link + 8, i, other)
            else:
                w32(em, link + 8, other, i)
            w32(em, nodes[i] + 0x13C + 4 * r.randrange(64), link)
    K["nodes"] = nodes
    K["tnode"] = nodes[r.randrange(n_nodes)] if r.random() < 0.85 else 0
    w32(em, ai + 0x3E2C, r.randrange(n_nodes))
    K["npos"] = [(100.0 * i, -50.0 * i, 10.0 + i) for i in range(n_nodes)]
    K["nmat"] = [0.6, 0.8, 0.0, -0.8, 0.6, 0.0, 0.0, 0.0, 1.0]
    steps = {}
    for f in range(n_nodes):
        for t in range(n_nodes):
            if r.random() < 0.85:
                st = em.alloc(8)
                w32(em, st, t)
                wf(em, st + 4, r.choice([1.0, 2.0, 3.0, 5.0]))
                steps[(f, t)] = st
    K["steps"] = steps
    grids = {}
    for i in range(n_nodes):
        if r.random() < 0.5:
            g = em.alloc(0x660)
            w32(em, g + 0xC, i, r.choice([0, 3, 100]), r.choice([0, 2, 3]), r.choice([0, 1, 4]))
            for c in range(400):
                w32(em, g + 0x20 + 4 * c, r.choice([0, 25, 100]))
            grids[i] = g
    K["grids"] = grids
    own = r.choice([0] + list(grids.values()))
    w32(em, ai + 0x3EA0, own, r.randrange(3), r.randrange(3))
    K["gw"], K["gh"] = r.choice([(2, 2), (3, 1), (5, 4), (1, 1)])
    w32(em, 0x5C8BB8, r.choice([-1, 0, 1, 2, 3, 4]))
    K["los"] = r.choice([0, 1])
    K["vis"] = r.choice([0.0, 0.5, 0.79, 1.0])
    K["innode"] = r.choice([0, 1, 1])
    K["masked"] = int(r.random() < 0.2)
    K["sides"] = r.choice([0, 1, 2, 3])
    K["arrived"] = r.choice([0, 1])
    K["tstate"] = r.choice([0, 1])
    K["tcount"] = r.choice([0, 2, 3, 4, 5])
    K["newtarget"] = target
    K["dist"] = r.choice([0.0, 300.0, 5000.0, 30000.0, 100000.0])
    K["tpos"] = (1000.5, 2000.25, 300.125)
    hpos = em.alloc(24)
    wd(em, hpos, 10.0, 20.0, 30.0)
    K["hpos"] = hpos
    K["pose"] = {p: int(r.random() < 0.7) for p in range(5)}
    K["stepret"] = r.choice([0, 1])
    K["pitch"] = r.choice([-0.3, 0.05, 0.2, 0.7])
    K["yaw"] = r.choice([-0.4, -0.1, 0.1, 0.9])
    # weapon
    weapon = em.alloc(0x40)
    K["weapon"] = weapon if r.random() < 0.85 else 0
    K["ammo"] = r.choice([0, 5, 5, 256])
    wtype = em.alloc(0x660)
    w32(em, wtype + 0x610, r.choice([-1, 0, 2, 3, 4]))
    w32(em, wtype + 0x648, r.choice([1, 10, 30]), r.choice([0, 1, 3, 10]))
    K["wtype"] = wtype
    gun = em.alloc(0x220)
    wd(em, gun + 0xF0, 500.0, 600.0, 70.0)
    wf(em, gun + 0x108, 0.36, 0.48, 0.8, -0.8, 0.6, 0.0, -0.48, -0.64, 0.6)
    wf(em, gun + 0x1E0, 30.0, -20.0, 45.0)
    w32(em, gun + 0x208, r.choice([human, player]))
    em.write(gun + 0x21C, bytes([r.choice([0, 1])]))
    K["gun"] = gun if r.random() < 0.8 else 0
    gref = em.alloc(4)
    w32(em, gref, gun)
    w32(em, human + 0x7B4, gref)
    # AI fields
    wd(em, ai + 0x20, 15.0, 25.0, 35.0)
    wf(em, ai + 0x78, *r.choice([(0.36, 0.48, 0.8, -0.8, 0.6, 0.0, -0.48, -0.64, 0.6),
                                 (1.0, 0.4431, 0.0, 0.0, 0.8885, 0.0, 0.0, 0.1192, 1.0)]))
    wd(em, ai + 0xB0, r.choice([0.0, 4000.0, 60000.0]))
    for ev in range(24):
        w32(em, ai + 0x87C + 4 * ev, int(r.random() < 0.1))
    w32(em, ai + 0x3AC8, r.choice([-1, 12]))
    w32(em, ai + 0x3D38, 7)
    w32(em, ai + 0x3D44, 11, 22, r.choice([0, 3, 8]), r.choice([0, 2, 5]))
    wf(em, ai + 0x3D54, r.choice(FLOATS), r.choice([0.0, 0.5, 1.0]), 0.0, r.choice([0.0, 0.4, 1.0]))
    wf(em, ai + 0x3D68, 0.1, 0.6, 0.25, 0.75, 2.0, 3.0)
    wf(em, ai + 0x3D88, r.choice(FLOATS), r.choice(FLOATS), 1000.0, 20000.0)
    wd(em, ai + 0x3DB0, r.choice([0.0, 400.0]), r.choice([-1.0, 0.0, 6000.0]), r.choice([1000.0, 50000.0]))
    w32(em, ai + 0x3DD0, r.choice([-1, 0, 0, 1, 2]), 5, 6, r.choice([0, 0, 5]))
    w32(em, ai + 0x3DE4, 0xDEAD, r.choice([-1, 2]), r.choice([-1, 1, 2]))
    em.write(ai + 0x3DF0, bytes([r.choice([0, 1])]))
    w32(em, ai + 0x3DF4, r.choice([-1, 0, 1]), r.choice([-1, 50, 100, 5000]))
    em.write(ai + 0x3DFD, bytes([r.choice([0, 1])]))
    w32(em, ai + 0x3E00, r.choice([-1, 3]))
    wd(em, ai + 0x3E88, 900.0, 1900.0, 250.0)
    return ai, a


SEEDS = range(200)
COMBAT = [
    (0x48ACC0, "AICombatAnya_Stand"),
    (0x48BC00, "AICombat_Stand"),
    (0x48C280, "AICombat_Kneel"),
    (0x48C540, "AICombat_Liedown"),
    (0x48C760, "AICombat_Reposition"),
    (0x48C860, "AICombat_Follow"),
    (0x48CAB0, "AICombat_LookForTarget"),
    (0x48CBD0, "AICombatRPG_Stand"),
    (0x48CDF0, "AICombatRPG_Kneel"),
    (0x48D080, "AICombatGuard_Stand"),
    (0x48D340, "AICombatGunner_Stand"),
    (0x48AB80, "AIDead_Update"),
]


@pytest.mark.parametrize("addr,name", COMBAT)
@pytest.mark.parametrize("seed", SEEDS)
def test_combat_state(addr, name, seed):
    check(e, fk, addr, name, setup=lambda em: world(em, seed), check_eax=False, cw=CWS[seed % 2])


STARTS = [
    (0x48AB70, "AIDead_Start"),
    (0x48ABA0, "AICombatPatrol_Start"),
    (0x48AC60, "AICombatAnya_Start"),
    (0x48AC90, "AICombatAnya_Enter"),
    (0x48BBD0, "AICombat_EnterStand"),
    (0x48C250, "AICombat_EnterKneel"),
    (0x48C510, "AICombat_EnterLiedown"),
    (0x48C750, "AICombat_EnterReposition"),
    (0x48C850, "AICombat_EnterFollow"),
    (0x48CAA0, "AICombat_EnterLookForTarget"),
    (0x48CB40, "AICombatRPG_Start"),
    (0x48D000, "AICombatGuard_Start"),
    (0x48D2C0, "AICombatGunner_Start"),
]


@pytest.mark.parametrize("addr,name", STARTS)
@pytest.mark.parametrize("seed", range(12))
def test_start(addr, name, seed):
    def setup(em):
        ai, a = world(em, seed)
        w32(em, a + 0x18, 0, 0, 0)
        return (ai, a)
    check(e, fk, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("seed", SEEDS)
def test_update(seed):
    def setup(em):
        ai, a = world(em, seed)
        return (ai, em.u32(a + 0x28) + 8)
    check(e, fk, 0x48A920, "AICombat_Update", setup=setup, check_eax=False, cw=CWS[seed % 2])


@pytest.mark.parametrize("seed", range(10))
def test_alert_others(seed):
    def setup(em):
        ai, a = world(em, seed)
        w32(em, ai + 0x3DE4, K["seennode"])
        return (ai,)
    check(e, fk, 0x48AB10, "AICombat_AlertOthers", setup=setup, check_eax=False)


@pytest.mark.parametrize("enter", [0, 0x48BBD0])
@pytest.mark.parametrize("step", [0, 0x48DB10])
def test_set_step(enter, step):
    def setup(em):
        ai, a = world(em, 1)
        return (ai, a, enter, step)
    check(e, fk, 0x48AE70, "AIAction_SetStep", setup=setup, check_eax=False)


@pytest.mark.parametrize("addr,name", [(0x48AE90, "AICombat_DecideFemale"), (0x48BFD0, "AICombat_Decide"),
                                       (0x48BE30, "AICombat_ChooseEvade")])
@pytest.mark.parametrize("seed", range(200))
def test_decide(addr, name, seed):
    def setup(em):
        ai, a = world(em, seed)
        w32(em, ai + 0x3DE4, K["seennode"])
        return (ai, em.u32(a + 0x28) + 8)
    check(e, fk, addr, name, setup=setup, cw=CWS[seed % 2])


@pytest.mark.parametrize("seed", range(150))
def test_find_cover(seed):
    def setup(em):
        ai, a = world(em, seed)
        out = em.alloc(4)
        w32(em, out, 0x5555)
        return (out, ai, seed % 2, (0, 1, 3, 5, 20)[seed % 5])
    check(e, fk, 0x48B050, "AICombat_FindCover", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(150))
def test_try_node(cw, seed):
    def setup(em):
        ai, a = world(em, seed)
        w32(em, 0x5C8BB4, seed % 6, (seed // 6) % 6)
        node = K["nodes"][seed % 6]
        if seed % 3 == 0:
            K["grids"][seed % 6] = em.u32(ai + 0x3EA0) or 0
        return (ai, node, seed % 2, (seed // 2) % 2)
    check(e, fk, 0x48B260, "AICombat_TryNode", setup=setup, cw=cw, eax_mask=0xFF)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(20))
def test_try_cell(cw, seed):
    def setup(em):
        ai, a = world(em, seed)
        g = em.alloc(0x660)
        w32(em, g + 0xC, 2, 0, (3, 4, 5, 1)[seed % 4], (2, 3, 1, 4)[seed % 4])
        for c in range(400):
            w32(em, g + 0x20 + 4 * c, (0, 24, 25, 60)[(c + seed) % 4])
        return (ai, g, K["nodes"][seed % 6], seed % 5, (seed // 5) % 4)
    check(e, fk, 0x48B780, "AICombat_TryCell", setup=setup, cw=cw, eax_mask=0xFF)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(12))
def test_can_see_from(cw, seed):
    def setup(em):
        ai, a = world(em, seed)
        return (K["nodes"][seed % 6], ai)
    check(e, fk, 0x48B6E0, "AICombat_CanSeeFrom", setup=setup, cw=cw, eax_mask=0xFF)


@pytest.mark.parametrize("seed", range(10))
def test_gun_ammo(seed):
    check(e, fk, 0x48B980, "HumanAI_GetGunAmmo", setup=lambda em: (world(em, seed)[0],))


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(80))
def test_start_burst(cw, seed):
    def setup(em):
        ai, a = world(em, seed)
        K["dist"] = (-1.0, 0.0, 0.005, 300.0, 5000.0, 1e6)[seed % 6]
        K["rand"] = [(0.0, 0.33, 0.5, 0.77, 1.0)[seed % 5]]
        return (ai, seed % 2)
    check(e, fk, 0x48B9C0, "AICombat_StartBurst", setup=setup, cw=cw, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("order", [-1, 0, 1, 2, 3, 4])
@pytest.mark.parametrize("free", [-1, 0, 5, 11])
@pytest.mark.parametrize("size", [(3, 4), (4, 3), (1, 1), (0, 2)])
def test_try_node_cells(cw, order, free, size):
    """A node whose grid has room: the cells are tried in the order picked by 0x5C8BB8."""
    def setup(em):
        ai, a = world(em, 3)
        node = K["nodes"][2]
        g = em.alloc(0x660)
        w32(em, g + 0xC, 2, 1, size[0], size[1])
        for c in range(400):
            w32(em, g + 0x20 + 4 * c, 0 if c == free else 30)
        K["grids"][2] = g
        w32(em, ai + 0x3EA0, 0)
        w32(em, 0x5C8BB8, order)
        return (ai, node, 0, 0)
    check(e, fk, 0x48B260, "AICombat_TryNode", setup=setup, cw=cw, eax_mask=0xFF)


def lie_evade(em, ai, a):
    """Under fire with the lie down chance below the first random number."""
    w32(em, ai + 0x3DF8, -1)
    w32(em, ai + 0x87C + 4 * 16, 1)
    K["masked"] = 0
    K["rand"] = [0.9, 0.0, 0.5]
    wf(em, ai + 0x3D54, 1.0, 0.5)


def kneel_grenade(em, ai, a):
    """Kneeling, visible, no ammo, grenade in range."""
    K["pose"] = {0: 0, 1: 1, 2: 0, 3: 0, 4: 0}
    K["seen"] = K["seennode"]
    K["dead"] = 0
    em.write(K["seennode"] + 0x29, b"\x01")
    w32(em, ai + 0x3DF8, 5000)
    w32(em, ai + 0x3DE8, 2)
    w32(em, a + 0x24, 0)
    K["ammo"] = 0
    K["weapon"] = 0x4444
    K["dist"] = 5000.0
    wd(em, ai + 0x3DB0, 0.0, 10.0)
    wf(em, ai + 0x3D88, 1.0, 1.0, 1000.0, 20000.0)
    em.write(ai + 0x3DFD, b"\x01")


def rpg_kneel_chance(em, ai, a):
    """RPG with ammo, standing, the random number below 0.4."""
    K["pose"] = {0: 1, 1: 1, 2: 0, 3: 0, 4: 0}
    K["seen"] = K["seennode"]
    K["dead"] = 0
    em.write(K["seennode"] + 0x28, b"\x01\x01")
    w32(em, a + 0x24, 0)
    K["ammo"] = 5
    K["weapon"] = 0x4444
    K["rand"] = [0.1]
    w32(em, ai + 0x3DD0, 0)
    w32(em, ai + 0x3DDC, 0)


TARGETED = [
    (0x48BE30, "AICombat_ChooseEvade", lie_evade, 1),
    (0x48BC00, "AICombat_Stand", lie_evade, 0),
    (0x48C280, "AICombat_Kneel", kneel_grenade, 0),
    (0x48CBD0, "AICombatRPG_Stand", rpg_kneel_chance, 0),
    (0x48CDF0, "AICombatRPG_Kneel", rpg_kneel_chance, 0),
]


@pytest.mark.parametrize("addr,name,mod,cd", TARGETED)
@pytest.mark.parametrize("seed", range(8))
def test_targeted(addr, name, mod, cd, seed):
    def setup(em):
        ai, a = world(em, seed)
        K["seen"] = K["seennode"]
        K["dead"] = 0
        mod(em, ai, a)
        return (ai, em.u32(a + 0x28) + 8) if cd else (ai, a)
    check(e, fk, addr, name, setup=setup, check_eax=bool(cd), cw=CWS[seed % 2])


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("size", [(1, 1), (1, 3), (3, 1), (-4, -6), (-1, 2)])
@pytest.mark.parametrize("radius", [-1.0, 0.5])
def test_try_node_clamp(cw, size, radius):
    """Odd node sizes and radii, so every clamp of the cover offset applies."""
    def setup(em):
        ai, a = world(em, 4)
        node = K["nodes"][1]
        wf(em, node + 0x24, radius)
        K["grids"].pop(1, None)
        K["gw"], K["gh"] = size
        return (ai, node, 0, 0)
    check(e, fk, 0x48B260, "AICombat_TryNode", setup=setup, cw=cw, eax_mask=0xFF)
