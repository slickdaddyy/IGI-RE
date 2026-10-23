import random
import struct

import pytest

from emu import Emu
from test_binocular_util import (Fakes, add_child, check, common, cstr, event, f32, f64, masked, new_task, rmatrix,
                                 rvec, set_events, set_handler, u32, u8)

MT, BT, CT = 0x60, 0x61, 0x62  # Medipack, Binocular, child
V = {}

e = Emu()
fk = Fakes(e)
common(e, fk, {b"Medipack": MT, b"Binocular": BT})
set_events(e)
WEAPON = event(0x477BF0)
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)


def config_find(em, a):
    name = em.read_cstr(a[1])
    return V.get("found", {}).get(name, 0)


def config_new(em, a):
    p = em.alloc(0x130)
    em.write(p + 0x124, struct.pack("<I", a[2]))
    return p


def script_fakes(ints, reals):
    fk.add(0x4B8A20, 2, ret=lambda em, a: ints[a[1] % len(ints)])
    fk.add(0x4B8A50, 2, ret=lambda em, a: reals[a[1] % len(reals)], st0="d")


fk.add(0x4FE730, 2, ret=config_find, fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4FE650, 3, ret=config_new, fmt=lambda em, a: (a[0], em.read_cstr(a[1]), a[2]))
fk.add(0x472600, 1, ret=lambda em, a: V.get("overlay", 0x2600))
fk.add(0x472630, 2, deref={1: 24})
fk.add(0x472650, 2, deref={1: 12})
fk.add(0x401AE0, 1, ret=lambda em, a: V.get("ref", 0xAE00))
fk.add(0x401B20, 1)
fk.add(0x4659D0, 1)
fk.add(0x4B0D10, 1)
fk.add(0x4E6B00, 2, ret=lambda em, a: V.get("sound", 0), fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4E6C00, 1)
script_fakes([5, -7, 12, 99, 61, 3, -90, 180], [12.5, -3.25, 0.1, 400.0, 33.3])
for slot in (0, event(0x4F1A70)):
    set_handler(e, slot, WEAPON, LOG2)
for slot in (2, event(0x4F2030)):
    set_handler(e, slot, WEAPON, LOG1)
set_handler(e, event(0x460BF0), CT, LOG2)
set_handler(e, event(0x487190), CT, LOG2)
e.baseline = e.snapshot()

CW = pytest.mark.parametrize("cw", [0x37F, 0x7F])


def test_medipack_open():
    check(e, fk, 0x470410, "Medipack_Open")


def test_medipack_create():
    check(e, fk, 0x470500, "Medipack_Create", lambda em: (new_task(em, MT, 0x1B0), 0x1234))


@pytest.mark.parametrize("index", [0, 4])
def test_medipack_parse_arg(index):
    def setup(em):
        a = em.alloc(12)
        u32(em, a, em.alloc(4), 0xA000, index)
        return 0, a
    check(e, fk, 0x470540, "Medipack_ParseArg", setup)


@pytest.mark.parametrize("old", [0, 1])
@pytest.mark.parametrize("found", [0, 1])
def test_medipack_config(old, found):
    def setup(em):
        V.clear()
        wt = em.alloc(0x700)
        if old:
            u32(em, wt + 0x6E4, em.alloc(16))
        if found:
            p = em.alloc(0x130)
            em.write(p + 0x124, f32(0.25))
            V["found"] = {b"Factor of human max damage to add for each round": p}
        a = em.alloc(8)
        u32(em, a, 0xC0F, wt)
        return 0, a
    check(e, fk, 0x470570, "Medipack_Config", setup)


def test_closes():
    def setup(em):
        em.write(0x5BE334, b"\x34\x12\x99\x99")
        em.write(0x5BE33C, b"\x78\x56\x99\x99")
    check(e, fk, 0x470600, "Medipack_Close", setup)
    check(e, fk, 0x470EB0, "Binocular_Close", setup)


def test_binocular_open():
    check(e, fk, 0x470610, "Binocular_Open")


def binocular(rng):
    b = new_task(e, BT, 0x1C8)
    e.write(b + 0x70, rmatrix(rng))
    wt = e.alloc(0x700)
    fov = e.alloc(12)
    e.write(fov, f32(rng.uniform(0.3, 0.8), rng.uniform(0.05, 0.4), rng.uniform(0.6, 1.2)))
    u32(e, wt + 0x6E4, fov)
    u32(e, b + 0x128, wt)
    e.write(b + 0x150, rvec(rng) + f32(1, 2, 3))
    u32(e, b + 0x1B0, rng.choice([0, 1]), 7, rng.choice([0, 0, 1]), rng.choice([0, 1]))
    ov = e.alloc(4)
    u32(e, ov, rng.choice([0, 0x0F00]))
    u32(e, b + 0x1C0, ov)
    snd = 0
    if rng.random() < 0.6:
        snd = e.alloc(4)
        u32(e, snd, rng.choice([0, 0x5000]))
    u32(e, b + 0x1C4, snd)
    return b


