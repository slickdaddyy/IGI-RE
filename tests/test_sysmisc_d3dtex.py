import random
import struct

import pytest

from test_d3d_util import D3DEmu
from test_sysmisc_util import Com, Fakes, check, check_error, f32, u32

DDRAW, DEVICE = 0x5CA10C, 0x5CA114
DESC = 0xBCAC00
POOL, FORMATS, FORMAT_COUNT = 0xA7C128, 0xA7C140, 0xA7CEBC
ACTIVE_TMU, LOD, TEX_COUNT, TEX_BYTES, RELEASE_TIME = 0xA7CEAC, 0xA7CEB0, 0xA7CEB4, 0xA7CEB8, 0xA7CEC0
DDERR_NOTFOUND = 0x887600FF

e = D3DEmu()
fk = Fakes(e)
ST = {"qpc": []}


def _size_call(em, a):
    s = a[2]
    w, h = em.u32(s), em.u32(s + 4)
    em.write(s + 8, u32(ST.get("outw", w)) + u32(ST.get("outh", h)))


fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(a[0]))
fk.add(0x4B0D10, 1)
fk.add(0x4B0B00, 4)
fk.add(0x4B0B70, 1)
fk.add(0x4B0EC0, 3, fmt=lambda em, a: (a[0], a[1], em.normalize(a[2])))
fk.add(0x4B1000, 3, fn=_size_call, fmt=lambda em, a: (a[0], a[1], em.read(a[2], 8)))
fk.add(0x4950E0, 0, ret=7)
fk.add(0x495170, 1, fmt=lambda em, a: (em.normalize(a[0]),))
for _a, _v in ((0x4B2700, 0x21), (0x4B26E0, 0x22), (0x4B26F0, 0x23), (0x4B2710, 0x24), (0x4B2640, 0x25)):
    fk.add(_a, 0, ret=_v)

device = Com(fk, {4: 3, 35: 3})
surface = Com(fk, {12: 3, 22: 2}, derefs={12: {1: 16}, 22: {1: 4}})


def _qpc(em):
    v = ST["qpc"].pop(0)
    em.write(em.arg(0), struct.pack("<Q", v))
    em.ret(1, 4)


e.stub("QueryPerformanceCounter", _qpc)


def set_desc(em, minw=1, minh=1, maxw=2048, maxh=2048, aspect=8, caps=0, devcaps=0, stages=8, textures=2):
    em.write(DESC, u32(devcaps))
    em.write(DESC + 0x5C, u32(caps))
    em.write(DESC + 0x7C, u32(minw) + u32(minh) + u32(maxw) + u32(maxh))
    em.write(DESC + 0x90, u32(aspect))
    em.write(DESC + 0xB8, struct.pack("<HH", stages, textures))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("w,h", [(1, 1), (4, 3), (16, 16), (0, 5), (7, 0), (-2, 3), (33, 5)])
def test_alpha_coverage(w, h, cw):
    rng = random.Random(w * 100 + h)
    n = max(w * h, 1)
    data = b"".join(u32(rng.getrandbits(32)) for _ in range(n))

    def setup(em):
        p = em.alloc(4 * n)
        em.write(p, data)
        return p, w, h
    check(e, fk, 0x50A620, "D3DTex_AlphaCoverage", setup=setup, check_st0=True, cw=cw, check_eax=False)


@pytest.mark.parametrize("colors", [False, True])
def test_alloc_palette(colors):
    def setup(em):
        pal = em.alloc(12)
        em.write(pal, b"\x77" * 12)
        d = em.alloc(12)
        c = 0
        if colors:
            c = em.alloc(0x400)
            em.write(c, bytes(range(256)) * 4)
        em.write(d, u32(pal) + u32(0) + u32(c))
        return (d,)
    check(e, fk, 0x50A670, "D3DTex_AllocPalette", setup=setup, check_eax=False)


