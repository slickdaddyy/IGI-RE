import random
import struct

import pytest

from emu import Emu
from test_graphtask_util import CWS, Fakes, check, f32, f64, i32, set_handler, task

emu = Emu()
fk = Fakes(emu)
R = {}
WORLD = 0x5A5A0000
TYPE, EV0, EVENT = 0x66, 0x21, 0x4A
STACK = (0x100000, 0x200000)


def project(e, a):
    """TransContext_Project(out, depth, pos): screen and depth from R by call."""
    x, y, d = R["proj"].pop(0) if R["proj"] else (100.0, 200.0, 3000.0)
    e.write(a[0], f32(x, y))
    e.write(a[1], f32(d))


def scale_size(e, a):
    w, h = R["half"].pop(0) if R["half"] else (4.0, 3.0)
    e.write(a[0], f32(w, h))


def coll_lines(e, a):
    for j in range(a[4]):
        e.write(a[1] + 0x70 * j + 0x58, i32(R["found"][j % len(R["found"])]))


def draw_fmt(e, a):
    words = list(a)
    if R.get("noalpha"):
        words[13] = None
    return tuple(words)


fk.add(0x401900, 5, 0x66, fmt=lambda e, a: (e.read_cstr(a[0]), a[1], a[2] & 0xFFFF, a[3], a[4]))
fk.add(0x4017C0, 1, EVENT)
fk.add(0x4018C0, 1, fmt=lambda e, a: (a[0] & 0xFF,))
fk.add(0x401A20, 1, fmt=lambda e, a: (a[0] & 0xFFFF,))
fk.add(0x401400, 3, fmt=lambda e, a: (a[0] & 0xFFFF, a[1] & 0xFF, e.normalize(a[2])))
fk.add(0x401530, 3, fmt=lambda e, a: (a[0] & 0xFFFF, a[1], e.normalize(a[2])))
fk.add(0x4CEA10, 0, EV0)
fk.add(0x4012A0, 3, lambda e, a: R["new"], fmt=lambda e, a: (a[0], a[1] & 0xFFFF, a[2]))
fk.add(0x4015F0, 1)
fk.add(0x4C7760, 1)
fk.add(0x4C7140, 5, fmt=lambda e, a: (a[0], a[1], e.read(a[2], 24), a[3], a[4]))
fk.add(0x4C6940, 4, fmt=lambda e, a: (a[0], a[1], e.read(a[2], 24), a[3]))
fk.add(0x4C7560, 2)
fk.add(0x497F20, 3, project, fmt=lambda e, a: (e.read(a[2], 24),))
fk.add(0x498040, 3, scale_size, fmt=lambda e, a: (e.read(a[1], 8), a[2]))
fk.add(0x4B53B0, 17, fmt=draw_fmt)
fk.add(0x4CC2A0, 8, coll_lines, fmt=lambda e, a: (a[0], a[1], e.read(a[2], 24 * a[4]), e.read(a[3], 24 * a[4]), a[4],
                                                   a[5] & 0xFF, e.u32(a[6] + 4), a[7]))
KID = fk.handler(2)
CALLBACK = fk.handler(1, fmt=lambda e, a: (e.u32(a[0]), e.read(a[0] + 8, 48)))
emu.baseline = emu.snapshot()


def base(e):
    R.clear()
    R["proj"], R["half"] = [], []
    e.call(0x4B0BA0)  # Mem_Init
    e.write(0x54E568, struct.pack("<H", TYPE))
    e.write(0x54E56A, bytes([EVENT]))
    e.write(0x54875C, struct.pack("<H", 0x1D))
    e.call(0x4B4720, 0xA7A4C0)
    e.call(0x4B47E0, 0xA7A4C0, 0x12332153)


def sprite(e, frames=0):
    s = e.alloc(0x40)
    if frames:
        a = e.alloc(0x10 + frames)
        e.write(a + 4, struct.pack("<h", frames))
        e.write(a + 0x10, bytes([(i * 3) % frames + 1 for i in range(frames)]))
        e.write(s + 0xC, i32(a))
    return s


