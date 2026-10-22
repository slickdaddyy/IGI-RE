import math
import struct

import pytest

from emu import Emu, ErrorShown
from test_humanbase_util import Fakes, check, rd, rf, set_handler, w8, w16, w32, wd, wf

E = Emu()
FK = Fakes(E)
S = {}  # per test values read by the fakes

CWS = pytest.mark.parametrize("cw", [0x37F, 0x7F])

# event slots and task types handed out by the faked getters
EV_DRAW, EV_BONE, EV_AI, EV_DET, EV_HUD, EV6, EV14, EV2, EV4, EV_W0, EV_W1 = range(0x20, 0x2B)
T_HUMAN, T_BASE, T_RIGID, T_WEAPON, T_OTHER = 0x150, 0x151, 0x152, 0x153, 0x154
WORLD = 0x5A5A0000


def sv(key, default=0):
    return lambda e, a: S.get(key, default)


def cstr(i):
    return lambda e, a: [e.read_cstr(v) if j == i else e.normalize(v) for j, v in enumerate(a)]


def floats(e, p, n):
    return list(struct.unpack("<%df" % n, e.read(p, 4 * n)))


def write_pos(key, n=3, fmt="d"):
    def act(e, a):
        if key in S:
            e.write(a[0], struct.pack("<%d%s" % (n, fmt), *S[key]))
    return act


def vec_len(e, a):
    x, y, z = rd(e, a[0], 3)
    return math.sqrt(x * x + y * y + z * z)


def asin(e, a):
    x = struct.unpack("<f", struct.pack("<I", a[0]))[0]
    return math.asin(max(-1.0, min(1.0, x)))


def lines_of_sight(e, a):
    if "vis_out" in S:
        wf(e, a[7], S["vis_out"])
    return S.get("los", 1)


def los_fmt(e, a):
    return [a[0], e.read(a[1], 24), e.read(a[2], 24), a[3] & 0xFF, e.read(a[4], 16), a[5], a[6]]


def anim_event(e, a):
    if "animev" in S:
        e.write(a[0], struct.pack("<2i2i3f", *S["animev"]))


def euler(e, a):
    x, y, z = S.get("euler", (0.1, 0.2, 0.3))
    wf(e, a[1], x)
    wf(e, a[2], y)
    wf(e, a[3], z)


def name_out(e, a):
    e.write(a[0], b"Soldier7\0")


def sprintf(e, a):
    fmt = e.read_cstr(a[1]).decode()
    s = fmt % e.read_cstr(a[2]).decode()
    e.write(a[0], s.encode() + b"\0")
    return len(s)


def coll(e, a):
    if "coll_to" in S:
        wd(e, a[2], *S["coll_to"])
    if "coll_v" in S:
        wf(e, a[3], *S["coll_v"])


def coll_fmt(n):
    def f(e, a):
        r = [a[0], e.read(a[1], 24), e.read(a[2], 24), e.read(a[3], 12)]
        if n > 4:
            r.append(e.u32(a[4]))
        if n > 5:
            r.append(a[5])
        return r
    return f


def count_pts(e, a):
    pts = S.get("pts", [])
    for i, p in enumerate(pts):
        wd(e, a[0] + 24 * i, *p)
    return len(pts)


def noise_fmt(e, a):
    p = a[0]
    t, team, u8, rad, pos, u14, src = struct.unpack("<3IfIII", e.read(p, 0x1C))
    return [t, team, u8, rad, e.read(pos, 24), u14, src]


def vararg_fmt(e, a):
    f = e.read_cstr(a[0])
    if b"%d" in f:
        return [f, e.read(a[1], 4)]
    return [f, e.read(a[1], 8)] if b"%" in f else [f]


GETTERS = [(0x4D14D0, EV_DRAW), (0x4D9610, T_BASE), (0x477C20, EV_BONE), (0x4C48C0, T_RIGID), (0x450C80, EV_AI),
           (0x477BF0, T_WEAPON), (0x489610, EV_DET), (0x487190, EV_HUD), (0x416820, EV6), (0x416890, EV14),
           (0x4167F0, EV2), (0x416800, EV4), (0x4CEA10, EV_W0), (0x4CEA20, EV_W1), (0x416920, WORLD),
           (0x464250, 0x1234)]
for addr, v in GETTERS:
    FK.add(addr, 0, v)

FK.add(0x416D40, 0, sv("game"))
FK.add(0x4E81F0, 0, sv("cam"))
FK.add(0x4F2060, 2, sv("weapon"))
FK.add(0x4F2310, 3, deref={1: 24, 2: 40})
FK.add(0x4D03D0, 3, sv("cull"), fmt=lambda e, a: [floats(e, a[0], 3), a[1]])
FK.add(0x4CFF70, 1, sv("lod"))
FK.add(0x4D38C0, 2)
FK.add(0x4D64C0, 2, 7)
FK.add(0x489F70, 2)
FK.add(0x4D06C0, 2, sv("vis2"), fmt=lambda e, a: [a[0], e.read(a[1], 4)])
FK.add(0x4028C0, 0, sv("frame", 100))
FK.add(0x497F20, 3, fmt=lambda e, a: [e.read(a[2], 24)],
       act=lambda e, a: (wf(e, a[0], *S.get("scr", (300.0, 200.0))), wf(e, a[1], S.get("depth", 1000.0))))
FK.add(0x4B7E00, 1, sv("th", 3))
FK.add(0x4B6E90, 2, fmt=cstr(1))
FK.add(0x4B6F50, 1, fmt=lambda e, a: [a[0], e.read(a[0], 0x28)])
FK.add(0x4E7C40, 0, sv("font"))
FK.add(0x4B0DB0, 0, sv("memmode"))
FK.add(0x4B0D40)
FK.add(0x4B0D50)
FK.add(0x4B6EC0, 4, sv("newtext"))
FK.add(0x416D80, 0, sv("xp"))
FK.add(0x463150, 1, sv("player"))
FK.add(0x463AD0, 3, deref={1: 12})
FK.add(0x489C60, 2, sv("zone"))
FK.add(0x489E10, 2)
FK.add(0x416C90, 5, sv("falloff"), st0="d")
FK.add(0x4C3440, 1, sv("dpos"))
FK.add(0x4D0950, 1, sv("vradius", 1.0), st0="d")
FK.add(0x4B3130, 1, vec_len, deref={0: 24}, st0="d")
FK.add(0x465800, 2, act=write_pos("campos"), fmt=lambda e, a: [a[1]])
FK.add(0x465880, 2, act=write_pos("cammat", 9, "f"), fmt=lambda e, a: [a[1]])
FK.add(0x465820, 2)
FK.add(0x47CE00, 3)
FK.add(0x4E7200, 3, fmt=cstr(0))
FK.add(0x401CF0, 2, lambda e, a: int((a[0] & 0xFFFF) == (a[1] & 0xFFFF)),
       fmt=lambda e, a: [a[0] & 0xFFFF, a[1] & 0xFFFF])
FK.add(0x477C50, 5, fmt=lambda e, a: a[:4] + [a[4] & 0xFF])
FK.add(0x46AB30, 0, sv("mode46", 1))
FK.add(0x4CE410, 8, lines_of_sight, fmt=los_fmt)
FK.add(0x467F30, 1)
FK.add(0x491CF0, 0, sv("display"))
FK.add(0x463740, 1, sv("att"))
FK.add(0x4872B0, 1)
FK.add(0x4B6CE0, 1, sv("sw", 64))
FK.add(0x4B6D00, 1, sv("sh", 32))
FK.add(0x4B4E00, 9)
FK.add(0x4B5560, 25)
FK.add(0x47B290, 4, count_pts, fmt=lambda e, a: [a[1], a[2], e.read(a[3], 24)])
FK.add(0x4028B0, 0, sv("ticks"))
FK.add(0x4E6B00, 2, sv("snd"), fmt=cstr(1))
FK.add(0x4E6C30, 2, deref={1: 24})
FK.add(0x463130, 1, sv("ateam"))
FK.add(0x450CE0, 1, fmt=noise_fmt)
FK.add(0x4F1270, 2, fmt=lambda e, a: [a[1]], act=name_out)
FK.add(0x4A53B3, 3, sprintf, fmt=lambda e, a: [e.read_cstr(a[1]), e.read_cstr(a[2])])
FK.add(0x4B7EE0, 2, fmt=cstr(0))
FK.add(0x4B7FA0, 2, fmt=cstr(0))
FK.add(0x4B80A0, 1, fmt=cstr(0))
FK.add(0x504AC0, 1, sv("mask"))
FK.add(0x5042B0, 2, lambda e, a: int(rd(e, a[1]) > 0))
FK.add(0x4C1830, 1)
FK.add(0x4018C0, 1, fmt=lambda e, a: [a[0] & 0xFF])
FK.add(0x401A20, 1, fmt=lambda e, a: [a[0] & 0xFFFF])
FK.add(0x504A90, 1)
FK.add(0x4B8920, 1, fmt=cstr(0))
FK.add(0x401F80, 1)
FK.add(0x4017C0, 1, 0x77, fmt=lambda e, a: [a[0] & 0xFFFF])
FK.add(0x415020, 1, sv("cheat"))
FK.add(0x4E7C00, 2, fmt=vararg_fmt)
FK.add(0x463680, 1, sv("carrier"))
FK.add(0x4B3C50, 4, fmt=lambda e, a: [e.read(a[0], 40)], act=euler)
FK.add(0x463700, 2)
FK.add(0x4168A0, 0, sv("tasklist"))
FK.add(0x4B38E0, 4)
FK.add(0x4D3210, 2, act=anim_event)
FK.add(0x4EC0E0, 3, deref={2: 24})
FK.add(0x4B27D0, 1, asin, st0="d")
FK.add(0x462BA0, 4, fmt=coll_fmt(4), act=coll)
FK.add(0x4C6940, 4, deref={2: 24})
FK.add(0x4CEC10, 1, sv("vmodel"), fmt=cstr(0))
FK.add(0x4D9700, 2)
FK.add(0x4B3BE0, 2)


