import math
import random
import struct

import pytest

from emu import Emu
from test_animtrig_util import CWS, add_stubs, check_cw
from test_menu_util import RET, check as _check, f32, logfake, u32

e = Emu()
add_stubs(e)
logfake(e, 0x4B0C60, 2, effect=lambda em, a: em.alloc(a[0]))
logfake(e, 0x4B0D10, 1)
CW = CWS[:2]


def check(orig, ours, setup=None, cw=None, **kw):
    kw.setdefault("check_eax", False)
    if cw is None:
        return _check(e, orig, ours, setup, **kw)
    return check_cw(e, orig, ours, cw, setup, **kw)


def rot(rng):
    a, b, c = (rng.uniform(-math.pi, math.pi) for _ in range(3))
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    x = [[1, 0, 0], [0, ca, -sa], [0, sa, ca]]
    y = [[cb, 0, sb], [0, 1, 0], [-sb, 0, cb]]
    z = [[cc, -sc, 0], [sc, cc, 0], [0, 0, 1]]

    def mul(p, q):
        return [[sum(p[i][k] * q[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    m = mul(mul(x, y), z)
    return [m[i][j] for i in range(3) for j in range(3)]


def vec(em, *v):
    p = em.alloc(4 * len(v))
    f32(em, p, *v)
    return p


def body(em, rng, forces=0, cap=8, ang=True, zero_some=False):
    b = em.alloc(0x64)
    f32(em, b, 1500.0, 1 / 1500.0, 0)
    f32(em, b + 0xC, *(rng.uniform(1e-6, 1e-3) for _ in range(3)))
    f32(em, b + 0x18, *(rng.uniform(-300, 300) for _ in range(3)))
    f32(em, b + 0x30, *((rng.uniform(-5e4, 5e4) for _ in range(3)) if ang else (0, 0, 0)))
    f32(em, b + 0x48, *(rng.uniform(-1, 1) for _ in range(3)))
    buf = em.alloc(0x18 * cap)
    u32(em, b + 0x54, buf, cap, forces, 99)
    for i in range(forces):
        vals = [rng.uniform(-2e5, 2e5) for _ in range(3)] + [rng.uniform(-4000, 4000) for _ in range(3)]
        if zero_some and i % 3 == 1:
            vals[3:] = [0, 0, 0]
        if zero_some and i % 3 == 2:
            vals[:3] = [0, 0, 0]
        f32(em, buf + 0x18 * i, *vals)
    return b


def parts(em, rng, n):
    p = em.alloc(0x1C * max(n, 1))
    for i in range(n):
        area = rng.uniform(-1, 3) if i % 4 else 0.0
        nx, ny, nz = (rng.uniform(-1, 1) for _ in range(3))
        l = math.sqrt(nx * nx + ny * ny + nz * nz)
        f32(em, p + 0x1C * i, area, nx / l, ny / l, nz / l, *(rng.uniform(-3000, 3000) for _ in range(3)))
    return p


@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("cw", CW)
def test_aero(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        m = vec(em, *rot(rng), 0)
        v = vec(em, *((0, 0, 0) if seed == 0 else (rng.uniform(-500, 500) for _ in range(3))))
        k = -0.5 if seed == 1 else float.fromhex("0x1.56e264p-12")
        return parts(em, rng, 6), 6, struct.unpack("<I", struct.pack("<f", k))[0], body(em, rng), m, v
    check(0x4EC2C0, "Body_AddAeroForces", setup, cw)


@pytest.mark.parametrize("n", [(0, 0, 0), (3, 4, 0), (0, 0, -2.5), (1e-20, 2e-20, 0)])
@pytest.mark.parametrize("cw", CW)
def test_aero_part_init(n, cw):
    def setup(em):
        p = em.alloc(0x1C)
        return (p,) + tuple(struct.unpack("<I", struct.pack("<f", v))[0] for v in (2.5, 10, 20, 30) + n)
    check(0x4EC4D0, "AeroPart_Init", setup, cw)


@pytest.mark.parametrize("count", [0, 5, -1])
def test_alloc_forces(count):
    check(0x4EC5B0, "Body_AllocForces", lambda em: (em.alloc(0x64), count))


def fbits(*v):
    return tuple(struct.unpack("<I", struct.pack("<f", x))[0] for x in v)


@pytest.mark.parametrize("cw", CW)
def test_set_mass(cw):
    check(0x4EC5E0, "Body_SetMass", lambda em: (body(em, random.Random(1)),) + fbits(1234.5, 3.0, 7.0, 11.0), cw)


@pytest.mark.parametrize("has", [0, 1])
def test_free(has):
    def setup(em):
        b = body(em, random.Random(2))
        if not has:
            u32(em, b + 0x54, 0)
        return (b,)
    check(0x4EC630, "Body_Free", setup)


def test_reset():
    check(0x4EC660, "Body_Reset", lambda em: (body(em, random.Random(3)),))


@pytest.mark.parametrize("seed,forces,ang,diag", [
    (0, 0, True, 0), (1, 4, True, 0), (2, 6, False, 0), (3, 5, True, 1), (4, 0, False, 0), (5, 7, True, 2),
    (6, 3, True, 3), (7, 2, True, 4), (8, 9, True, 0)])
@pytest.mark.parametrize("cw", CW)
def test_integrate(seed, forces, ang, diag, cw):
    def setup(em):
        rng = random.Random(seed)
        m = rot(rng)
        if diag == 1:  # trace <= 0, x largest
            m = [1, 0, 0, 0, -1, 0, 0, 0, -1]
        elif diag == 2:  # y largest
            m = [-1, 0, 0, 0, 1, 0, 0, 0, -1]
        elif diag == 3:  # z largest
            m = [-1, 0, 0, 0, -1, 0, 0, 0, 1]
        elif diag == 4:
            m = [-0.5, 0.2, 0.1, 0.3, -0.6, 0.2, 0.1, 0.4, -0.2]
        mp = vec(em, *m, 7.0)
        return body(em, rng, forces, cap=12, ang=ang, zero_some=seed % 2 == 1), mp
    check(0x4EC6A0, "Body_Integrate", setup, cw, check_eax=True)


@pytest.mark.parametrize("v", [(0, 0, 0), (3, -4, 12), (1e-30, 0, 0), (-0.0, 0, 0), (250.5, 1e3, -7)])
@pytest.mark.parametrize("cw", CW)
def test_update(v, cw):
    def setup(em):
        b = body(em, random.Random(4))
        f32(em, b + 0x18, *v)
        return (b,)
    check(0x4ECDB0, "Body_Update", setup, cw)


@pytest.mark.parametrize("cw", CW)
@pytest.mark.parametrize("dims", [(4096.0, 2048.0, 8192.0), (1.5, 1e-3, 3e4)])
def test_init(cw, dims):
    check(0x4ECEA0, "Body_Init", lambda em: (em.alloc(0x64),) + fbits(850.0, *dims) + (6,), cw)


@pytest.mark.parametrize("cw", CW)
def test_set_box(cw):
    check(0x4ECED0, "Body_SetBox", lambda em: (em.alloc(0x64),) + fbits(7.25, 3.3, 1.7, 2.9), cw)


def test_add_force():
    def setup(em):
        b = body(em, random.Random(5), 2)
        return b, vec(em, 1, 2, 3, 4, 5, 6)
    check(0x4ECF50, "Body_AddForce", setup)


@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("cw", CW)
def test_point_velocity(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        b = body(em, rng)
        out = em.alloc(12)
        p = vec(em, *(rng.uniform(-2000, 2000) for _ in range(3)))
        return out, b, p if seed else out
    check(0x4ECF80, "Body_GetPointVelocity", setup, cw)