def particle(e, p, rng, pos=None):
    pos = pos or [rng.uniform(-1e5, 1e5) for _ in range(3)]
    e.write(p, f64(*pos))
    e.write(p + 0x18, f32(*[rng.uniform(-60, 60) for _ in range(3)]))
    e.write(p + 0x24, struct.pack("<hhhh", rng.randint(0, 3), rng.choice([0, 0, 2]), rng.randint(0, 3),
                                  rng.randint(0, 4)))
    e.write(p + 0x2C, f32(rng.choice([0.0, rng.uniform(-0.2, 0.2)]), rng.uniform(0, 1), rng.uniform(0.01, 0.3),
                          rng.uniform(-3, 3), rng.uniform(0.01, 2)))
    e.write(p + 0x40, f64(*[x + rng.uniform(-500, 500) for x in pos]))


def group(e, rng, count=5, cap=8, frames=0, flags=0, light=0.0, grow=0.0, callback=0):
    t = task(e, 0x100, TYPE)
    buf, lst = e.alloc(0x58 * cap), e.alloc(4 * cap)
    for i in range(cap):
        e.write(lst + 4 * i, i32(buf + 0x58 * i))
    order = list(range(cap))
    rng.shuffle(order)
    for i in range(cap):
        e.write(lst + 4 * i, i32(buf + 0x58 * order[i]))
        particle(e, buf + 0x58 * order[i], rng)
    e.write(t + 0x68, i32(buf, lst, e.alloc(0x70 * cap), e.alloc(24 * cap), e.alloc(24 * cap), e.alloc(4 * cap),
                          sprite(e, frames)))
    box = [rng.uniform(-1e5, 1e5) for _ in range(3)]
    e.write(t + 0x88, f64(*box, *[x + rng.uniform(0, 9000) for x in box]))
    e.write(t + 0xB8, i32(WORLD))
    e.write(t + 0xBC, f32(rng.uniform(10, 300), rng.uniform(10, 300), *[rng.uniform(-2, 2) for _ in range(3)],
                          *[rng.uniform(0, 255) for _ in range(3)], light, rng.uniform(20000, 80000), grow))
    e.write(t + 0xE8, i32(flags, callback, count, rng.randint(1, 3), cap))
    e.write(t + 0xFC, bytes([1 if frames else 0, 1]))
    return t


def test_open_close():
    check(emu, fk, 0x4FCF50, "Particle2D_Open", base, check_eax=False)
    check(emu, fk, 0x4FDEC0, "Particle2D_Close", base, check_eax=False)
    check(emu, fk, 0x4FDEF0, "Particle2D_GetCollideEvent", base, eax_mask=0xFF)


def test_create():
    def setup(e):
        base(e)
        return (group(e, random.Random(1)),)
    check(emu, fk, 0x4FD5E0, "Particle2D_Create", setup, check_eax=False)


@pytest.mark.parametrize("frames,cap", [(0, 6), (4, 1), (0, 0)])
def test_new(frames, cap):
    def setup(e):
        base(e)
        R["new"] = task(e, 0x100, TYPE)
        e.write(0xA7A4D0, i32(0x1000, 0x1200 if frames else 0x5000))
        g, c, s = e.alloc(12), e.alloc(12), e.alloc(8)
        e.write(g, f32(0.5, -1.5, -9.8))
        e.write(c, f32(255, 128, 64))
        e.write(s, f32(40, 30))
        return (0x7A5C, WORLD, sprite(e, frames), g, c, s) + struct.unpack("<I", f32(50000)) + \
            (0x80000, 2) + struct.unpack("<2I", f32(0.01, 0.5)) + (cap, 0x4E4E4E)
    check(emu, fk, 0x4FDF00, "Particle2D_New", setup)


