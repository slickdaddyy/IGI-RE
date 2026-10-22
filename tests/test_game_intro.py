import struct

import pytest

from emu import Emu, ErrorShown, compare
from test_game_stubs import PyFake, fakes, u32

emu = Emu()

TYPE = 0x57B198
KEYS = 0xC28C62

F = fakes(emu, {
    0x4018F0: (0, 7), 0x401530: 3, 0x4B12F0: 2, 0x4E68D0: 1, 0x4E69C0: 1, 0x4B13A0: 1,
    0x4B8890: 4, 0x4B80B0: (1, 0x5100), 0x4B85B0: 1, 0x4B83D0: 1, 0x4B8920: 1,
    0x4B5B60: (1, 1), 0x4B5F00: (2, 0x6000), 0x491610: 2, 0x4905A0: (3, 0x7000),
    0x490D10: (1, 1), 0x4903B0: 1, 0x4028A0: 1, 0x4015F0: 1, 0x4B63D0: 1, 0x48F2D0: (0, 0),
})
PyFake(emu, 0x401900, log=(0, 1, ("w", 2), 3), ret=0x1234)
PyFake(emu, 0x401400, log=(("w", 0), ("b", 1), 2))
PyFake(emu, 0x401A20, log=(("w", 0),))
PyFake(emu, 0x4012A0, log=(0, ("w", 1), (2, 2), lambda e: e.read(e.arg(2) + 8, 1)[0]), ret=0x8000)
PROFILE = emu.alloc(0x600)
GET_PROFILE = PyFake(emu, 0x406220, ret=PROFILE)

# script arguments: ints are 100 + i, reals 0.5 + 1.25 * i, strings "arg<i>"
PyFake(emu, 0x4B8A20, log=(0, 1), ret=lambda e: 100 + e.arg(1))
PyFake(emu, 0x4B8A50, log=(0, 1), fret=lambda e: 0.5 + 1.25 * e.arg(1))


def get_string(e):
    e.write(e.arg(2), b"arg%d\0" % e.arg(1))


PyFake(emu, 0x4B8A80, log=(0, 1, 3), fn=get_string, ret=lambda e: e.arg(2))
PyFake(emu, 0x491240, log=(0, 1, lambda e: list(struct.unpack("<4I", e.read(e.arg(2), 16)))))


def subtitle(e):
    d = e.arg(1)
    words = [e.u32(d + 4 * j) for j in range(1, 13)] + [e.read(d + 0x34, 1)[0]]
    return words + list(struct.unpack("<4I", e.read(e.u32(d), 16)))


PyFake(emu, 0x491280, log=(0, subtitle))
emu.baseline = emu.snapshot()

OUT = emu.alloc(24)
IGNORE = [(OUT + 4, OUT + 8), (OUT + 20, OUT + 24)]


def test_init():
    compare(emu, 0x415070, "IntroScreen_Init", check_eax=False, normalized=True)


@pytest.mark.parametrize("script", [0x5100, 0])
def test_load_subtitles(script):
    def setup(e):
        F[0x4B80B0].ret = script
        p = e.alloc(16)
        e.write(p, b"intro.qsc\0")
        return 0x5555, p
    compare(emu, 0x415170, "IntroScreen_LoadSubtitles", setup=setup, eax_mask=0xFF, normalized=True)


def intro(e, count=3, index=1, movie=0x7000, subs=0x6000, loop=0):
    s = e.alloc(0x38)
    movies = e.alloc(16)
    e.write(movies, u32(0x9000) + u32(0x9100) + u32(0x9200) + u32(0x9300))
    e.write(s + 0x20, u32(count) + u32(index) + u32(movies) + u32(movie) + u32(subs) + bytes([loop]))
    return s


@pytest.mark.parametrize("exists", [0, 1])
@pytest.mark.parametrize("profile", [0, 1])
@pytest.mark.parametrize("loop", [0, 1])
def test_create(exists, profile, loop):
    def setup(e):
        F[0x4B5B60].ret = exists
        GET_PROFILE.ret = PROFILE if profile else 0
        s = e.alloc(0x38)
        e.write(s, b"\x55" * 0x38)
        movies = e.alloc(8)
        e.write(movies, u32(0x9000) + u32(0x9100))
        a = e.alloc(12)
        e.write(a, u32(2) + u32(movies) + bytes([loop, 0x11, 0x22, 0x33]))
        return s, a
    compare(emu, 0x4150E0, "IntroScreen_Create", setup=setup, check_eax=False, normalized=True)


def test_register_font():
    compare(emu, 0x4151E0, "MovieSubtitle_RegisterFont", args=(OUT, 0x5555, 0x5A00), ignore=IGNORE,
            normalized=True)


def test_register_text():
    compare(emu, 0x415260, "MovieSubtitle_RegisterText", args=(OUT, 0x5555, 0x5A00), ignore=IGNORE,
            normalized=True)


@pytest.mark.parametrize("movie", [0, 0x7000])
@pytest.mark.parametrize("playing", [0, 1])
@pytest.mark.parametrize("esc", [0, 1])
@pytest.mark.parametrize("count,index,loop", [(3, 1, 0), (3, 3, 0), (3, 3, 1), (1, 1, 1), (0, 0, 0)])
@pytest.mark.parametrize("opened", [0x7100, 0])
def test_run(movie, playing, esc, count, index, loop, opened):
    def setup(e):
        F[0x490D10].ret = playing
        F[0x4905A0].ret = opened
        e.write(KEYS + 1, b"\x80" if esc else b"\x7f")
        return (intro(e, count, index, movie, 0x6000, loop),)
    compare(emu, 0x415380, "IntroScreen_Run", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("movie,subs", [(0, 0), (0x7000, 0), (0, 0x6000), (0x7000, 0x6000)])
def test_delete(movie, subs):
    compare(emu, 0x415450, "IntroScreen_Delete", setup=lambda e: (intro(e, movie=movie, subs=subs),),
            check_eax=False, normalized=True)


def test_shutdown():
    compare(emu, 0x415480, "IntroScreen_Shutdown", setup=lambda e: e.write(TYPE, struct.pack("<H", 0x123)),
            check_eax=False, normalized=True)


@pytest.mark.parametrize("mode,usa", [(0, 0), (0, 1), (1, 0)])
@pytest.mark.parametrize("loop", [0, 1])
def test_new(mode, usa, loop):
    def setup(e):
        F[0x48F2D0].ret = usa
        e.write(TYPE, struct.pack("<H", 0x123))
        return 0x4400, mode, loop
    compare(emu, 0x4154B0, "IntroScreen_New", setup=setup, normalized=True)


@pytest.mark.parametrize("mode", [2, -1])
def test_new_bad_mode(mode):
    for fn in (0x4154B0, "IntroScreen_New"):
        emu.reset()
        with pytest.raises(ErrorShown, match="Invalid introscreen mode"):
            emu.call(fn, 0x4400, mode, 0)
