import random
import struct

import pytest

from emu import Emu
from test_fxobj_util import (LOG, RET, add_child, cstr, d64, f32, floatfake, handler, logfake, pyfake, run,
                             set_events, set_handler, task, u32, vfake)

ENV, SOUND, OTHER = 0x61, 0x62, 0x63
DATA = {}

emu = Emu()
set_events(emu)
logfake(emu, 0x4E6B00, 2, strings=(1,),
        effect=lambda e, a: task(e, 0x120, SOUND) if RET.get(0x4E6B00, 1) else 0)  # Sound_Play
logfake(emu, 0x4E6C00, 1)  # Sound_Stop
logfake(emu, 0x4E6C30, 2, derefs={1: 24})  # Sound_SetPos
logfake(emu, 0x4E6C60, 3)
vfake(emu, 0x4E7BD0, 9, show=lambda e, a: (e.read_cstr(a[0]),) + tuple(a[1:1 + 2 * e.read_cstr(a[0]).count(b"%")]))
logfake(emu, 0x4E8C50, 0, ret=SOUND)
logfake(emu, 0x416A70, 1, ret=0x7400)  # Game_GetPlayer
logfake(emu, 0x416920, 0, ret=0x9999)  # Game_GetWorld
floatfake(emu, 0x4F80F0, 1, derefs={0: 24})
for slot in (1,):
    for t in (SOUND, OTHER):
        set_handler(emu, slot, t, handler(emu, "run_%x" % t))


def find(e, c, type_):
    while c and e.u32(c):
        if e.read(c + 0x1C, 2) == struct.pack("<H", type_ & 0xFFFF):
            return c
        c = e.u32(c)
    return 0


def find_child(e):
    t, type_ = e.arg(0), e.arg(1)
    LOG[-1].append(("FindChild", e.normalize(t), type_ & 0xFFFF)) if LOG else None
    e.ret(find(e, e.u32(t + 8), type_))


def find_next(e):
    c, type_ = e.arg(0), e.arg(1)
    LOG[-1].append(("FindNext", e.normalize(c), type_ & 0xFFFF)) if LOG else None
    e.ret(find(e, e.u32(c), type_))


pyfake(emu, 0x401BE0, find_child)
pyfake(emu, 0x401C40, find_next)


def hits(e):
    lists, counts, n = e.arg(0), e.arg(1), e.arg(2)
    LOG[-1].append(("hits",)) if LOG else None
    for i, (p, c) in enumerate(DATA["lists"]):
        u32(e, lists + 4 * i, p)
        u32(e, counts + 4 * i, c)
    u32(e, n, len(DATA["lists"]))
    e.ret(0)


pyfake(emu, 0x4F8140, hits)


def coll_lines(e):
    a = [e.arg(i) for i in range(8)]
    if LOG:
        LOG[-1].append(("Coll_Lines", a[0], e.read(a[2], 48), e.read(a[3], 48), a[4], a[5] & 0xFF,
                        e.read(a[6], 8), a[7]))
    for i, (found, dist) in enumerate(DATA["echo"]):
        u32(e, a[1] + 0x70 * i + 0x58, found)
        d64(e, a[1] + 0x70 * i + 0x60, dist)
    e.ret(0)


pyfake(emu, 0x4CC2A0, coll_lines)
emu.baseline = emu.snapshot()


def listener(e, a=0.6, b=-0.8):
    p = e.alloc(0x40)
    f32(e, p + 8, a)
    f32(e, p + 0x14, b)
    d64(e, p + 0x28, 1000.5, -2000.25, 300.0)
    u32(e, 0x57BDAC, p)
    return p


def env(e, echo=0, surfaces=0, volume=0.5, sound=0, left=(0.0,) * 4, right=(0.0,) * 4):
    t = task(e, 0x5C, ENV)
    e.write(t + 0x20, bytes([echo, surfaces, 0, 0]))
    e.write(t + 0x24, b"rain_2\0")
    f32(e, t + 0x34, volume)
    u32(e, t + 0x38, sound)
    f32(e, t + 0x3C, *left, *right)
    return t


def sound(e, parent, name, volume, type_=SOUND):
    s = task(e, 0x120, type_)
    f32(e, s + 0xC8, volume)
    e.write(s + 0xFF, name + b"\0")
    add_child(e, parent, s)
    return s


def test_create():
    run(emu, 0x436C60, "EnvironmentSound_Create", lambda e: (env(e, 1, 1, sound=0x7777),))


