"""GlassShatter_AddTri (0x44A670): axis aligned and degenerate triangles, both shard modes."""
import random
import struct

import pytest

from test_glass_addtri import e, fk, setup_tri
from test_glass_fakes import check, f32


def tri_case(pts, hit, normal, mode=0, ten=10.0, seed=0):
    def setup(em):
        p = setup_tri(em, random.Random(seed), 300, True, mode=mode, ten=ten, normal=list(normal))
        for q, v in zip(p, pts):
            em.write(q, f32(*v))
        em.write(0x57C250, f32(*hit))
        return p
    return setup


def run(setup, cw=0x37F):
    check(e, fk, 0x44A670, "GlassShatter_AddTri", setup, cw=cw, normalized=False)


def along(k, a, t):
    p = list(a)
    p[k] += t
    return p


A = [12.5, -40.25, 7.75]
B = [130.0, 55.5, -60.0]
HIT = [20.0, -30.0, 10.0]


@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("normal", [(1, 0, 0), (0, 1, 0), (0, 0, 1)])
@pytest.mark.parametrize("k", range(3))
def test_addtri_axis_edge(k, normal, mode):
    """One edge runs along an axis, so the inside test divides by zero."""
    run(tri_case([A, B, along(k, A, 90.0)], HIT, normal, mode))


@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("k", range(3))
def test_addtri_axis_plane(k, mode):
    pts = [[A[i] if i == k else A[i] + d[i] for i in range(3)] for d in ([0, 0, 0], [100, 50, -30], [-20, 80, 60])]
    run(tri_case(pts, HIT, (0.3, 0.4, 0.8), mode))


@pytest.mark.parametrize("mode", [0, 1])
def test_addtri_repeated_corner(mode):
    run(tri_case([A, A, B], HIT, (0.3, 0.4, 0.8), mode))


@pytest.mark.parametrize("ten", [0.0, -1.0])
def test_addtri_mode1_still(ten):
    run(tri_case([A, B, [A[0], 90.0, 40.0]], HIT, (0.3, 0.4, 0.8), 1, ten=ten))


# hits next to a corner, where the distance to the edge rounds above the
# distance to the corner
NEAR = [
    ([[409.7550158894022, 159.21481361982444, 108.94482550856674],
      [229.40018032274486, -116.31036710996011, 356.9491268730603],
      [454.64630340173517, 438.45920071380885, 12.499934502988253]],
     [235.89775143544733, 249.492508581314, 289.36971091114776],
     [-0.742479745522403, -0.10891820862274498, -0.6609543488921151], 0x7F),
    ([[156.6565057107391, 148.20280456237435, -205.5072229005276],
      [202.62555272878524, -3.5063440071716627, -385.80935430799],
      [-187.99507329942958, -156.65752872140206, 296.20301118522366]],
     [91.9326062663122, 47.23605231410778, -137.0546879419586],
     [-0.8623534611553916, 0.25708747913240915, -0.4361794769434478], 0x37F),
]


@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("case", range(len(NEAR)))
def test_addtri_near_corner(case, mode):
    pts, hit, n, cw = NEAR[case]
    run(tri_case(pts, hit, n, mode), cw)