def human(e, type_=T_HUMAN):
    h = e.alloc(0xCF8)
    w16(e, h + 0x1C, type_)
    # empty child list
    w32(e, h + 0x8, h + 0xC)
    w32(e, h + 0x10, h + 0x8)
    return h


def view(e, h):
    v = e.alloc(0x200)
    w32(e, h + 0x4EC, v)
    return v


def game(e, e8=0, viewer=0):
    g = e.alloc(0x100)
    st = e.alloc(0x100)
    w32(e, g + 0x5C, st)
    w32(e, st + 0xE8, e8)
    pp = e.alloc(4)
    w32(e, pp, viewer)
    w32(e, st + 0x68, pp)
    S["game"] = g
    return st


def cam(e, st):
    c = e.alloc(0x60)
    w32(e, c + 0x54, st)
    S["cam"] = c
    return c


def go(orig, name, setup, cw=None, check_eax=False, **kw):
    return check(E, FK, orig, name, setup=setup, cw=cw, check_eax=check_eax, **kw)


def reset(**kw):
    S.clear()
    S.update(kw)


# --- drawing ---------------------------------------------------------------------

def draw_setup(viewer, mode, weapon, frozen=0, **kw):
    def setup(e):
        reset(**kw)
        h = human(e)
        st = game(e, mode, h if viewer else 0)
        c = cam(e, st)
        if viewer == 2:
            w32(e, c + 0x54, 0)
        w8(e, h + 0xD8, frozen)
        wd(e, h + 0x20, 1000.0, -2000.5, 300.25)
        w32(e, h + 0xA0, 0x44800000)
        set_handler(e, EV_DRAW, T_BASE, S["hdraw"])
        set_handler(e, 4, T_BASE, S["hdraw"])
        set_handler(e, EV_BONE, T_HUMAN, S["hbone"])
        set_handler(e, EV_DRAW, T_RIGID, S["hdraw"])
        set_handler(e, 4, T_RIGID, S["hdraw"])
        if weapon:
            S["weapon"] = e.alloc(0x40)
        return h, 0x1234
    return setup


def bone_act(e, a):
    p = a[1]
    w32(e, p + 4, 0x5555)
    wd(e, p + 8, 1.5, 2.5, 3.5)
    wf(e, p + 0x20, *[0.5 + i for i in range(10)])


HDRAW = FK.handler(2)
HBONE = FK.handler(2, deref={1: 4}, act=bone_act)


@pytest.mark.parametrize("viewer,mode,weapon,frozen,lod,cull", [
    (1, 1, 1, 0, 0, 0), (1, 0, 1, 0, 0, 0), (0, 1, 0, 0, 0, 0), (2, 1, 1, 1, 2, 0), (0, 0, 1, 1, 4, 0),
    (0, 0, 1, 1, -1, 0), (0, 0, 1, 1, 1, 1)])
@CWS
def test_draw2(viewer, mode, weapon, frozen, lod, cull, cw):
    go(0x45F050, "sub_45F050",
       draw_setup(viewer, mode, weapon, frozen, lod=lod, cull=cull, hdraw=HDRAW, hbone=HBONE), cw=cw)


def wake_setup(frozen, lod, cull, pos):
    def setup(e):
        reset(lod=lod, cull=cull)
        S["cam"] = e.alloc(0x60)
        wd(e, 0xBCAB08, 100.5, -2000.25, 33.0)
        wf(e, 0xBCAAE0, 0.36, 0.48, -0.8, -0.8, 0.6, 0.0, 0.48, 0.64, 0.6)
        h = human(e)
        w8(e, h + 0xD8, frozen)
        wd(e, h + 0x20, *pos)
        wf(e, h + 0xA0, 1234.5)
        return (h,)
    return setup


@pytest.mark.parametrize("frozen,lod,cull", [(0, 0, 0), (1, 0, 0), (1, 3, 0), (1, 4, 0), (1, -1, 0), (1, 0, 1)])
@pytest.mark.parametrize("pos", [(1000.1, 2000.3, -3000.7), (123456.789, -98765.4321, 0.1)])
@CWS
def test_wake_if_visible(frozen, lod, cull, pos, cw):
    go(0x45F140, "Human_WakeIfVisible", wake_setup(frozen, lod, cull, pos), cw=cw)


def text_setup(viewer, depth, texts, unk74, vis2, th, scr, memmode, font):
    def setup(e):
        h = draw_setup(viewer, 1, 0, depth=depth, th=th, scr=scr, memmode=memmode, font=font, vis2=vis2, frame=77,
                       hdraw=HDRAW, hbone=HBONE)(e)[0]
        w32(e, h + 0x74, unk74)
        S["newtext"] = e.alloc(0x28)
        for i in range(32):
            w8(e, h + 0x8D4 + 0x21 * i, 0x41 + i)
            w8(e, h + 0x8D5 + 0x21 * i, 0)
            if texts and (i % 3 != 2 or texts == 2):
                t = e.alloc(0x28)
                w32(e, t + 0x24, 0x11)
                w32(e, h + 0x854 + 4 * i, t)
        return h, 0x99
    return setup


@pytest.mark.parametrize("viewer,depth,texts,unk74,vis2,th,scr,memmode,font", [
    (0, 1000.0, 1, 1, 0, 3, (300.4, 200.6), 0, 0x4444),
    (0, 1000.0, 2, 0, 1, -5, (-30.5, 1e9), 1, 0),
    (0, 0.0, 1, 1, 1, 3, (300.4, 200.6), 0, 0),
    (0, -5.0, 1, 1, 1, 3, (300.4, 200.6), 0, 0),
    (0, 184320.0, 1, 1, 1, 3, (300.4, 200.6), 0, 0),
    (0, 184319.9, 0, 1, 1, 3, (300.5, 201.5), 0, 0x55),
    (0, 184319.9, 0, 1, 1, 3, (300.5, 201.5), 1, 0x55),
    (1, 1000.0, 1, 1, 0, 3, (300.4, 200.6), 0, 0),
])
@CWS
def test_draw(viewer, depth, texts, unk74, vis2, th, scr, memmode, font, cw):
    go(0x45F280, "Human_DrawHandler", text_setup(viewer, depth, texts, unk74, vis2, th, scr, memmode, font), cw=cw)


def test_draw_weapon():
    go(0x45F280, "Human_DrawHandler", draw_setup(0, 1, 1, hdraw=HDRAW, hbone=HBONE, depth=-1.0))


def test_wake():
    for frozen in (0, 1):
        def setup(e):
            reset()
            h = human(e)
            w8(e, h + 0xD8, frozen)
            return (h,)
        go(0x45F240, "Human_Wake", setup)


# --- collision ---------------------------------------------------------------------

def seg_setup(segs, radius=1000.0, pos=(500.0, -700.0, 30.0), single=False, filt=0xFF, answer=1):
    def setup(e):
        reset()
        h = human(e)
        wd(e, h + 0x20, *pos)
        wf(e, h + 0xA0, radius)
        s = e.alloc(24 * len(segs))
        en = e.alloc(24 * len(segs))
        for i, (a, b) in enumerate(segs):
            wd(e, s + 24 * i, *a)
            wd(e, en + 24 * i, *b)
        res = e.alloc(4)
        S["res"] = res
        set_handler(e, 0x30, T_HUMAN, FILTER)
        S["answer"] = answer
        set_handler(e, 9, T_BASE, HLINES)
        set_handler(e, 8, T_BASE, HLINES)
        m = e.alloc(0x40)
        if single:
            w32(e, m, s)
            w32(e, m + 4, en)
            w8(e, m + 8, filt)
            w32(e, m + 0xC, res)
            return h, m
        w32(e, m, s)
        w32(e, m + 4, en)
        w32(e, m + 8, len(segs))
        w8(e, m + 0xC, filt)
        w32(e, m + 0x10, res)
        return h, m
    return setup