def test_dealloc_palette():
    def setup(em):
        pal = em.alloc(12)
        em.write(pal, u32(7) + u32(0) + u32(0x5000))
        return (pal,)
    check(e, fk, 0x50A6A0, "D3DTex_DeAllocPalette", setup=setup, check_eax=False)


def test_init():
    def setup(em):
        for i, a in enumerate((0x546FA8, 0x546FAC, 0x546FB0, 0x546FB4, 0x546FB8, 0x546FBC, 0xA84A40, 0xA84A44,
                               0xA84A48)):
            em.write(a, u32(0x40 + i))
    check(e, fk, 0x50A6C0, "D3DTex_Init", setup=setup, check_eax=False)


def size_case(seed):
    rng = random.Random(seed)
    w = rng.choice([1, 2, 3, 5, 64, 100, 256, 300, 1000, 4096, rng.randint(1, 5000), 0x80000001])
    h = rng.choice([1, 2, 3, 7, 64, 128, 200, 512, 2000, rng.randint(1, 5000)])
    lim = dict(minw=rng.choice([0, 1, 2, 8]), minh=rng.choice([0, 1, 4, 16]), maxw=rng.choice([256, 1024, 2048]),
               maxh=rng.choice([256, 1024, 2048]), aspect=rng.choice([0, 1, 2, 8, 0x100, 0x400]))
    return w, h, lim


@pytest.mark.parametrize("seed", range(60))
def test_get_texture_size(seed):
    w, h, lim = size_case(seed)

    def setup(em):
        set_desc(em, **lim)
        s = em.alloc(16)
        em.write(s, u32(w) + u32(h) + u32(0x11) + u32(0x22))
        return (s,)
    check(e, fk, 0x50A830, "D3DTex_GetTextureSize", setup=setup, check_eax=False)


def fmt_entry(rbits, gbits, bbits, abits):
    shift = 0
    masks = []
    for bits in (bbits, gbits, rbits, abits):
        masks.append(((1 << bits) - 1) << shift if bits else 0)
        shift += bits
    b, g, r, a = masks
    pf = u32(0x20) + u32(0x40 | (0x1 if a else 0)) + u32(0) + u32(shift) + u32(r) + u32(g) + u32(b) + u32(a)
    return pf


def set_formats(em, fmts):
    """fmts: (pf bytes, aBits, rgbBits); the other derived fields get filler values."""
    em.write(FORMAT_COUNT, u32(len(fmts)))
    for i, (pf, ab, rgb) in enumerate(fmts):
        em.write(FORMATS + 0x44 * i, pf + u32(i) * 4 + u32(ab) + u32(i) * 3 + u32(rgb))


FORMAT_SETS = [
    [(fmt_entry(5, 6, 5, 0), 0, 16)],
    [(fmt_entry(5, 6, 5, 0), 0, 16), (fmt_entry(5, 5, 5, 1), 1, 15), (fmt_entry(4, 4, 4, 4), 4, 12)],
    [(fmt_entry(4, 4, 4, 4), 4, 12), (fmt_entry(5, 5, 5, 1), 1, 15), (fmt_entry(5, 6, 5, 0), 0, 16),
     (fmt_entry(5, 5, 5, 0), 0, 15)],
]


@pytest.mark.parametrize("fset", range(len(FORMAT_SETS)))
@pytest.mark.parametrize("alpha", [0.0, 0.1, 0.2, 0.2000001, 0.9, -1.0, float("nan")])
@pytest.mark.parametrize("flags", [0, 0x20000000])
def test_find_format(fset, alpha, flags):
    if fset == 0 and flags == 0 and alpha > 0:
        pytest.skip("no alpha format: both copy from NULL")
    def setup(em):
        set_formats(em, FORMAT_SETS[fset])
        out = em.alloc(0x20)
        return out, 3, flags, struct.unpack("<I", f32(alpha))[0]
    check(e, fk, 0x50AA70, "D3DTex_FindFormat", setup=setup, check_eax=False)


