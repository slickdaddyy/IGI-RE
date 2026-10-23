import itertools
import random
import struct

import pytest

from test_vehbase_util import (CWS, EDIT, RIGID, add_child, check, child_list, cstr, ev, f32, f64, make, new_task,
                               rmatrix, set_handler)

e, fk = make()
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)
LOG2MOVE = fk.handler(2, fmt=lambda em, a: [em.normalize(a[0]), em.read(em.u32(a[1]), 24),
                                            em.read(em.u32(a[1] + 4), 40)])

TINY = 2.0 ** -70
NAN = float("nan")
SCALES = [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 1.0, 1.0), (0.5, 0.25, 2.5), (NAN, 1.0, 2.0), (1.0, NAN, 2.0)]


def tricky():
    """Matrices that lose a term when a product is summed in the wrong order."""
    out = []
    for perm in itertools.permutations((1.0, TINY, -1.0)):
        out.append(f32(*[perm[i // 3] for i in range(9)]) + struct.pack("<i", 3))
        out.append(f32(*[perm[i % 3] for i in range(9)]) + struct.pack("<i", 4))
    return out


MATS = [rmatrix(random.Random(i)) for i in range(3)] + tricky()


def obj(em, seed=0, scale=(1.0, 2.0, 1.5)):
    rng = random.Random(seed)
    o = new_task(em, EDIT, 0x1A0)
    em.write(o + 0x20, f64(*[rng.uniform(-1e4, 1e4) for _ in range(3)]))
    em.write(o + 0x68, struct.pack("<II", 0x6868, 0x6C6C))
    em.write(o + 0x70, rmatrix(rng))
    em.write(o + 0x98, f32(*scale))
    em.write(o + 0xF0, f64(*[rng.uniform(-1e5, 1e5) for _ in range(3)]))
    em.write(o + 0x108, rmatrix(rng))
    em.write(o + 0x130, b"crate_02\0")
    em.write(o + 0x158, f64(*[rng.uniform(-1e3, 1e3) for _ in range(3)]))
    em.write(o + 0x170, rmatrix(rng))
    return o


def test_open():
    check(e, fk, 0x4F2740, "EditRigidObj_Open", check_eax=False)


def test_close():
    check(e, fk, 0x4F3360, "EditRigidObj_Close", check_eax=False)


def test_get_type():
    check(e, fk, 0x4F3380, "EditRigidObj_GetType", eax_mask=0xFFFF)


def test_get_params():
    check(e, fk, 0x4F2B80, "EditRigidObj_GetParams", setup=lambda em: (0, em.alloc(4)), check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("m", MATS)
@pytest.mark.parametrize("kids", [[], [0x61, 0x62]])
def test_move(m, kids, cw):
    def setup(em):
        rng = random.Random(len(kids))
        o = obj(em, 5)
        em.write(o + 0x170, MATS[(MATS.index(m) + 3) % len(MATS)])
        child_list(em, o, ev(0x547F40), LOG2MOVE, kids)
        msg = em.alloc(8)
        pos = em.alloc(24)
        em.write(pos, f64(*[rng.uniform(-1e6, 1e6) for _ in range(3)]))
        mat = em.alloc(40)
        em.write(mat, m)
        em.write(msg, struct.pack("<II", pos, mat))
        return o, msg
    check(e, fk, 0x4F2400, "EditRigidObj_Move", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("scale", SCALES)
def test_create(scale, cw):
    check(e, fk, 0x4F2930, "EditRigidObj_Create", setup=lambda em: (obj(em, 1, scale),), check_eax=False, cw=cw)


@pytest.mark.parametrize("parts", [None, [], [0x61, 0x62]])
@pytest.mark.parametrize("kids", [[], [0x63], [0x63, 0x64, 0x63]])
def test_run(parts, kids):
    def setup(em):
        o = obj(em)
        em.write(o + 0xE8, b"\0" * 4)
        if parts is not None:
            c = new_task(em, 0x65, 0x40)
            em.write(o + 0xE8, struct.pack("<I", c))
            set_handler(em, 1, 0x65, LOG1)
            child_list(em, c, 1, LOG1, parts)
        child_list(em, o, 1, LOG1, kids)
        return (o,)
    check(e, fk, 0x4F2AC0, "EditRigidObj_Run", setup=setup, check_eax=False)


@pytest.mark.parametrize("parts", [0, 1])
def test_delete(parts):
    def setup(em):
        o = obj(em)
        em.write(o + 0xE8, struct.pack("<I", 0x4567 if parts else 0))
        return (o,)
    check(e, fk, 0x4F2B50, "EditRigidObj_Delete", setup=setup, check_eax=False)


def ancestor(em, m, seed=7):
    rng = random.Random(seed)
    a = new_task(em, RIGID, 0xF0)
    em.write(a + 0x20, f64(*[rng.uniform(-1e5, 1e5) for _ in range(3)]))
    em.write(a + 0x70, m)
    return a


POSES = [None, (1.5, -2.25, 3.125), (1e5 + 1e-9, -3.3e4, 7.7e-3), (12345.678, 0.0, -98765.4321)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("m", MATS)
@pytest.mark.parametrize("pos", POSES)
def test_attach(m, pos, cw):
    def setup(em):
        o = obj(em, 2)
        a = 0
        if pos is not None:
            a = ancestor(em, m)
            em.write(o + 0xF0, f64(*[p + q for p, q in zip(pos, struct.unpack("<3d", em.read(a + 0x20, 24)))]))
            em.write(o + 0x108, MATS[(MATS.index(m) + 5) % len(MATS)])
        fk.rets[0x401CC0] = a
        return (o,)
    check(e, fk, 0x4F2D50, "EditRigidObj_Attach", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("model", [0, 0x4321])
@pytest.mark.parametrize("old_parts", [0, 1])
@pytest.mark.parametrize("kids", [[], [0x61, 0x62]])
@pytest.mark.parametrize("scale", SCALES[:2] + SCALES[4:])
def test_params_changed(model, old_parts, kids, scale, cw):
    def setup(em):
        o = obj(em, 3, scale)
        em.write(o + 0x140, f32(0.25, 0.5, 0.75, 0.1, 0.2, 0.3))
        em.write(o + 0xE8, struct.pack("<I", 0x4567 if old_parts else 0))
        c = new_task(em, 0x65, 0x40)
        child_list(em, c, ev(0x54E0E5), LOG2, kids)
        fk.rets[0x4E0930] = c
        fk.rets[0x4CEC10] = model
        fk.rets[0x401CC0] = ancestor(em, MATS[1])
        return o, 0
    check(e, fk, 0x4F2B90, "EditRigidObj_ParamsChanged", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("parts", [None, [], [0x61, 0x62]])
@pytest.mark.parametrize("kids", [[], [0x63, 0x64]])
@pytest.mark.parametrize("addr,name,slot", [(0x4F30A0, "EditRigidObj_Activate", 0x548176),
                                            (0x4F3240, "EditRigidObj_Deactivate", 0x548177)])
@pytest.mark.parametrize("scale", SCALES[:2] + SCALES[4:])
def test_activate(addr, name, slot, parts, kids, scale):
    def setup(em):
        o = obj(em, 4, scale)
        em.write(o + 0xE8, b"\0" * 4)
        if parts is not None:
            c = new_task(em, 0x65, 0x40)
            em.write(o + 0xE8, struct.pack("<I", c))
            child_list(em, c, ev(slot), LOG2, parts)
        child_list(em, o, ev(slot), LOG2, kids)
        msg = em.alloc(8)
        em.write(msg, struct.pack("<II", 0x1212, 0x3434))
        return o, msg
    check(e, fk, addr, name, setup=setup, check_eax=False)


def test_new():
    def setup(em):
        o = new_task(em, EDIT, 0x1A0)
        c = new_task(em, 0x65, 0x40)
        fk.rets[0x4012A0] = o
        fk.rets[0x4E0930] = c
        fk.rets[0x4CEC10] = 0x4321
        fk.rets[0x401CC0] = 0
        em.write(o + 0x98, f32(1.0, 1.0, 1.0))
        em.write(o + 0xE8, b"\0" * 4)
        pos = em.alloc(24)
        em.write(pos, f64(1.0, 2.0, 3.0))
        mat = em.alloc(40)
        em.write(mat, MATS[0])
        rgb = em.alloc(24)
        em.write(rgb, f32(0.1, 0.2, 0.3, 0.4, 0.5, 0.6))
        return 0x1000, cstr(em, b"box_edit_1"), pos, mat, rgb, rgb + 12
    check(e, fk, 0x4F3390, "EditRigidObj_New", setup=setup)
