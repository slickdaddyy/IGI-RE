"""DownloadTexture and the surface upload behind it, with fake DirectDraw surfaces."""
import random
import struct

import pytest

from test_d3d_util import D3DEmu
from test_sysmisc_util import Com, Fakes, check, check_error, u32

DDRAW, DEVICE = 0x5CA10C, 0x5CA114
DESC = 0xBCAC00
FORMATS, FORMAT_COUNT = 0xA7C140, 0xA7CEBC
ACTIVE_TMU, LOD, TEX_COUNT = 0xA7CEAC, 0xA7CEB0, 0xA7CEB4
DDERR_NOTFOUND = 0x887600FF

e = D3DEmu()
fk = Fakes(e)
ST = {}


def pow2(v):
    p = 1
    while p < v:
        p *= 2
    return p


def _size_call(em, a):
    s = a[2]
    w, h = em.u32(s), em.u32(s + 4)
    em.write(s + 8, u32(ST.get("outw", pow2(w))) + u32(ST.get("outh", pow2(h))))


fk.add(0x4B0C60, 2, ret=lambda em, a: em.alloc(a[0]))
fk.add(0x4B0D10, 1)
fk.add(0x4B1000, 3, fn=_size_call, fmt=lambda em, a: (a[0], a[1], em.read(a[2], 8)))
fk.add(0x4950E0, 0, ret=7)
fk.add(0x4B2700, 0, ret=0x21)
fk.add(0x4B2640, 0, ret=1)


def fail(slot):
    return lambda em, a: ST.get("fail", {}).get(slot, 0)


ddraw = Com(fk, {6: 4}, derefs={6: {1: 0x7C}})
device = Com(fk, {35: 3})
surf = Com(fk, {2: 1, 5: 6, 12: 3, 22: 2, 25: 5, 32: 2}, derefs={12: {1: 16}, 22: {1: 4}, 25: {2: 0x7C}})
for _s in (5, 12, 22, 25, 32):
    surf.rets[_s] = fail(_s)
ddraw.rets[6] = fail(6)


def new_surface(em, w, h, pf, mips=1, sys=False):
    """A chain of mips surfaces with halving sizes; pf is the 0x20 byte pixel format."""
    objs = []
    for i in range(max(mips, 1)):
        o = surf.new(em)
        ST["info"][o] = dict(w=max(w >> i, 1), h=max(h >> i, 1), pf=pf, mips=mips, sys=sys)
        objs.append(o)
    for a, b in zip(objs, objs[1:]):
        ST["info"][a]["next"] = b
    return objs[0]


def _create(em, a):
    sd = a[1]
    flags = em.u32(sd + 4)
    h, w = em.u32(sd + 8), em.u32(sd + 12)
    mips = em.u32(sd + 0x18) if flags & 0x20000 else 1
    pf = em.read(sd + 0x48, 0x20)
    sys = bool(em.u32(sd + 0x68) & 0x800)
    em.write(a[2], u32(new_surface(em, w, h, pf, mips, sys)))


def _get_desc(em, a):
    i = ST["info"][a[0]]
    sd = a[1]
    em.write(sd + 8, u32(i["h"]) + u32(i["w"]))
    em.write(sd + 0x18, u32(i["mips"]))
    em.write(sd + 0x48, i["pf"])


def _attached(em, a):
    i = ST["info"][a[0]]
    if "next" in i and not ST.get("fail", {}).get(12):
        em.write(a[2], u32(i["next"]))
        return 0
    return ST.get("fail", {}).get(12, DDERR_NOTFOUND)


