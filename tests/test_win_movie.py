import struct

import pytest

from emu import Emu
from test_win_util import LOG, api, check, check_raises, com, cstr, logged, pyfake, trap, u32

emu = Emu()

M = 0x5C8E48
PAUSE_START = 0x5C8E40
APP = 0x5C8BC0
BACK = 0x5CA100
DDRAW = 0x5CA10C
WINPROCS = 0x5C8D28
WINPROC_LIST = 0x5C8DF0
MODES = 0xBCAE20
MODE_COUNT = 0xC10384
METHODS = 0x542310
RENDER_FUNCS = 0xA94E84
PREF = 0x5C9F9C

TICK = [5000]
FAIL_ALLOC = [None]
ALLOCS = [0]
SOUND = [0, 0]


def f32(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def alloc(e):
    ALLOCS[0] += 1
    if FAIL_ALLOC[0] == ALLOCS[0]:
        return 0
    return e.alloc(e.arg(0))


def text_new(e):
    t = e.alloc(0x28)
    u32(e, t + 0x24, 0x1001)
    return (t, e.read_cstr(e.arg(3)))


def resource_load(e):
    data = cstr(e, b"Loaded subtitle text")
    if e.arg(1):
        u32(e, e.arg(1), 0x30)
    return data


FAKES = [
    (0x4B0C60, "Mem_Alloc", 2, alloc), (0x4B0D10, "Mem_Free", 1, None),
    (0x4B1020, "File_ResolvePath", 3, lambda e: (e.write(e.arg(0), b"res:" + e.read_cstr(e.arg(1)) + b"\0"), 0)[1]),
    (0x4B5F00, "Resource_Load", 2, resource_load), (0x4B63D0, "Resource_Flush", 1, None),
    (0x4B64E0, "Resource_Find", 1, lambda e: (int(b"sub" in e.read_cstr(e.arg(0))), e.read_cstr(e.arg(0)))),
    (0x4B6EC0, "Text_New", 4, text_new), (0x4B6F30, "Text_Free", 1, None), (0x4B6F50, "Text_DrawAlt", 1, None),
    (0x4B7000, "Sprite_New", 1, lambda e: (0x7700, e.read_cstr(e.arg(0)))), (0x4B7030, "Sprite_Delete", 1, None),
    (0x4B7050, "Sprite_StringWidth", 2, lambda e: (10 * len(e.read_cstr(e.arg(1))), e.read_cstr(e.arg(1)))),
    (0x4B7E00, "Text_GetHeight", 1, lambda e: 13),
    (0x4950E0, "sub_4950E0", 0, lambda e: 2), (0x495CB0, "sub_495CB0", 0, lambda e: SOUND[0]),
    (0x495CC0, "Sound_SetUpdateRate", 1, None), (0x496650, "sub_496650", 1, None),
    (0x4966E0, "sub_4966E0", 1, lambda e: SOUND[1]), (0x496770, "sub_496770", 4, lambda e: SOUND[0] * 0x6600),
    (0x496E40, "sub_496E40", 1, None), (0x496E60, "sub_496E60", 2, None),
    (0x497390, "sub_497390", 2, lambda e: (e.write(e.arg(0), b"snd/" + e.read_cstr(e.arg(1)) + b"\0"),
                                           (0, e.read_cstr(e.arg(1))))[1]),
    (0x497410, "sub_497410", 1, lambda e: (0x41, e.read_cstr(e.arg(0)))),
    (0x497450, "sub_497450", 1, lambda e: (0x45, e.read_cstr(e.arg(0)))),
    (0x4974A0, "sub_4974A0", 1, None), (0x497570, "sub_497570", 2, None), (0x4975F0, "sub_4975F0", 1, None),
    (0x497620, "sub_497620", 2, None),
    (0x497680, "sub_497680", 2, lambda e: (0, e.read(e.arg(1), 16))),
    (0x497700, "sub_497700", 0, None), (0x4977F0, "sub_4977F0", 0, None),
    (0x4B0F30, "RenderMode_Select", 1, None),
]
for addr, name, n, before in FAKES:
    pyfake(emu, addr, name, n, before=before)

api(emu, "GetTickCount", 0, lambda: TICK[0])
for name, n, ret in [("AVIFileInit", 0, 0), ("AVIFileExit", 0, 0), ("AVIFileRelease", 1, 0),
                     ("AVIStreamRelease", 1, 0), ("ICClose", 1, 0), ("ICSendMessage", 4, 0),
                     ("GetDC", 1, 0x3D), ("ReleaseDC", 2, 1), ("GetSystemMetrics", 1, 1024),
                     ("GetWindowLongA", 2, 0), ("AdjustWindowRect", 3, 1), ("SetWindowPos", 7, 1),
                     ("UpdateWindow", 1, 1), ("SetFocus", 1, 0)]:
    api(emu, name, n, ret)


def reset_globals(depth=16):
    TICK[0] = 5000
    FAIL_ALLOC[0] = None
    ALLOCS[0] = 0
    SOUND[:] = [0, 0]
    api(emu, "GetDeviceCaps", 2, depth)
    api(emu, "AVIStreamRead", 7, 0)
    emu.stub("ICDecompress", logged("ICDecompress", 6, pop=0))


class Parity:
    """compare() runs setup once for the original, then once for ours."""
    def __init__(self):
        self.n = 0

    def ours(self):
        self.n += 1
        return self.n % 2 == 0


def winprocs(e, fn=None):
    """An empty WinProc callback list, or one holding fn."""
    e.write(WINPROCS, b"\0" * 12 * 16)
    if fn:
        e.write(WINPROCS, struct.pack("<3I", WINPROC_LIST + 4, WINPROC_LIST, fn))
        e.write(WINPROC_LIST, struct.pack("<3I", WINPROCS, 0, WINPROCS))
    else:
        e.write(WINPROC_LIST, struct.pack("<3I", WINPROC_LIST + 4, 0, WINPROC_LIST))


def winproc_cb(e, parity):
    return e.symbols["Movie_WinProcCB"] if parity.ours() else 0x490530


def put(e, off, *vals, fmt="I"):
    e.write(M + off, struct.pack("<%d%s" % (len(vals), fmt), *[v & 0xFFFFFFFF if fmt == "I" else v for v in vals]))


def fmt_header(e, p, w=320, h=240, bits=24, size=0):
    e.write(p, struct.pack("<IiiHHIIiiII", 0x28, w, h, 1, bits, 0, size, 0, 0, 0, 0))


def out_format(e, w=640, h=-480, bits=16):
    p = e.alloc(0x34)
    fmt_header(e, p, w, h, bits)
    put(e, 0x154, p)
    return p


# --- simple ones --------------------------------------------------------------

def test_init():
    reset_globals()
    check(emu, 0x490380, "Movie_Init", setup=lambda e: e.write(M, b"\x5a" * 0x1150), check_eax=False)


def test_shutdown():
    check(emu, 0x4903A0, "Movie_Shutdown", check_eax=False)


@pytest.mark.parametrize("msg", [6, 5])
@pytest.mark.parametrize("wparam", [0, 1, 2, 0x10000, 0x10001])
@pytest.mark.parametrize("paused", [0, 4000])
def test_winproc_cb(msg, wparam, paused):
    reset_globals()

    def setup(e):
        u32(e, PAUSE_START, paused)
        put(e, 8, 300)
        return (0x1234, msg, wparam, 0)
    check(emu, 0x490530, "Movie_WinProcCB", setup=setup)


@pytest.mark.parametrize("sound,playing", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_is_playing(sound, playing):
    check(emu, 0x491220, "Movie_IsPlaying", setup=lambda e: e.write(M + 0x28, bytes([playing, sound])))


@pytest.mark.parametrize("index", [-1, 0, 3, 4])
@pytest.mark.parametrize("name", [True, False])
@pytest.mark.parametrize("movie", [True, False])
@pytest.mark.parametrize("taken", [False, True])
def test_set_font(index, name, movie, taken):
    def setup(e):
        if taken:
            put(e, 0x1B8 + 4 * max(index, 0), 0x5500)
        return (M if movie else 0, index, cstr(e, b"font_big") if name else 0)
    check(emu, 0x491240, "Movie_SetFont", setup=setup, check_eax=False)


# --- close --------------------------------------------------------------------

@pytest.mark.parametrize("subs", [False, True])
@pytest.mark.parametrize("sound,playing", [(0, 0), (0x6600, 0), (0x6600, 1)])
@pytest.mark.parametrize("handles", [False, True])
@pytest.mark.parametrize("winproc", [False, True])
def test_close(subs, sound, playing, handles, winproc):
    reset_globals()
    parity = Parity()

    def setup(e):
        winprocs(e, winproc_cb(e, parity) if winproc else None)
        e.write(M, b"\x11" * 0x1150)
        e.write(M + 0x28, bytes([1, playing, int(winproc), 0]))
        put(e, 0x158, sound)
        e.write(M + 0x168, bytes([int(subs)]))
        put(e, 0x1148, 3 if subs else 0)
        for i in range(3):
            put(e, 0x1C8 + 0x7C * i + 0x24, i)
            put(e, 0x1C8 + 0x7C * i + 0x34, 0x8000 + i)
            for j in range(i):
                put(e, 0x1C8 + 0x7C * i + 0x58 + 4 * j, 0x9000 + 16 * i + j)
        put(e, 0x1B8, 0, 0x5501, 0, 0x5503)
        for off in (0x160, 0x164, 0x154, 0x15C, 0x9C, 0x2C):
            put(e, off, 0x3000 + off if handles else 0)
        return (M,)
    check(emu, 0x4903B0, "Movie_Close", setup=setup, check_eax=False)


# --- open file ----------------------------------------------------------------

def avi_stubs(e, fail=None, streams=(b"auds", b"vids"), size=0x28):
    def file_open(e):
        u32(e, e.arg(0), 0xF11E)
        return (0x80040000 if fail == "open" else 0, e.read_cstr(e.arg(1)))

    def file_info(e):
        e.write(e.arg(1), bytes(range(0x6C)))
        u32(e, e.arg(1) + 0xC, len(streams))
        return 1 if fail == "info" else 0

    def get_stream(e):
        i = e.arg(3)
        u32(e, e.arg(1), 0 if streams[i] is None else 0x5700 + i)
        return 0

    def stream_info(e):
        i = e.arg(0) - 0x5700
        info = bytearray(range(0x10, 0x10 + 0x8C))
        info[0:4] = streams[i]
        info[0x14:0x18] = struct.pack("<I", 1000)
        info[0x18:0x1C] = struct.pack("<I", 15000)
        info[0x20:0x24] = struct.pack("<I", 450 + i)
        e.write(e.arg(1), bytes(info))
        return 1 if fail == "sinfo" else 0

    def read_format(e):
        if e.arg(2):
            fmt_header(e, e.arg(2), 320, 240, 24, 0)
            return 1 if fail == "format" else 0
        u32(e, e.arg(3), size)
        return 0
    api(e, "AVIFileOpenA", 4, before=file_open)
    api(e, "AVIFileInfoA", 3, before=file_info)
    api(e, "AVIFileGetStream", 4, before=get_stream)
    api(e, "AVIStreamInfoA", 3, before=stream_info)
    api(e, "AVIStreamReadFormat", 4, before=read_format)


@pytest.mark.parametrize("fail,streams,size", [
    (None, (b"auds", b"vids"), 0x28), ("open", (b"vids",), 0x28), ("info", (b"vids",), 0x28),
    (None, (b"auds", None), 0x28), (None, (), 0x28), ("sinfo", (b"vids",), 0x28), (None, (b"vids",), 0x2C),
    ("format", (b"vids",), 0x28), (None, (b"vids", b"auds", b"vids"), 0x28),
])
def test_open_file(fail, streams, size):
    reset_globals()

    def setup(e):
        winprocs(e)
        avi_stubs(e, fail, streams, size)
        return (M, cstr(e, b"movies:intro.avi"))
    check(emu, 0x490680, "Movie_OpenFile", setup=setup, eax_mask=0xFF)


# --- video setup --------------------------------------------------------------

def display(e, count=1, w=640, h=480, depth=16):
    for i in range(10):
        u32(e, METHODS + 4 * i, i + 1)
    for i in range(16):
        u32(e, RENDER_FUNCS + 4 * i, trap(e, "rf%d" % i, logged("rf%d" % i, 1, pop=0)))
    for i in range(count):
        p = MODES + 0x11C * i
        e.write(p, struct.pack("<5i", 0, w, h, 1, depth) + b"\0\0\1\0" + struct.pack("<i", 2))
    u32(e, MODE_COUNT, count)


def pixel_format(fail):
    def before(e):
        p = e.arg(1)
        e.write(p + 0xC, struct.pack("<4I", 16, 0xF800, 0x7E0, 0x1F))
        return (0x88760000 if fail else 0, e.read(p, 8))
    return before


@pytest.mark.parametrize("fullscreen,depth", [(1, 16), (0, 8), (0, 16), (0, 24), (0, 32)])
@pytest.mark.parametrize("width", [320, 640])
def test_init_video(fullscreen, depth, width):
    run_init_video(fullscreen, depth, width)


@pytest.mark.parametrize("fail", ["modes", "back", "locate", "begin", "alloc1", "alloc2"])
def test_init_video_fail(fail):
    run_init_video(0, 16, 320, fail)


def test_init_video_pref():
    run_init_video(1, 16, 320, pref=True)


def test_init_video_pixel_format_error():
    reset_globals()

    def setup(e):
        u32(e, APP + 0x40, 1)
        display(e, 1, 640, 480)
        fmt_header(e, M + 0x12C, 320, 240, 24, 100)
        u32(e, BACK, com(e, "back", {21: (2, 0, pixel_format(True))}))
        return (M,)
    check_raises(emu, 0x490860, "Movie_InitVideo", setup=setup)


def run_init_video(fullscreen, depth, width, fail=None, pref=False):
    reset_globals(depth)

    def setup(e):
        ALLOCS[0] = 0
        FAIL_ALLOC[0] = {"alloc1": 2, "alloc2": 3}.get(fail)
        winprocs(e)
        u32(e, APP + 0x40, fullscreen)
        scale = 2 if width < 640 else 1
        display(e, 0 if fail == "modes" else 1, width * scale, 240 * scale, depth if not fullscreen else 16)
        if pref:
            u32(e, PREF, cstr(e, b"Some card"))
            u32(e, PREF + 4, cstr(e, b"dev0"))
        fmt_header(e, M + 0x12C, width, 240, 24, 100)
        put(e, 0xA4, 0x63766964)
        u32(e, BACK, 0 if fail == "back" else com(e, "back", {21: (2, 0, pixel_format(False))}))
        api(e, "ICLocate", 5, 0 if fail == "locate" else 0x1C00)
        api(e, "ICSendMessage", 4, 1 if fail == "begin" else 0)
        return (M,)
    check(emu, 0x490860, "Movie_InitVideo", setup=setup, eax_mask=0xFF)


# --- sound and subtitles ------------------------------------------------------

@pytest.mark.parametrize("enabled", [0, 1])
def test_open_sound(enabled):
    reset_globals()

    def setup(e):
        SOUND[0] = enabled
        return (M, cstr(e, b"movies:intro.avi"))
    check(emu, 0x490B80, "Movie_OpenSound", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("ok", [0, 1, 0x100])
def test_load_subtitles(ok):
    reset_globals()

    def setup(e):
        out_format(e, 512, -384)
        load = trap(e, "load", logged("load", 2, ok, pop=0, before=lambda e: (ok, e.read_cstr(e.arg(1)))))
        return (M, cstr(e, b"movies:intro.avi"), load)
    check(emu, 0x490C40, "Movie_LoadSubtitles", setup=setup, eax_mask=0xFF)


# --- playback -----------------------------------------------------------------

def playing_movie(e, frame=-1, drawn=-1, start=1000, paused=0, duration=30000, sound=0, sound_frames=0):
    put(e, 0, 0, start, paused, duration, 0, sound_frames, 0, frame, drawn)
    e.write(M + 0x28, b"\1\0\1\0")
    put(e, 0x9C, 0x5701)
    put(e, 0xB4, 1000, 15000)
    fmt_header(e, M + 0x12C, 320, 240, 24, 0x1000)
    out_format(e, 640, -480, 16)
    put(e, 0x158, sound)
    put(e, 0x15C, 0x1C00, 0x6000, 0x7000)
    u32(e, BACK, com(e, "back", {21: (2, 0, pixel_format(False)), 5: (6, 0)}))
    u32(e, DDRAW, com(e, "ddraw", {6: (4, 0, create_surface(e, False))}))


def create_surface(e, fail):
    surf = com(e, "surf", {})

    def before(e):
        u32(e, e.arg(2), surf)
        return (0x88760000 if fail else 0, e.read(e.arg(1), 0x7C))
    return before


@pytest.mark.parametrize("read,dec", [(0, 0), (1, 0), (0, 1)])
@pytest.mark.parametrize("flags", [0, 0x80000000])
def test_read_frame(read, dec, flags):
    reset_globals()

    def setup(e):
        playing_movie(e)
        api(e, "AVIStreamRead", 7, read)
        e.stub("ICDecompress", logged("ICDecompress", 6, dec, pop=0))
        return (M, 17, flags)
    check(emu, 0x490E90, "Movie_ReadFrame", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("fail", [None, "format", "create"])
def test_blit_frame(fail):
    reset_globals()

    def setup(e):
        playing_movie(e)
        if fail == "format":
            u32(e, BACK, com(e, "back", {21: (2, 0, pixel_format(True))}))
        if fail == "create":
            u32(e, DDRAW, com(e, "ddraw", {6: (4, 0, create_surface(e, True))}))
        return (M,)
    if fail:
        check_raises(emu, 0x4910C0, "Movie_BlitFrame", setup=setup)
    else:
        check(emu, 0x4910C0, "Movie_BlitFrame", setup=setup, check_eax=False)


def subtitle(e, i, start, duration, fade_in, fade_out, lines=2, in_on=1, out_on=1):
    base = M + 0x1C8 + 0x7C * i
    e.write(base, struct.pack("<10i", 1, 20, 400, in_on, out_on, start, duration, fade_in, fade_out, lines))
    e.write(base + 0x28, struct.pack("<3f", 1.0, 0.5, 0.3))
    for j in range(lines):
        t = e.alloc(0x28)
        u32(e, t + 0x24, 0x100 * j)
        u32(e, base + 0x58 + 4 * j, t)


@pytest.mark.parametrize("time", [0, 500, 999, 1000, 1200, 2999, 3000, 3700, 4000, 4001, 9000])
@pytest.mark.parametrize("in_on,out_on", [(1, 1), (-1, 1), (1, -1), (-1, -1)])
def test_draw_subtitles(time, in_on, out_on):
    reset_globals()

    def setup(e):
        playing_movie(e, frame=5, drawn=4)
        put(e, 0, time)
        e.write(M + 0x168, b"\1")
        put(e, 0x1148, 2, 0)
        subtitle(e, 0, 1000, 2000, 700, 1000, 2, in_on, out_on)
        subtitle(e, 1, 5000, 1000, 0, 0, 1)
        return (M,)
    check(emu, 0x490F10, "Movie_Draw", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("drawn,subs,cur", [(5, 1, 0), (4, 0, 0), (4, 1, 2)])
def test_draw(drawn, subs, cur):
    reset_globals()

    def setup(e):
        playing_movie(e, frame=5, drawn=drawn)
        e.write(M + 0x168, bytes([subs]))
        put(e, 0x1148, 2, cur)
        return (M,)
    check(emu, 0x490F10, "Movie_Draw", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("start,now,frame,sf", [
    (0, 1000, -1, 0), (1000, 1000, -1, 0), (1000, 1100, -1, 0), (1000, 1100, 0, 6), (1000, 1100, 1, 0),
    (1000, 1200, 0, 0), (1000, 1500, 2, 3), (1000, 40000, 3, 2300), (1000, 1066, 0, 0),
])
@pytest.mark.parametrize("sound,sound_playing", [(0, 0), (0x6600, 0), (0x6600, 1)])
def test_update(start, now, frame, sf, sound, sound_playing):
    reset_globals()

    def setup(e):
        TICK[0] = now
        SOUND[1] = sound_playing
        playing_movie(e, frame=frame, drawn=frame, start=start, paused=0, sound=sound, sound_frames=sf)
        e.write(M + 0x29, bytes([sound_playing]))
        api(e, "AVIStreamRead", 7, 0)
        return (M,)
    check(emu, 0x490D10, "Movie_Update", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("frame", [-1, 1, 3])
def test_update_read_fails(frame):
    reset_globals()

    def setup(e):
        TICK[0] = 1500
        playing_movie(e, frame=frame, drawn=frame)
        api(e, "AVIStreamRead", 7, 1)
        return (M,)
    check(emu, 0x490D10, "Movie_Update", setup=setup, eax_mask=0xFF)


# --- subtitle text ------------------------------------------------------------

def sub_def(e, text, font=1, centered=0, times=(1.0, 2.5, 0.6, 1.4)):
    p = e.alloc(0x38)
    e.write(p, struct.pack("<6I", cstr(e, text), font, 30, 400, 1, 0xFFFFFFFF))
    e.write(p + 0x18, struct.pack("<7f", 1.0, 0.75, 0.5, *times))
    e.write(p + 0x34, bytes([centered]))
    return p


@pytest.mark.parametrize("text", [b"Hello", b"sub_intro_1", b"x" * 130 + b"sub"])
@pytest.mark.parametrize("centered", [0, 1])
@pytest.mark.parametrize("times", [(1.0, 2.5, 0.6, 1.4), (1.5, 2.5, -0.5, 3.49)])
def test_add_subtitle(text, centered, times):
    reset_globals()

    def setup(e):
        out_format(e, 640, -480)
        put(e, 0x1B8, 0x7701, 0x7702)
        put(e, 0x1148, 3)
        return (M, sub_def(e, text, 1, centered, times))
    check(emu, 0x491280, "Movie_AddSubtitle", setup=setup, check_eax=False)


TEXTS = [
    b"", b"short", b"one two three four five six seven eight nine ten eleven twelve",
    b"line one\nline two\r\nline three", b"averyveryveryverylongwordthatdoesnotfit and more",
    b"a b c d e f g h i j k l m n o p q r s t u v w x y z a b c d e f g h i j k l m n o p",
    b"\n\n\n\n\n\n\n\n\n\n", b"x\ny\nz w v u t s r q p o n m l k j i h g f e d c b a",
]


@pytest.mark.parametrize("text", TEXTS)
@pytest.mark.parametrize("centered,x", [(0, 30), (1, 30), (0, 500)])
def test_layout_subtitle(text, centered, x):
    reset_globals()

    def setup(e):
        out_format(e, 640, -480)
        put(e, 0x1B8, 0x7701, 0x7702)
        sub = M + 0x1C8 + 0x7C
        e.write(sub, struct.pack("<3i", 1, x, 400))
        e.write(sub + 0x28, struct.pack("<3f", 1.0, 0.75, 0.33))
        put(e, 0x1C8 + 0x7C + 0x34, cstr(e, text))
        e.write(sub + 0x78, bytes([centered]))
        return (M, sub)
    check(emu, 0x491420, "Movie_LayoutSubtitle", setup=setup, check_eax=False)


# --- the whole open -----------------------------------------------------------

@pytest.mark.parametrize("fail", [None, "file", "video", "subs", "nosubs"])
def test_open(fail):
    reset_globals()

    def setup(e):
        SOUND[0] = 1
        winprocs(e)
        u32(e, APP + 0x40, 1)
        display(e, 0 if fail == "video" else 1, 640, 480, 16)
        avi_stubs(e, "open" if fail == "file" else None)
        u32(e, BACK, com(e, "back", {21: (2, 0, pixel_format(False))}))
        api(e, "ICLocate", 5, 0x1C00)
        api(e, "ICSendMessage", 4, 0)
        out_format(e)
        load = 0
        if fail != "nosubs":
            load = trap(e, "load", logged("load", 2, 0 if fail == "subs" else 1, pop=0))
        return (cstr(e, b"movies:intro.avi"), load, 0x77)
    check(emu, 0x4905A0, "Movie_Open", setup=setup)
