import itertools
import random
import struct

import pytest

from test_door_util import (EV_CHANGED, EV_DAC, EV_MAGIC_MOVE, EV_MOVE, NAN, SCALES, check, children, f32, f64,
                            make_emu, new_task, obj, rmatrix, rvec, set_handler, tick, u32)

e, fk = make_emu()
DOOR = 0x62
TINY = 2.0 ** -70
ONES = f32(*[1.0] * 9) + b"\0\0\0\0"

# VarString offsets
OPEN, CLOSE, LOCK = 0x1A0, 0x1F0, 0x240
EXPR = {}


def has_expr(em, a):
    return EXPR.get(a[0] - fk.door, (0, 0))[0]


def eval_expr(em, a):
    return EXPR.get(a[0] - fk.door, (0, 0.0))[1]


def points(em, a):
    em.write(a[0], fk.points[a[2]])


def coll(em, a):
    u32(em, a[1], fk.hit[1])
    u32(em, a[1] + 0x58, fk.hit[0])


fk.rets[0x4F16C0] = has_expr
fk.rets[0x4F16E0] = eval_expr
fk.door = 0
fk.points = {1: f32(1.0, 2.0, 3.0), 2: f32(-4.0, 50.5, 0.25)}
fk.hit = (0, 0)
fk.add(0x414ED0, 3, ret=points, fmt=lambda em, a: (a[1], a[2]))
fk.add(0x4CC2A0, 8, ret=coll, fmt=lambda em, a: (a[0], em.read(a[2], 24), em.read(a[3], 24), a[4], a[5] & 0xFF,
                                                   em.u32(a[6] + 4), a[7]))
e.baseline = e.snapshot()


