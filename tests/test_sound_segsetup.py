import math
import random
import struct

import pytest

from emu import Emu
from test_shadow_fakes import CWS, Fakes, check, f32, f64, i32

e = Emu()
fk = Fakes(e)
MODEL = [0]
PARTS = {}


def get_verts(em, a):
    verts, count = PARTS[a[2]]
    if a[5]:
        i32(em, a[5], count)
    return verts


fk.add(0x4CEC10, 1, ret=lambda em, a: MODEL[0], fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x49A050, 6, ret=get_verts, fmt=lambda em, a: (a[0], a[1], a[2], a[3], a[4], a[5] != 0))
fk.add(0x4C6940, 4, fmt=lambda em, a: (a[0], a[1], em.read(a[2], 24), a[3]))
fk.add(0x49A010, 1)
fk.add(0x4B0D10, 1)
fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(a[0]))
e.baseline = e.snapshot()

# sphere tree of 4 nodes over 12 triangles: 0 root, children 1 and 2, 2 has child 3
NODES = [(0, 2, 1, 0), (2, 3, 0, 2), (5, 0, 3, 0), (5, 7, 0, 0)]


def fbits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def rotation(em, rng, spare=None):
    m = em.alloc(40)
    em.call(0x4B38E0, m, *[fbits(rng.uniform(-math.pi, math.pi)) for _ in range(3)])
    i32(em, m + 36, rng.randrange(2000) if spare is None else spare)
    return m


def make_model(em, rng, nlod, flags, xr, nodes_per_part, radii):
    models = em.alloc(0x40)
    em.write(models + 0x3C, struct.pack("<h", nlod))
    for k in range(nlod):
        info = em.alloc(0x80)
        i32(em, info + 0x20, flags[k])
        nv = rng.randrange(1, 12)
        p = em.alloc(0x14)
        v = em.alloc(0x20 * nv)
        for j in range(nv):
            em.write(v + 0x20 * j, f32(rng.uniform(*xr), rng.uniform(-9, 9), rng.uniform(-9, 9)))
        i32(em, p + 0xC, nv)
        i32(em, p + 0x10, v)
        i32(em, info + 0x24, p)
        parts = em.alloc(0x40)
        for j, nn in enumerate(nodes_per_part):
            tris = em.alloc(8 * 12)
            for t in range(12):
                em.write(tris + 8 * t, struct.pack("<4H", *[rng.randrange(30) for _ in range(3)], 0))
            src = em.alloc(0x18 * max(nn, 1))
            for n in range(nn):
                first, count, child, nxt = NODES[n]
                em.write(src + 0x18 * n, f32(rng.uniform(*xr), rng.uniform(-5, 5), rng.uniform(-5, 5), 0.5)
                         + struct.pack("<4H", first, count, child if child < nn else 0, nxt if nxt < nn else 0))
            i32(em, parts + 0x20 * j + 0xC, nn)
            i32(em, parts + 0x20 * j + 0x10, tris)
            i32(em, parts + 0x20 * j + 0x1C, src)
        i32(em, info + 0x2C, parts)
        em.write(info + 0x78, f32(radii[k]))
        x = em.alloc(0x10)
        i32(em, x + 0xC, info)
        lod = em.alloc(0x10)
        i32(em, lod + 0xC, x)
        i32(em, models + 4 * k, lod)
    return models


def setup_for(seed, rigid, reversed_, segments, ends="rand", model=True, nodes=(4, 3), flags=None,
              old=None, xr=(-40.0, 60.0), vr=60.0):
    rng = random.Random(seed)

    def setup(em):
        rng.seed(seed)
        o = em.alloc(0x5B8)
        em.write(o, bytes(rng.randrange(256) for _ in range(0x5B8)))
        em.write(o + 0x20, f64(1.0, 2.0, 3.0))
        i32(em, o + 0xF0, 0x7777)
        i32(em, o + 0x5A0, 0)
        i32(em, o + 0x5A8, 0)
        if ends == "same":
            a = b = [rng.uniform(-500, 500) for _ in range(3)]
        elif ends == "tiny":
            a, b = [0.0, 0.0, 0.0], [0.0, 1e-300, 0.0]
        elif ends == "x":
            a, b = [-100.0, 3.0, 4.0], [250.0, 3.0, 4.0]
        else:
            a = [rng.uniform(-500, 500) for _ in range(3)]
            b = [rng.uniform(-500, 500) for _ in range(3)]
        pa, pb = em.alloc(24), em.alloc(24)
        em.write(pa, f64(*a))
        em.write(pb, f64(*b))
        ra, rb = rotation(em, rng), rotation(em, rng)
        nlod = rng.randrange(1, 4)
        fl = flags or [0] * nlod
        MODEL[0] = make_model(em, rng, nlod, (fl + [0] * 3)[:nlod], xr, nodes,
                              [rng.uniform(0.5, 30) for _ in range(nlod)]) if model else 0
        for part in range(2):
            n = rng.randrange(30, 40)
            vp = em.alloc(12 * n)
            em.write(vp, f32(*[rng.uniform(-vr, vr) for _ in range(3 * n)]))
            PARTS[part] = (vp, n)
        if old == "same" and model:
            i32(em, o + 0x5B0, MODEL[0])
        if old in ("same", "other"):
            for part in range(2):
                buf = em.alloc(0x18 * 8)
                i32(em, o + 0x5A0 + 8 * part, buf)
                i32(em, o + 0x5A4 + 8 * part, nodes[part])
        name = em.alloc(16)
        em.write(name, b"spline_1\0")
        return o, pa, pb, ra, rb, rigid | 0x100, reversed_ | 0x200, segments, 0x33, name
    return setup


def run(setup, cw):
    check(e, fk, 0x4E4530, "SplineObjSegment_Setup", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("reversed_", [0, 1])
def test_bent(cw, seed, reversed_):
    rng = random.Random(seed)
    run(setup_for(seed, 0, reversed_, rng.randrange(1, 21)), cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("segments", [0, 1, 2, 20])
@pytest.mark.parametrize("ends", ["rand", "same", "x"])
def test_bent_segments(cw, segments, ends):
    run(setup_for(segments * 7 + len(ends), 0, 0, segments, ends=ends), cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", ["reuse", "replace", "emptypart", "flags", "nomodel", "negative", "small"])
def test_bent_cases(cw, case):
    kw = {"reuse": dict(old="same"), "replace": dict(old="other"), "emptypart": dict(nodes=(0, 4)),
          "flags": dict(flags=[1, 0, 1]), "nomodel": dict(model=False), "negative": dict(model=False),
          "small": dict(vr=0.5)}[case]
    run(setup_for(len(case) * 13, 0, 1, -1 if case == "negative" else 5, **kw), cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(5))
@pytest.mark.parametrize("reversed_", [0, 1])
@pytest.mark.parametrize("ends", ["rand", "same", "tiny", "x"])
def test_rigid(cw, seed, reversed_, ends):
    run(setup_for(seed + 100, 1, reversed_, 7, ends=ends, old="other" if seed & 1 else None), cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", ["nomodel", "flags", "allflags"])
def test_rigid_cases(cw, case):
    kw = {"nomodel": dict(model=False), "flags": dict(flags=[1, 0, 1]), "allflags": dict(flags=[1, 1, 1])}[case]
    run(setup_for(len(case), 1, 0, 3, **kw), cw)