FILTER = FK.handler(2, act=lambda e, a: w8(e, a[1], S["answer"]))
HLINES = FK.handler(2)

SEGS = [
    [((0.0, 0.0, 0.0), (1.0, 1.0, 1.0))],
    [((0.0, 2000.0, 0.0), (1.0, 2000.0, 0.0))],
    [((-1000.0, -700.0, 30.0), (2000.0, -700.0, 30.0))],
    [((0.0, 2000.0, 0.0), (1.0, 2000.0, 0.0)), ((500.0, -700.0, 30.0), (500.5, -699.0, 31.0))],
    [((1600.0, -700.0, 30.0), (1700.0, -700.0, 30.0))],
    [((1500.0, -700.0, 30.0), (1700.0, -700.0, 30.0))],
    [((-600.0, -700.0, 30.0), (-700.0, -700.0, 30.0))],
    [((-500.0, -700.0, 30.0), (-700.0, -700.0, 30.0))],
    [((500.0, 400.0, 30.0), (500.0, 300.5, 30.0))],
    [((500.0, -1800.0, 30.0), (500.0, -1700.5, 30.0))],
    [((500.0, -700.0, 1031.0), (500.0, -700.0, 1030.5))],
    [((500.0, -700.0, -971.0), (500.0, -700.0, -970.5))],
    [((500.0, -700.0, 1030.0), (500.0, -700.0, 1031.0))],
    [((500.0, -700.0, -969.99), (500.0, -700.0, -970.01))],
    [((0.0, 300.0, 30.0), (1000.0, 300.0, 30.0))],
    [((0.0, 300.0001, 30.0), (1000.0, 300.0001, 30.0))],
    [((0.0, 299.9999, 30.0), (1000.0, 299.9999, 30.0))],
    [((1207.1, -700.0, 30.0), (500.0, -1407.1, 30.0))],
    [((1207.2, -700.0, 30.0), (500.0, -1407.2, 30.0))],
    [((1207.10678, -700.0, 30.0), (500.0, -1407.10678, 30.0))],
    [((1316.4966, -700.0, 607.35), (-83.7, -1123.3, 1.1))],
    [((1e6, 1.0, 2.0), (-1e6, 1.5, 2.5))],
    [((1914.21356, -700.0, 30.0), (500.0, -2114.21356, 30.0))],
    [((1914.21357, -700.0, 30.0), (500.0, -2114.21357, 30.0))],
]


@pytest.mark.parametrize("segs", SEGS)
@CWS
def test_lines_hit_bounds(segs, cw):
    def setup(e):
        h, m = seg_setup(segs)(e)
        return h, e.u32(m), e.u32(m + 4), len(segs)
    go(0x45F580, "Human_LinesHitBounds", setup, cw=cw, check_eax=True)


@pytest.mark.parametrize("segs", SEGS[:4])
@pytest.mark.parametrize("filt,answer", [(0xFF, 0), (0x30, 0), (0x30, 1), (0x31, 0)])
def test_coll_lines(segs, filt, answer):
    go(0x45F500, "Human_CollLinesHandler", seg_setup(segs, filt=filt, answer=answer))
    go(0x45F800, "Human_CollLineHandler", seg_setup(segs, single=True, filt=filt, answer=answer))


# --- walls and ground --------------------------------------------------------------------

EV8, EV13 = 0x2B, 0x2C


def hit(found=0, pos=(0.0, 0.0, 0.0), normal=(0.0, 0.0, 0.0), dist=0.0, task=0, mat=0, mat2=0):
    return (struct.pack("<II3d", task, 0x44, *pos) + bytes(0x18) + struct.pack("<3fIIIIIIId", *normal, 0, 0, mat, mat2,
            0, found, 0, dist) + bytes(8))


def hits_act(key):
    def act(e, a):
        calls = S.get(key, [])
        n = S.get(key + "_n", 0)
        S[key + "_n"] = n + 1
        if calls:
            recs = calls[min(n, len(calls) - 1)]
            for i, r in enumerate(recs[:a[4] if key == "whits" else a[3]]):
                e.write(a[1] + 0x70 * i, r)
        elif key == "ghits":
            for i in range(a[3]):
                e.write(a[1] + 0x70 * i, hit())
    return act


def lines_fmt(e, a):
    n = a[4]
    return [a[0], e.read(a[2], 24 * n), e.read(a[3], 24 * n), n, a[5] & 0xFF, e.u32(a[6] + 4), a[7], a[8]]


def drop_fmt(e, a):
    n = a[3]
    return [a[0], e.read(a[2], 24 * n), n, e.read(a[4], 16), a[5] & 0xFF, e.u32(a[6] + 4), a[7], a[8]]


def foot(e, a):
    wf(e, a[0], *S.get("foot", (1.0, 2.0, 409.6)))
    return a[0]


FK.add(0x48A340, 3, foot, fmt=lambda e, a: [a[1], a[2]])
FK.add(0x416850, 0, EV8)
FK.add(0x416880, 0, EV13)
FK.add(0x4CDB20, 9, fmt=lines_fmt, act=hits_act("whits"))
FK.add(0x4CBE40, 9, fmt=drop_fmt, act=hits_act("ghits"))
FK.add(0x4F9720, 2, sv("gnode"))
FK.add(0x4F9770, 2, act=lambda e, a: wd(e, a[0], *S["gnodepos"]), fmt=lambda e, a: [a[1]])
FK.add(0x4AF9B0, 1, sv("routes"))
FK.add(0x452920, 1, sv("nodepos"))
FK.add(0x463550, 1)
FK.add(0x463310, 2)
FK.add(0x463640, 2)

N_UP = (0.0, 0.0, 1.0)
N_DOWN = (0.0, 0.0, -1.0)
WALL_X = (-0.98, 0.15, 0.05)


def walls_setup(frm, to, bits, crouch, whits, foot_=(1.0, 2.0, 409.6)):
    def setup(e):
        reset(whits=whits, foot=foot_, player=1)
        w32(e, 0x5BDC1C, 1)
        h = human(e)
        w8(e, h + 0x33D, crouch)
        wf(e, h + 0x664, 11.0, 12.0, 13.0)
        f = e.alloc(24)
        t = e.alloc(24)
        wd(e, f, *frm)
        wd(e, t, *to)
        v = e.alloc(12)
        b = e.alloc(4)
        w32(e, b, bits)
        return h, f, t, v, b
    return setup


FROM = (1000.0, 2000.0, 300.0)
WALLS = [
    (FROM, FROM, 4, 0, []),
    (FROM, (1100.0, 2010.0, 299.0), 4, 0, []),
    (FROM, (1100.0, 2010.0, 299.0), 0xC, 1, [[hit(), hit(1, (1050.0, 2005.0, 1000.0), WALL_X, 50.0), hit()], []]),
    (FROM, (1100.0, 2010.0, 299.0), 4, 0, [[hit(), hit(1, (1050.0, 2005.0, 1000.0), WALL_X, 50.0),
                                            hit(1, (1040.0, 2005.0, 6000.0), (-0.7, -0.7, 0.0), 40.0)]]),
    (FROM, (1100.0, 2010.0, 299.0), 4, 0, [[hit(), hit(1, (1050.0, 2005.0, 1000.0), (0.98, 0.15, 0.05), 50.0),
                                            hit()]]),
    (FROM, (1100.0, 2010.0, 299.0), 4, 0, [[hit(1, (1050.0, 2005.0, 600.0), WALL_X, 50.0),
                                            hit(1, (1050.0, 2005.0, 1000.0), (0.0, 0.0, 0.0), 50.0), hit()]]),
    (FROM, (1100.0, 2010.0, 299.0), 4, 0, [[hit(), hit(1, (1050.0, 2005.0, 1000.0), (-0.5, 0.0, 0.5), 50.0),
                                            hit()]]),
    (FROM, (1100.0, 2010.0, 250.0), 0, 0, [[hit(1, (1080.0, 2008.0, 260.0), N_UP, 30.0), hit(), hit()]]),
    (FROM, (1100.0, 2010.0, 250.0), 0, 0, [[hit(1, (1080.0, 2008.0, 260.0), (0.1, 0.0, 0.95), 30.0), hit(),
                                            hit()]]),
    (FROM, (1100.0, 2010.0, 350.0), 0, 1, [[hit(), hit(), hit(1, (1080.0, 2008.0, 7000.0), N_DOWN, 30.0)]]),
    (FROM, (1100.0, 2010.0, 350.0), 0, 0, [[hit(), hit(), hit(1, (1080.0, 2008.0, 7000.0), (0.3, 0.3, -0.6),
                                                             30.0)]]),
    (FROM, (1100.0, 2010.0, 350.0), 0, 0, [[hit(), hit(1, (1050.0, 2005.0, 1000.0), WALL_X, 50.0),
                                            hit(1, (1080.0, 2008.0, 7000.0), N_DOWN, 30.0)]]),
    (FROM, (1100.0, 2010.0, 350.0), 0, 0, [[hit(1, (1080.0, 2008.0, 260.0), N_UP, 30.0), hit(),
                                            hit(1, (1080.0, 2008.0, 7000.0), N_DOWN, 30.0)]]),
    (FROM, (1100.0, 2010.0, 299.0), 4, 1, [[hit(), hit(1, (1050.0, 2005.0, 1000.0), WALL_X, 50.0), hit()]]),
    (FROM, (1000.0, 2000.0, 299.0), 4, 0, [[hit(), hit(1, (1050.0, 2005.0, 1000.0), WALL_X, 50.0), hit()]]),
]