def tricky():
    out = []
    for perm in itertools.permutations((1.0, TINY, -1.0)):
        out.append(f32(*[perm[i // 3] for i in range(9)]) + b"\x05\0\0\0")
        out.append(f32(*[perm[i % 3] for i in range(9)]) + b"\x07\0\0\0")
    return out


def model_chain(e, solid):
    z = e.alloc(0x80)
    e.write(z + 0x7C, struct.pack("<h", solid))
    y = e.alloc(0x10)
    u32(e, y + 0xC, z)
    x = e.alloc(0x10)
    u32(e, x + 0xC, y)
    m = e.alloc(4)
    u32(e, m, x)
    return m


def door(e, rng=None, scale=SCALES[0], **kw):
    rng = rng or random.Random(3)
    d = obj(e, DOOR, 0x328, rng, scale)
    fk.door = d
    v = dict(slide=(0.0, 0.0), slide_pos=0.0, base=None, pickable=0, open_angle=90.0, open_time=0.5,
             model=b"door_01", angle_target=0.0, angle=0.0, slide_target=0.0, slide_len=0.0,
             flags=(0, 0, 1, 0, 1, 0, 0), opening=0, pick_time=4.0, ticks=5, move_sound=0, e8=0, solid=None)
    v.update(kw)
    e.write(d + 0xF0, rvec(rng))
    e.write(d + 0x108, f32(*v["slide"]))
    e.write(d + 0x110, v["base"] or rmatrix(rng))
    e.write(d + 0x138, f32(v["slide_pos"]))
    e.write(d + 0x13C, bytes([v["pickable"]]))
    e.write(d + 0x140, f32(v["open_angle"], v["open_time"]))
    e.write(d + 0x148, v["model"].ljust(16, b"\0")[:16])
    for i, s in enumerate((b"door_open", b"door_shut", b"door_move")):
        e.write(d + 0x158 + 0x10 * i, s + b"\0")
    e.write(d + 0x188, f32(v["angle_target"], v["angle"], v["slide_target"], v["slide_len"]))
    e.write(d + 0x198, bytes(v["flags"]))
    e.write(d + 0x290, bytes([v["opening"]]))
    e.write(d + 0x294, f32(v["pick_time"]))
    u32(e, d + 0x298, v["ticks"])
    e.write(d + 0x29C, rmatrix(rng))
    e.write(d + 0x2C8, rvec(rng))
    u32(e, d + 0x2E0, v["move_sound"])
    e.write(d + 0x2E8, rvec(rng, 50.0))
    e.write(d + 0x300, rmatrix(rng))
    u32(e, d + 0xE8, v["e8"])
    if v["solid"] is not None:
        u32(e, d + 0x6C, model_chain(e, v["solid"]))
    return d


def parent(e, rng, matrix=None):
    p = new_task(e, 0x30, 0xF0)
    e.write(p + 0x20, rvec(rng))
    e.write(p + 0x70, matrix or rmatrix(rng))
    return p


def move_msg(e, rng, m=None):
    pos, mat, msg = e.alloc(24), e.alloc(40), e.alloc(8)
    e.write(pos, rvec(rng))
    e.write(mat, m or rmatrix(rng))
    u32(e, msg, pos, mat)
    return msg


ZERO_ROW = f32(0.0, 0.0, 0.0, 0.5, 0.25, -1.0, 0.75, 0.0, 0.5) + b"\0\0\0\0"
NAN_ROW = f32(NAN, 0.0, 0.0, 0.5, 0.25, -1.0, 0.75, 0.0, 0.5) + b"\0\0\0\0"


@pytest.mark.parametrize("seed", range(10))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_move(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        d = door(e, rng, slide=(rng.uniform(-3, 3), rng.uniform(-3, 3)), slide_pos=rng.random())
        children(e, fk, d, EV_MOVE, seed % 3, deref={1: 8})
        return d, move_msg(e, rng)
    check(e, fk, 0x445DE0, "Door_MoveHandler", setup, cw=cw)


@pytest.mark.parametrize("m", tricky() + [ZERO_ROW, NAN_ROW])
@pytest.mark.parametrize("side", ["msg", "local"])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_move_order(m, side, cw):
    def setup(e):
        rng = random.Random(5)
        d = door(e, rng)
        e.write(d + 0x300, ONES if side == "msg" else m)
        return d, move_msg(e, rng, m if side == "msg" else ONES)
    check(e, fk, 0x445DE0, "Door_MoveHandler", setup, cw=cw)


@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_place(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        return (door(e, rng, scale=SCALES[seed % len(SCALES)], slide=(rng.uniform(-3, 3), rng.uniform(-3, 3)),
                     slide_pos=rng.random()),)
    check(e, fk, 0x446290, "Door_Place", setup, cw=cw)


@pytest.mark.parametrize("kids", [0, 1, 3])
def test_move_children(kids):
    def setup(e):
        d = door(e)
        children(e, fk, d, EV_MOVE, kids, deref={1: 8})
        return (d,)
    check(e, fk, 0x4463B0, "Door_MoveChildren", setup)


def test_get_move_msg():
    check(e, fk, 0x446450, "Door_GetMoveMsg", lambda e: (e.alloc(8), door(e)))


@pytest.mark.parametrize("best", [-1.0, 0.0, 1e12, 10.0, NAN, 1e30])
def test_find_nearest(best):
    def setup(e):
        d = door(e)
        q = e.alloc(0x28)
        e.write(q, f64(1.0, 2.0, 3.0, best))
        u32(e, q + 0x20, 0x2020)
        return d, q
    check(e, fk, 0x446470, "Door_FindNearest", setup)


@pytest.mark.parametrize("v", [0, 1, 7])
def test_sub_4464b0(v):
    def setup(e):
        a = e.alloc(12)
        e.write(a, bytes([0x55] * 8 + [v]))
        return 0, a
    check(e, fk, 0x4464B0, "sub_4464B0", setup)


def test_open():
    check(e, fk, 0x4464C0, "Door_Open")


@pytest.mark.parametrize("scale", SCALES)
def test_create(scale):
    check(e, fk, 0x4468F0, "Door_CreateHandler", lambda e: (door(e, scale=scale),))


@pytest.mark.parametrize("id_", [-1, 0, 5])
@pytest.mark.parametrize("handled", [0, 1])
@pytest.mark.parametrize("e8", [0, 0x4E8])
def test_delete(id_, handled, e8):
    def setup(e):
        fk.rets[0x4F1050] = id_
        set_handler(e, EV_DAC, DOOR, fk.handler(1) if handled else 0)
        return (door(e, e8=e8),)
    check(e, fk, 0x446AC0, "Door_DeleteHandler", setup)


@pytest.mark.parametrize("locked,pickable", [(0, 0), (1, 0), (1, 1), (0, 1)])
@pytest.mark.parametrize("seed", range(16))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_check_use(locked, pickable, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        d = door(e, rng, pickable=pickable, flags=(locked, 0, 1, 0, 1, 0, 0))
        fk.point = rvec(rng, 100.0)
        base = struct.unpack("<3d", e.read(d + 0x20, 24))
        pt = struct.unpack("<3d", fk.point)
        q = e.alloc(0x88)
        e.write(q + 8, f64(*[b + p + rng.uniform(-3000, 3000) for b, p in zip(base, pt)]))
        if seed == 5:
            e.write(q + 8, f64(NAN))
        return d, q
    check(e, fk, 0x446B50, "Door_CheckUse", setup, cw=cw)


@pytest.mark.parametrize("locked", [0, 1])
def test_sub_446ce0(locked):
    def setup(e):
        h = e.alloc(4)
        u32(e, h, 0x5000)
        return door(e, flags=(locked, 0, 0, 0, 0, 0, 0)), h
    check(e, fk, 0x446CE0, "Door_DrawIcon", setup)


def run_setup(seed):
    def setup(e):
        rng = random.Random(seed)
        pick = rng.choice
        EXPR.clear()
        for off, vals in ((OPEN, [0.0, 1.0, NAN]), (CLOSE, [0.0, 2.0, NAN]), (LOCK, [0.0, 1.6, 255.0, -3.5])):
            EXPR[off] = (pick([0, 1]), pick(vals))
        fk.rets[0x4E6B00] = pick([0, 0x50D])
        fk.rets[0x401CC0] = parent(e, rng) if pick([0, 1]) else 0
        fk.hit = (pick([0, 1]), pick([0, 0x717]))
        fk.points = {1: f32(*[rng.uniform(-50, 50) for _ in range(3)]),
                     2: f32(*[rng.uniform(-50, 50) for _ in range(3)])}
        e8 = 0
        if pick([0, 1]):
            e8 = new_task(e, 0x52, 0x40)
            set_handler(e, 1, 0x52, fk.handler(1))
            children(e, fk, e8, EV_MAGIC_MOVE, pick([0, 2]), deref={1: 12}, type0=0x48)
        angle = pick([0.0, 45.0, 90.0, -90.0, 100.0, 89.9])
        d = door(e, rng, scale=pick(SCALES[:4]), solid=pick([0, 1, -1]),
                 slide=pick([(0.0, 0.0), (1.0, 0.0), (0.0, 2.0), (NAN, 0.0), (1.5, -0.5)]),
                 slide_pos=pick([0.0, 0.5, 1.0, 0.99, 0.01]), slide_len=pick([1.0, 2.0, 0.0]),
                 open_angle=pick([90.0, -90.0, 0.0, NAN, 120.0]), open_time=pick([0.5, 2.0, 0.05]),
                 angle=angle, angle_target=pick([0.0, angle, 90.0]), slide_target=pick([0.0, 1.0]),
                 flags=[pick([0, 1]) for _ in range(6)] + [0], opening=pick([0, 1]),
                 move_sound=pick([0, 0x5050]), e8=e8)
        children(e, fk, d, 1, pick([0, 2]), nargs=1)
        children(e, fk, d, EV_MOVE, pick([0, 1]), deref={1: 8}, type0=0x44)
        return (d,)
    return setup


@pytest.mark.parametrize("seed", range(300))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_run(seed, cw):
    check(e, fk, 0x446D10, "Door_RunHandler", run_setup(seed), cw=cw)


@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_update_local(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        fk.rets[0x401CC0] = parent(e, rng) if seed else 0
        return (door(e, rng),)
    check(e, fk, 0x4474E0, "Door_UpdateLocal", setup, cw=cw)


@pytest.mark.parametrize("m", tricky())
@pytest.mark.parametrize("side", ["own", "parent"])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_update_local_order(m, side, cw):
    def setup(e):
        rng = random.Random(3)
        fk.rets[0x401CC0] = parent(e, rng, m if side == "parent" else ONES)
        return (door(e, rng, base=ONES if side == "parent" else m),)
    check(e, fk, 0x4474E0, "Door_UpdateLocal", setup, cw=cw)


@pytest.mark.parametrize("found,model", [(1, b"door_01"), (0, b"door_01"), (0, b"x*"), (0, b"")])
@pytest.mark.parametrize("arg", [None, b"", b"x"])
@pytest.mark.parametrize("seed", range(3))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_params_changed(found, model, arg, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        fk.rets[0x4CEC10] = 0xC0DE if found else 0
        sub = new_task(e, 0x51, 0x40)
        fk.rets[0x4E0930] = sub
        fk.rets[0x401CC0] = parent(e, rng) if seed else 0
        d = door(e, rng, model=model, slide=[(0.0, 0.0), (3.0, -4.0), (NAN, 1.0)][seed], move_sound=seed and 0x5050,
                 e8=seed and 0x4E8)
        children(e, fk, d, EV_CHANGED, seed)
        a = 0
        if arg is not None:
            a = e.alloc(4)
            e.write(a, arg + b"\0")
        return d, a
    try:
        check(e, fk, 0x447830, "Door_ParamsChanged", setup, cw=cw)
    finally:
        fk.rets[0x4CEC10] = 0xC0DE


def test_get_params():
    def setup(e):
        u32(e, 0x57C1E8, 0x1E8)
        return 0, e.alloc(4)
    check(e, fk, 0x447A80, "Door_GetParams", setup)


@pytest.mark.parametrize("registered", [0, 1])
@pytest.mark.parametrize("addr,name", [(0x447A90, "Door_AddSymbols"), (0x447BE0, "Door_RemoveSymbols")])
def test_symbols(registered, addr, name):
    check(e, fk, addr, name, lambda e: (door(e, flags=(0, 0, 1, 0, 1, 0, registered)),))


def test_close():
    def setup(e):
        u32(e, 0x57C1E8, 0x1E8)
        u32(e, 0x57C1C0, 0x99990044)
    check(e, fk, 0x447D00, "Door_Close", setup)


def test_get_type():
    def setup(e):
        u32(e, 0x57C1C0, 0x99990044)
    check(e, fk, 0x447D20, "Door_GetType", setup, check_eax=True, eax_mask=0xFFFF)


def test_get_pick_time():
    check(e, fk, 0x447D30, "Door_GetPickTime", lambda e: (door(e, pick_time=2.5),), check_st0=True)


@pytest.mark.parametrize("v", [0, 1])
@pytest.mark.parametrize("addr,name", [(0x447D40, "Door_IsLocked"), (0x447D50, "Door_IsPickable")])
def test_getters(v, addr, name):
    check(e, fk, addr, name, lambda e: (door(e, pickable=v, flags=(v, 0, 0, 0, 0, 0, 0)),), check_eax=True,
          eax_mask=0xFF)


def test_set_picked():
    check(e, fk, 0x447D60, "Door_SetPicked", lambda e: (door(e), 0x12345601))


@pytest.mark.parametrize("open_,closed,opening", list(itertools.product([0, 1], repeat=3)))
def test_get_state(open_, closed, opening):
    check(e, fk, 0x447D70, "Door_GetState",
          lambda e: (door(e, flags=(0, open_, closed, 0, 0, 0, 0), opening=opening),), check_eax=True)
