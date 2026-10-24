"""Branch cases for SplineObjSegment_Hit (0x4E16E0) and Sector_CreateMagicObjs (0x4DFA50)."""
import struct

import pytest

from test_shadow_fakes import f32, f64, i32, u16
from test_shadow_ray import CWS, SCENE, make_scene, run
from test_shadow_ray import e as ray_emu


def lodinfo(em, o):
    mods = em.u32(o + 0x6C)
    lm = em.u32(mods + 4 * em.u32(o + 0xA4))
    return em.u32(em.u32(lm + 0xC) + 0xC)


def obj_pos(em, o):
    return list(struct.unpack("<3d", em.read(o + 0x20, 24)))


def set_ray(em, q, k, a, b):
    em.write(em.u32(q) + 24 * k, f64(*a))
    em.write(em.u32(q + 4) + 24 * k, f64(*b))


def tweak(base, fn):
    def setup(em):
        o, q = base(em)
        fn(em, o, q)
        return o, q
    return setup


def no_parts(em, o, q):
    i32(em, lodinfo(em, o) + 0x2C, 0)


def no_rays(em, o, q):
    i32(em, q + 8, 0)


def outside(axis, sign):
    """The first ray passes the bounding cube on one side."""
    def fn(em, o, q):
        pos = obj_pos(em, o)
        r = struct.unpack("<f", em.read(o + 0xF4, 4))[0]
        a, b = list(pos), list(pos)
        a[axis] += sign * (r + 10.0)
        b[axis] += sign * (r + 20.0)
        b[(axis + 1) % 3] += 5.0
        set_ray(em, q, 0, a, b)
    return fn


def part_of(em, o, q):
    return em.u32(lodinfo(em, o) + 0x2C) + 0x20 * em.u32(q + 0x14)


def identity_frame(em, o):
    em.write(o + 0x70, f32(1, 0, 0, 0, 1, 0, 0, 0, 1))
    em.write(o + 0x520, f32(1, 0, 0, 0, 1, 0, 0, 0, 1))


def to_world(em, o, p):
    """Object space to world for an object with an identity frame."""
    pos = obj_pos(em, o)
    if em.read(o + 0x5B4, 1)[0]:
        lo, hi = struct.unpack("<2f", em.read(o + 0xFC, 8))
        p = [p[0] - (lo + hi) * 0.5, p[1], p[2]]
    return [pos[i] + p[i] for i in range(3)]


def axis_tri(em, o, q):
    """A triangle whose vertices share one coordinate exactly: (axis, vertices)."""
    part = part_of(em, o, q)
    tris, verts = em.u32(part + 0x10), em.u32(part + 0x14)
    for k in range(em.u32(part)):
        t = struct.unpack("<3H", em.read(tris + 8 * k, 6))
        vs = [struct.unpack("<3f", em.read(verts + 16 * i, 12)) for i in t]
        for ax in range(3):
            if vs[0][ax] == vs[1][ax] == vs[2][ax]:
                return ax, vs
    raise AssertionError("no axis triangle")


def parallel(em, o, q):
    """The first ray runs inside the plane of an axis aligned triangle."""
    identity_frame(em, o)
    ax, vs = axis_tri(em, o, q)
    c = [sum(v[i] for v in vs) / 3 for i in range(3)]
    c[ax] = vs[0][ax]
    j = (ax + 1) % 3
    a, b = list(c), list(c)
    a[j] -= 60.0
    b[j] += 60.0
    set_ray(em, q, 0, to_world(em, o, a), to_world(em, o, b))


def empty_nodes(em, o, q):
    """Every node but the root loses its triangles."""
    part = part_of(em, o, q)
    for nodes, count in ((em.u32(part + 0x1C), em.u32(part + 0xC)),
                         (em.u32(o + 0x5A0 + 8 * em.u32(q + 0x14)), em.u32(o + 0x5A4 + 8 * em.u32(q + 0x14)))):
        for k in range(1, count):
            u16(em, nodes + 0x18 * k + 0x12, 0)


def near_prev(em, o, q):
    """Each hit of a first pass comes back as a previous hit just behind the
    real one (and without a back material), so the new hit lands within 0.1."""
    identity_frame(em, o)
    em.write(o + 0x98, f32(1.0))
    em.call(0x4E16E0, o, q)
    lo, hi = struct.unpack("<2f", em.read(o + 0xFC, 8))
    mid = (lo + hi) * 0.5 if em.read(o + 0x5B4, 1)[0] else 0.0
    froms, tos, hits = em.u32(q), em.u32(q + 4), em.u32(q + 0x20)
    for k in range(em.u32(q + 8)):
        h = hits + 0x70 * k
        if em.u32(h + 0x58) != 1:
            continue
        a = struct.unpack("<3d", em.read(froms + 24 * k, 24))
        b = struct.unpack("<3d", em.read(tos + 24 * k, 24))
        d = [b[i] - a[i] for i in range(3)]
        ln = sum(x * x for x in d) ** 0.5
        dist = struct.unpack("<d", em.read(h + 0x60, 8))[0] + 0.05
        p = [a[i] + d[i] / ln * dist for i in range(3)]
        p[0] += mid
        em.write(h + 8, f64(*p))
        i32(em, h + 0x4C, 0)
        em.write(h + 0x60, f64(1e9))


