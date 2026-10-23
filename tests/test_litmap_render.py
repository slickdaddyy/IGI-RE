"""Hemicube_DrawLightmapSet (0x521E40): a lightmap set's meshes drawn into a hemicube face."""
import math
import random
import struct

import pytest
from unicorn import UC_HOOK_CODE

from emu import Emu, compare
from test_patrol_util import with_cw

RENDER = 0x521E40
OURS = "Hemicube_DrawLightmapSet"
DRAW = 0x521110
CLIP = 0x521960
LIGHTMAP_BUFFER = 0xA83B28
NAN = float("nan")

e = Emu()

# with fake["on"] the two rasterisers only log their arguments, with the
# pointed-to vertices and uvs as bytes
fake = {"on": False, "skip_src": False}
runs = []


def _rasteriser(nargs, floats, uvs):
    def cb(uc, addr, size, user):
        if not fake["on"]:
            return
        args = [e.arg(i) for i in range(nargs)]
        shown = []
        for i, a in enumerate(args):
            if i == 3 and fake["skip_src"]:
                continue
            if i in floats:
                shown.append(e.read(a, 12))
            elif i in uvs:
                shown.append(e.read(a, 8))
            else:
                shown.append(a)
        runs[-1].append((addr,) + tuple(shown))
        e.ret(0)
    return cb


e.uc.hook_add(UC_HOOK_CODE, _rasteriser(17, (10, 11, 12), (13, 14, 15)), begin=DRAW, end=DRAW)
e.uc.hook_add(UC_HOOK_CODE, _rasteriser(21, (14, 15, 16), (17, 18, 19)), begin=CLIP, end=CLIP)


def f32(p, *v):
    e.write(p, struct.pack("<%df" % len(v), *v))


def u32(p, *v):
    e.write(p, struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v]))


def u16(p, *v):
    e.write(p, struct.pack("<%dH" % len(v), *[x & 0xFFFF for x in v]))