def set_pool(em, handles, cap=8):
    """A Pool of 4 byte slots with a back index at +4 (stride 8)."""
    data = em.alloc(8 * cap + 4 * cap)
    free = data + 8 * cap
    order = list(range(cap))
    random.Random(len(handles)).shuffle(order)
    em.write(free, b"".join(u32(i) for i in order))
    for n, hnd in enumerate(handles):
        em.write(data + 8 * order[n], u32(hnd) + u32(n))
    em.write(POOL, u32(data) + u32(free) + u32(len(handles)) + u32(cap) + u32(8) + u32(4))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("w,h,outw,outh", [(64, 64, 64, 64), (100, 30, 128, 32), (3, 300, 4, 256)])
@pytest.mark.parametrize("mipmap,caps", [(0, 0x100), (0, 0), (1, 0x100)])
@pytest.mark.parametrize("fmt", [1, 2])
def test_alloc_texture(w, h, outw, outh, mipmap, caps, fmt, cw):
    def setup(em):
        ST["outw"], ST["outh"] = outw, outh
        set_desc(em, caps=caps)
        set_formats(em, FORMAT_SETS[1])
        set_pool(em, [0x1111, 0x2222])
        t = em.alloc(0x24)
        em.write(t, b"\x55" * 0x24)
        d = em.alloc(0x30)
        em.write(d, u32(t) + u32(0x3000) + u32(0) + u32(w * 2) + u32(0) + u32(0) + u32(w) + u32(h) + u32(fmt)
                 + u32(0) + u32(mipmap) + u32(0x6000))
        return (d,)
    check(e, fk, 0x50A940, "D3DTex_AllocTexture", setup=setup, check_eax=False, cw=cw)


def surface_chain(em, sizes, attach_err=DDERR_NOTFOUND):
    """Surfaces linked by GetAttachedSurface; sizes are (w, h, bits)."""
    objs = [surface.new(em) for _ in sizes]
    info = {o: s for o, s in zip(objs, sizes)}
    nxt = {o: n for o, n in zip(objs, objs[1:])}

    def get_desc(em, a):
        w, h, bits = info[a[0]]
        em.write(a[1] + 8, u32(h) + u32(w))
        em.write(a[1] + 0x54, u32(bits))

    def attached(em, a):
        if a[0] in nxt:
            em.write(a[2], u32(nxt[a[0]]))
            return 0
        return attach_err
    surface.fns[22] = get_desc
    surface.rets[22] = 0
    surface.rets[12] = attached
    return objs[0]


@pytest.mark.parametrize("sizes", [[(64, 64, 16)], [(64, 32, 16), (32, 16, 16), (16, 8, 16)], [(3, 5, 32)]])
def test_surface_bytes(sizes):
    def setup(em):
        return (surface_chain(em, sizes),)
    check(e, fk, 0x50AC90, "D3DTex_SurfaceBytes", setup=setup)


def test_surface_bytes_null():
    check(e, fk, 0x50AC90, "D3DTex_SurfaceBytes", args=(0,))


@pytest.mark.parametrize("which", ["desc", "attach"])
def test_surface_bytes_errors(which):
    def setup(em):
        s = surface_chain(em, [(8, 8, 16), (4, 4, 16)], attach_err=0x80004005)
        if which == "desc":
            surface.rets[22] = 0x8876017C
        return (s,)
    msg = check_error(e, fk, 0x50AC90, "D3DTex_SurfaceBytes", setup=setup)
    assert "Couldn't" in msg


@pytest.mark.parametrize("in_pool", [False, True])
@pytest.mark.parametrize("with_surface", [False, True])
def test_dealloc_texture(in_pool, with_surface):
    def setup(em):
        em.write(DEVICE, u32(device.new(em)))
        t = em.alloc(0x24)
        em.write(t, b"\x44" * 0x20 + u32(surface_chain(em, [(16, 16, 16), (8, 8, 16)]) if with_surface else 0))
        set_pool(em, [0x10, t, 0x30] if in_pool else [0x10, 0x20])
        em.write(TEX_COUNT, u32(5) + u32(100000))
        em.write(RELEASE_TIME, struct.pack("<Q", 0xFFFFFFF0))
        ST["qpc"] = [0x1FFFFFFF0, 0x200000005]
        return (t,)
    check(e, fk, 0x50AB50, "D3DTex_DeAllocTexture", setup=setup, check_eax=False)


