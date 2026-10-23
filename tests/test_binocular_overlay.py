import random
import struct

import pytest

from emu import Emu
from test_binocular_util import (Fakes, add_child, check, common, cstr, event, f32, f64, masked, new_task, rmatrix,
                                 rvec, set_events, set_handler, u32, u8)

OT, HT, PT, XT = 0x70, 0x71, 0x72, 0x73  # overlay, human, holder, other
V = {}

e = Emu()
fk = Fakes(e)
common(e, fk, {b"BinocularOverlay": OT, b"BinocularOverlayHolder": PT})
set_events(e)
DISPLAY = fk.code(struct.pack("<3I", 0, 800, 600))
GAME = fk.code(struct.pack("<I", 0) * 16 + struct.pack("<I", 0x4444))


def text_new(em, a):
    t = em.alloc(0x28)
    u32(em, t + 0x20, 0x5A5A)
    return t


def coll_lines(em, a):
    res = bytearray(0x70)
    if V.get("hit") is not None:
        res[0x58:0x5C] = struct.pack("<I", 1)
        res[0x60:0x68] = f64(V["hit"])
    em.write(a[1], bytes(res))


def eye(em, a):
    em.write(a[0], f64(*V.get("eye", (1.0, 2.0, 3.0))))


def pop(key, default=0):
    def run(em, a):
        vals = V.get(key)
        return vals.pop(0) if vals else default
    return run


def los(em, a):
    vals = V.get("vis")
    em.write(a[7], f32(vals.pop(0) if vals else 0.0))
    return 1


def timer(em, a):
    V["now"] = V.get("now", 5000) + V.get("tick", 7)
    return V["now"]


def strip_log(em, a):
    s = em.read(a[0], 20)
    n = struct.unpack("<I", s[12:16])[0]
    return (s[:16], em.read(struct.unpack("<I", s[16:20])[0], 0x24 * n))


def sprintf(em, a):
    """crt_sprintf for "%s: %0.f%s" and "%s: %0.f+%s"."""
    fmt = em.read_cstr(a[1])
    num = struct.unpack("<d", struct.pack("<II", a[3], a[4]))[0]
    out = em.read_cstr(a[2]) + b": " + (b"%.0f" % num) + (b"+" if b"+" in fmt else b"") + em.read_cstr(a[5])
    em.write(a[0], out + b"\0")
    return len(out)


fk.add(0x4A53B3, 6, ret=sprintf, fmt=lambda em, a: (em.read_cstr(a[1]), em.read_cstr(a[2]), a[3], a[4],
                                                    em.read_cstr(a[5])))
fk.add(0x4B6EC0, 4, ret=text_new, fmt=lambda em, a: (a[0], a[1], a[2], em.read_cstr(a[3])))
fk.add(0x4B6E90, 2, fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4B6F30, 1)
fk.add(0x4B6F50, 1, deref={0: 12})
fk.add(0x4B7DF0, 1, ret=lambda em, a: V.get("tw", 37))
fk.add(0x4B7E00, 1, ret=lambda em, a: V.get("th", 13))
fk.add(0x4F1320, 1)
fk.add(0x4F1340, 1)
fk.add(0x4F16C0, 1, ret=lambda em, a: V.get("hasexpr", 0) | 0x4400)
fk.add(0x4F16E0, 2, ret=lambda em, a: V.get("eval", 0.0), st0="d")
fk.add(0x472600, 1, ret=0x2600)
fk.add(0x485320, 1, ret=0x5320)
fk.add(0x416D40, 0, ret=GAME)
fk.add(0x4015F0, 1)
fk.add(0x401AE0, 1, ret=0xAE00)
fk.add(0x401B20, 1)
fk.add(0x497D70, 4)
fk.add(0x497E70, 1)
fk.add(0x497ED0, 0)
fk.add(0x491CF0, 0, ret=DISPLAY)
fk.add(0x48F2D0, 0, ret=lambda em, a: V.get("usa", 0))
fk.add(0x416920, 0, ret=0x9000)
fk.add(0x4CC2A0, 8, ret=coll_lines,
       fmt=lambda em, a: (a[0], em.read(a[2], 24), em.read(a[3], 24), a[4], a[5] & 0xFF, em.read(a[6] + 4, 16), a[7]))
fk.add(0x4B64E0, 1, ret=lambda em, a: V.get("res", 0), fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4B5F00, 2, ret=lambda em, a: V["label"], fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))
fk.add(0x4B6CE0, 1, ret=lambda em, a: V.get("sw", 64))
fk.add(0x4B51F0, 15)
fk.add(0x4B5860, 14)
fk.add(0x4B5730, 15)
fk.add(0x4B4A00, 10)
fk.add(0x401CF0, 2, ret=lambda em, a: (a[0] & 0xFFFF) == HT, fmt=masked(0xFFFF, 0xFFFF))
fk.add(0x460C60, 0, ret=lambda em, a: V["list"])
fk.add(0x465800, 2, ret=eye, fmt=lambda em, a: (a[1],))
fk.add(0x4D03D0, 3, ret=pop("cull"), fmt=lambda em, a: (em.read(a[0], 12), a[1], a[2]))
fk.add(0x4CE410, 8, ret=los, fmt=lambda em, a: (a[0], em.read(a[1], 24), em.read(a[2], 24), a[3] & 0xFF,
                                                em.read(a[4] + 4, 12), a[5], a[6]))