@pytest.mark.parametrize("frm,to,bits,crouch,whits", WALLS)
@CWS
def test_collide_walls(frm, to, bits, crouch, whits, cw):
    go(0x461A70, "Human_CollideWalls", walls_setup(frm, to, bits, crouch, whits), cw=cw)


HSTEP = FK.add(FK.code(b"\xc3"), 2, fmt=lambda e, a: [a[0], e.read(e.u32(a[1]), 24), e.read(e.u32(a[1] + 4), 24)]
               + list(struct.unpack("<IB3xIIII", e.read(a[1] + 8, 0x18))),
               act=lambda e, a: (w32(e, e.u32(a[1] + 0x20) + 0x58, S["step"][0]),
                                 w32(e, e.u32(a[1] + 0x20) + 0x4C, S["step"][1])))


def ground_setup(to, bits, crouch, ghits, reach, task=0, step=(0, 0), foot_=(1.0, 2.0, 409.6), attach=0):
    def setup(e):
        reset(ghits=ghits, foot=foot_, step=step)
        h = human(e)
        w8(e, h + 0x33D, crouch)
        wf(e, h + 0x728, 1.0, 2.0, 3.0)
        w32(e, h + 0x76C, attach)
        t = 0
        if task:
            t = e.alloc(0x40)
            w16(e, t + 0x1C, T_OTHER if task == 1 else T_RIGID)
            set_handler(e, 10, T_OTHER, HSTEP)
        S["ghits"] = [[g.replace(struct.pack("<I", 0x7A5C), struct.pack("<I", t), 1) for g in c] for c in ghits]
        f = e.alloc(24)
        t_ = e.alloc(24)
        wd(e, f, *FROM)
        wd(e, t_, *to)
        v = e.alloc(12)
        b = e.alloc(4)
        w32(e, b, bits)
        return h, f, t_, v, b, struct.unpack("<I", struct.pack("<f", reach))[0]
    return setup


G0 = hit(1, (1100.0, 2010.0, 250.0), (0.1, 0.2, 0.95), 90.0, task=0x7A5C, mat=0x111, mat2=0x222)
ROOF = hit(1, (1100.0, 2010.0, 7000.0), N_DOWN, 6000.0)
G_FAR = hit(1, (1100.0, 2010.0, -5000.0), (0.0, 0.0, 1.0), 9000.0, task=0x7A5C)
GROUNDS = [
    ((1100.0, 2010.0, 299.0), 0x4, 0, [[hit(), hit()], [hit()]], 2048.0, 0, (0, 0)),
    ((1100.0, 2010.0, 299.0), 0x4, 0, [[G0, hit()]], 2048.0, 0, (0, 0)),
    ((1100.0, 2010.0, 299.0), 0x4, 1, [[G0, ROOF]], 0.0, 0, (0, 0)),
    ((1100.0, 2010.0, 299.0), 0x0, 0, [[G0, ROOF]], 8192.0, 1, (1, 0x333)),
    ((1100.0, 2010.0, 299.0), 0x0, 0, [[G0, ROOF]], 8192.0, 1, (0, 0x333)),
    ((1100.0, 2010.0, 299.0), 0x0, 0, [[G0, ROOF]], 8192.0, 2, (1, 0x333)),
    ((1000.0, 2000.0, 299.0), 0x0, 0, [[G0, hit(1, (0.0, 0.0, 0.0), N_DOWN, 7000.0)]], 8192.0, 0, (0, 0)),
    ((1000.0, 2000.0, 299.0), 0x0, 0, [[G0, hit(1, (0.0, 0.0, 0.0), (0.0, 0.0, 0.5), 10.0)]], 8192.0, 0, (0, 0)),
    ((1100.0, 2010.0, 299.0), 0x4, 0, [[hit(), ROOF], [G0]], 2048.0, 0, (0, 0)),
    ((1100.0, 2010.0, 299.0), 0x4, 0, [[hit(1, (1100.0, 2010.0, 250.0), (0.0, 0.0, 0.0), 1.0), hit()]], 2048.0,
     0, (0, 0)),
    ((1100.0, 2010.0, 299.0), 0x4, 0, [[G_FAR, hit()]], 2048.0, 0, (0, 0)),
    ((1100.0, 2010.0, 299.0), 0x4, 0, [[G_FAR, hit()]], 4889.3994140625, 0, (0, 0)),
    ((1100.0, 2010.0, 299.0), 0x5, 0, [[G_FAR, hit()]], 8192.0, 1, (1, 0x333)),
    ((1100.0, 2010.0, 299.0), 0x4, 0, [[hit(), hit()], [G_FAR]], 2048.0, 1, (0, 0)),
]


@pytest.mark.parametrize("to,bits,crouch,ghits,reach,task,step", GROUNDS)
@CWS
def test_probe_ground(to, bits, crouch, ghits, reach, task, step, cw):
    go(0x462390, "Human_ProbeGround", ground_setup(to, bits, crouch, ghits, reach, task, step), cw=cw)


@pytest.mark.parametrize("path,node,routes,head,cur,unk64,attach,nodepos", [
    (0, 0, 0, 0, None, 0, 0, None),
    (1, 0, 0, 0, None, 0, 0, None),
    (1, 1, 0, 0, None, 1, 0, None),
    (1, 1, 0, 0, None, 1, 1, None),
    (1, 1, 1, 0, None, 1, 0, None),
    (1, 1, 1, 1, (0.0, 0.0, 0.0), 0, 0, (1100.0, 2010.0, 400.0)),
    (1, 1, 1, 1, (1100.0, 2010.0, 260.0), 0, 0, (1100.0, 2010.0, 400.0)),
    (1, 1, 1, 1, (1150.0, 2010.0, 260.0), 0, 0, (1100.0, 2010.0, 400.0)),
    (1, 1, 1, 1, (1150.0, 2010.0, 260.0), 0, 0, (1500.0, 2010.0, 400.0)),
    (1, 1, 1, 1, (1105.0, 2012.0, 260.0), 0, 0, (1100.0, 2010.0, 400.0)),
    (1, 1, 1, 1, (1000.0, 2010.0, 260.0), 0, 0, (1100.0, 2010.0, 400.0)),
])
@CWS
def test_probe_ground_ai(path, node, routes, head, cur, unk64, attach, nodepos, cw):
    def setup(e):
        r = ground_setup((1100.0, 2010.0, 299.0), 4, 0, [], 2048.0, attach=attach)(e)
        h = r[0]
        ai = e.alloc(0x3E30)
        w32(e, h + 0x800, ai)
        w32(e, ai + 0x3E2C, 77)
        if path:
            p = e.alloc(0x70)
            w32(e, p + 0x14, 0x1414)
            w8(e, p + 0x64, unk64)
            w32(e, ai + 0x3B6C, p)
        if node:
            n = e.alloc(0x140)
            w32(e, n + 0x138, 0x138)
            S["gnode"] = n
            S["gnodepos"] = (5.0, 6.0, 280.0)
        S["routes"] = routes
        n0 = e.alloc(0x20)
        w32(e, ai + 0xE0, n0)
        if head:
            w32(e, n0, e.alloc(8))
        if nodepos:
            np_ = e.alloc(24)
            wd(e, np_, *nodepos)
            S["nodepos"] = np_
        wd(e, h + 0x808, 1090.0, 2000.0, 200.0)
        if cur:
            wd(e, h + 0x820, *cur)
        return r
    go(0x462390, "Human_ProbeGround", setup, cw=cw)


# --- small handlers -------------------------------------------------------------------