def _lock(em, a):
    i = ST["info"][a[0]]
    bits = struct.unpack("<I", i["pf"][12:16])[0]
    pitch = i["w"] * max(bits // 8, 1) + ST.get("pad", 0)
    p = em.alloc(pitch * i["h"] + 64)
    em.write(p, b"\xEE" * (pitch * i["h"] + 64))
    em.write(a[2] + 0x10, u32(pitch))
    em.write(a[2] + 0x24, u32(p + 32))


ddraw.fns[6] = _create
surf.fns[22] = _get_desc
surf.fns[25] = _lock
surf.rets[12] = _attached


def pf(bits, r, g, b, a=0, flags=None):
    if flags is None:
        flags = 0x40 | (1 if a else 0)
    return u32(0x20) + u32(flags) + u32(0) + u32(bits) + u32(r) + u32(g) + u32(b) + u32(a)


PF = {
    "8888": pf(32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000),
    "x888": pf(32, 0xFF0000, 0xFF00, 0xFF),
    "bgr8": pf(32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000),
    "565": pf(16, 0xF800, 0x7E0, 0x1F),
    "1555": pf(16, 0x7C00, 0x3E0, 0x1F, 0x8000),
    "4444": pf(16, 0xF00, 0xF0, 0xF, 0xF000),
    "r0": pf(16, 0, 0x7E0, 0x1F, 0xF000),
    "g0": pf(16, 0xF800, 0, 0x1F),
    "b0": pf(16, 0xF800, 0x7E0, 0),
    "24": pf(24, 0xFF0000, 0xFF00, 0xFF),
    "pal": pf(8, 0, 0, 0, 0, flags=0x20),
}


def reset_world(em):
    ST.clear()
    ST["info"] = {}
    em.write(DDRAW, u32(ddraw.new(em)))
    em.write(DEVICE, u32(device.new(em)))


def pixels(em, n, seed):
    rng = random.Random(seed)
    p = em.alloc(4 * n + 8)
    em.write(p, b"".join(u32(rng.getrandbits(32)) for _ in range(n)))
    return p


UPLOAD_CASES = [
    ("8888", 4, 4, 4, 4, 0), ("8888", 4, 4, 4, 4, 8), ("8888", 8, 4, 5, 3, 0), ("x888", 4, 2, 4, 2, 0),
    ("bgr8", 3, 3, 2, 1, 0), ("565", 4, 4, 4, 4, 0), ("565", 5, 4, 3, 2, 4), ("1555", 2, 2, 2, 2, 0),
    ("4444", 7, 3, 7, 3, 2), ("4444", 4, 4, 1, 1, 0), ("r0", 2, 2, 2, 2, 0), ("g0", 2, 2, 1, 2, 0),
    ("b0", 3, 2, 3, 2, 0), ("8888", 4, 4, 0, 2, 0), ("565", 4, 3, 2, 0, 0), ("x888", 2, 6, 2, 2, 0),
]


@pytest.mark.parametrize("case", range(len(UPLOAD_CASES)))
def test_upload(case):
    fmt, w, h, sw, sh, pad = UPLOAD_CASES[case]

    def setup(em):
        reset_world(em)
        ST["pad"] = pad
        dst = new_surface(em, w, h, PF[fmt])
        src = pixels(em, max(sw * sh, 1) + 4, case) + 16
        return dst, w, h, src, sw, sh
    check(e, fk, 0x50B4F0, "D3DTex_Upload", setup=setup, check_eax=False)


@pytest.mark.parametrize("what", ["desc", "create", "lock", "unlock", "blt", "24", "pal"])
def test_upload_errors(what):
    def setup(em):
        reset_world(em)
        dst = new_surface(em, 4, 4, PF[what] if what in PF else PF["565"])
        slot = {"desc": 22, "create": 6, "lock": 25, "unlock": 32, "blt": 5}.get(what)
        if slot:
            ST["fail"] = {slot: 0x88760096}
        return dst, 4, 4, pixels(em, 16, 1), 4, 4
    msg = check_error(e, fk, 0x50B4F0, "D3DTex_Upload", setup=setup)
    assert "format" in msg or "Couldn't" in msg


def set_desc(em, caps=0, devcaps=0):
    em.write(DESC, u32(devcaps))
    em.write(DESC + 0x5C, u32(caps))


def set_formats(em, entries):
    em.write(FORMAT_COUNT, u32(len(entries)))
    for i, (p, ab, rgb) in enumerate(entries):
        em.write(FORMATS + 0x44 * i, p + u32(i) * 4 + u32(ab) + u32(i) * 3 + u32(rgb))


SRC_BPP = {1: 1, 2: 2, 3: 4, 4: 4, 5: 2, 0x43: 4}


def texture(em, fmt, w, h, mipmap, seed, old_surface=False):
    rng = random.Random(seed)
    bpp = SRC_BPP.get(fmt, 4)
    size = 0
    lw, lh = w, h
    while True:
        size += lw * lh * bpp
        if not (fmt & 0x40) or lw <= 1 or lh <= 1:
            break
        lw, lh = lw // 2, lh // 2
    data = em.alloc(size + 16)
    em.write(data, bytes(rng.getrandbits(8) for _ in range(size)))
    pal = em.alloc(12)
    colors = em.alloc(0x400)
    em.write(colors, bytes(rng.getrandbits(8) for _ in range(0x400)))
    em.write(pal, u32(7) + u32(0) + u32(colors))
    t = em.alloc(0x24)
    old = new_surface(em, 8, 8, PF["565"]) if old_surface else 0
    em.write(t, u32(7) + b"\x55" * 0x1C + u32(old))
    d = em.alloc(0x30)
    em.write(d, u32(t) + u32(pal) + u32(0) + u32(w * bpp) + u32(0) + u32(0) + u32(w) + u32(h) + u32(fmt)
             + u32(rng.choice([0, 0x20000000])) + u32(mipmap) + u32(data))
    return d


FORMAT_SET = [(PF["565"], 0, 16), (PF["1555"], 1, 15), (PF["4444"], 4, 12)]

DL_CASES = [
    # fmt, w, h, mipmap, caps, devcaps, tmu, lod, old surface
    (3, 4, 4, 0, 0x100, 0, -1, 0, False),
    (3, 5, 3, 0, 0x100, 0x80000, 1, 0, True),
    (2, 6, 5, 0, 0, 0, -1, 0, False),
    (5, 4, 2, 0, 0, 0, 2, 0, False),
    (1, 8, 4, 0, 0, 0, -1, 0, False),
    (4, 3, 7, 0, 0, 0x80000, -1, 0, True),
    (3, 8, 8, 1, 0x100, 0, -1, 0, False),
    (3, 8, 4, 1, 0, 0, 1, 1, False),
    (3, 16, 16, 1, 0, 0, -1, 2, True),
    (3, 4, 16, 1, 0, 0, -1, 9, False),
    (0x43, 8, 8, 1, 0, 0, -1, 0, False),
    (0x43, 16, 4, 1, 0x100, 0, -1, 1, False),
    (0x43, 8, 8, 1, 0, 0, -1, 7, True),
    (0x43, 4, 4, 0, 0x100, 0, -1, 0, False),
    (0x43, 8, 2, 0, 0, 0, -1, 0, False),
]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", range(len(DL_CASES)))
def test_download(case, cw):
    fmt, w, h, mipmap, caps, devcaps, tmu, lod, old = DL_CASES[case]

    def setup(em):
        reset_world(em)
        set_desc(em, caps, devcaps)
        set_formats(em, FORMAT_SET)
        em.write(ACTIVE_TMU, u32(tmu) + u32(lod) + u32(4) + u32(5000))
        return (texture(em, fmt, w, h, mipmap, case, old),)
    check(e, fk, 0x50AD90, "D3DTex_DownloadTexture", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("what", ["pow2", "format6", "format41", "format0", "create", "desc", "attach",
                                  "attach43"])
def test_download_errors(what):
    def setup(em):
        reset_world(em)
        set_desc(em)
        set_formats(em, FORMAT_SET)
        em.write(ACTIVE_TMU, u32(-1) + u32(0) + u32(4) + u32(5000))
        fmt = {"format6": 6, "format41": 0x41, "format0": 0, "attach43": 0x43}.get(what, 3)
        w = 6 if what == "pow2" else 8
        mip = 1 if what in ("pow2", "attach", "attach43") else 0
        slot = {"create": 6, "desc": 22, "attach": 12, "attach43": 12}.get(what)
        if slot:
            ST["fail"] = {slot: 0x88760096}
        return (texture(em, fmt, w, 8, mip, 3),)
    msg = check_error(e, fk, 0x50AD90, "D3DTex_DownloadTexture", setup=setup)
    assert "texture" in msg or "Couldn't" in msg
