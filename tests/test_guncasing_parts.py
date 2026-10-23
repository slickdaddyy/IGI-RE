import random
import struct

import pytest

from test_guncasing_util import (CWS, Emu, Fakes, add_child, check, check_error, cstr, f32, new_task, rmatrix, rvec,
                                 set_handler, u32)

e = Emu()
fk = Fakes(e)
FT, CT, MT, RT, OT = 0x37, 0x38, 0x39, 0x3A, 0x3B
EVM, EV1, EV4 = 0x60, 0x61, 0x62
LIST, MODEL, RNG2, WORLD = 0x5A000000, 0x5B000000, 0x5C000000, 0x5D000000
GETTERS = {0x4F1A70: 0x63, 0x4C56D0: 0x64, 0x4C56E0: 0x65, 0x4C56F0: 0x66, 0x4D9FB0: EVM, 0x4D9850: 0x67,
           0x4CEA20: 0x68, 0x4CEA10: 0x69, 0x47E6F0: 0x6A, 0x416830: 0x6B, 0x4C48C0: RT, 0x4D9FA0: 0x6C}
R = {}
s = lambda em, p: em.read_cstr(p)

for addr, v in GETTERS.items():
    fk.add(addr, 0, ret=v)
fk.add(0x401900, 5, ret=lambda em, a: R.setdefault("types", [FT, CT, MT]).pop(0),
       fmt=lambda em, a: (s(em, a[0]), a[1], a[2] & 0xFFFF, a[3], a[4]))
fk.add(0x401400, 3, mask=[0xFFFF, 0xFF])
fk.add(0x401530, 3, mask=[0xFFFF])
fk.add(0x401A20, 1, mask=[0xFFFF])
fk.add(0x4B8810, 2, fmt=lambda em, a: (s(em, a[0]), a[1]))
fk.add(0x4D9860, 0, ret=LIST)
fk.add(0x4C1790, 2)
fk.add(0x4C17C0, 2)
fk.add(0x4CEC10, 1, ret=lambda em, a: R["model"], fmt=lambda em, a: (s(em, a[0]),))
fk.add(0x4C5710, 3)
fk.add(0x4C4950, 3)
fk.add(0x4E75A0, 0, ret=lambda em, a: R["frame"])
fk.add(0x416D30, 0, ret=RNG2)
fk.add(0x4B47C0, 2, ret=lambda em, a: R["rand"])
fk.add(0x4B3640, 3)
fk.add(0x4E81F0, 0, ret=lambda em, a: R["view"])
DRAW = fk.handler(2)
MOVE = fk.handler(2, deref={1: 12})
HIDE = fk.handler(2, fmt=lambda em, a: (a[0], e.u32(a[1]), em.write(a[1], u32(R["vis"])))[:2])
BONE = fk.handler(2, fmt=lambda em, a: (a[0], em.write(a[1], R["bone"]))[:1])
e.baseline = e.snapshot()


def args(em, rng, name=b"flame01"):
    a = em.alloc(0x90)
    em.write(a, u32(cstr(em, name), WORLD))
    em.write(a + 0x20, rvec(rng, 50) + rmatrix(rng) + rmatrix(rng) + u32(rng.randrange(9), rng.randrange(9)))
    return a


def part(em, type_, rng):
    """A part under a gun's collision task, the gun under its owner."""
    owner = new_task(em, OT, 0x40)
    gun = new_task(em, 0x3C, 0x1B0)
    add_child(em, owner, gun)
    coll = new_task(em, 0x3D, 0x40)
    em.write(coll + 0x20, u32(gun))
    p = new_task(em, type_, 0x140)
    add_child(em, coll, p)
    em.write(p + 0x70, rmatrix(rng))
    em.write(p + 0xF0, rvec(rng, 50) + rmatrix(rng))
    return p, gun, owner


def test_open_close():
    def setup(em):
        R["types"] = [FT, CT, MT]
    check(e, fk, 0x477220, "GunParts_Open", setup, check_eax=False)

    def setup2(em):
        em.write(0x54074C, struct.pack("<HHH", FT, CT, MT))
    check(e, fk, 0x477BC0, "GunParts_Close", setup2, check_eax=False)


@pytest.mark.parametrize("which", [(0x477600, "GunFlame_CreateHandler"), (0x477A70, "GunPart_CreateHandler")])
def test_create(which):
    def setup(em):
        rng = random.Random(1)
        R["model"] = MODEL
        return (part(em, FT, rng)[0], args(em, rng))
    check(e, fk, which[0], which[1], setup, check_eax=False)

    def setup_err(em):
        rng = random.Random(2)
        R["model"] = 0
        return (part(em, FT, rng)[0], args(em, rng, b"nomodel"))
    check_error(e, fk, which[0], e.symbols[which[1]], setup_err)


def test_flame_delete():
    check(e, fk, 0x4776A0, "GunFlame_DeleteHandler", lambda em: (part(em, FT, random.Random(1))[0],),
          check_eax=False)


@CWS
@pytest.mark.parametrize("seed", range(20))
def test_move(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        p = part(em, FT, rng)[0]
        msg = em.alloc(12)
        pos, mat = em.alloc(24), em.alloc(40)
        em.write(pos, rvec(rng))
        em.write(mat, rmatrix(rng))
        em.write(msg, u32(pos, mat, p))
        return (p, msg)
    check(e, fk, 0x4776C0, "GunPart_MoveHandler", setup, check_eax=False, cw=cw)


@CWS
@pytest.mark.parametrize("last,start,frame,handler", [(5, -1, 10, 1), (9, -1, 10, 1), (10, 4, 10, 1),
                                                      (10, 5, 10, 0), (12, 7, 10, 1), (100, 3, 10, 0)])
def test_flame_render(last, start, frame, handler, cw):
    def setup(em):
        rng = random.Random(frame + start)
        R.update(frame=frame, rand=rng.randrange(3))
        p, gun, _ = part(em, FT, rng)
        em.write(gun + 0x138, u32(last))
        em.write(p + 0x13C, u32(start))
        set_handler(em, 4, RT, DRAW)
        if handler:
            set_handler(em, EVM, FT, MOVE)
        fr = em.alloc(0x40)
        return (p, fr)
    check(e, fk, 0x4778E0, "GunFlame_RenderHandler", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("handler,vis", [(0, 1), (1, 0), (1, 5)])
def test_hide(handler, vis):
    def setup(em):
        R["vis"] = vis
        em.write(0x5407B8, bytes([0x70, EV1, 0x72, 0x73, EV4]))
        p, gun, owner = part(em, FT, random.Random(3))
        em.write(p + 0xEC, u32(7))
        if handler:
            set_handler(em, EV1, OT, HIDE)
        h = em.alloc(4)
        em.write(h, u32(0x55))
        return (p, h)
    check(e, fk, 0x4779E0, "GunPart_HideHandler", setup, check_eax=False)


@pytest.mark.parametrize("on,handler", [(0, 1), (1, 0), (1, 1)])
def test_moving_part_render(on, handler):
    def setup(em):
        rng = random.Random(on * 2 + handler)
        view = em.alloc(0x60)
        em.write(view + 0x5C, u32(on))
        R.update(view=view, bone=rvec(rng) + rmatrix(rng))
        em.write(0x5407B8, bytes([0x70, EV1, 0x72, 0x73, EV4]))
        p, gun, owner = part(em, MT, rng)
        set_handler(em, 4, RT, DRAW)
        if handler:
            set_handler(em, EV4, OT, BONE)
        return (p,)
    check(e, fk, 0x477B10, "GunMovingPart_RenderHandler", setup, check_eax=False)