def d64(p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def rotation(rng):
    a, b, c, d = (rng.gauss(0, 1) for _ in range(4))
    n = math.sqrt(a * a + b * b + c * c + d * d)
    a, b, c, d = a / n, b / n, c / n, d / n
    return [a * a + b * b - c * c - d * d, 2 * (b * c - a * d), 2 * (b * d + a * c),
            2 * (b * c + a * d), a * a - b * b + c * c - d * d, 2 * (c * d - a * b),
            2 * (b * d - a * c), 2 * (c * d + a * b), a * a - b * b - c * c + d * d]


def matrix(m, count=0):
    p = e.alloc(0x28)
    f32(p, *m)
    u32(p + 0x24, count)
    return p


def ids(n, base):
    p = e.alloc(4 * n)
    u32(p, *range(base, base + n))
    return p


def lightmaps(rng, ntex):
    """A Lightmaps block with ntex small power-of-two id textures."""
    lm = e.alloc(0x3C + 0x2C * ntex)
    info = e.alloc(0x28 * ntex)
    u32(lm + 0x38, info)
    for t in range(ntex):
        w, h = rng.choice([1, 2, 4, 8]), rng.choice([1, 2, 4, 8])
        tex = lm + 0x3C + 0x2C * t
        u32(tex + 0x24, ids(w * h, 0x5000 + 0x100 * t))
        e.write(tex + 0x28, struct.pack("<hh", w, h))
        u32(info + 0x28 * t + 0x24, ids(w * h, 0x6000 + 0x100 * t))
    return lm


def mesh(rng, verts, chunks, unk8=1):
    """verts: [(pos, uv)]; chunks: [(texture, base, flag, indices)]."""
    vp = e.alloc(0x28 * max(len(verts), 1))
    for i, (pos, uv) in enumerate(verts):
        f32(vp + 0x28 * i, *pos)
        f32(vp + 0x28 * i + 0x20, *uv)
    first = prev = None
    for tex, base, flag, idx in chunks:
        pad = rng.choice([0, 2, 6])
        c = e.alloc(0x20 + 2 * len(idx) + pad)
        u16(c + 0xC, len(idx), 0xFFFF)
        u16(c + 0x12, tex, base)
        u16(c + 0x18, flag)
        u16(c + 0x20, *idx)
        if prev is None:
            first = c
        else:
            u16(prev + 0xE, c - (prev + 0x20))
        prev = c
    m = e.alloc(0x20)
    u32(m, 0, 1, unk8, 0, len(verts), vp, 0, first)
    return m


def lod(mesh_ptr, fmt=3):
    hdr = e.alloc(0x28)
    u32(hdr + 0x20, fmt, mesh_ptr)
    data = e.alloc(0x10)
    u32(data + 0xC, hdr)
    lp = e.alloc(0x10)
    u32(lp + 0xC, data)
    return lp


def lightmap_set(refs):
    """refs: [(lightmaps, lod, matrix, pos, lodIndex, lodCount)]."""
    s = e.alloc(0x18 + 0x24 * len(refs))
    u32(s, len(refs))
    for i, (lm, lp, mp, pos, li, lc) in enumerate(refs):
        pp = e.alloc(24)
        d64(pp, *pos)
        u32(s + 0x18 + 0x24 * i, lm, lp, 0, mp, pp, 0, li, lc, 0)
    info = e.alloc(0xE0)
    u32(info + 0x88, s)
    return info


def random_mesh(rng, ntex, spread, nan=False):
    nv = rng.randrange(10, 40)
    verts = []
    for _ in range(nv):
        pos = [rng.uniform(-spread, spread) for _ in range(3)]
        if nan and rng.random() < 0.1:
            pos[rng.randrange(3)] = NAN
        verts.append((pos, (rng.uniform(-0.5, 1.5), rng.uniform(-0.5, 1.5))))
    if rng.random() < 0.3:
        verts[1] = (list(verts[0][0]), verts[1][1])
    chunks = []
    for _ in range(rng.randrange(1, 4)):
        base = rng.randrange(3)
        idx = []
        for _ in range(rng.randrange(3, 16)):
            if rng.random() < 0.15:
                a = rng.randrange(nv - base)
                tri = rng.choice([[a, a, (a + 1) % (nv - base)], [a, (a + 1) % (nv - base), a],
                                  [(a + 1) % (nv - base), a, a]])
            else:
                tri = rng.sample(range(nv - base), 3)
            idx += tri
        idx += [rng.randrange(nv - base) for _ in range(rng.choice([0, 0, 1, 2]))]
        chunks.append((rng.randrange(ntex), base, rng.choice([0, 0, 1, 2]), idx))
    return verts, chunks


def buffers(rng, w, h, ortho):
    n = max(w * h, 1)
    idp = e.alloc(4 * n)
    dp = e.alloc(4 * n)
    far = 1e30 if ortho else 0.0
    f32(dp, *[far if rng.random() < 0.7 else rng.uniform(0, 0.5 if not ortho else 40) for _ in range(n)])
    u32(idp, *[rng.randrange(1 << 32) for _ in range(n)])
    return idp, dp


def back_buffer():
    p = e.alloc(4 * 64)
    u32(p, *range(0x9000, 0x9040))
    u32(LIGHTMAP_BUFFER, p)


def random_scene(seed, ortho):
    """A camera with a few objects around it, some crossing the near plane."""
    rng = random.Random(seed * 2 + ortho)
    back_buffer()
    w = rng.choice([16, 24, 32])
    h = w if rng.random() < 0.7 else rng.choice([8, 16, 32])
    origin = [rng.uniform(-100, 100) for _ in range(3)]
    cam = rotation(rng)
    lod_ = rng.randrange(3)
    refs = []
    for i in range(rng.randrange(2, 5)):
        ntex = rng.randrange(1, 4)
        lm = lightmaps(rng, ntex)
        verts, chunks = random_mesh(rng, ntex, rng.choice([4.0, 10.0, 16.0]), nan=seed % 7 == 6)
        lp = lod(mesh(rng, verts, chunks))
        # object centre in camera space, mostly in front
        cz = rng.uniform(-4, 25)
        cx, cy = rng.uniform(-0.7, 0.7) * max(cz, 6), rng.uniform(-0.7, 0.7) * max(cz, 6)
        world = [origin[r] + cam[r] * cx + cam[3 + r] * cy + cam[6 + r] * cz for r in range(3)]
        lc = rng.randrange(1, 4)
        li = lod_ if i == 0 else rng.choice([lod_, lod_, rng.randrange(lc), lc - 1])
        refs.append((lm, lp, matrix(rotation(rng), rng.choice([0, 3, 999, 1000])), world, li, lc))
    info = lightmap_set(refs)
    idp, dp = buffers(rng, w, h, ortho)
    op = e.alloc(24)
    d64(op, *origin)
    mp = matrix(cam, rng.choice([0, 5, 1000]))
    half = w * 0.5 - 0.5
    radius = rng.uniform(8, 20)
    return [info, op, mp, idp, dp, w, h, fbits(half), fbits(half - rng.choice([0, 1])), lod_, ortho,
            seed & 1, (seed >> 1) & 1, fbits(radius), (seed >> 2) & 1]


def run(setup, cw=0x37F, fake_on=False, skip_src=False):
    fake["on"], fake["skip_src"] = fake_on, skip_src
    del runs[:]

    def s(em):
        args = setup()
        runs.append([])
        return tuple(args)
    try:
        compare(e, with_cw(e, RENDER, cw), with_cw(e, OURS, cw), setup=s, check_eax=False)
    finally:
        fake["on"] = False
    assert runs[0] == runs[1], "rasteriser calls differ:\n orig %s\n ours %s" % (runs[0], runs[1])
    return runs[0]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("ortho", [0, 1])
@pytest.mark.parametrize("seed", range(8))
def test_render(seed, ortho, cw):
    run(lambda: random_scene(seed, ortho), cw)


@pytest.mark.parametrize("cw", [0xC7F, 0x77F])
@pytest.mark.parametrize("ortho", [0, 1])
@pytest.mark.parametrize("seed", [1, 3])
def test_render_truncating(seed, ortho, cw):
    run(lambda: random_scene(seed, ortho), cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("ortho", [0, 1])
@pytest.mark.parametrize("seed", range(16))
def test_render_calls(seed, ortho, cw):
    calls = run(lambda: random_scene(seed, ortho), cw, fake_on=True)
    if 1 <= seed <= 3:
        assert calls


@pytest.mark.parametrize("ortho", [0, 1])
@pytest.mark.parametrize("seed", range(4))
def test_render_id_mode(seed, ortho):
    """idMode 2 keeps the previous id texture, first an uninitialised one."""
    def setup():
        args = random_scene(seed, ortho)
        args[14] = 2 + seed
        return args
    run(setup, 0x7F, fake_on=True, skip_src=True)


def simple_scene(rng, verts, chunks, ortho=1, cam=None, R=None, counts=(0, 0), pos=(0.0, 0.0, 0.0), lod_=0,
                 li=0, lc=1, fmt=3, unk8=1, w=16, h=16, clear=1, flagged=0, nrefs=1, mode=0):
    back_buffer()
    refs = []
    for _ in range(nrefs):
        lm = lightmaps(rng, 2)
        lp = lod(mesh(rng, verts, chunks, unk8), fmt)
        refs.append((lm, lp, matrix(R or [1, 0, 0, 0, 1, 0, 0, 0, 1], counts[0]), list(pos), li, lc))
    info = lightmap_set(refs)
    idp, dp = buffers(rng, w, h, ortho)
    op = e.alloc(24)
    d64(op, 0.0, 0.0, 0.0)
    mp = matrix(cam or [1, 0, 0, 0, 1, 0, 0, 0, 1], counts[1])
    return [info, op, mp, idp, dp, w, h, fbits(7.5), fbits(7.5), lod_, ortho, flagged, clear, fbits(20.0), mode]


QUADS = [([-6.0, -6.0, 10.0], (0, 0)), ([6.0, -6.0, 12.0], (1, 0)), ([6.0, 6.0, 14.0], (1, 1)),
         ([-6.0, 6.0, 9.0], (0, 1)), ([0.0, 0.0, 2.0], (0.5, 0.5)), ([0.0, 0.0, 0.0], (0.5, 0.2)),
         ([0.0, 0.0, -3.0], (0.2, 0.5)), ([3.0, 0.0, 0.0], (0.1, 0.1)), ([0.0, 3.0, 0.0], (0.9, 0.1))]
TRIS = [0, 1, 2, 0, 2, 3, 2, 1, 0, 0, 1, 4, 4, 1, 0, 5, 1, 2, 2, 1, 5, 6, 0, 1, 1, 0, 6, 5, 7, 8, 8, 7, 5,
        7, 5, 4, 4, 5, 7]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("ortho", [0, 1])
@pytest.mark.parametrize("case", ["plain", "last", "skip_lod", "format", "unk8", "empty", "no_pixels",
                                  "few_indices", "wrap", "flagged", "noclear", "mode1", "no_verts"])
def test_render_cases(case, ortho, cw):
    def setup():
        rng = random.Random(case)
        kw = dict(ortho=ortho)
        chunks = [(0, 0, 0, TRIS), (1, 0, 1, TRIS)]
        if case == "last":
            kw.update(lod_=3, li=1, lc=2)
        elif case == "skip_lod":
            kw.update(lod_=1, li=0, lc=3, nrefs=2)
        elif case == "format":
            kw.update(fmt=2)
        elif case == "unk8":
            kw.update(unk8=0)
        elif case == "empty":
            kw.update(nrefs=0)
        elif case == "no_pixels":
            kw.update(w=0)
        elif case == "few_indices":
            chunks = [(0, 0, 0, [0, 1]), (1, 0, 0, []), (0, 0, 0, TRIS[:9])]
        elif case == "wrap":
            chunks = [(0, 0xFFFE, 0, [2, 3, 4, 3, 4, 5])]
        elif case == "flagged":
            kw.update(flagged=1)
        elif case == "noclear":
            kw.update(clear=0)
        elif case == "mode1":
            kw.update(mode=1)
        elif case == "no_verts":
            return simple_scene(rng, [], [(0, 0, 0, [0, 0, 1, 2, 2, 2])], **kw)
        return simple_scene(rng, QUADS, chunks, **kw)
    run(setup, cw)
    run(setup, cw, fake_on=True)


ROWS = [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1], [NAN, 0, 0], [0, NAN, 0], [0, 0, NAN]]


@pytest.mark.parametrize("ortho", [0, 1])
@pytest.mark.parametrize("rows", [(1, 0, 3), (0, 0, 0), (2, 1, 3), (3, 4, 2), (1, 2, 3), (1, 3, 2), (1, 1, 1),
                                  (4, 2, 3), (5, 3, 1), (6, 1, 2), (3, 2, 1), (2, 3, 1), (1, 2, 0)])
def test_render_reortho(rows, ortho):
    """More than 1000 products: the exe re-orthonormalises the camera matrix and throws it away."""
    def setup():
        rng = random.Random(str(rows))
        cam = ROWS[rows[0]] + ROWS[rows[1]] + ROWS[rows[2]]
        return simple_scene(rng, QUADS, [(0, 0, 0, TRIS)], ortho=ortho, cam=cam, counts=(600, 600))
    run(setup, 0x7F, fake_on=True)


# triangles crossing the near plane with degenerate normals and view directions
NEAR = [([0.0, 0.0, 0.0], (0, 0)), ([1.0, 0.0, 0.0], (1, 0)), ([2.0, 0.0, 0.0], (0, 1)),
        ([0.0, 0.0, 5.0], (1, 1)), ([0.0, 1.0, 1.0], (0.5, 0)), ([1.0, 1.0, 1.0], (0, 0.5)),
        ([NAN, 1.0, 1.0], (0.5, 0.5)), ([0.0, 0.0, 9.0], (0.2, 0.3)), ([4.0, 0.0, 9.0], (0.3, 0.2)),
        ([0.0, 4.0, 9.0], (0.7, 0.8)), ([0.0, 0.0, -2.0], (0.6, 0.6))]
NEAR_TRIS = [0, 1, 2, 2, 1, 0, 0, 1, 3, 3, 1, 0, 4, 5, 3, 3, 5, 4, 6, 4, 3, 3, 4, 6, 3, 1, 0, 10, 8, 9, 9, 8, 10,
             10, 7, 8, 1, 4, 10, 7, 4, 1, 3, 10, 7, 7, 10, 3]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("ortho", [0, 1])
@pytest.mark.parametrize("pos", [(0.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0), (0.0, 0.0, -1.0),
                                 (NAN, 0.0, 0.0), (0.0, NAN, 0.0), (0.0, 0.0, NAN), (0.5, -0.5, 2.0)])
@pytest.mark.parametrize("last", [0, 1])
def test_render_near(pos, last, ortho, cw):
    def setup():
        rng = random.Random(str(pos))
        kw = dict(lod_=2, li=0, lc=1) if last else {}
        R = [1, 0, 0, 0, 1, 0, 0, 0, 1] if pos[0] == 0 else [0, 0, 1, 1, 0, 0, 0, 1, 0]
        return simple_scene(rng, NEAR, [(0, 0, 0, NEAR_TRIS), (1, 0, 0, NEAR_TRIS[::-1])], ortho=ortho, pos=pos,
                            R=R, **kw)
    run(setup, cw, fake_on=True)
    if not any(math.isnan(v) for v in pos):
        run(setup, cw)
