import random
import struct

import pytest

from emu import Emu
from test_shadow_fakes import Fakes, add_child, check, f32, f64, i32, new_task, rmatrix, set_handler, u16, with_children

ST, SOT = 0x30, 0x31
EV3, EVM = 0x25, 0x26
DEFS, DEF_COUNT = 0xB97E00, 0xA4433C

e = Emu()
fk = Fakes(e)
MODELS = {}
fk.add(0x4CEC10, 1, ret=lambda em, a: MODELS.get(em.read_cstr(a[0]), 0), fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4D0D10, 1, ret=lambda em, a: em.u32(a[0] + 0x40))
fk.add(0x4D0CF0, 2, ret=lambda em, a: em.u32(a[0] + 0x44) + 0x40 * a[1])
fk.add(0x4012A0, 3, ret=lambda em, a: new_task(em, a[1] & 0xFFFF, 0x1A0),
       fmt=lambda em, a: (a[0], a[1] & 0xFFFF, a[2] and em.read(a[2], 0x90)))
fk.add(0x4C5700, ret=EV3)
fk.add(0x4D9FB0, ret=EVM)
fk.add(0x4015F0, 1)
fk.add(0x4AF810, 3, fmt=lambda em, a: tuple(em.read_cstr(x) for x in a[:2 + (b"defined" in em.read_cstr(a[0]))]))
LOGA = fk.add(fk.code(b"\xc3"), 2, fmt=lambda em, a: (a[0], em.read(a[1], 8), em.read(a[1] + 0x88, 8)))
LOGM = fk.add(fk.code(b"\xc3"), 2, fmt=lambda em, a: (a[0], em.read(a[1], 12)))
u16(e, 0x548694, ST)
u16(e, 0x548696, SOT)
u16(e, 0xA5E364, 0x60)
e.baseline = e.snapshot()


def cstr(em, s):
    p = em.alloc(len(s) + 1)
    em.write(p, s + b"\0")
    return p


def make_model(em, bounds, flag=0, atts=(), name=b"model"):
    """VirModel: model[0] is the LOD; the fakes find the attachments at lod+0x40/+0x44."""
    info = em.alloc(0xA0)
    em.write(info + 0x50, f32(*bounds))
    i32(em, info + 0x98, flag)
    x = em.alloc(0x10)
    i32(em, x + 0xC, info)
    lod = em.alloc(0x48)
    i32(em, lod + 0xC, x)
    arr = em.alloc(0x40 * max(len(atts), 1))
    for k, (aname, pos, m) in enumerate(atts):
        em.write(arr + 0x40 * k, aname.ljust(16, b"\0") + f32(*pos) + f32(*m))
    em.write(lod + 0x40, struct.pack("<II", len(atts), arr))
    model = em.alloc(0x30)
    i32(em, model, lod)
    em.write(model + 0x14, name + b"\0")
    return model


def defs(em, entries):
    for k, (name, model, type_, data) in enumerate(entries):
        em.write(DEFS + 0x28 * k, name.ljust(16, b"\0") + model.ljust(16, b"\0") + struct.pack("<HHI", type_, 0, data))
    i32(em, DEF_COUNT, len(entries))


def rmat(rng, kind):
    if kind == "id":
        return (1, 0, 0, 0, 1, 0, 0, 0, 1)
    if kind == "zero":
        return (0,) * 9
    if kind == "col0":
        return (0, 1, 0, 0, 0, 1, 0, 0, 0)
    return tuple(rng.uniform(-1.2, 1.2) for _ in range(9))


@pytest.mark.parametrize("kind", ["id", "zero", "col0", "rand1", "rand2"])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_create_magic_objs(kind, cw):
    rng = random.Random(kind)
    om = rmatrix(rng)
    atts = [(b"radar", (10.5, -3.25, 7.0), rmat(rng, kind)), (b"none", (1, 2, 3), rmat(rng, "rand")),
            (b"nomodel", (0, 0, 0), rmat(rng, kind)), (b"sub", (-100.0, 50.0, 0.125), rmat(rng, "rand"))]
    pos = f64(*[rng.uniform(-1e4, 1e4) for _ in range(3)])

    def setup(em):
        MODELS.clear()
        MODELS[b"dish"] = make_model(em, (1, 2, 3, 4))
        MODELS[b"subm"] = make_model(em, (1, 2, 3, 4), flag=1)
        set_handler(em, EV3, 0x70, LOGA)
        set_handler(em, EV3, ST, LOGA)
        defs(em, [(b"other", b"x", 0x71, 0x5000), (b"radar", b"dish", 0x70, 0x6000), (b"nomodel", b"zz", 0x70, 0),
                  (b"sub", b"subm", 0x72, 0x7000), (b"radar", b"dish", 0x73, 0)])
        so = new_task(em, SOT, 0x1A0)
        em.write(so + 0x20, pos)
        i32(em, so + 0x68, 0x4444)
        i32(em, so + 0x6C, make_model(em, (0, 0, 0, 1), atts=atts))
        em.write(so + 0x70, om)
        i32(em, so + 0xE8, new_task(em, 0x60))
        i32(em, so + 0x148, new_task(em, 0x60))
        return so, 0x8888
    check(e, fk, 0x4DFA50, "Sector_CreateMagicObjs", setup=setup, cw=cw)


