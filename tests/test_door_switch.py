import itertools
import random

import pytest

from test_door_util import (EV_DAC, EV_MOVE, NAN, SCALES, check, children, f32, f64, make_emu, new_task, obj,
                            rmatrix, rvec, set_handler, tick, u32)

e, fk = make_emu()
SW = 0x61
TINY = 2.0 ** -70


def tricky():
    """Matrices whose products lose a term when summed in the wrong order."""
    out = []
    for perm in itertools.permutations((1.0, TINY, -1.0)):
        out.append(f32(*[perm[i // 3] for i in range(9)]) + b"\x05\0\0\0")
        out.append(f32(*[perm[i % 3] for i in range(9)]) + b"\x07\0\0\0")
    return out


def switch(e, rng=None, scale=SCALES[0], flags=(1, 1, 0, 0, 0, 0), press_tick=100, registered=0, destroyable=1,
           matrix=None, local=None):
    rng = rng or random.Random(2)
    s = obj(e, SW, 0x228, rng, scale)
    e.write(s + 0xF0, rvec(rng))
    e.write(s + 0x108, matrix or rmatrix(rng))
    e.write(s + 0x130, rvec(rng, 50.0))
    e.write(s + 0x148, local or rmatrix(rng))
    for i in range(5):
        e.write(s + 0x170 + 0x10 * i, b"switch_m%d\0" % i)
    e.write(s + 0x1C0, bytes(flags))
    u32(e, s + 0x1C8, press_tick)
    e.write(s + 0x1CC, bytes([registered]))
    e.write(s + 0x220, bytes([destroyable]))
    return s


def test_press():
    def setup(e):
        tick(e, 4321)
        return (switch(e, flags=(1, 0, 0, 0, 0, 1)),)
    check(e, fk, 0x444BC0, "Switch_Press", setup)


def test_open():
    check(e, fk, 0x444BF0, "Switch_Open")


@pytest.mark.parametrize("scale", SCALES)
def test_create(scale):
    def setup(e):
        u32(e, 0xA44344, 0x4344)
        return (switch(e, scale=scale),)
    check(e, fk, 0x444F20, "Switch_CreateHandler", setup)


RUN = [(0, 0.0), (1, 0.0), (1, 1.0), (1, 2.6), (1, -1.5), (1, 256.0), (1, 0.5)]


@pytest.mark.parametrize("expr,value", RUN)
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_run_expr(expr, value, cw):
    def setup(e):
        fk.rets[0x4F16C0] = expr
        fk.rets[0x4F16E0] = value
        tick(e, 50)
        s = switch(e)
        children(e, fk, s, 1, 2, nargs=1)
        return (s,)
    check(e, fk, 0x4450F0, "Switch_RunHandler", setup, cw=cw)


@pytest.mark.parametrize("flags", range(64))
@pytest.mark.parametrize("now", [102, 103])
@pytest.mark.parametrize("sound", [0, 0x50D])
def test_run_state(flags, now, sound):
    def setup(e):
        fk.rets[0x4F16C0] = 0
        fk.rets[0x4E6B00] = sound
        tick(e, now)
        return (switch(e, flags=[flags >> i & 1 for i in range(6)]),)
    check(e, fk, 0x4450F0, "Switch_RunHandler", setup)


@pytest.mark.parametrize("id_", [-1, 0, 5])
@pytest.mark.parametrize("handled", [0, 1])
def test_delete(id_, handled):
    def setup(e):
        fk.rets[0x4F1050] = id_
        set_handler(e, EV_DAC, SW, fk.handler(1) if handled else 0)
        return (switch(e),)
    check(e, fk, 0x445270, "Switch_DeleteHandler", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57C1B4, 0x1B4)
        return 0, e.alloc(4)
    check(e, fk, 0x4452E0, "Switch_GetParams", setup)


def parent(e, rng, matrix=None):
    p = new_task(e, 0x30, 0xF0)
    e.write(p + 0x20, rvec(rng))
    e.write(p + 0x70, matrix or rmatrix(rng))
    return p


@pytest.mark.parametrize("has_parent", [0, 1])
@pytest.mark.parametrize("scale", SCALES[:5])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_params_changed(has_parent, scale, cw):
    def setup(e):
        rng = random.Random(7)
        fk.rets[0x401CC0] = parent(e, rng) if has_parent else 0
        return (switch(e, rng, scale=scale, flags=(0, 1, 0, 0, 0, 0)),)
    check(e, fk, 0x4452F0, "Switch_ParamsChanged", setup, cw=cw)


@pytest.mark.parametrize("seed", range(12))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_update_local(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        fk.rets[0x401CC0] = parent(e, rng) if seed else 0
        return (switch(e, rng),)
    check(e, fk, 0x4453E0, "Switch_UpdateLocal", setup, cw=cw)


@pytest.mark.parametrize("m", tricky())
@pytest.mark.parametrize("side", ["own", "parent"])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_update_local_order(m, side, cw):
    ones = f32(*[1.0] * 9) + b"\0\0\0\0"

    def setup(e):
        rng = random.Random(3)
        fk.rets[0x401CC0] = parent(e, rng, m if side == "parent" else ones)
        return (switch(e, rng, matrix=ones if side == "parent" else m),)
    check(e, fk, 0x4453E0, "Switch_UpdateLocal", setup, cw=cw)


@pytest.mark.parametrize("destroyed", [0, 1])
@pytest.mark.parametrize("seed", range(16))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_check_use(destroyed, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        s = switch(e, rng, flags=(1, 1, destroyed, 0, 0, 0))
        fk.point = rvec(rng, 100.0)
        q = e.alloc(0x88)
        e.write(q + 8, rvec(rng, 4000.0) if seed % 3 else f64(*[NAN] * 3))
        return s, q
    check(e, fk, 0x445730, "Switch_CheckUse", setup, cw=cw)


def test_sub_445820():
    def setup(e):
        h = e.alloc(4)
        u32(e, h, 0x5000)
        return 0, h
    check(e, fk, 0x445820, "Switch_DrawIcon", setup)


@pytest.mark.parametrize("destroyable", [0, 1])
def test_destroy(destroyable):
    check(e, fk, 0x445840, "Switch_Destroy", lambda e: (switch(e, destroyable=destroyable),))


@pytest.mark.parametrize("registered", [0, 1])
@pytest.mark.parametrize("addr,name", [(0x445860, "Switch_AddSymbols"), (0x445980, "Switch_RemoveSymbols")])
def test_symbols(registered, addr, name):
    check(e, fk, addr, name, lambda e: (switch(e, registered=registered),))


def move_msg(e, rng, m=None):
    pos, mat, msg = e.alloc(24), e.alloc(40), e.alloc(8)
    e.write(pos, rvec(rng))
    e.write(mat, m or rmatrix(rng))
    u32(e, msg, pos, mat)
    return msg


@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("kids", [0, 2])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_move(seed, kids, cw):
    def setup(e):
        rng = random.Random(seed)
        s = switch(e, rng, scale=SCALES[seed % len(SCALES)])
        children(e, fk, s, EV_MOVE, kids, deref={1: 8})
        return s, move_msg(e, rng)
    check(e, fk, 0x445A80, "Switch_MoveHandler", setup, cw=cw)


@pytest.mark.parametrize("m", tricky())
@pytest.mark.parametrize("side", ["msg", "local"])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_move_order(m, side, cw):
    ones = f32(*[1.0] * 9) + b"\0\0\0\0"

    def setup(e):
        rng = random.Random(5)
        s = switch(e, rng, local=ones if side == "msg" else m)
        return s, move_msg(e, rng, m if side == "msg" else ones)
    check(e, fk, 0x445A80, "Switch_MoveHandler", setup, cw=cw)


def test_close():
    def setup(e):
        u32(e, 0x57C1B4, 0x1B4)
        u32(e, 0x57C1B8, 0x99990044)
    check(e, fk, 0x445DC0, "Switch_Close", setup)