def test_small_handlers():
    def setup(e):
        reset()
        w32(e, 0x5BDC18, 0x13572468)
        h = human(e)
        S["out"] = e.alloc(8)
        w32(e, S["out"], 0xFFFFFFFF)
        return h, S["out"]
    for addr, name in [(0x45F880, "Human_GetParamTableHandler"), (0x45F900, "Human_GetWeaponsHandler"),
                       (0x45F910, "Human_GetAmmoHandler"), (0x45F920, "Task_FalseHandler"),
                       (0x4606D0, "Human_GetSoundInfoHandler")]:
        go(addr, name, setup)


@pytest.mark.parametrize("found", [0, 1])
def test_param_changed(found):
    def setup(e):
        reset(vmodel=0x7777 if found else 0)
        h = human(e)
        e.write(h + 0x100, b"soldier_1\0")
        wd(e, h + 0xE0, 1.0, 2.0, 3.0)
        wf(e, h + 0xF8, 1.25)
        wf(e, h + 0xA0, 99.5)
        return h, 0
    go(0x45F890, "Human_ParamChangedHandler", setup)


# --- damage --------------------------------------------------------------------------

def damage_setup(h, total=10.0, mx=100.0, active=1, bits=0x5):
    wf(E, h + 0x254, total, mx)
    for i in range(17):
        wf(E, h + 0x260 + 8 * i, 1.5 * i, 0.25 + 0.5 * i)
    w8(E, h + 0xCF6, active)
    w32(E, h + 0x2F4, bits)


@pytest.mark.parametrize("player,xp,active,zone,dmg,total", [
    (0, 0, 1, 3, 25.5, 10.0), (1, 1, 1, 3, 25.5, 10.0), (1, 0, 1, 16, 25.5, 99.0), (0, 0, 0, 3, 25.5, 10.0),
    (0, 0, 1, 5, 33.3333, 0.1), (0, 0, 1, 0, 40.0, 100.0 - 0.25 * 40.0), (0, 0, 1, 7, 1e-3, 99.99999)])
@CWS
def test_hit(player, xp, active, zone, dmg, total, cw):
    def setup(e):
        reset(player=player, xp=xp, zone=zone)
        h = human(e)
        damage_setup(h, total, active=active)
        m = e.alloc(0x20)
        w32(e, m, 0x77)
        wf(e, m + 4, dmg, 0.5, -0.25, 1.0)
        return h, m
    go(0x45F930, "Human_HitHandler", setup, cw=cw)


@pytest.mark.parametrize("player,xp,active,falloff,total", [
    (0, 0, 1, 0.5, 10.0), (1, 1, 1, 0.5, 10.0), (1, 0, 1, 0.0, 10.0), (0, 0, 0, 0.5, 10.0),
    (0, 0, 1, 1e30, 10.0), (0, 0, 1, 0.123456789, 99.0), (0, 0, 1, float("nan"), 10.0),
    (0, 0, 1, -0.0, 10.0)])
@CWS
def test_blast(player, xp, active, falloff, total, cw):
    def setup(e):
        reset(player=player, xp=xp, falloff=falloff)
        h = human(e)
        damage_setup(h, total, active=active)
        wd(e, h + 0x20, 1.0, 2.0, 3.0)
        wf(e, h + 0xA0, 1000.1)
        return h, 0x4321
    go(0x45F9D0, "Human_BlastHandler", setup, cw=cw)


@pytest.mark.parametrize("active,dist,scale,hit,speed,player,xp,ai,immortal,total", [
    (1, 500.0, (2.0, 1.0, 1.5), (0, 0), 60.0, 0, 0, 0, 0, 10.0),
    (0, 500.0, (2.0, 1.0, 1.5), (0, 0), 60.0, 0, 0, 0, 0, 10.0),
    (1, 5000.0, (2.0, 1.0, 1.5), (0, 0), 60.0, 0, 0, 0, 0, 10.0),
    (1, 1999.0, (1.0, 1.0, 2.0), (0, 0), 60.0, 0, 0, 0, 0, 10.0),
    (1, 2001.0, (1.0, 3.0, 2.0), (0, 0), 60.0, 0, 0, 0, 0, 10.0),
    (1, 2001.0, (1.0, 2.0, 2.0), (0, 0), 60.0, 0, 0, 0, 0, 10.0),
    (1, 500.0, (2.0, 1.0, 1.5), (1, 0), 60.0, 0, 0, 0, 0, 10.0),
    (1, 500.0, (2.0, 1.0, 1.5), (1, 1), 60.0, 0, 0, 0, 0, 10.0),
    (1, 500.0, (2.0, 1.0, 1.5), (0, 0), 49.9, 0, 0, 0, 0, 10.0),
    (1, 500.0, (2.0, 1.0, 1.5), (0, 0), 50.0, 0, 0, 0, 0, 10.0),
    (1, 500.0, (2.0, 1.0, 1.5), (0, 0), 60.0, 1, 1, 0, 0, 10.0),
    (1, 500.0, (2.0, 1.0, 1.5), (0, 0), 60.0, 1, 0, 1, 1, 10.0),
    (1, 500.0, (2.0, 1.0, 1.5), (0, 0), 60.0, 0, 0, 1, 0, 10.0),
    (1, 500.0, (2.0, 1.0, 1.5), (0, 0), 10.0, 0, 0, 0, 0, 100.0),
])
@CWS
def test_crush(active, dist, scale, hit, speed, player, xp, ai, immortal, total, cw):
    def setup(e):
        reset(player=player, xp=xp, vradius=1000.0, hit=hit)
        h = human(e)
        damage_setup(h, total, active=active)
        p = e.alloc(24)
        wd(e, p, 100.0, 200.0, 300.0)
        S["dpos"] = p
        o = e.alloc(0x100)
        w16(e, o + 0x1C, T_RIGID)
        wd(e, o + 0x20, 100.0 + dist * 0.6, 200.0 - dist * 0.8, 300.0)
        wf(e, o + 0x98, *scale)
        w32(e, o + 0x6C, 0x6666)
        set_handler(e, 9, T_RIGID, HCOLL)
        if ai:
            a = e.alloc(0x3AC0)
            w8(e, a + 0x3ABC, immortal)
            w32(e, h + 0x800, a)
        v = e.alloc(12)
        wf(e, v, speed * 0.6, 0.0, speed * 0.8)
        m = e.alloc(8)
        w32(e, m, o)
        w32(e, m + 4, v)
        return h, m
    go(0x45FA80, "Human_CrushHandler", setup, cw=cw)


def coll_hits(e, a):
    m = a[1]
    hits = e.u32(m + 0x20)
    found, u50 = S["hit"]
    w32(e, hits + 0x58, found)
    w32(e, hits + 0x50, u50)


def coll_fmt2(e, a):
    m = a[1]
    st, en, cnt, filt, res, u14, u18, magic = struct.unpack("<3IB3xIIII", e.read(m, 0x20))
    hits = e.u32(m + 0x20)
    return [a[0], e.read(st, 24), e.read(en, 24), cnt, filt, res, u14, u18, magic, e.read(hits + 0x4C, 0x10)]


HCOLL = FK.add(FK.code(b"\xc3"), 2, act=coll_hits, fmt=coll_fmt2)


# --- camera and misc ----------------------------------------------------------------

@pytest.mark.parametrize("override,inp,prev,toggle,cammode", [
    (0, 0.0, 0.0, 0, 1), (1, 0.0, 0.0, 0, 1), (0, 1.0, 0.0, 0, 3), (0, 1.0, 0.0, 1, 3), (0, 1.0, 1.0, 1, 3),
    (0, -2.0, 0.0, 0, 3), (1, 1.0, 0.0, 0, 3), (0, float("nan"), 0.0, 0, 3), (0, 1.0, float("nan"), 0, 3)])
def test_camera_info(override, inp, prev, toggle, cammode):
    def setup(e):
        reset(campos=(1.5, 2.5, 3.5), cammat=[0.1 * i for i in range(9)])
        h = human(e)
        v = view(e, h)
        wf(e, v + 0x1E4, 1.25)
        w32(e, v + 0x1E8, 0x1E8)
        w8(e, v + 0x13A, override)
        wd(e, v + 0x140, 9.0, 8.0, 7.0)
        wf(e, v + 0x158, *[2.0 + i for i in range(10)])
        wf(e, h + 0x78, *[-1.0 - i for i in range(10)])
        w32(e, h + 0x4F0, cammode)
        w8(e, h + 0x4F4, toggle)
        wf(e, h + 0x1B0 + 19 * 4, inp)
        wf(e, h + 0x110 + 19 * 4, prev)
        info = e.alloc(0x74)
        e.write(info, bytes(range(0x74)))
        return h, info
    go(0x45FCD0, "Human_GetHumanCameraInfoHandler", setup)


def children(e, h, types):
    """Child list of h with tasks of the given types."""
    head = h + 0x8
    prev = head
    nodes = []
    for t in types:
        n = e.alloc(0x140)
        w16(e, n + 0x1C, t)
        w32(e, n + 4, prev)
        w32(e, prev, n)
        nodes.append(n)
        prev = n
    w32(e, prev, h + 0xC)
    w32(e, h + 0x10, prev)
    return nodes


