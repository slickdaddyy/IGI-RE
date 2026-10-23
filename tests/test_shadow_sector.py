import random
import struct

import pytest

from emu import Emu
from test_shadow_fakes import (Fakes, add_child, check, f32, i32, new_task, pool_init, pool_take, rmatrix, rvec,
                               set_handler, u16, with_children)

LINKS = 0xA5E318
ST, SOT, SIT = 0x30, 0x31, 0x32  # Sector, SectorObject, SectorInteriorObject types
RIGID = 0x33
EV0, EV1, CLIP, EV2 = 0x21, 0x22, 0x23, 0x24

e = Emu()
fk = Fakes(e)
MODELS = {b"m1": 0x111000, b"m2": 0x222000}
VALUE = {"radius": 2.5}
fk.add(0x4C5710, 3)
fk.add(0x4015F0, 1)
fk.add(0x4012A0, 3, ret=lambda em, a: new_task(em, 0x60, 0x40), fmt=lambda em, a: (a[0], a[1] & 0xFFFF, a[2]))
fk.add(0x4CEC10, 1, ret=lambda em, a: MODELS.get(em.read_cstr(a[0]), 0), fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4C4950, 3)
fk.add(0x4CEA10, ret=EV0)
fk.add(0x4CEA20, ret=EV1)
fk.add(0x4D14D0, ret=CLIP)
fk.add(0x4C48C0, ret=RIGID)
fk.add(0x4C56F0, ret=EV2)
fk.add(0x4D0950, 1, ret=lambda em, a: VALUE["radius"], st0="f")
fk.add(0x4C7140, 5, deref={2: 24})
fk.add(0x4DCCB0, 3, deref={2: 24})
fk.add(0x4DCDD0, 1)
fk.add(0x4C7560, 2)
fk.add(0x401A20, 1, fmt=lambda em, a: (a[0] & 0xFFFF,))
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)
u16(e, 0x548694, ST)
u16(e, 0x548696, SOT)
u16(e, 0x548698, SIT)
e.baseline = e.snapshot()
rng = random.Random(7)


def sector(em, size=0x1A0, type_=SOT):
    s = new_task(em, type_, size)
    pool_init(em, s + 0x130, 4, 100)
    return s


def link(em, s, obj, other=0):
    item = pool_take(em, s + 0x130)
    i32(em, item, obj)
    l = pool_take(em, LINKS)
    em.write(l, struct.pack("<III", item, s, other))
    return l


def test_create():
    def setup(em):
        s = em.alloc(0x1A0)
        em.write(s, b"\xab" * 0x1A0)
        return (s,)
    check(e, fk, 0x4DDF60, "SectorObject_Create", setup=setup)