@pytest.mark.parametrize("old", [0, 0x7777])
def test_reset(old):
    def setup(e):
                return (env(e, 1, 1, sound=old, left=(0.5, 0.25, 1, 0), right=(1, 1, 1, 1)),)
    run(emu, 0x436CB0, "EnvironmentSound_Reset", setup)
    run(emu, 0x4374D0, "EnvironmentSound_Changed", setup)


def test_get_params():
    run(emu, 0x4374C0, "EnvironmentSound_GetParams", lambda e: (0, e.alloc(4)))


def test_set_listener():
    run(emu, 0x437500, "EnvironmentSound_SetListener", lambda e: (0x12345678,))


@pytest.mark.parametrize("r", [0.0, 1.0, float("nan")])
@pytest.mark.parametrize("volume", [0.0, 0.01, 0.5, 0.99, 1.0, 1.5, -0.5, float("nan")])
def test_update_ambient(r, volume):
    def setup(e):
        RET[0x4F80F0] = r
        listener(e)
        return (env(e, volume=volume, sound=0x7777),)
    run(emu, 0x4370A0, "EnvironmentSound_UpdateAmbient", setup)


ECHO = [[(0, 0.0), (0, 0.0)], [(1, 100.0), (1, 9000.0)], [(1, 4096.0 * 2.5), (1, 4096.0 * 3.5)],
        [(1, 4096.0 * 4.6), (1, 4096.0 * 9.9)], [(1, 40960.0), (1, float("nan"))], [(1, 40959.0), (0, 5.0)]]


@pytest.mark.parametrize("echo", ECHO)
@pytest.mark.parametrize("dirs", [(0.6, -0.8), (0.0, 1.0), (-0.0, 0.0)])
@pytest.mark.parametrize("levels", [((0.0,) * 4, (0.0,) * 4), ((0.5, 0.005, 1.0, 0.9999), (0.3, 1.0, 0.01, 0.2))])
def test_update_echo(echo, dirs, levels):
    def setup(e):
        DATA["echo"] = echo
        listener(e, *dirs)
        return (env(e, left=levels[0], right=levels[1]),)
    run(emu, 0x436DA0, "EnvironmentSound_UpdateEcho", setup)


def material(e, name):
    m = e.alloc(0x180)
    e.write(m + 0x134, name + b"\0")
    return m


def hit_list(e, rng, mats, n):
    p = e.alloc(0x70 * max(n, 1))
    for i in range(n):
        h = p + 0x70 * i
        d64(e, h + 8, rng.uniform(-1e4, 1e4), rng.uniform(-1e4, 1e4), rng.uniform(-1e3, 1e3))
        u32(e, h + 0x4C, rng.choice(mats + [0]))
        u32(e, h + 0x58, rng.choice([0, 1, 1, 1]))
    return p


@pytest.mark.parametrize("seed", range(12))
def test_update_surfaces(seed):
    def setup(e):
        rng = random.Random(seed)
        t = env(e, sound=0)
        mats = [material(e, n) for n in (b"wood", b"metal", b"grass", b"water")]
        own = sound(e, t, b"rain_2", 0.5)
        u32(e, t + 0x38, own)
        for name in rng.sample([b"wood", b"metal", b"stone", b"water"], rng.randint(0, 3)):
            sound(e, t, name, rng.choice([0.0, 0.01, 0.5, 0.95, 1.0]))
        task(e, 0x40, OTHER)
        add_child(e, t, task(e, 0x120, OTHER))
        DATA["lists"] = [(hit_list(e, rng, mats, c), c) for c in
                         [rng.choice([0, 1, 3, 8, 20]) for _ in range(rng.randint(0, 4))]]
        RET[0x4E6B00] = rng.choice([0, 1])
        return (t,)
    run(emu, 0x437150, "EnvironmentSound_UpdateSurfaces", setup)


@pytest.mark.parametrize("flags", [(0, 0), (1, 0), (0, 1), (1, 1)])
@pytest.mark.parametrize("children", [0, 2])
def test_run(flags, children):
    def setup(e):
        RET[0x4F80F0] = 1.0
        DATA["echo"] = ECHO[1]
        DATA["lists"] = []
        listener(e)
        t = env(e, *flags, sound=0x7777)
        for i in range(children):
            add_child(e, t, task(e, 0x40, OTHER if i else SOUND))
        return (t,)
    run(emu, 0x436D10, "EnvironmentSound_Run", setup)