HAI = FK.handler(2)


@pytest.mark.parametrize("kids,who", [([], 0), ([T_WEAPON], 0), ([T_WEAPON, T_OTHER, T_WEAPON], 0),
                                      ([T_WEAPON], 1), ([T_WEAPON], 2), ([T_WEAPON], 3)])
def test_pass_ai_event(kids, who):
    def setup(e):
        reset()
        h = human(e)
        children(e, h, kids)
        set_handler(e, EV_AI, T_WEAPON, HAI)
        m = e.alloc(0x20)
        w32(e, m + 0x14, h if who & 1 else 5)
        w32(e, m + 0x18, h if who & 2 else 6)
        w32(e, 0xAFA7E0, 3)
        return h, m
    go(0x45FDA0, "sub_45FDA0", setup)


@pytest.mark.parametrize("dead,active", [(0, 1), (1, 0)])
def test_detect_info(dead, active):
    def setup(e):
        reset()
        h = human(e)
        w32(e, h + 0xFC, 2)
        w8(e, h + 0xCF5, dead)
        w8(e, h + 0xCF6, active)
        m = e.alloc(0x28)
        e.write(m, b"\xEE" * 0x28)
        return h, m
    go(0x45FE30, "sub_45FE30", setup)


@pytest.mark.parametrize("same,team,best,pos", [
    (0, 1, -1.0, (100.0, 200.0, 300.0)), (1, 1, -1.0, (100.0, 200.0, 300.0)), (0, 2, -1.0, (100.0, 200.0, 300.0)),
    (0, 1, 10.0, (100.0, 200.0, 300.0)), (0, 1, 1e9, (100.0, 200.0, 300.0)), (0, 1, 0.0, (100.0, 200.0, 300.0)),
    (0, 1, float("nan"), (100.0, 200.0, 300.0)), (0, 1, 1e9, (12345.678, -9876.54321, 0.001))])
@CWS
def test_nearest_mate(same, team, best, pos, cw):
    def setup(e):
        reset()
        h = human(e)
        w32(e, h + 0xFC, 1)
        wd(e, h + 0x20, 1000.25, -2000.125, 50.0625)
        m = e.alloc(0x34)
        p = e.alloc(24)
        wd(e, p, *pos)
        mt = e.alloc(36)
        wf(e, mt, 0.36, 0.48, -0.8, -0.8, 0.6, 0.0, 0.48, 0.64, 0.6)
        w32(e, m, h if same else 0)
        w32(e, m + 4, p)
        w32(e, m + 8, mt)
        wd(e, m + 0x10, best)
        w32(e, m + 0x30, team)
        return h, m
    go(0x45FE70, "sub_45FE70", setup, cw=cw)


@pytest.mark.parametrize("dead", [0, 1])
def test_ammo_pickup(dead):
    def setup(e):
        reset()
        h = human(e)
        w8(e, h + 0xCF5, dead)
        m = e.alloc(8)
        w32(e, m, 12)
        w32(e, m + 4, 30)
        return h, m
    go(0x45FF80, "WeaponAmmoPickup", setup)


@pytest.mark.parametrize("dead,kids,ids,ammo", [
    (1, [], [], []), (0, [], [], []), (0, [T_WEAPON], [12], [1]), (0, [T_WEAPON], [12], [0]),
    (0, [T_WEAPON], [0], [1]), (0, [T_OTHER, T_WEAPON, T_WEAPON], [12, 5, 12], [1, 1, 1]),
    (0, [T_OTHER, T_WEAPON, T_WEAPON], [12, 5, 7], [1, 1, 1]), (0, [T_WEAPON, T_WEAPON], [5, 12], [1, 1])])
def test_gun_pickup(dead, kids, ids, ammo):
    def setup(e):
        reset()
        h = human(e)
        w8(e, h + 0xCF5, dead)
        nodes = children(e, h, kids)
        for n, i, a in zip(nodes, ids, ammo):
            if i:
                p = e.alloc(4)
                w32(e, p, i)
                w32(e, n + 0x128, p)
            if a:
                p = e.alloc(4)
                w32(e, p, 40 + i)
                w32(e, n + 0x12C, p)
        m = e.alloc(8)
        w32(e, m, 12)
        w32(e, m + 4, 30)
        return h, m
    go(0x45FFC0, "WeaponGunPickup", setup)


@pytest.mark.parametrize("w", [0, 0x4545])
def test_get_weapon(w):
    def setup(e):
        reset(weapon=w)
        return human(e), 0
    go(0x460070, "Human_GetWeaponHandler", setup, check_eax=True)


@pytest.mark.parametrize("arg,fp", [(0, 0), (0, 1), (1, 0), (5, 1)])
def test_first_person(arg, fp):
    def setup(e):
        reset()
        h = human(e)
        v = view(e, h)
        w8(e, v + 0xD9, fp)
        m = e.alloc(4)
        w32(e, m, arg)
        return h, m
    go(0x460090, "sub_460090", setup)


def bone_setup(arg, fp):
    def setup(e):
        reset()
        h = human(e)
        v = view(e, h)
        w8(e, v + 0xD9, fp)
        wd(e, v + 0x20, 1000.1, -2000.2, 3000.3)
        wd(e, h + 0x20, -50.5, 60.6, 7.7e5)
        vm = e.alloc(0x1D00)
        bm = e.alloc(0xC00)
        w32(e, v + 0x70, vm)
        w32(e, h + 0x70, bm)
        for base, mem in ((0x1A4, vm), (0x1AE8, vm), (0x1C74, vm), (0xB70, bm)):
            wf(e, mem + base, *[0.1 * (i + 1) + base for i in range(13)])
        m = e.alloc(0x48)
        e.write(m, b"\xAB" * 0x48)
        w32(e, m, arg)
        return h, m
    return setup


@pytest.mark.parametrize("arg,fp", [(0, 0), (0, 1), (1, 0), (1, 1)])
@CWS
def test_weapon_bone(arg, fp, cw):
    go(0x4600C0, "Human_GetWeaponBoneHandler", bone_setup(arg, fp), cw=cw)
    go(0x4601A0, "sub_4601A0", bone_setup(arg, fp), cw=cw)
    go(0x460210, "sub_460210", bone_setup(arg, fp), cw=cw)


@pytest.mark.parametrize("mode46,team,active,cull,vis", [
    (0, 0, 1, 0, 1.0), (3, 0, 1, 0, 1.0), (1, 0, 1, 0, 1.0), (1, 2, 1, 0, 0.5), (1, 1, 1, 0, 0.0),
    (1, 3, 0, 0, 1.0), (2, 0, 1, 1, 1.0), (1, 0, 1, 0, float("nan"))])
@CWS
def test_see_query(mode46, team, active, cull, vis, cw):
    def setup(e):
        reset(mode46=mode46, cull=cull, vis_out=vis)
        h = human(e)
        w32(e, h + 0xFC, team)
        w8(e, h + 0xCF6, active)
        wd(e, h + 0x20, 1000.25, -2000.125, 50.0625)
        wf(e, h + 0xA0, 512.0)
        q = e.alloc(0x24)
        p = e.alloc(24)
        wd(e, p, 12.5, 2000.75, -300.3)
        mt = e.alloc(36)
        wf(e, mt, 0.36, 0.48, -0.8, -0.8, 0.6, 0.0, 0.48, 0.64, 0.6)
        w32(e, q, p)
        w32(e, q + 4, mt)
        return h, q
    go(0x460280, "sub_460280", setup, cw=cw)


# --- HUD -------------------------------------------------------------------------------

HHUD = FK.handler(2)
HATT = FK.add(FK.code(b"\xc3"), 2, fmt=lambda e, a: [a[0], e.read(a[1], 4)])


@pytest.mark.parametrize("mode,weapon,u7b8,att,total,mx,size", [
    (1, 1, 0, 1, 25.0, 100.0, (64, 32)), (0, 1, 0, 1, 25.0, 100.0, (64, 32)), (1, 0, 1, 1, 0.0, 100.0, (63, 31)),
    (1, 1, 0, 0, 100.0, 100.0, (64, 32)), (1, 1, 0, 2, 150.0, 100.0, (64, 32)), (1, 0, 0, 1, 33.3, 77.7, (17, -9)),
    (1, 0, 0, 1, 1e-6, 3.0, (5, 7)), (1, 0, 0, 1, 1.0, 0.0, (5, 7)), (1, 0, 0, 1, 0.0, 0.0, (5, 7))])