fk.add(0x490370, 0, ret=timer)
STRIP = fk.add(fk.code(b"\xc3"), 1, fmt=strip_log)
u32(e, 0xA84A54, 9)
u32(e, 0xA94E84 + 9 * 4, STRIP)
e.baseline = e.snapshot()

CW = pytest.mark.parametrize("cw", [0x37F, 0x7F])


def view(rng):
    e.write(0xBCAAE0, rmatrix(rng) + b"\0" * 4 + rvec(rng, 2000)
            + f32(0, 0, rng.uniform(300, 900), rng.uniform(300, 900), rng.uniform(0, 640), rng.uniform(0, 480)))


def human(rng, pos=None):
    h = new_task(e, HT, 0xD00)
    e.write(h + 0x20, f64(*pos) if pos else rvec(rng, 3000))
    e.write(h + 0xA0, f32(rng.uniform(100, 600)))
    u32(e, h + 0xFC, rng.choice([1, 1, 2]))
    u32(e, h + 0x4EC, 0x4EC0)
    u8(e, h + 0xCF5, rng.random() < 0.15)
    u8(e, h + 0xCF6, rng.random() < 0.9)
    return h


def overlay(rng, parent=None):
    ov = new_task(e, OT, 0x2E00)
    if parent:
        u32(e, ov + 0x14, parent)
    for i in range(6):
        u32(e, ov + 0x20 + 4 * i, text_new(e, None))
    e.write(ov + 0x38, rmatrix(rng) + rvec(rng) + f32(rng.uniform(0.2, 1.0)))
    d = [rng.uniform(-1, 1) for _ in range(3)]
    e.write(ov + 0x7C, f32(*d))
    e.write(ov + 0x138, struct.pack("<3I", rng.getrandbits(32), rng.getrandbits(32), 0))
    for i in range(12):
        e.write(ov + 0x2B54 + 0x14 * i, f32(rng.uniform(0, 1.2), 0, rng.uniform(0, 0.1))
                + struct.pack("<ii", rng.choice([-1, 0, 0, 3]), 30))
    return ov


def target(rng, ov, i, h=None):
    t = ov + 0x2C44 + 0x2C * i
    u32(e, t, h or 0, rng.choice([0, 1]), rng.choice([0, 1]), rng.choice([0, 0, 1]), rng.choice([0, 1]))
    e.write(t + 0x14, f32(rng.uniform(100, 500), rng.uniform(1, 9000)))
    u32(e, t + 0x1C, rng.randrange(0, 6000), rng.randrange(4000, 6000))
    e.write(t + 0x24, f32(rng.uniform(0, 640), rng.uniform(0, 480)))


def test_open():
    check(e, fk, 0x470FA0, "BinocularOverlay_Open")


def test_create():
    check(e, fk, 0x471120, "BinocularOverlay_Create", lambda em: (new_task(em, OT, 0x2E00),))


def test_grid_and_meter_init():
    check(e, fk, 0x471280, "sub_471280", lambda em: (em.alloc(0x2A10), 0x18))
    check(e, fk, 0x4712E0, "VUMeter_Init", lambda em: (em.alloc(0x14), 30, 0x3CA3D70A))


def test_holder_create_delete():
    def setup(em):
        h = new_task(em, PT, 0x78)
        u8(em, h + 0x74, 1)
        return (h,)
    check(e, fk, 0x471310, "BinocularHolder_Create", setup)
    for ref in (0, 1, 2):
        def s(em, ref=ref):
            h = new_task(em, PT, 0x78)
            if ref:
                r = em.alloc(4)
                u32(em, r, 0x3000 if ref == 2 else 0)
                u32(em, h + 0x20, r)
            return (h,)
        check(e, fk, 0x471400, "BinocularHolder_Delete", s)


@pytest.mark.parametrize("expr", [0, 1])
@pytest.mark.parametrize("active", [0, 1])
@pytest.mark.parametrize("ref", [0, 1, 2])
@pytest.mark.parametrize("val", [0.0, 1.0, 2.6])
def test_holder_run(expr, active, ref, val):
    def setup(em):
        V.clear()
        V["hasexpr"] = expr
        V["eval"] = val
        h = new_task(em, PT, 0x78)
        u8(em, h + 0x74, active)
        if ref:
            r = em.alloc(4)
            u32(em, r, 0x3000 if ref == 2 else 0)
            u32(em, h + 0x20, r)
        return (h,)
    check(e, fk, 0x471330, "BinocularHolder_Run", setup)


def test_delete():
    check(e, fk, 0x4713C0, "BinocularOverlay_Delete", lambda em: (overlay(random.Random(1)),))


