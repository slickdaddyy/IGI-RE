import random
import struct

import pytest

from test_d3d_util import D3DEmu, check, logged, pyfake, trap, u32
from test_d3d_util import com
from test_dxinit_util import DEV, check_cw, f32, noise, vertex_buffer

e = D3DEmu()

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]
DEVICE = 0x5CA114
LISTS = (0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4)
INDEXES = (0xA7CED8, 0xA7D9F8, 0xA7D9F4, 0xBCACEC)
TEX, PAL, STRIP_ID, DRAW_ID = 0xA84A50, 0xA84A58, 0xA84A54, 0x54E890
SORT_ACTIVE, CTX_VALID = 0xA84A4C, 0x6E5BB0
VB, VB_SIZE, VB_USED = 0xBCAD00, 0xBCAD04, 0xBCAD08
LAST_FLAGS, LAST_TEX = 0xA7DA00, 0xA7DA04
SCENE_MIN = 0xBCABD0
NEAR = 0xBCADC0

FRAME = [1]
pyfake(e, 0x497800, "Frame_Alloc", 1, before=lambda e: e.alloc(e.arg(0)) if FRAME[0] else 0)
pyfake(e, 0x4B72E0, "SortRender_Add", 3, before=lambda e: e.alloc(e.arg(1)))
pyfake(e, 0x4950E0, "Direct3D_GetRenderMode", 0, ret=6)
pyfake(e, 0x4B0EC0, "RenderMode_SetMethod", 3)


def node_bytes(e, p):
    if not p:
        return None
    return e.read(p, 0x28 + 0x1C * min(e.u32(p + 0x1C), 64))


pyfake(e, 0x497860, "Primitive_Draw", 1, before=lambda e: (None, node_bytes(e, e.arg(0))))
pyfake(e, 0x4978B0, "RenderQueue_Add", 2, before=lambda e: (None, node_bytes(e, e.arg(1))))




def fl(v):
    return struct.pack("<f", v)


def scene(em, rng):
    """Clip box, depth range and the state lists."""
    x0, y0 = rng.uniform(-5, 50), rng.uniform(-5, 50)
    em.write(SCENE_MIN, f32(x0, y0, x0 + rng.uniform(100, 600), y0 + rng.uniform(100, 400)))
    em.write(0xBCADA0, fl(rng.uniform(0.5, 4)))      # min z
    em.write(0xBCADC8, fl(rng.uniform(500, 9000)))   # max z
    em.write(0xBCADB4, fl(rng.uniform(0, 0.1)))      # viewport min z
    em.write(0xBCAD8C, fl(rng.uniform(0.9, 1)))      # viewport max z
    em.write(0xBCABEC, fl(rng.choice([0.001, 0.25, 1.0])))
    em.write(0xBCAB6C, fl(rng.uniform(1, 1.01)))
    em.write(NEAR, fl(rng.uniform(0.5, 4)))
    u32(em, CTX_VALID, rng.choice([0, 1, 1]))
    u32(em, SORT_ACTIVE, rng.choice([0, 1]))
    u32(em, DRAW_ID, 0x33)


def texture(em, rng):
    t = em.alloc(0x24)
    em.write(t, noise(0x10, 3) + f32(rng.uniform(0.5, 2), rng.uniform(0.5, 2)) + noise(8, 4)
             + struct.pack("<I", 0x5000 + rng.randrange(100)))
    return t


def verts(em, rng, count, inside=1.0):
    p = em.alloc(0x24 * count)
    x0, y0, x1, y1 = struct.unpack("<4f", em.read(SCENE_MIN, 16))
    out = []
    for _ in range(count):
        if rng.random() < inside:
            x, y = rng.uniform(x0, x1), rng.uniform(y0, y1)
        else:
            x, y = rng.uniform(x0 - 100, x1 + 100), rng.uniform(y0 - 100, y1 + 100)
        z = rng.choice([-1.0, 0.0, 0.01, rng.uniform(1, 50), rng.uniform(1, 8000), 1e7])
        out += [x, y, z] + [rng.uniform(0, 1) for _ in range(2)] + [rng.uniform(0, 1.2) for _ in range(4)]
    em.write(p, f32(*out))
    return p