def test_create_magic_objs_none():
    def setup(em):
        so = new_task(em, SOT, 0x1A0)
        return so, 0
    check(e, fk, 0x4DFA50, "Sector_CreateMagicObjs", setup=setup)


SPHERES = [
    [(0, 0, 0, 5)],
    [(0, 0, 0, 5), (100, 0, 0, 10)],
    [(0, 0, 0, 50), (10, 0, 0, 10)],
    [(10, 0, 0, 10), (0, 0, 0, 50)],
    [(1, 2, 3, 4), (1, 2, 3, 6), (1, 2, 3, 2)],
    [(1.5, -20.25, 3, 4), (40, 2.5, -3, 6.5), (-17, 33, 9, 2), (0.1, 0.2, 0.3, 100), (300, -250, 12, 7.75)],
]


@pytest.mark.parametrize("spheres", SPHERES)
@pytest.mark.parametrize("parts", [0, 1])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_calc_bounds(spheres, parts, cw):
    def setup(em):
        so = new_task(em, SOT, 0x1A0)
        cont = new_task(em, 0x60)
        i32(em, so + 0x148, cont)
        for k, sp in enumerate(spheres):
            sub = new_task(em, ST, 0x148)
            i32(em, sub + 0x6C, make_model(em, sp))
            add_child(em, cont, sub)
            if parts and k == 0:
                p = new_task(em, 0x60)
                i32(em, sub + 0xE8, p)
                c = new_task(em, 0x61, 0xF0)
                i32(em, c + 0x6C, make_model(em, (7, 7, 7, 3)))
                add_child(em, p, c)
        return so, so + 0x190, so + 0x19C
    check(e, fk, 0x4DF500, "SectorObject_CalcBounds", setup=setup, cw=cw)


@pytest.mark.parametrize("model,sectors", [(0, 0), (1, 0), (1, 1)])
def test_calc_bounds_model(model, sectors):
    def setup(em):
        so = new_task(em, SOT, 0x1A0)
        if model:
            i32(em, so + 0x6C, make_model(em, (9, 8, 7, 6)))
        if sectors:
            i32(em, so + 0x148, new_task(em, 0x60))
        return so, so + 0x190, so + 0x19C
    check(e, fk, 0x4DF500, "SectorObject_CalcBounds", setup=setup)


@pytest.mark.parametrize("subs", [-1, 0, 1, 3])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_update(subs, cw):
    rng = random.Random(subs + 5)
    data = [(rmatrix(rng), f64(*[rng.uniform(-500, 500) for _ in range(3)])) for _ in range(4)]
    om, opos = rmatrix(rng), f64(*[rng.uniform(-1e4, 1e4) for _ in range(3)])

    def setup(em):
        so = new_task(em, SOT, 0x1A0)
        i32(em, so + 0x6C, 1)
        i32(em, so + 0x18C, 1)
        em.write(so + 0x20, opos)
        em.write(so + 0x70, om)
        p = new_task(em, 0x60)
        i32(em, so + 0xE8, p)
        with_children(em, p, EVM, LOGM, 2)
        if subs >= 0:
            c = new_task(em, 0x60)
            i32(em, so + 0x148, c)
            for k in range(subs):
                sub = new_task(em, ST, 0x148)
                em.write(sub + 0x108, data[k][0])
                em.write(sub + 0xF0, data[k][1])
                sp = new_task(em, 0x60)
                i32(em, sub + 0xE8, sp)
                with_children(em, sp, EVM, LOGM, k, 0x50)
                add_child(em, c, sub)
        return (so,)
    check(e, fk, 0x4DEEC0, "SectorObject_Update", setup=setup, cw=cw)


@pytest.mark.parametrize("model", [0, 1])
@pytest.mark.parametrize("old", [0, 1])
def test_update_rebuild(model, old):
    def setup(em):
        MODELS.clear()
        MODELS[b"dish"] = make_model(em, (1, 2, 3, 4))
        set_handler(em, EV3, 0x70, LOGA)
        defs(em, [(b"radar", b"dish", 0x70, 0x6000)])
        so = new_task(em, SOT, 0x1A0)
        if model:
            i32(em, so + 0x6C, make_model(em, (5, 6, 7, 8), atts=[(b"radar", (1, 2, 3), rmat(None, "id"))]))
        if old:
            i32(em, so + 0xE8, 0x1111)
            i32(em, so + 0x148, 0x2222)
        return (so,)
    check(e, fk, 0x4DEEC0, "SectorObject_Update", setup=setup)