RAY_CASES = (
    [("noparts", make_scene(s, r), no_parts) for s, r in ((0, 0), (2, 1))] +
    [("norays", make_scene(5, r), no_rays) for r in (0, 1)] +
    [("outside", make_scene(7, 0), outside(ax, sg)) for ax, sg in ((0, -1), (1, -1), (1, 1), (2, -1))] +
    [("parallel", make_scene(s, r, preset="none"), parallel) for s, r in ((0, 1), (2, 0), (4, 0), (9, 1))] +
    [("empty", make_scene(s, r), empty_nodes) for s in (0, 3) for r in (0, 1)] +
    [("skip", make_scene(s, r, skipset=range(200), preset="none"), None) for s in (0, 1) for r in (0, 1)] +
    [("near", make_scene(s, 1, preset="none"), near_prev) for s in (0, 6, 7)] +
    [("seed", make_scene(s, 0, s & 1, nrays=40), None) for s in (102, 107, 115)]
)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(RAY_CASES)), ids=lambda i: "%s%d" % (RAY_CASES[i][0], i))
def test_ray(cw, case):
    _, base, fn = RAY_CASES[case]
    run(tweak(base, fn) if fn else base, cw)


# --- Sector_CreateMagicObjs ----------------------------------------------------

import test_shadow_build as build  # noqa: E402

MAGIC_DEFS = [(b"other", b"x", 0x71, 0x5000), (b"radar", b"dish", 0x70, 0x6000), (b"ab", b"dish", 0x74, 0x6100),
              (b"sub", b"subm", 0x72, 0x7000)]


def magic_case(mats, om, names=None, def_list=MAGIC_DEFS, stamp=0):
    """One attachment per local matrix in mats, on a sector object with matrix om."""
    names = names or [b"radar"] * len(mats)

    def setup(em):
        build.MODELS.clear()
        build.MODELS[b"dish"] = build.make_model(em, (1, 2, 3, 4))
        build.MODELS[b"subm"] = build.make_model(em, (1, 2, 3, 4), flag=1)
        build.MODELS[b"x"] = build.make_model(em, (1, 2, 3, 4))
        build.set_handler(em, build.EV3, 0x70, build.LOGA)
        build.set_handler(em, build.EV3, build.ST, build.LOGA)
        build.defs(em, def_list)
        so = build.new_task(em, build.SOT, 0x1A0)
        em.write(so + 0x20, f64(1000.5, -2000.25, 30.0))
        i32(em, so + 0x68, 0x4444)
        atts = [(n, (10.5 * k, -3.25, 7.0), m) for k, (n, m) in enumerate(zip(names, mats))]
        i32(em, so + 0x6C, build.make_model(em, (0, 0, 0, 1), atts=atts))
        em.write(so + 0x70, f32(*om))
        i32(em, so + 0x94, stamp)
        i32(em, so + 0xE8, build.new_task(em, 0x60))
        i32(em, so + 0x148, build.new_task(em, 0x60))
        return so, 0x8888
    return setup


def run_magic(setup, cw=0x37F):
    build.check(build.e, build.fk, 0x4DFA50, "Sector_CreateMagicObjs", setup=setup, cw=cw)


ID = (1, 0, 0, 0, 1, 0, 0, 0, 1)
PERM = (0, 1, 0, 0, 0, 1, 1, 0, 0)
# local matrices whose columns and cross products are zero in some parts, found by searching
MAGIC_MATS = [
    ([(0.0, 1e-25, -0.05550951284776673, 1.0, 0.0, -0.22038238595773185, 0.0, -1.0, -0.5424755574590947)],
     (0.8028549152229671, 0.0, 0.0, -0.23759152462357513, 2.0, 0.05525882872479637, 0.8783340378971731, 0.5, 1.0),
     2000),
    ([(0.0, 0.5, 0.0, 0.0, 0.6431002894870288, 0.0, 0.6693783241414566, 0.0, -0.7166387059466102),
      (-0.9364801082053267, -3.0, 0.4923697708090444, 0.0, -3.0, 1.0, 0.0, 1e-25, 0.09853986278503846)], ID, 0),
    ([(0.0, 2.0, -0.3040369840863846, 0.0, 0.0, 0.0, 0.2568970039642815, 0.0, -0.7217476819570581),
      (0.6838617170870476, 0.0, 1.0, -0.23529337435965103, -0.13415681723892736, 0.5, 1e-25, -0.7616265351882503, 2.0)],
     PERM, 2000),
    ([(0.0, -1.0, 1e-25, -1.0, 2.0, 0.0, 0.0, 0.0, 2.0),
      (-1.0, 1.0, 1.0, -0.46186157958873864, -3.0, 1e-25, 0.0, -3.0, 1.0),
      (2.0, -1.0, -0.3407128793589429, 1.0, -0.9389310501251804, 0.2449041217952732, 2.0, -0.5031470092324621, 0.0)],
     PERM, 2000),
    ([(1.0, 1.0, -0.04455653751406419, 0.0, 0.0, 0.5, 0.0, 1e-25, 0.0)], ID, 0),
    ([(1.0, -1.0, 0.0, 0.0, -0.11535771745937229, -0.8763756104184071, 0.333689274730137, 1e-25, -0.6938381374072551),
      (0.0, 2.0, 0.6811421226431069, 0.0, 0.0, -0.7378057308735761, 1.0, -0.13852337989632058, 0.5)], ID, 2000),
]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", range(len(MAGIC_MATS)))
def test_magic_zero_parts(case, cw):
    mats, om, stamp = MAGIC_MATS[case]
    run_magic(magic_case(mats, om, stamp=stamp), cw)


def test_magic_no_attachments():
    run_magic(magic_case([], ID))


def test_magic_no_defs():
    run_magic(magic_case([ID], ID, def_list=[]))


@pytest.mark.parametrize("name", [b"ab", b"ac", b"other", b"radar"])
def test_magic_names(name):
    """Even length names end on the first byte of a compared pair; "other" has no handler."""
    run_magic(magic_case([ID, PERM], ID, names=[name, b"ab"]))
