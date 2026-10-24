import math
import random
import struct

import pytest

from emu import Emu, compare


@pytest.fixture(autouse=True, params=[0x37F, 0x7F, 0xC7F, 0x77F], ids=hex)
def cw(request):
    """Every test at the emulator default and at the control words the game uses."""
    Emu.cw = request.param
    yield
    Emu.cw = None


emu = Emu()
rng = random.Random(7)
NAN = float("nan")
INF = float("inf")


def floats(e, *v):
    p = e.alloc(4 * len(v))
    e.write(p, struct.pack("<%df" % len(v), *v))
    return p


def doubles(e, *v):
    p = e.alloc(8 * len(v))
    e.write(p, struct.pack("<%dd" % len(v), *v))
    return p


def f32(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def d64(v):
    return struct.unpack("<II", struct.pack("<d", v))


def rand():
    return rng.choice([rng.uniform(-1, 1), rng.uniform(-1000, 1000), rng.uniform(-1e-3, 1e-3), 0.0])


def vecs(n, dim):
    out = [tuple(rand() for _ in range(dim)) for _ in range(n)]
    out += [(0.0,) * dim, (-0.0,) * dim, (1.0,) + (0.0,) * (dim - 1), (NAN,) * dim, (-3.0,) * dim]
    return out


SCALARS = [-2.0, -1.0000001, -1.0, -0.5, -0.0, 0.0, 0.25, 0.9999999, 1.0, 1.5, 1e30, -INF, INF, NAN]


@pytest.mark.parametrize("x", SCALARS)
@pytest.mark.parametrize("orig,ours", [(0x4B2760, "Math_ACosF"), (0x4B27D0, "Math_ASinF")])
def test_acos_asin_float(orig, ours, x):
    compare(emu, orig, ours, args=(f32(x),), check_eax=False, check_st0=True)


@pytest.mark.parametrize("x", SCALARS)
@pytest.mark.parametrize("orig,ours", [(0x4B28A0, "Math_ACos"), (0x4B2840, "Math_ASin")])
def test_acos_asin_double(orig, ours, x):
    compare(emu, orig, ours, args=d64(x), check_eax=False, check_st0=True)


@pytest.mark.parametrize("v", vecs(20, 2))
def test_vec2(v):
    w = (rand(), rand())
    compare(emu, 0x4B2930, "Vec2_Length", setup=lambda e: (floats(e, *v),), check_eax=False, check_st0=True)
    compare(emu, 0x4B2900, "Vec2_Normalize", setup=lambda e: (floats(e, *v),), check_eax=False, check_st0=True)
    compare(emu, 0x4B2950, "Vec2_Distance", setup=lambda e: (floats(e, *v), floats(e, *w)), check_eax=False,
            check_st0=True)
    compare(emu, 0x4B2980, "Vec2_Dot", setup=lambda e: (floats(e, *v), floats(e, *w)), check_eax=False,
            check_st0=True)


@pytest.mark.parametrize("v", vecs(20, 2))
def test_vec2d(v):
    w = (rand(), rand())
    compare(emu, 0x4B29D0, "Vec2d_Length", setup=lambda e: (doubles(e, *v),), check_eax=False, check_st0=True)
    compare(emu, 0x4B29A0, "Vec2d_Normalize", setup=lambda e: (doubles(e, *v),), check_eax=False, check_st0=True)
    compare(emu, 0x4B29F0, "Vec2d_Dot", setup=lambda e: (doubles(e, *v), doubles(e, *w)), check_eax=False,
            check_st0=True)


ORDERS = [(3.0, 2.0, 1.0), (3.0, 1.0, 2.0), (2.0, 3.0, 1.0), (1.0, 3.0, 2.0), (2.0, 1.0, 3.0), (1.0, 2.0, 3.0),
          (-1.0, -2.0, -3.0), (2.0, 2.0, 2.0), (2.0, 2.0, 1.0), (NAN, 1.0, 2.0), (1.0, NAN, 2.0), (1.0, 2.0, NAN)]


@pytest.mark.parametrize("v", vecs(20, 3) + ORDERS)
def test_vec3_fast_length(v):
    compare(emu, 0x4B2A10, "Vec3_FastLength", setup=lambda e: (floats(e, *v),), check_eax=False, check_st0=True)


@pytest.mark.parametrize("a", vecs(10, 3))
def test_vec3_angle(a):
    for b in vecs(6, 3) + [a, tuple(-x for x in a), tuple(2 * x for x in a)]:
        compare(emu, 0x4B2B30, "Vec3_Angle", setup=lambda e: (floats(e, *a), floats(e, *b)), check_eax=False,
                check_st0=True)


@pytest.mark.parametrize("p", vecs(8, 3))
def test_vec3_ray_plane(p):
    for d, n in [(vecs(1, 3)[0], vecs(1, 3)[0]), ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0)), ((1e-6, 0, 0), (1, 0, 0)),
                 ((-1e-6, 0, 0), (1, 0, 0)), ((0.5, 0.5, 0), (0, 1, 0)), ((NAN, 0, 0), (1, 0, 0))]:
        compare(emu, 0x4B2C70, "Vec3_RayPlane",
                setup=lambda e: (e.alloc(12), floats(e, *p), floats(e, *d), floats(e, *n)))


@pytest.mark.parametrize("v", vecs(20, 3) + [(0.0, 1.0, 1.0), (1.0, 0.0, 0.0), (0.0, 0.0, -5.0), (NAN, 1, 1)])
def test_vec3_to_angles(v):
    compare(emu, 0x4B2D00, "Vec3_ToAngles", setup=lambda e: (floats(e, *v), e.alloc(4), e.alloc(4)),
            check_eax=False)


NORMALS = [(0, 1, 0), (0, -1, 0.5), (0, 0, 1), (0.5, 0.2, -1), (1, 0, 0), (-1, 0.5, 0.5), (1, 1, 1), (NAN, 1, 0)]


@pytest.mark.parametrize("n", NORMALS)
def test_triangle_coords(n):
    cases = [((0.2, 0.2, 0.2), (1, 0, 0), (0, 1, 1)), ((0.9, 0.9, 0.9), (1, 0, 0), (0, 1, 1)),
             ((0.3, 0.1, 0.4), (0, 0, 1), (1, 1, 0)), ((-0.5, 0.2, 0.1), (1, 2, 3), (3, 1, 2)),
             ((0.1, 0.1, 0.1), (0, 0, 0), (1, 1, 1)), ((0.5, 0.5, 0.5), (1, 1, 1), (1, 1, 1)),
             ((0.1, 0.2, 0.3), (0, 1, 0), (1, 0, 0)), ((0.1, 0.2, 0.3), (0, 0, 1), (1, 0, 0))]
    cases += [tuple(vecs(1, 3)[0] for _ in range(3)) for _ in range(10)]
    for p, e1, e2 in cases:
        for outs in (True, False):
            compare(emu, 0x4B2D90, "Vec3_TriangleCoords",
                    setup=lambda e: (e.alloc(4) if outs else 0, e.alloc(4) if outs else 0,
                                     floats(e, *p), floats(e, *e1), floats(e, *e2), floats(e, *n)),
                    eax_mask=0xFF)