@CWS
def test_render(mode, weapon, u7b8, att, total, mx, size, cw):
    def setup(e):
        reset(sw=size[0], sh=size[1])
        h = human(e)
        st = game(e, mode)
        d = e.alloc(12)
        w32(e, d + 4, 641)
        w32(e, d + 8, 479)
        S["display"] = d
        if weapon:
            w = e.alloc(0x40)
            w16(e, w + 0x1C, T_WEAPON)
            S["weapon"] = w
            set_handler(e, EV_HUD, T_WEAPON, HHUD)
        w8(e, h + 0x7B8, u7b8)
        if att:
            a = e.alloc(0x40)
            w16(e, a + 0x1C, T_OTHER if att == 1 else T_RIGID)
            S["att"] = a
            set_handler(e, EV6, T_OTHER, HATT)
        wf(e, h + 0x254, total, mx)
        hud = e.alloc(0x100)
        m = e.alloc(8)
        w32(e, m, hud)
        return h, m
    go(0x460400, "Human_RenderHandler", setup, cw=cw)


# --- gunshots --------------------------------------------------------------------------

@pytest.mark.parametrize("shooter,steam,team,player,ticks,snd,npts,ateam", [
    (None, 0, 1, 0, 0, 0, 2, 5), ("self", 0, 1, 0, 0, 0, 2, 5), ("null", 0, 1, 0, 0, 0, 2, 5),
    ("null", 0, 0, 0, 0, 0, 2, 5), ("human", 1, 1, 0, 0, 0, 2, 5), ("human", 0, 1, 0, 0, 0, 3, 5),
    ("other", 0, 1, 0, 0, 0, 1, 7), ("other", 0, 0, 1, 50, 0, 2, 5), ("other", 0, 0, 1, 50, 1, 2, 5),
    ("other", 0, 0, 1, 10, 1, 2, 5), ("other", 0, 0, 0, 10, 1, 0, 5)])
def test_gunshot(shooter, steam, team, player, ticks, snd, npts, ateam):
    def setup(e):
        reset(player=player, ticks=ticks, snd=0x5D5D if snd else 0, ateam=ateam,
              pts=[(1.0 * i, 2.0 * i, 3.0 + i) for i in range(npts)])
        w16(e, 0x5BDAF8, T_HUMAN)
        h = human(e)
        w32(e, h + 0xFC, team)
        w32(e, h + 0x848, 20)
        wd(e, h + 0x20, 5.0, 6.0, 7.0)
        m = e.alloc(12)
        w32(e, m, 0x3131)
        if shooter:
            r = e.alloc(4)
            w32(e, m + 8, r)
            if shooter == "self":
                w32(e, r, h)
            elif shooter == "human":
                a = human(e)
                w32(e, a + 0xFC, steam)
                w32(e, r, a)
            elif shooter == "other":
                a = human(e, T_OTHER)
                w32(e, r, a)
        return h, m
    go(0x4606F0, "Human_GunshotHandler", setup)


# --- script vars, activation -------------------------------------------------------------

@pytest.mark.parametrize("flag", [0, 1])
def test_script_vars(flag):
    def setup(e):
        reset()
        h = human(e)
        w8(e, h + 0xCF4, flag)
        return h, 0
    go(0x460850, "Human_RegisterVarsHandler", setup)
    go(0x4608D0, "Human_UnregisterVarsHandler", setup)


HWORLD = FK.handler(2)


@pytest.mark.parametrize("active", [0, 1])
def test_activate(active):
    def setup(e):
        reset()
        h = human(e)
        w8(e, h + 0xCF6, active)
        set_handler(e, EV_W0, T_BASE, HWORLD)
        set_handler(e, EV_W1, T_BASE, HWORLD)
        return h, 0x2222
    go(0x460950, "Human_ActivateHandler", setup)
    go(0x4609A0, "Human_DeactivateHandler", setup)


HPRE = FK.add(FK.code(b"\xc3"), 2, fmt=lambda e, a: [a[0], e.read(a[1], 12)])


@pytest.mark.parametrize("active", [0, 1])
def test_pre_delete(active):
    def setup(e):
        reset()
        h = human(e)
        w8(e, h + 0xCF6, active)
        set_handler(e, EV_W0, T_HUMAN, HPRE)
        return h, 0
    go(0x460AC0, "Human_PreDeleteHandler", setup)


@pytest.mark.parametrize("team", [0, 1, 2])
def test_is_team1(team):
    def setup(e):
        reset()
        h = human(e)
        w32(e, h + 0xFC, team)
        out = e.alloc(4)
        w32(e, out, 0x77777777)
        return h, out
    go(0x4609F0, "sub_4609F0", setup)


def list_setup(n, mask):
    def setup(e):
        reset(mask=mask)
        lst = e.alloc(8 + 4 * n)
        w32(e, lst, n)
        for i in range(n):
            h = human(e)
            w32(e, h + 0xFC, i % 3)
            wd(e, h + 0x20, (-1.0) ** i * (i + 1), 0.0, 0.0)
            w32(e, lst + 8 + 4 * i, h)
        w32(e, 0x5BDC24, lst)
        for i in range(8):
            w32(e, 0x5BDAD8 + 4 * i, 1 << i)
        m = e.alloc(8)
        w32(e, m, 0x9898)
        w32(e, m + 4, 3)
        return (m,)
    return setup


@pytest.mark.parametrize("n,mask", [(0, 7), (1, 1), (5, 0), (6, 3), (6, 4), (7, 0xFF)])
def test_criteria(n, mask):
    go(0x460A10, "Human_CountTeamCriteria", list_setup(n, mask))
    go(0x460A70, "Human_CountCriteria", list_setup(n, mask))


@pytest.mark.parametrize("ev0,ev1", [(0xFF, 0xFF), (3, 0xFF), (0xFF, 4), (5, 6)])
def test_close(ev0, ev1):
    def setup(e):
        reset()
        w32(e, 0x5BDC24, 0x1111)
        w32(e, 0x5BDC18, 0x2222)
        e.write(0x53F314, bytes([ev0, ev1, 0x21, 0x22]))
        w16(e, 0x5BDAF8, 0x123)
        w32(e, 0x5BDB00, 0x3333)
        for i in range(8):
            w32(e, 0x5BDAD8 + 4 * i, 0x4000 + i)
        return ()
    go(0x460B20, "Human_Close", setup)


@pytest.mark.parametrize("ev", [0xFF, 0x2C])
def test_getters(ev):
    def setup(e):
        reset()
        e.write(0x53F314, bytes([ev, ev, 0x21, 0x22]))
        w16(e, 0x5BDAF8, 0x123)
        w32(e, 0x5BDC18, 0x2222)
        w32(e, 0x5BDC24, 0x1111)
        return ()
    for addr, name, mask in [(0x460BE0, "Human_GetType", 0xFFFF), (0x460BF0, "Human_GetEvent0", 0xFF),
                             (0x460C10, "Human_GetEvent1", 0xFF), (0x460C30, "Human_GetGunshotEvent", 0xFF),
                             (0x460C40, "Human_GetPunchEvent", 0xFF), (0x460C50, "Human_GetParamTable", None),
                             (0x460C60, "Human_GetList", None)]:
        go(addr, name, setup, check_eax=True, eax_mask=mask or 0xFFFFFFFF)


def test_setters():
    def setup(e):
        reset()
        return human(e), 0xABCD
    go(0x460C70, "Human_SetAI", setup)
    go(0x460CE0, "sub_460CE0", setup)


@pytest.mark.parametrize("player,cheat,last,debug", [(0, 1, 0, 1), (1, 0, 0, 0), (1, 0, 0, 1), (1, 1, 100, 0),
                                                     (1, 1, 99, 0), (1, 1, 99, 1)])
def test_printf(player, cheat, last, debug):
    def setup(e):
        reset(player=player, cheat=cheat, frame=100)
        w32(e, 0x5BDAFC, last)
        w32(e, 0x5BDC1C, debug)
        h = human(e)
        f = e.alloc(16)
        e.write(f, b"PosX: %.2f\n\0")
        return h, f, 0x11223344, 0x55667788
    go(0x460C80, "Human_printf", setup)


# --- movement --------------------------------------------------------------------------

def carrier_act(e, a):
    if S.get("cpos") is None:
        w32(e, a[1], 0)
        return
    p = e.alloc(24)
    wd(e, p, *S["cpos"])
    m = e.alloc(40)
    wf(e, m, 0.36, 0.48, -0.8, -0.8, 0.6, 0.0, 0.48, 0.64, 0.6, 1.5)
    o = e.alloc(24)
    wd(e, o, *S["coff"])
    e.write(a[1], struct.pack("<3I", p, m, o))


HCARRY = FK.add(FK.code(b"\xc3"), 2, act=carrier_act, fmt=lambda e, a: [a[0]])