def draw_setup(seed, track=False):
    def setup(em):
        rng = random.Random(seed)
        V.clear()
        V["usa"] = seed & 1
        V["res"] = (seed >> 1) & 1
        V["label"] = cstr(em, b"Entfernung")
        V["hit"] = rng.choice([None, rng.uniform(0, 9e6)])
        V["sw"] = rng.choice([64, 256, 1])
        V["th"] = rng.choice([13, 20])
        u32(em, 0x5BE344, em.alloc(0x40))
        view(rng)
        parent = new_task(em, HT if track else XT, 0x40)
        u32(em, parent + 0x14, new_task(em, XT, 0x40))
        ov = overlay(rng, parent)
        V["list"] = DISPLAY
        if track:
            hs = [0, parent] + [human(rng) for _ in range(5)]
            rng.shuffle(hs)
            lst = em.alloc(8 + 4 * len(hs))
            u32(em, lst, len(hs), 0, *hs)
            V["list"] = lst
            V["cull"] = [rng.choice([0, 0, 1]) for _ in hs]
            V["vis"] = [rng.choice([0.0, 1.0, 0.5]) for _ in hs]
            V["now"] = rng.randrange(4000, 9000)
            V["tick"] = rng.choice([0, 7, 300])
            for i in range(10):
                if rng.random() < 0.6:
                    target(rng, ov, i, rng.choice(hs) or new_task(em, HT, 0x40))
        return (ov,)
    return setup


@CW
@pytest.mark.parametrize("seed", range(8))
def test_status_display(seed, cw):
    check(e, fk, 0x471440, "BinocularOverlay_StatusDisplayHandler", draw_setup(seed), cw=cw)


@CW
@pytest.mark.parametrize("seed", range(12))
def test_draw(seed, cw):
    check(e, fk, 0x471480, "BinocularOverlay_Draw", draw_setup(seed, track=seed > 5), cw=cw)


@CW
@pytest.mark.parametrize("seed", range(30))
def test_vumeter_update(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        m = em.alloc(0x14)
        em.write(m, f32(rng.choice([0.0, -0.5, rng.uniform(0, 1.5)]), 0, rng.uniform(0, 0.6))
                 + struct.pack("<ii", rng.choice([-2, 0, 0, 5]), rng.choice([30, 7, 1])))
        r = em.alloc(12)
        em.write(r, struct.pack("<3I", rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32)))
        return m, r
    check(e, fk, 0x4719D0, "VUMeter_Update", setup, cw=cw)


@CW
@pytest.mark.parametrize("vertical", [0, 1])
@pytest.mark.parametrize("flip", [0, 1])
def test_vumeter_draw(vertical, flip, cw):
    def setup(em):
        m = em.alloc(0x14)
        em.write(m, f32(0.37))
        a = [struct.unpack("<I", f32(v))[0] for v in (101.5, 433.25, 61.3, 74.9)]
        return (m, *a, vertical, flip)
    check(e, fk, 0x471A90, "VUMeter_Draw", setup, cw=cw)


@CW
@pytest.mark.parametrize("seed", range(40))
def test_track_movement(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        V.clear()
        view(rng)
        player = new_task(em, rng.choice([HT, XT]), 0xD00)
        mid = new_task(em, rng.choice([XT, HT]), 0x40)
        u32(em, mid + 0x14, player)
        ov = overlay(rng, mid)
        hs = [0, player] + [human(rng) for _ in range(rng.randrange(14))]
        rng.shuffle(hs)
        lst = em.alloc(8 + 4 * len(hs))
        u32(em, lst, len(hs), 0, *hs)
        V["list"] = lst
        V["cull"] = [rng.choice([0, 0, 1]) for _ in hs]
        V["vis"] = [rng.choice([0.0, 1.0, 0.5]) for _ in hs]
        V["now"] = rng.randrange(4000, 9000)
        V["tick"] = rng.choice([0, 7, 300, 1000])
        for i in range(10):
            if rng.random() < 0.7:
                target(rng, ov, i, rng.choice(hs) or new_task(em, HT, 0x40))
        return (ov,)
    check(e, fk, 0x471B60, "BinocularOverlay_TrackMovement", setup, cw=cw)


@pytest.mark.parametrize("seed", range(30))
def test_add_target(seed):
    def setup(em):
        rng = random.Random(seed)
        V.clear()
        ov = overlay(rng)
        hs = [new_task(em, HT, 0x40) for _ in range(12)]
        for i in range(10):
            if rng.random() < 0.8:
                target(rng, ov, i, rng.choice(hs))
        scr = em.alloc(8)
        em.write(scr, f32(12.5, 99.0))
        depth = struct.unpack("<I", f32(rng.uniform(1, 9000)))[0]
        return ov, rng.choice(hs), depth, 0x43800000, scr, rng.choice([0, 1]), rng.choice([0, 1]), rng.choice([0, 1])
    check(e, fk, 0x471F60, "BinocularOverlay_AddTarget", setup, check_eax=True)


@CW
@pytest.mark.parametrize("seed", range(10))
def test_draw_compass(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        V.clear()
        V["th"] = rng.choice([13, 20, 7])
        V["tw"] = rng.choice([37, 8])
        return (overlay(rng),)
    check(e, fk, 0x472050, "BinocularOverlay_DrawCompass", setup, cw=cw)