@pytest.mark.parametrize("count,frames,timer", [(3, 0, 15), (3, 7, -1), (8, 5, 3), (0, 1000003, 0x7FFF)])
@pytest.mark.parametrize("cw", CWS)
def test_add(count, frames, timer, cw):
    def setup(e):
        base(e)
        rng = random.Random(count)
        t = group(e, rng, count=count)
        e.write(0xA7A4DC, struct.pack("<h", timer))
        pos, vel = e.alloc(24), e.alloc(12)
        e.write(pos, f64(1.5, 2.5, -3.5))
        e.write(vel, f32(4, 5, 6))
        return (t, pos, vel) + struct.unpack("<4I", f32(0.9, 0.05, 1.25, 0.75)) + (0x12340007, frames)
    check(emu, fk, 0x4FE120, "Particle2D_Add", setup, check_eax=False, cw=cw)


DIRS = [(0, 0, 1), (0, 0, 0), (1, 0.2, 0), (0.3, -0.8, 0.5), (-5, 1, 1), (0, 1e-30, 0)]


@pytest.mark.parametrize("d", DIRS)
@pytest.mark.parametrize("cw", CWS)
def test_emit(d, cw):
    def setup(e):
        base(e)
        rng = random.Random(str(d))
        t = group(e, rng, count=2)
        for _ in range(rng.randint(0, 5)):
            e.call(0x4B4770, 0xA7A4C0)
        pos, dv = e.alloc(24), e.alloc(12)
        e.write(pos, f64(1e4, -2e4, 300.5))
        e.write(dv, f32(*d))
        return (t, pos, dv) + struct.unpack("<7I", f32(rng.uniform(0, 1.5), 10, 90, 1, 0.1, 0.5, 2)) + (3, 25)
    check(emu, fk, 0x4FE210, "Particle2D_Emit", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("lights", [0, 1, 4])
@pytest.mark.parametrize("cw", CWS)
def test_get_light(lights, cw):
    def setup(e):
        base(e)
        rng = random.Random(lights)
        t = group(e, rng)
        e.write(t + 0xDC, f32(0.7))
        base_ = e.alloc(0x40 * 8)
        for i in range(8):
            e.write(base_ + 0x40 * i + 0xC, f32(*[rng.uniform(0, 1.5) for _ in range(3)]))
        idx = e.alloc(16)
        e.write(idx, i32(*[rng.randint(0, 7) for _ in range(4)]))
        e.write(0xB041E0, i32(base_, idx, lights))
        e.write(0xB041F0, i32(0x40))
        e.write(0xB818A0, f32(0.2, 0.3, 0.25))
        return t, e.alloc(12)
    check(emu, fk, 0x4FD4E0, "Particle2D_GetLight", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("box", ["zero", "flat", "tiny", "big"])
@pytest.mark.parametrize("cw", CWS)
def test_radius_center(box, cw):
    def setup(e):
        base(e)
        t = group(e, random.Random(box))
        lo = [1234.5, -50.25, 7.0]
        size = {"zero": [0, 0, 0], "flat": [0, 0, 3.5], "tiny": [1e-9, 0, 0], "big": [9000, 7000.5, 123]}[box]
        e.write(t + 0x88, f64(*lo, *[a + b for a, b in zip(lo, size)]))
        return (t,)
    check(emu, fk, 0x4FDC20, "Particle2D_GetRadius", setup, check_eax=False, check_st0=True, cw=cw)

    def setup2(e):
        return (e.alloc(24),) + setup(e)
    check(emu, fk, 0x4FDCD0, "Particle2D_GetCenter", setup2, check_eax=False, cw=cw)


@pytest.mark.parametrize("count,i", [(5, 4), (5, 1), (1, 0), (0, 0)])
def test_remove(count, i):
    def setup(e):
        base(e)
        return group(e, random.Random(2), count=count), i
    check(emu, fk, 0x4FDD30, "Particle2D_Remove", setup, check_eax=False)


def test_delete():
    def setup(e):
        base(e)
        t = group(e, random.Random(3))
        for off, size in ((0x68, 0x58), (0x6C, 4), (0x70, 0x70), (0x74, 24), (0x78, 24), (0x7C, 4)):
            e.write(t + off, i32(e.call(0x4B0C60, size * 8, 4)[0]))
        e.write(0xA7A4D0, i32(0x9000))
        return (t,)
    check(emu, fk, 0x4FDD70, "Particle2D_Delete", setup, check_eax=False)


@pytest.mark.parametrize("kids", [0, 1, 3])
def test_add_to_world(kids):
    def setup(e):
        base(e)
        t = group(e, random.Random(kids))
        h = KID
        for k in range(kids):
            c = task(e, 0x40, 0x30 + k % 2, t)
        set_handler(e, EV0, 0x30, h)
        msg = e.alloc(8)
        e.write(msg, i32(0x11, WORLD))
        return t, msg
    check(emu, fk, 0x4FDE00, "Particle2D_AddToWorld", setup, check_eax=False)


DRAW = [dict(), dict(light=0.8), dict(flags=0x80000), dict(flags=0x1080000, light=0.3), dict(flags=0x8),
        dict(frames=5, flags=0x8), dict(grow=0.01, flags=0x80000), dict(count=0), dict(light=0.5, count=0)]


@pytest.mark.parametrize("k", range(len(DRAW)))
@pytest.mark.parametrize("cw", CWS)
def test_draw(k, cw):
    kw = DRAW[k]

    def setup(e):
        base(e)
        rng = random.Random(k)
        t = group(e, rng, count=kw.get("count", 6), frames=kw.get("frames", 0), flags=kw.get("flags", 0),
                  light=kw.get("light", 0.0), grow=kw.get("grow", 0.0))
        R["noalpha"] = not kw.get("flags")
        R["proj"] = [(rng.uniform(0, 640), rng.uniform(0, 480), d) for d in (3000, 1000, 2048, 2049, 90000, 40000)]
        R["half"] = [(rng.choice([0.25, rng.uniform(1, 30)]), rng.choice([0.4, rng.uniform(1, 30)])) for _ in range(6)]
        base_ = e.alloc(0x40)
        e.write(base_ + 0xC, f32(0.5, 0.9, 0.1))
        idx = e.alloc(4)
        e.write(0xB041E0, i32(base_, idx, 1))
        e.write(0xB041F0, i32(0x40))
        e.write(0xB818A0, f32(0.2, 0.3, 0.25))
        return (t,)
    check(emu, fk, 0x4FD020, "Particle2D_Draw", setup, check_eax=False, cw=cw)


RUN = [dict(), dict(count=0), dict(count=0, keep=1), dict(frames=4), dict(callback=1), dict(callback=1, count=8),
       dict(fade=1), dict(callback=1, count=1, fade=1), dict(near=1)]


@pytest.mark.parametrize("k", range(len(RUN)))
@pytest.mark.parametrize("seed", range(3))
@pytest.mark.parametrize("cw", [0x37F, 0x7F, 0xC7F])
def test_run(k, seed, cw):
    kw = RUN[k]

    def setup(e):
        base(e)
        rng = random.Random(seed * 100 + k)
        t = group(e, rng, count=kw.get("count", 6), frames=kw.get("frames", 0), callback=CALLBACK if kw.get("callback") else 0,
                  grow=rng.choice([0.0, -0.02, 0.01]))
        if kw.get("keep"):
            e.write(t + 0xFD, b"\0")
        if kw.get("fade"):
            for i in range(8):
                p = e.u32(e.u32(t + 0x6C) + 4 * i)
                e.write(p + 0x26, struct.pack("<h", 0))
                e.write(p + 0x30, f32(rng.uniform(0, 0.2), 0.1))
        if kw.get("near"):
            ps = [e.u32(e.u32(t + 0x6C) + 4 * i) for i in range(6)]
            lo = struct.unpack("<3d", e.read(ps[0], 24))
            for p in ps:
                e.write(p, f64(*[x + rng.uniform(0, 100) for x in lo]))
            e.write(t + 0x88, f64(*[x - 50 for x in lo], *[x + 300 for x in lo]))
        R["found"] = [rng.random() < 0.4 for _ in range(8)]
        e.write(0xA7A4D8, i32(rng.choice([0, 100, 255])))
        return (t,)
    check(emu, fk, 0x4FD660, "Particle2D_Run", setup, check_eax=False, cw=cw)