def strip(em, rng, count, lines=0, flags=0, queue=-1, inside=1.0):
    s = em.alloc(0x14)
    em.write(s, struct.pack("<iIiiI", lines, flags, queue, count, verts(em, rng, count, inside)))
    return s


def random_flags(rng):
    f = 0
    for bit in (4, 0x20, 8, 0x10, 0x80000, 0x800000, 0x10000000, 0x20000, 0x40000, 0x40000000, 0x80000000,
                0x1000000, 0x400000):
        if rng.random() < 0.3:
            f |= bit
    return f


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(12))
def test_emit(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        FRAME[0] = seed != 5
        scene(em, rng)
        u32(em, TEX, texture(em, rng) if seed % 3 else 0)
        u32(em, PAL, 0x4444)
        return (strip(em, rng, rng.randrange(1, 6), lines=seed & 1, flags=random_flags(rng), queue=rng.choice([-1, 2])),)
    check_cw(e, 0x50D4C0, "D3DQVertex_Emit", cw, setup=setup, check_eax=False)
    FRAME[0] = 1


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("flags", [0, 4, 0x20, 0x24, 0x10000000, 0x20000, 0x60000, 0x40020000])
def test_emit_flags(flags, cw):
    def setup(em):
        rng = random.Random(flags)
        scene(em, rng)
        u32(em, SORT_ACTIVE, 1)
        u32(em, TEX, texture(em, rng))
        return (strip(em, rng, 4, flags=flags),)
    check_cw(e, 0x50D4C0, "D3DQVertex_Emit", cw, setup=setup, check_eax=False)


def state_fn(em, name):
    return trap(em, name, logged(name, 1, pop=0, before=lambda e: (None, e.read(e.arg(0), 8))))


def device(em, rng):
    dev = com(em, "dev", DEV)
    u32(em, DEVICE, dev)
    for i, p in enumerate(LISTS):
        lst = em.alloc(0x14 + 4 * 3)
        em.write(lst, struct.pack("<IiIII", 0xABCDABCD, rng.randrange(-1, 3), 3, 3, 0xDCBADCBA))
        em.write(lst + 0x14, struct.pack("<3I", *(state_fn(em, "state%d.%d" % (i, j)) for j in range(3))))
        u32(em, p, lst)
        u32(em, INDEXES[i], rng.randrange(3))
    return dev


def strip_node(em, rng, count, lines, flags, tex):
    n = em.alloc(0x28 + 0x1C * count)
    em.write(n, noise(0x28 + 0x1C * count, rng.randrange(40)))
    em.write(n + 0xC, struct.pack("<II", tex, 0))
    em.write(n + 0x14, struct.pack("<I", flags))
    em.write(n + 0x1C, struct.pack("<iiI", count, lines, n + 0x28))
    return n


@pytest.mark.parametrize("seed", range(16))
def test_draw_strip(seed):
    def setup(em):
        rng = random.Random(seed)
        device(em, rng)
        vb, _ = vertex_buffer(em, 0x1C * 64, lock_ret=1 if seed == 7 else 0)
        u32(em, VB, vb)
        u32(em, VB_SIZE, 40)
        u32(em, VB_USED, rng.choice([0, 3, 36, 39, 40]))
        tex = texture(em, rng) if seed % 3 else 0
        u32(em, LAST_FLAGS, random_flags(rng))
        u32(em, LAST_TEX, rng.choice([0, tex, 0x1234]))
        count = rng.choice([1, 2, 2, 3, 5])
        return (strip_node(em, rng, count, rng.choice([0, 0, 1]), random_flags(rng), tex),)
    check(e, 0x50D7E0, "D3DQVertex_DrawStrip", setup=setup, check_eax=False)


def test_init():
    def setup(em):
        rng = random.Random(1)
        device(em, rng)
        u32(em, STRIP_ID, 4)
        u32(em, DRAW_ID, 5)
    check(e, 0x50DC30, "D3DQVertex_Init", setup=setup, check_eax=False)


@pytest.mark.parametrize("flags", [0, 4, 8, 0x10, 0x20000, 0x40000, 0x60000, 0x80000000, 0x1000000, 0x400000,
                                   0x1400018, 0x8000003C])
@pytest.mark.parametrize("tex", [0, 1])
def test_states(flags, tex):
    def setup(em):
        rng = random.Random(flags)
        device(em, rng)
        u32(em, 0xBCAD78, 7)
        st = em.alloc(8)
        em.write(st, struct.pack("<II", flags, texture(em, rng) if tex else 0))
        return (st,)
    check(e, 0x50DCC0, "D3DQVertex_SetRenderStates", setup=setup, check_eax=False)
    check(e, 0x50DE00, "D3DQVertex_SetTextureStates", setup=setup, check_eax=False)
    check(e, 0x50E000, "D3DQVertex_SetBlendStates", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(16))
def test_strip(seed, cw):
    """Strip, and through it the three clippers and Emit."""
    def setup(em):
        rng = random.Random(seed)
        scene(em, rng)
        u32(em, TEX, texture(em, rng) if seed % 3 else 0)
        count = [2, 2, 3, 6][seed % 4]
        lines = seed % 5 == 1
        return (strip(em, rng, count, lines=lines, flags=rng.choice([0, 0x20000, 0x40000, 4]),
                      inside=rng.choice([1.0, 0.7, 0.3])),)
    check_cw(e, 0x50E0D0, "D3DQVertex_Strip", cw, setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(10))
@pytest.mark.parametrize("which", ["lines", "strip", "rect"])
def test_clippers(which, seed, cw):
    addr, name, count = {"lines": (0x50E1E0, "D3DQVertex_ClipLines", 5), "strip": (0x50E520, "D3DQVertex_ClipStrip", 6),
                         "rect": (0x50E7F0, "D3DQVertex_ClipRect", 2)}[which]

    def setup(em):
        rng = random.Random(seed + 1000)
        scene(em, rng)
        s = strip(em, rng, count, lines=which == "lines", flags=rng.choice([0, 0x20000]), inside=0.5)
        if seed == 3:   # on the clip edges
            x0, y0, x1, y1 = struct.unpack("<4f", em.read(SCENE_MIN, 16))
            vp = em.u32(s + 16)
            for i in range(count):
                em.write(vp + 0x24 * i, f32([x0, x1 + 1, x1 + 2, x1][i % 4], [y0, y1 + 1, y1, y1 + 3][i % 4]))
        return (s,)
    check_cw(e, addr, name, cw, setup=setup, check_eax=False)


def clip_verts(em, rng, n):
    ptrs = em.alloc(4 * n)
    for i in range(n):
        c = em.alloc(0x28)
        em.write(c, f32(rng.uniform(0, 600), rng.uniform(0, 400), rng.choice([rng.uniform(0.001, 2), 1.0, 0.25]),
                        *[rng.uniform(0, 1.5) for _ in range(6)]) + struct.pack("<I", rng.randrange(32)))
        u32(em, ptrs + 4 * i, c)
    return ptrs


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
def test_emit_clipped(seed, cw):
    def setup_line(em):
        rng = random.Random(seed)
        scene(em, rng)
        s = strip(em, rng, 2, lines=seed % 3 == 0, flags=random_flags(rng))
        return clip_verts(em, rng, 2), s
    check_cw(e, 0x50E450, "D3DQVertex_EmitLine", cw, setup=setup_line, check_eax=False)

    def setup_tri(em):
        rng = random.Random(seed)
        scene(em, rng)
        s = strip(em, rng, 3, flags=random_flags(rng))
        return clip_verts(em, rng, 3), s
    check_cw(e, 0x50E730, "D3DQVertex_EmitTriangle", cw, setup=setup_tri, check_eax=False)