@pytest.mark.parametrize("parts,sectors", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_delete(parts, sectors):
    def setup(em):
        pool_init(em, LINKS, 0xC, 1000)
        s = sector(em)
        i32(em, s + 0xE8, 0x5000 if parts else 0)
        i32(em, s + 0x148, 0x6000 if sectors else 0)
        link(em, s, em.alloc(0x70))
        return (s,)
    check(e, fk, 0x4DDFF0, "SectorObject_Delete", setup=setup)


@pytest.mark.parametrize("n", [0, 1, 4])
def test_remove_all(n):
    def setup(em):
        pool_init(em, LINKS, 0xC, 1000)
        a, b = sector(em), sector(em)
        objs = [em.alloc(0x70) for _ in range(6)]
        for i in range(n):
            link(em, b, objs[i])
            link(em, a, objs[i], i)
        link(em, b, objs[5])
        l = pool_take(em, LINKS)
        em.write(l, struct.pack("<III", 0x1234, a, 9))
        return (a,)
    check(e, fk, 0x4DE040, "Sector_RemoveAllObjects", setup=setup)


@pytest.mark.parametrize("addr,name,slot", [(0x4DE240, "SectorObject_Slot10", 10), (0x4DE2D0, "SectorObject_Slot9", 9),
                                            (0x4DE360, "SectorObject_Slot8", 8)])
@pytest.mark.parametrize("kids", [-1, 0, 2])
def test_forward(addr, name, slot, kids):
    def setup(em):
        set_handler(em, slot, ST, LOG2)
        s = new_task(em, SOT, 0x1A0)
        if kids >= 0:
            c = new_task(em, 0x60)
            i32(em, s + 0x148, c)
            with_children(em, c, slot, LOG2, kids)
        return s, 0x4242
    check(e, fk, addr, name, setup=setup)


QUERY_FMT = lambda em, a: (a[0], em.read(a[1], 8), em.read(a[1] + 12, 0x8C),
                           em.read(em.u32(a[1] + 8), 4 * em.u32(a[1] + 4)))


def set_flags(em, a):
    q = a[1]
    n = em.u32(q + 4)
    flags = em.u32(q + 8)
    for i in range(n):
        if (i + a[0]) % 3:
            i32(em, flags + 4 * i, i + 1)


H11 = fk.add(fk.code(b"\xc3"), 2, ret=lambda em, a: set_flags(em, a) or 0, fmt=QUERY_FMT)


@pytest.mark.parametrize("kids", [-1, 0, 1, 3])
def test_slot11(kids):
    h = H11
    junk = bytes(rng.randrange(256) for _ in range(0x98))

    def setup(em):
        set_handler(em, 11, ST, LOG2)
        s = new_task(em, SOT, 0x1A0)
        if kids >= 0:
            c = new_task(em, 0x60)
            i32(em, s + 0x148, c)
            with_children(em, c, 11, h, kids)
        flags = em.alloc(4 * 5)
        em.write(flags, struct.pack("<5I", 1, 0, 7, 1, 1))
        q = em.alloc(0x98)
        em.write(q, junk)
        em.write(q, struct.pack("<III", 9, 5, flags))
        return s, q
    check(e, fk, 0x4DE3F0, "SectorObject_Slot11", setup=setup)


@pytest.mark.parametrize("model", [0, 1])
@pytest.mark.parametrize("parts", [0, 1])
@pytest.mark.parametrize("subs", [-1, 0, 2])
def test_run(model, parts, subs):
    def setup(em):
        s = new_task(em, SOT, 0x1A0)
        i32(em, s + 0x6C, model)
        with_children(em, s, 1, LOG1, 2)
        if parts:
            p = new_task(em, 0x60)
            i32(em, s + 0xE8, p)
            with_children(em, p, 1, LOG1, 2, 0x48)
        if subs >= 0:
            c = new_task(em, 0x60)
            i32(em, s + 0x148, c)
            for k, sub in enumerate(with_children(em, c, 1, LOG1, subs, 0x50, 0x148)):
                with_children(em, sub, 1, LOG1, k + 1, 0x58)
                if k:
                    p = new_task(em, 0x60)
                    i32(em, sub + 0xE8, p)
                    with_children(em, p, 1, LOG1, 2, 0x5C)
        return (s,)
    check(e, fk, 0x4DE4E0, "SectorObject_Run", setup=setup)


def test_sector_create():
    data = rvec(rng) + rvec(rng) + rmatrix(rng) + rmatrix(rng)

    def setup(em):
        s = em.alloc(0x148)
        em.write(s, b"\xcd" * 0x148)
        a = em.alloc(0x90)
        m = em.alloc(4)
        em.write(m, b"m2\0")
        em.write(a, struct.pack("<II", m, 0x7777) + data
                 + struct.pack("<II", 0x8888, 0x9999))
        return s, a
    check(e, fk, 0x4DE6B0, "Sector_Create", setup=setup)


def test_sector_delete():
    def setup(em):
        pool_init(em, LINKS, 0xC, 1000)
        s = sector(em, 0x148, ST)
        i32(em, s + 0xE8, 0x5000)
        link(em, s, em.alloc(0x70))
        return (s,)
    check(e, fk, 0x4DE740, "Sector_Delete", setup=setup)


@pytest.mark.parametrize("addr,name,ev", [(0x4DE770, "Sector_WorldAdd", EV0), (0x4DE800, "Sector_WorldRemove", EV1)])
@pytest.mark.parametrize("parts", [-1, 0, 2])
def test_sector_world(addr, name, ev, parts):
    def setup(em):
        pool_init(em, LINKS, 0xC, 1000)
        s = sector(em, 0x148, ST)
        link(em, s, em.alloc(0x70))
        if parts >= 0:
            p = new_task(em, 0x60)
            i32(em, s + 0xE8, p)
            with_children(em, p, ev, LOG2, parts)
        return s, 0x4141
    check(e, fk, addr, name, setup=setup)


@pytest.mark.parametrize("subs", [-1, 0, 1, 3])
def test_near_clip(subs):
    def setup(em):
        set_handler(em, CLIP, RIGID, LOG2)
        s = sector(em)
        for i in range(3):
            o = new_task(em, 0x40 + i)
            if i < 2:
                set_handler(em, CLIP, 0x40 + i, LOG2)
            item = pool_take(em, s + 0x130)
            i32(em, item, o)
        if subs >= 0:
            c = new_task(em, 0x60)
            i32(em, s + 0x148, c)
            for k in range(subs):
                sub = sector(em, 0x148, ST)
                add_child(em, c, sub)
                for i in range(k):
                    item = pool_take(em, sub + 0x130)
                    i32(em, item, new_task(em, 0x41))
        return s, 0x4343
    check(e, fk, 0x4DE8A0, "SectorObject_NearClip", setup=setup)


@pytest.mark.parametrize("addr,name,ev", [(0x4DE970, "SectorObject_WorldAdd", EV0),
                                          (0x4DEA80, "SectorObject_WorldRemove", EV1)])
@pytest.mark.parametrize("parts,subs", [(-1, -1), (0, 2), (2, 0), (2, 3)])
def test_object_world(addr, name, ev, parts, subs):
    def setup(em):
        pool_init(em, LINKS, 0xC, 1000)
        s = sector(em)
        link(em, s, em.alloc(0x70))
        if parts >= 0:
            p = new_task(em, 0x60)
            i32(em, s + 0xE8, p)
            with_children(em, p, ev, LOG2, parts)
        if subs >= 0:
            p = new_task(em, 0x60)
            i32(em, s + 0x148, p)
            with_children(em, p, ev, LOG2, subs, 0x50)
        return s, 0x4141
    check(e, fk, addr, name, setup=setup)


@pytest.mark.parametrize("scale", [(1, 2, 3), (3, 2, 1), (2, 3, 1), (1, 1, 1), (0.5, 0.25, 0.75)])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_interior_world_add(scale, cw):
    pos = rvec(rng)

    def setup(em):
        VALUE["radius"] = 3.3
        o = new_task(em, SIT, 0xF0)
        em.write(o + 0x20, pos)
        i32(em, o + 0x6C, 0x111000)
        em.write(o + 0x98, f32(*scale))
        with_children(em, o, EV0, LOG2, 2)
        p = new_task(em, 0x60)
        i32(em, o + 0xE8, p)
        with_children(em, p, EV0, LOG2, 1, 0x50)
        a = em.alloc(8)
        em.write(a, struct.pack("<II", 0, 0x7000))
        return o, a
    check(e, fk, 0x4DEB90, "SectorInteriorObject_WorldAdd", setup=setup, cw=cw)


@pytest.mark.parametrize("parts", [0, 1])
def test_interior_world_remove(parts):
    def setup(em):
        o = new_task(em, SIT, 0xF0)
        with_children(em, o, EV1, LOG2, 2)
        if parts:
            p = new_task(em, 0x60)
            i32(em, o + 0xE8, p)
            with_children(em, p, EV1, LOG2, 2, 0x50)
        a = em.alloc(8)
        em.write(a, struct.pack("<II", 0x7000, 0))
        return o, a
    check(e, fk, 0x4DED30, "SectorInteriorObject_WorldRemove", setup=setup)


def test_close():
    def setup(em):
        pool_init(em, LINKS, 0xC, 1000)
        pool_init(em, 0xA5E338, 0x20, 20)
    check(e, fk, 0x4DEE40, "Sector_Close", setup=setup)


def test_get_types():
    check(e, fk, 0x4DEE80, "SectorObject_GetType", check_eax=True, eax_mask=0xFFFF)
    check(e, fk, 0x4E0830, "SectorInteriorObject_GetType", check_eax=True, eax_mask=0xFFFF)


@pytest.mark.parametrize("name", [b"m1", b"m2", b"zz"])
def test_set_model(name):
    def setup(em):
        s = new_task(em, SOT, 0x1A0)
        i32(em, s + 0x6C, 0x111000)
        i32(em, s + 0x18C, 1)
        n = em.alloc(4)
        em.write(n, name + b"\0")
        return s, n
    check(e, fk, 0x4DEE90, "SectorObject_SetModel", setup=setup)


def test_set_matrix():
    m0 = rmatrix(rng)

    def setup(em):
        s = new_task(em, SOT, 0x1A0)
        m = em.alloc(40)
        em.write(m, m0)
        return s, m
    check(e, fk, 0x4E0810, "SectorObject_SetMatrix", setup=setup)