def test_binocular_create():
    check(e, fk, 0x470760, "Binocular_Create", lambda em: (new_task(em, BT, 0x1C8), 0x1234))


@pytest.mark.parametrize("seed", range(6))
def test_binocular_delete_stop_unzoom(seed):
    def setup(em):
        return (binocular(random.Random(seed)),)
    check(e, fk, 0x4707D0, "Binocular_Delete", setup)
    check(e, fk, 0x470800, "Binocular_StopSound", setup)
    check(e, fk, 0x470840, "Binocular_Unzoom", setup)


def use_setup(seed):
    def setup(em):
        rng = random.Random(seed)
        V.clear()
        V["sound"] = rng.choice([0, 0x5150])
        V["ref"] = 0x5EF0
        b = binocular(rng)
        for _ in range(rng.randrange(3)):
            add_child(em, b, new_task(em, CT, 0x40))
        h = em.alloc(0xD00)
        view = em.alloc(0x200)
        fov = struct.unpack("<3f", em.read(em.u32(em.u32(b + 0x128) + 0x6E4), 12))
        em.write(view + 0x1E4, f32(rng.choice([fov[0], fov[1], fov[2], rng.uniform(0.01, 1.5)])))
        u32(em, h + 0x4EC, view)
        u8(em, h + 0xCF5, rng.random() < 0.1)
        for off in (0x1CC, 0x12C, 0x1E8, 0x1EC):
            em.write(h + off, f32(rng.choice([0.0, 0.0, 1.0])))
        a = em.alloc(0x30)
        pos = 0
        if rng.random() < 0.7:
            pos = em.alloc(24)
            em.write(pos, rvec(rng))
        m = 0
        if rng.random() < 0.7:
            m = em.alloc(40)
            em.write(m, rmatrix(rng))
        u32(em, a + 4, pos, m)
        u32(em, a + 0x14, rng.random() < 0.85)
        u32(em, a + 0x2C, h)
        return b, a
    return setup


@CW
@pytest.mark.parametrize("seed", range(60))
def test_binocular_use(seed, cw):
    check(e, fk, 0x470890, "Binocular_Use", use_setup(seed), cw=cw)


@pytest.mark.parametrize("seed", range(4))
def test_binocular_status_display(seed):
    def setup(em):
        rng = random.Random(seed)
        b = binocular(rng)
        u32(em, b + 0x1B0, seed & 1)
        for _ in range(seed):
            add_child(em, b, new_task(em, CT, 0x40))
        return b, 0x4321
    check(e, fk, 0x470BC0, "Binocular_StatusDisplay", setup)


@pytest.mark.parametrize("seed", range(4))
def test_binocular_params_changed(seed):
    def setup(em):
        V.clear()
        return binocular(random.Random(seed)), 0x4321
    check(e, fk, 0x470C80, "Binocular_ParamsChanged", setup)


@CW
@pytest.mark.parametrize("index", [0, 3, 5])
def test_binocular_parse_arg(index, cw):
    def setup(em):
        a = em.alloc(12)
        u32(em, a, em.alloc(12), 0xA000, index)
        return 0, a
    check(e, fk, 0x470CE0, "Binocular_ParseArg", setup, cw=cw)


@CW
@pytest.mark.parametrize("found", range(8))
def test_binocular_config(found, cw):
    def setup(em):
        V.clear()
        wt = em.alloc(0x700)
        if found & 1:
            u32(em, wt + 0x6E4, em.alloc(16))
        V["found"] = {}
        for bit, name, val in ((1, b"Binocular - Default FOV (degrees)", 63.0),
                               (2, b"Binocular - Max FOV (degrees)", 101.5),
                               (4, b"Binocular - Min FOV (degrees)", 3.3)):
            if found & bit:
                p = em.alloc(0x130)
                em.write(p + 0x124, f32(val))
                V["found"][name] = p
        a = em.alloc(8)
        u32(em, a, 0xC0F, wt)
        return 0, a
    check(e, fk, 0x470D70, "Binocular_Config", setup, cw=cw)


def view(rng):
    v = e.alloc(0x60)
    e.write(v, rmatrix(rng) + b"\0" * 4 + rvec(rng) + f32(0, 0, rng.uniform(100, 900), rng.uniform(100, 900),
                                                          rng.uniform(0, 640), rng.uniform(0, 480)))
    return v


@CW
@pytest.mark.parametrize("seed", range(20))
def test_view_project(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        v = view(rng)
        p = em.alloc(24)
        em.write(p, rvec(rng))
        out = em.alloc(12)
        return v, out, out + 8, p
    check(e, fk, 0x470EC0, "View_Project", setup, cw=cw)


@CW
@pytest.mark.parametrize("seed", range(6))
def test_view_scale_size(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        v = view(rng)
        size = em.alloc(8)
        em.write(size, f32(rng.uniform(1, 500), rng.uniform(1, 500)))
        return v, em.alloc(8), size, struct.unpack("<I", f32(rng.uniform(0.5, 9000)))[0]
    check(e, fk, 0x470F70, "View_ScaleSize", setup, cw=cw)