@pytest.mark.parametrize("attach,carrier,handler,cpos", [
    (1, 0, 1, (1000.5, -2000.25, 30.125)), (0, 0, 1, (1000.5, -2000.25, 30.125)), (0, 1, 1, (1.0, 2.0, 3.0)),
    (1, 0, 0, (1.0, 2.0, 3.0)), (1, 0, 1, None), (1, 0, 1, (123456.789, -0.001, 7e5))])
@CWS
def test_carrier_move(attach, carrier, handler, cpos, cw):
    def setup(e):
        o = e.alloc(0x40)
        w16(e, o + 0x1C, T_OTHER)
        reset(carrier=o if carrier else 0, cpos=cpos, coff=(0.5, -0.25, 12.125))
        h = human(e)
        wd(e, h + 0x20, 999.9, -1999.8, 29.7)
        e.write(h + 0x728, b"\x11" * 0x40)
        if attach:
            w32(e, h + 0x76C, o)
        if handler:
            set_handler(e, EV14, T_OTHER, HCARRY)
        return (h,)
    go(0x460CF0, "sub_460CF0", setup, cw=cw)


def use_act(e, a):
    if "found" in S:
        w32(e, a[1] + 0x80, S["found"])


def use_fmt(e, a):
    q = a[1]
    return [a[0], e.read(q, 4), e.read(q + 8, 0x3C), e.read(q + 0x48, 0x3C)]


HUSE = FK.add(FK.code(b"\xc3"), 2, act=use_act, fmt=use_fmt)
HCTRL = FK.add(FK.code(b"\xc3"), 2, fmt=lambda e, a: [a[0], e.read(a[1], 4), e.read(a[1] + 8, 0x40)])


@pytest.mark.parametrize("bits,u7b8,player,ntasks,ctrl", [
    (0, 0, 0, 2, 1), (4, 0, 0, 0, 0), (4, 0, 1, 3, 0), (4, 1, 0, 2, 1), (4, 0, 0, 1, 2), (0xFF, 0, 1, 2, 1)])
def test_use_and_control(bits, u7b8, player, ntasks, ctrl):
    def setup(e):
        reset(player=player, campos=(1.0, 2.0, 3.0), cammat=[0.5 * i for i in range(9)], found=0x4141)
        h = human(e)
        v = view(e, h)
        w32(e, h + 0x2F4, bits)
        w8(e, h + 0x7B8, u7b8)
        wd(e, h + 0x20, 9.0, 8.0, 7.0)
        wf(e, h + 0x78, *[1.0 + i for i in range(10)])
        e.write(h + 0x534, bytes(range(1, 29)))
        w32(e, h + 0x7BC, 41)
        lst = e.alloc(8 + 4 * ntasks)
        w32(e, lst, ntasks)
        for i in range(ntasks):
            t = e.alloc(0x40)
            w16(e, t + 0x1C, T_OTHER)
            w32(e, lst + 8 + 4 * i, t)
        S["tasklist"] = lst
        set_handler(e, EV2, T_OTHER, HUSE)
        if ctrl:
            r = e.alloc(4)
            w32(e, h + 0x7B4, r)
            if ctrl == 1:
                t = e.alloc(0x40)
                w16(e, t + 0x1C, T_OTHER)
                w32(e, r, t)
                set_handler(e, EV4, T_OTHER, HCTRL)
        return (h,)
    go(0x460EE0, "sub_460EE0", setup)


@pytest.mark.parametrize("attach,inp,prev,frozen,fov", [
    (1, 0.0, 0.0, 0, 1.0), (0, 0.5, 0.25, 0, 1.0), (0, 0.5, 0.25, 0, 0.0), (0, 0.03, 0.01, 0, 1.0),
    (0, -0.03, 0.01, 0, 1.0), (0, -0.7, 0.9, 1, 1.0), (0, 0.05999999, 0.0, 0, 1.0), (0, -1.0, 0.0, 0, 2.5),
    (0, 0.05, 0.02, 0, "tan")])
@CWS
def test_update_body(attach, inp, prev, frozen, fov, cw):
    def setup(e):
        reset(euler=(0.25, -0.5, 1.75))
        h = human(e)
        v = view(e, h)
        f = fov
        if fov == "tan":
            f = math.tan(struct.unpack("<f", struct.pack("<f", math.pi / 4))[0])
        wf(e, v + 0x1E4, f)
        wf(e, v + 0x134, 0.125)
        wf(e, h + 0x1B0, inp)
        wf(e, h + 0x110, prev)
        w8(e, h + 0x510, frozen)
        wf(e, h + 0x4F8, 9.0, 9.0, 9.0, 9.0, 9.0, 0.3)
        wf(e, h + 0x75C, 0.0, 0.0, -0.0123)
        if attach:
            o = e.alloc(0x100)
            wf(e, o + 0x70, 0.36, 0.48, -0.8, -0.8, 0.6, 0.0, 0.48, 0.64, 0.6)
            w32(e, o + 0x94, 7)
            w32(e, h + 0x76C, o)
            wf(e, h + 0x788, 0.6, 0.0, 0.8, 0.0, 1.0, 0.0, -0.8, 0.0, 0.6)
            w32(e, h + 0x7AC, 30)
        return (h,)
    go(0x4610C0, "Human_UpdateBody", setup, cw=cw)


@pytest.mark.parametrize("sound", [-1, 0, 12])
@CWS
def test_anim_sound(sound, cw):
    def setup(e):
        reset(animev=(0, sound, 0, 0, 10.5, -20.25, 30.125))
        h = human(e)
        wd(e, h + 0x20, 1000.1, 2000.2, -3000.3)
        wf(e, h + 0x78, 0.36, 0.48, -0.8, -0.8, 0.6, 0.0, 0.48, 0.64, 0.6)
        return (h,)
    go(0x4613D0, "sub_4613D0", setup, cw=cw)


@pytest.mark.parametrize("ground,bits,normal,flags,coll_bits", [
    (0, 0, (0.0, 0.0, 1.0), (0, 0, 0, 0, 0), None),
    (1, 0, (0.0, 0.0, 1.0), (0, 0, 0, 0, 0), None),
    (1, 4, (0.0, 0.0, 0.0), (0, 1, 1, 0, 1), None),
    (1, 4, (0.0, 0.0, 2.0), (0, 1, 1, 0, 1), 0x14),
    (1, 4, (0.9, 0.1, 0.1), (0, 1, 1, 0, 1), None),
    (1, 4, (0.6, 0.3, 0.4), (0, 1, 1, 0, 1), 0),
    (1, 4, (0.6, 0.3, 0.8), (0, 0, 1, 0, 0), None),
    (1, 4, (0.5, 0.3, 1.0), (0, 1, 0, 0, 0), None),
    (1, 0x14, (0.2, 0.3, 1.0), (1, 1, 1, 0, 1), None),
    (1, 0x14, (0.2, 0.3, 1.0), (0, 0, 0, 0, 0), None),
    (1, 4, (-0.75, 0.25, 0.35), (0, 1, 1, 0, 1), 0x10),
    (1, 4, (0.0, 0.0, -1.0), (0, 0, 1, 0, 0), None),
    (1, 4, (0.5, 0.3, 0.5), (0, 1, 1, 0, 1), None),
    (1, 4, (0.6, 0.3, 0.3), (0, 1, 1, 0, 1), None),
    (1, 4, (-0.2, 0.1, 0.9), (0, 0, 1, 0, 0), None),
])
@CWS
def test_update_move(ground, bits, normal, flags, coll_bits, cw):
    def setup(e):
        reset(player=1, cheat=0, frame=5)
        w32(e, 0x5BDC1C, 1)
        if coll_bits is not None:
            S["coll_to"] = (12345.0, -98765.0, 333.0)
            S["coll_v"] = (0.25, 0.5, 0.75)
            S["ghits"] = [[hit(1, (12345.0, -98765.0, 300.0), (0.1, 0.2, 0.9), 50.0)]]
            S["whits"] = [[hit(), hit(1, (12346.0, -98760.0, 400.0), (-0.9, 0.3, 0.01), 3.0), hit()]]
        h = human(e)
        w32(e, h + 0x2F4, bits)
        w32(e, h + 0x6F8, ground)
        wf(e, h + 0x6D8, *normal)
        wf(e, h + 0x664, 10.1, -20.2, 30.3)
        wf(e, h + 0x520, 1.01, 2.02, -3.03)
        wf(e, h + 0x670, 5.5, 6.6, -7.7)
        wf(e, h + 0x67C, 100.25, -200.5, 3.75)
        wf(e, h + 0x728, 0.125, 0.25, -0.375)
        wd(e, h + 0x20, 12345.678, -98765.4321, 333.333)
        wd(e, h + 0x720, 4096.5)
        wf(e, h + 0xA0, 600.0)
        wf(e, h + 0x688, 1.0, 2.0, 3.0)
        e.write(h + 0x694, bytes(flags))
        return (h,)
    go(0x4614B0, "Human_UpdateMove", setup, cw=cw)