@pytest.mark.parametrize("used,remove", [(1, 0), (3, 0), (3, 1), (3, 2), (5, 3)])
def test_dealloc_texture_pool(used, remove):
    def setup(em):
        em.write(DEVICE, u32(device.new(em)))
        ts = [em.alloc(0x24) for _ in range(used)]
        for t in ts:
            em.write(t, b"\0" * 0x24)
        set_pool(em, ts)
        return (ts[remove],)
    check(e, fk, 0x50AB50, "D3DTex_DeAllocTexture", setup=setup, check_eax=False)


@pytest.mark.parametrize("w,h", [(1, 1), (2, 8), (256, 64), (0, 5), (-4, 8), (7, 3), (1024, 4096)])
def test_mip_levels(w, h):
    check(e, fk, 0x50B4C0, "D3DTex_MipLevels", args=(w, h))


@pytest.mark.parametrize("stages,textures", [(8, 2), (1, 4), (3, 3), (0x8000, 1)])
def test_get_tmus(stages, textures):
    def setup(em):
        set_desc(em, stages=stages, textures=textures)
        out = em.alloc(4)
        em.write(out, b"\xff" * 4)
        return (out,)
    check(e, fk, 0x50BB80, "D3DTex_GetTMUs", setup=setup, check_eax=False)


def test_tmu_lod():
    def setup(em):
        em.write(ACTIVE_TMU, u32(3) + u32(4))
        p = em.alloc(4)
        em.write(p, u32(9))
        return (p,)
    for orig, ours in ((0x50BBB0, "D3DTex_SetActiveTMU"), (0x50BBC0, "D3DTex_GetActiveTMU"),
                       (0x50BBD0, "D3DTex_SetLOD")):
        check(e, fk, orig, ours, setup=setup, check_eax=False)


def test_enum_formats():
    def setup(em):
        em.write(FORMAT_COUNT, u32(9))
        em.write(DEVICE, u32(device.new(em)))
    check(e, fk, 0x50BBE0, "D3DTex_EnumFormats", setup=setup, check_eax=False)


def pf_case(seed):
    rng = random.Random(seed)

    def mask():
        if rng.random() < 0.15:
            return 0
        lo = rng.randint(0, 20)
        return ((1 << rng.randint(1, 8)) - 1) << lo & 0xFFFFFFFF
    flags = rng.choice([0x40, 0x41, 0x40, 0x41, 0x20, 0x60, 0x0])
    bits = rng.choice([8, 15, 16, 16, 24, 32])
    return u32(0x20) + u32(flags) + u32(0) + u32(bits) + b"".join(u32(mask()) for _ in range(4))


@pytest.mark.parametrize("seed", range(40))
def test_add_format(seed):
    def setup(em):
        em.write(FORMAT_COUNT, u32(seed % 5))
        pf = em.alloc(0x20)
        em.write(pf, pf_case(seed))
        return pf, 0
    check(e, fk, 0x50BC00, "D3DTex_AddFormat", setup=setup)


def test_shutdown():
    check(e, fk, 0x50BD70, "D3DTex_Shutdown", check_eax=False)


fk.add(0x50BD80, 0)
fk.add(0x50EC00, 0)
fk.add(0x50DC30, 0)
fk.add(0x4C1780, 0)
fk.add(0x4B0E70, 1, ret=0x55, fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4B0EB0, 1)


def test_driver():
    def setup(em):
        em.write(0x54E890, u32(0x31))
    for orig, ours in ((0x509910, "D3DTex_Register"), (0x509920, "D3DTex_Unregister"),
                       (0x509930, "QVertex_InitDriver"), (0x509960, "QVertex_ShutdownDriver")):
        check(e, fk, orig, ours, setup=setup, check_eax=False)
