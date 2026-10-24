import math
import struct

import pytest

from emu import Emu
from test_menu_util import LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, set_handler, task, trap, u16, u32

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]

# sound
FREE, ACTIVE, NEXTID, DISABLED, SILENT, TASKS, DEFS = 0xA5E5EC, 0xA5E5F4, 0xA5E5F8, 0xA5E60A, 0xA5E60B, 0xA5E60C, 0xA5E610
# screen
DRAW_EVENT, SCREEN_TYPE, NEXT_MODE, OVERLAYS, MODE, FRAME = 0x5488C8, 0xA5E618, 0xA5E620, 0xA5E740, 0xA5E940, 0xA5EA60
PENDING, OVERLAY_COUNT = 0xA5EA65, 0xA5EA68
# debug text
OWN_SCENE, DT_TASK, DT_DIRTY, DT_ON, DT_TYPE, DT_WIN, DT_FONT = 0x5488D8, 0xA5EA70, 0xA5EA74, 0xA5EA75, 0xA5EA76, 0xA5EBD0, 0xA5EBD4
# viewport
VP_CALLBACKS, VP_COUNT, VP_CURRENT, VP_DRAWN = 0xA5EBE0, 0xA5EC60, 0xA5EC68, 0xA5EC6C

e = Emu()
for addr, n, kw in [
        (0x4012A0, 3, {"masks": {1: 0xFFFF}}), (0x401400, 3, {"masks": {0: 0xFFFF, 1: 0xFF}}),
        (0x401530, 3, {"masks": {0: 0xFFFF}}), (0x401810, 1, {"masks": {0: 0xFFFF}}), (0x4018E0, 1, {}),
        (0x4018F0, 0, {}), (0x401900, 4, {"strings": (0,), "masks": {2: 0xFFFF}}), (0x401A20, 1, {"masks": {0: 0xFFFF}}),
        (0x4C1780, 0, {}), (0x4C17C0, 2, {}), (0x4C1830, 1, {}),
        (0x496650, 1, {}), (0x496770, 4, {"derefs": {3: 0x58}}), (0x496E40, 1, {}), (0x496E60, 2, {"derefs": {1: 0x58}}),
        (0x491A90, 1, {"derefs": {0: 0x11C}}), (0x491D00, 0, {}), (0x491D20, 0, {}),
        (0x491630, 1, {}), (0x4916A0, 1, {}), (0x4916C0, 1, {}), (0x4916E0, 1, {}), (0x491CF0, 0, {}), (0x48F1F0, 0, {}),
        (0x497570, 2, {}), (0x4975F0, 1, {}), (0x497620, 2, {}), (0x497680, 2, {"derefs": {1: 16}}), (0x497700, 0, {}),
        (0x497E70, 1, {"derefs": {0: 0xA8}}), (0x497EE0, 1, {"derefs": {0: 0xA8}}),
        (0x4B7330, 1, {}), (0x4B7360, 1, {}), (0x4B73A0, 1, {}),
        (0x4B6420, 3, {"strings": (0,)}), (0x4B64B0, 1, {"strings": (0,)}),
        (0x4B7000, 1, {"strings": (0,)}), (0x4B7030, 1, {}), (0x4B7630, 3, {}), (0x4B76D0, 1, {}),
        (0x4B77F0, 4, {"strings": (1,)}), (0x4B7850, 2, {}), (0x4B7930, 10, {}), (0x4B7C40, 1, {}),
        (0x4EBBA0, 2, {}), (0x4EBBF0, 3, {"derefs": {1: 40, 2: 24}}), (0x4D2650, 3, {"derefs": {1: 9}})]:
    logfake(e, addr, n, **kw)
logfake(e, 0x4966E0, 1, effect=lambda e, a: a[0] & 1)  # Voice_IsPlaying: odd voices play


def _create(e, a):
    # TransContext_Create: fill the context from its arguments
    e.write(a[0], e.read(a[1], 40) + e.read(a[2], 24) + struct.pack("<I", a[3]) + bytes(range(0xA8 - 68)))
    return 0


logfake(e, 0x497D70, 4, derefs={1: 40, 2: 24}, masks={0: 0}, effect=_create)


def with_cw(e, target, cw):
    """Entry point that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    cache = e.__dict__.setdefault("cw_tramps", {})
    if (addr, cw) not in cache:
        p = e.fake_top
        e.fake_top += 32
        e.write(p, struct.pack("<HH", cw, 0) + b"\xd9\x2d" + struct.pack("<I", p) +
                b"\xe9" + struct.pack("<i", addr - (p + 15)))
        cache[(addr, cw)] = p + 4
    return cache[(addr, cw)]


def run(orig, ours, setup=None, cw=None, **kw):
    RET.clear()
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def u8(e, p, *v):
    e.write(p, bytes(v))


H1, H2, H3, H4 = (handler(e, "vp%d" % i) for i in range(4))


def children(e, parent, types):
    kids = []
    for t in types:
        c = task(e, 0x30, t)
        add_child(e, parent, c)
        kids.append(c)
    return kids


# --- sound -------------------------------------------------------------------

def insts(e, n):
    base = e.alloc(0x78 * n)
    return [base + 0x78 * i for i in range(n)]


def link(e, head, items):
    prev = 0
    for i, p in enumerate(items):
        u32(e, p, prev, items[i + 1] if i + 1 < len(items) else 0)
        prev = p
    u32(e, head, items[0] if items else 0)


def params(e, vol, n=0x58):
    p = e.alloc(0x58)
    e.write(p, bytes((i * 7 + 3) & 0xFF for i in range(0x58)))
    f32(e, p, vol)
    return p


def sound_lists(e, nfree=2, voice=0, hash_=0x55, id_=7):
    """Two active sounds (the second one is hash_/id_ with voice) and nfree free ones."""
    s = insts(e, 2 + nfree)
    link(e, ACTIVE, s[:2])
    link(e, FREE, s[2:])
    u32(e, s[0] + 8, 1, 0x11, 0x56, 7, 3, 0x201)
    u32(e, s[1] + 8, 1, 0x12, hash_, id_, 3, voice)
    u32(e, NEXTID, 0x40)
    return s


def test_remove_task():
    def setup(e):
        u32(e, TASKS, 0x1234)
        return (0x5678,)
    run(0x4E6D40, "Sound_RemoveTask", setup)


@pytest.mark.parametrize("disabled", [0, 1])
@pytest.mark.parametrize("id_", [0, 9])
def test_handle_play(disabled, id_):
    def setup(e):
        sound_lists(e)
        u8(e, DISABLED, disabled)
        RET[0x496770] = 0x300
        h = e.alloc(0x68)
        u32(e, h, 0x13, 0x77, id_, 0x10)
        e.write(h + 0x10, e.read(params(e, 0.5), 0x58))
        return (h,)
    run(0x4E6D60, "SoundHandle_Play", setup)


@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("given", [0, 1])
@pytest.mark.parametrize("voice", [0, 0x203])
@pytest.mark.parametrize("silent", [0, 1])
@pytest.mark.parametrize("play", [0, 0x301])
@pytest.mark.parametrize("vol", [0.0, 0.75, -0.0, float("nan")])
@pytest.mark.parametrize("nfree", [0, 1])
def test_request(found, given, voice, silent, play, vol, nfree):
    if not given and vol != 0.75 or not found and voice or found and nfree:
        pytest.skip("same as another case")

    def setup(e):
        sound_lists(e, nfree, voice, 0x55 if found else 0x99)
        u8(e, SILENT, silent)
        RET[0x496770] = play
        return 7, 0x21, 0x55, 0x8003, params(e, vol) if given else 0
    run(0x4E6DB0, "SoundInst_Request", setup)


@pytest.mark.parametrize("case", range(5))
def test_handle_is_playing(case):
    def setup(e):
        s = sound_lists(e, 1, 0)
        h = e.alloc(0x68)
        u32(e, h, 0x13, 0x55, 7)
        if case == 1:
            u32(e, s[1] + 0x1C, 0x205)
        elif case == 2:
            u32(e, s[1] + 0x1C, 0x206)
        elif case == 3:
            u32(e, h + 8, 8)
        elif case == 4:
            link(e, ACTIVE, [])
        return (h,)
    run(0x4E6FA0, "SoundHandle_IsPlaying", setup, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("vol", [-1.0, -0.0, 0.0, 0.5, 1.0, 1.5, float("nan"), float("-inf")])
def test_clamp_volume(vol):
    def setup(e):
        p = params(e, vol)
        e.write(p + 0x18, struct.pack("<d", 123.456))
        return (p,)
    run(0x4E6FF0, "SoundParams_ClampVolume", setup, check_st0=True)


GRAPH = [0.0, 0.3, 0.31, 0.9, 1.7, -2.0, 0.25, 0.125, 3.3, 1e-3, 77.0]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("x", [-0.1, 0.0, 0.05, 0.1, 0.25, 0.3, 0.35, 0.45, 0.5, 0.65, 0.9, 0.95, 0.99999, 1.0, 2.0,
                               float("nan"), 1e-30])
def test_graph_eval(x, cw):
    def setup(e):
        g = e.alloc(44)
        f32(e, g, *GRAPH)
        return e.alloc(4), g, struct.unpack("<I", struct.pack("<f", x))[0]
    run(0x4E7030, "SoundGraph_Eval", setup, cw=cw)


@pytest.mark.parametrize("n", [0, 1, 2])
def test_stop_all(n):
    def setup(e):
        s = sound_lists(e, 1, 0x207)
        link(e, ACTIVE, s[:n])
    run(0x4E7130, "Sound_StopAll", setup)


@pytest.mark.parametrize("disabled", [0, 1])
@pytest.mark.parametrize("given", [0, 1])
def test_play_name(disabled, given):
    def setup(e):
        sound_lists(e)
        u8(e, DISABLED, disabled)
        RET[0x496770] = 0x300
        return cstr(e, b"explo_02_m"), 0x10, params(e, 0.5) if given else 0
    run(0x4E7180, "Sound_PlayName", setup)


def _find(e):
    a = e.arg(1)
    LOG[-1].append(("find", e.read_cstr(e.u32(a))))
    u32(e, a + 4, e.u32(0xA5E000) or e.u32(a + 4))
    e.ret(0)


FIND = trap(e, "vp_find", _find)


@pytest.mark.parametrize("case", ["ok", "disabled", "nodefs", "empty", "missing", "group", "graph", "trigger"])
@pytest.mark.parametrize("given", [0, 1])
def test_play_defined(case, given):
    def setup(e):
        sound_lists(e)
        u8(e, DISABLED, case == "disabled")
        u8(e, 0x548814, 0x30)
        u16(e, 0x54DB4A, 0x61)
        u16(e, 0x54DADA, 0x62)
        u16(e, 0x54DA62, 0x63)
        RET[0x496770] = 0x300
        types = {"group": 0x61, "graph": 0x62, "trigger": 0x63}
        found = task(e, 0x140, types.get(case, 0x60))
        e.write(found + 0xFF, b"SOUND_SAMPLE\0")
        u32(e, 0xA5E000, 0 if case == "missing" else found)
        defs = [task(e, 0x20, 0x50), task(e, 0x20, 0x51)]
        lst = e.alloc(16)
        u32(e, lst, 0 if case == "nodefs" else 2, 2, *defs)
        u32(e, DEFS, lst)
        set_handler(e, 0x30, 0x50, FIND)
        set_handler(e, 0x30, 0x51, FIND)
        return cstr(e, b"" if case == "empty" else b"mapcomp_01"), 0x20, params(e, 0.25) if given else 0
    run(0x4E7200, "Sound_PlayDefined", setup)


def test_sound_small():
    run(0x4E7330, "Sound_GetTasks", lambda e: u32(e, TASKS, 0x4321), check_eax=True)
    for v in (0, 1, 0x100):
        run(0x4E7340, "Sound_SetSilent", lambda e: (v,))
        run(0x4E7350, "Sound_SetUnusedOff", lambda e: (v,))


# --- screen ------------------------------------------------------------------

def test_screen_init():
    def setup(e):
        RET[0x401810] = 0x25
        RET[0x4018F0] = 0x1234
        RET[0x401900] = 0x77
        e.write(MODE, b"\x5a" * 0x11C)
        u8(e, PENDING, 1)
    run(0x4E7370, "Screen_Init", setup)


@pytest.mark.parametrize("pending", [0, 1])
@pytest.mark.parametrize("kids", [0, 1, 3])
@pytest.mark.parametrize("overlays", [0, 2])
def test_screen_draw(pending, kids, overlays):
    def setup(e):
        u32(e, DRAW_EVENT, 0x25)
        u8(e, PENDING, pending)
        e.write(NEXT_MODE, bytes(i & 0xFF for i in range(0x11C)))
        u32(e, FRAME, 41)
        t = task(e, 0x20, 0x40)
        children(e, t, [0x41, 0x42, 0x41][:kids])
        set_handler(e, 0x25, 0x41, H1)
        set_handler(e, 0x25, 0x43, H2)
        o = [task(e, 0x20, 0x43), task(e, 0x20, 0x43)]
        u32(e, OVERLAYS, o[0], 0, o[1], 0)
        u32(e, OVERLAY_COUNT, overlays)
        return (t,)
    run(0x4E7400, "Screen_Draw", setup)


@pytest.mark.parametrize("pending", [0, 1])
def test_screen_small(pending):
    def setup(e):
        u32(e, DRAW_EVENT, 0x25)
        u16(e, SCREEN_TYPE, 0x66)
        u8(e, PENDING, pending)
        u32(e, FRAME, 99)
        RET[0x4012A0] = 0x5555
    run(0x4E7500, "Screen_Shutdown", setup)
    run(0x4E7530, "Screen_GetDrawEvent", setup, check_eax=True)
    run(0x4E75A0, "Screen_GetFrame", setup, check_eax=True)
    run(0x4E75B0, "Screen_GetType", setup, check_eax=True, eax_mask=0xFFFF)
    run(0x4E7580, "Screen_New", lambda e: setup(e) or (0x1111,), check_eax=True)

    def mode(e):
        setup(e)
        m = e.alloc(0x11C)
        e.write(m, bytes((i * 3) & 0xFF for i in range(0x11C)))
        return m, 0x105
    run(0x4E7540, "Screen_RequestMode", mode, check_eax=True)


# --- text window and debug text -------------------------------------------

def mem(e):
    e.call(0x4B0BA0)


def grid(e, rows=4):
    g = e.alloc(0x40)
    u32(e, g, 0)
    u16(e, g + 4, 8, rows)
    return g


def textwin(e, g=None, cols=6, cur=(0, 0), visible=1, x=10, y=20):
    w = e.alloc(0x24)
    u32(e, w, g if g is not None else grid(e), visible, x, y, cols, 3, cur[0], cur[1], 0x80)
    return w


def test_textwin_small():
    run(0x4E75C0, "TextWin_SetCursor", lambda e: (textwin(e), 3, 5))
    for g in (0, None):
        run(0x4E75E0, "TextWin_Clear", lambda e: (textwin(e, g, cur=(2, 2)),))
        run(0x4E77D0, "TextWin_Scroll", lambda e: (textwin(e, g),))
    for v in (0, 1, 0xFFFFFF80, 0x100):
        run(0x4E7830, "TextWin_SetVisible", lambda e: (textwin(e), v))


@pytest.mark.parametrize("sprite", [0, 0x777])
def test_textwin_new_free(sprite):
    def setup(e):
        mem(e)
        RET[0x4B7630] = 0x888
        return sprite, 80, 60
    run(0x4E7600, "TextWin_New", setup, check_eax=True)

    def free(e):
        mem(e)
        w = e.call(0x4E7600, sprite, 80, 60)[0]
        return (w,)
    run(0x4E7660, "TextWin_Free", free)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("visible,g,x,y", [(0, None, 1, 2), (1, 0, 1, 2), (1, None, 10, -20),
                                          (5, None, 0x1234567, -0x7654321)])
def test_textwin_draw(visible, g, x, y, cw):
    run(0x4E7690, "TextWin_Draw", lambda e: (textwin(e, g, visible=visible, x=x, y=y),), cw=cw)


@pytest.mark.parametrize("cur,n,rows", [((2, 1), 0, 4), ((2, 1), 1, 4), ((2, 1), 3, 4), ((0, 3), 1, 4), ((0, 3), 2, 2)])
def test_textwin_newline(cur, n, rows):
    run(0x4E7790, "TextWin_NewLine", lambda e: (textwin(e, grid(e, rows), cur=cur), n))


@pytest.mark.parametrize("cur,n", [((0, 0), 0), ((0, 0), 3), ((4, 0), 2), ((5, 2), 14)])
def test_textwin_advance(cur, n):
    run(0x4E77F0, "TextWin_Advance", lambda e: (textwin(e, cur=cur), n))


TEXTS = [b"", b"abc", b"abcdefgh", b"ab\ncd", b"ab\ncd\n", b"\n\n", b"line one\nline two is longer\nx"]


@pytest.mark.parametrize("text", TEXTS)
@pytest.mark.parametrize("g", [0, None])
def test_textwin_print(text, g):
    run(0x4E7710, "TextWin_Print", lambda e: (textwin(e, g, cur=(1, 1)), cstr(e, text)))


def va(e, *args):
    p = e.alloc(4 * len(args))
    u32(e, p, *args)
    return p


def test_textwin_vprintf():
    def setup(e):
        return textwin(e, cur=(1, 0)), cstr(e, b"%d: %s\n%x"), va(e, 42, cstr(e, b"hello"), 0xBEEF)
    run(0x4E76E0, "TextWin_VPrintf", setup)


def debugtext(e, on=1, font=0x999, own=0, dirty=0):
    w = textwin(e, grid(e), cur=(1, 1))
    u32(e, DT_WIN, w)
    u32(e, DT_FONT, font)
    u8(e, DT_ON, on)
    u8(e, OWN_SCENE, own)
    u8(e, DT_DIRTY, dirty)
    u16(e, DT_TYPE, 0x6A)
    u32(e, DT_TASK, 0x4444)
    return w


def test_debugtext_init():
    def setup(e):
        mem(e)
        RET[0x4018F0] = 0x1234
        RET[0x401900] = 0x6A
        u32(e, DRAW_EVENT, 0x25)
        debugtext(e)
    run(0x4E7840, "DebugText_Init", setup)


@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("font", [0, 0x999])
def test_debugtext_set_mode_delete(on, font):
    for own in (0, 1):
        run(0x4E7900, "DebugText_OnSetMode", lambda e: debugtext(e, on, font, own) and None)
        run(0x4E7A60, "DebugText_Delete", lambda e: debugtext(e, on, font, own) and None)
    run(0x4E7950, "DebugText_Create", lambda e: debugtext(e, on, font) and None)


def display(e, w=640, h=480):
    m = e.alloc(0x20)
    u32(e, m, 0, w, h)
    RET[0x491CF0] = m


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("on,own,dirty,mode", [(0, 0, 1, 0), (1, 0, 0, 0), (1, 0, 1, 0), (1, 1, 1, 0), (1, 1, 1, 1),
                                               (1, 0, 1, 1)])
def test_debugtext_draw(on, own, dirty, mode, cw):
    def setup(e):
        mem(e)
        if mode:
            e.call(0x4B0D40)
        debugtext(e, on, 0x999, own, dirty)
        display(e, 0x1234567, 0x7654321)
        RET[0x4B7000] = 0x919
        RET[0x4B7630] = 0x929
    run(0x4E7960, "DebugText_Draw", setup, cw=cw)
    if on:
        run(0x4E79A0, "DebugText_DrawText", setup, cw=cw)


@pytest.mark.parametrize("own", [0, 1, 0x100])
@pytest.mark.parametrize("enabled", [0, 1])
def test_debugtext_new(own, enabled):
    def setup(e):
        debugtext(e)
        RET[0x4012A0] = 0x5151
        RET[0x48F1F0] = enabled
        return 0x2222, own
    run(0x4E7AC0, "DebugText_New", setup, check_eax=True)


def test_debugtext_shutdown():
    def setup(e):
        mem(e)
        debugtext(e)
        u32(e, DT_WIN, e.call(0x4E7600, 0x777, 80, 60)[0])
    run(0x4E7B40, "DebugText_Shutdown", setup)


@pytest.mark.parametrize("on", [0, 1])
def test_debugtext_calls(on):
    run(0x4E7B80, "DebugText_SetCursor", lambda e: debugtext(e, on) and (4, 5))
    run(0x4E7BB0, "DebugText_Clear", lambda e: debugtext(e, on) and None)
    run(0x4E7BD0, "Debug_printf", lambda e: debugtext(e, on) and (cstr(e, b"x=%d %s!"), -5, cstr(e, b"ok")))
    run(0x4E7C00, "DebugText_VPrintf", lambda e: debugtext(e, on) and (cstr(e, b"%s\n%d"), va(e, cstr(e, b"a b"), 7)))
    run(0x4E7C30, "DebugText_IsEnabled", lambda e: debugtext(e, on) and None, check_eax=True, eax_mask=0xFF)
    run(0x4E7C40, "DebugText_GetFont", lambda e: debugtext(e, on) and None, check_eax=True)


# --- camera control ----------------------------------------------------------

def camera(e, squeeze=1.0):
    c = e.alloc(0x48)
    f32(e, c, *[0.5 + i for i in range(9)])
    u32(e, c + 0x24, 0x31)
    e.write(c + 0x28, struct.pack("<3d", 100.5, -20.25, 3.0))
    f32(e, c + 0x40, 0.75, squeeze)
    return c


def test_camctrl_init():
    def setup(e):
        RET[0x4018F0] = 0x1234
        RET[0x401900] = 0x70
    run(0x4E7C50, "CameraControl_Init", setup)
    run(0x4E7D30, "CameraControl_Shutdown", lambda e: u16(e, 0x54D974, 0x70))


@pytest.mark.parametrize("kids", [0, 1, 3])
@pytest.mark.parametrize("listener", [0, 0x3333])
def test_camctrl_run(kids, listener):
    def setup(e):
        t = task(e, 0x2C, 0x70)
        u32(e, t + 0x20, camera(e), 0, listener)
        children(e, t, [0x71, 0x72, 0x71][:kids])
        set_handler(e, 1, 0x71, H1)
        set_handler(e, 1, 0x72, 0)
        return (t,)
    run(0x4E7C90, "CameraControl_Run", setup)


def test_camctrl_new():
    def setup(e):
        u16(e, 0x54D974, 0x70)
        RET[0x4012A0] = task(e, 0x2C, 0x70)
        u32(e, RET[0x4012A0] + 0x20, 9, 9, 9)
        return 0x1000, 0x2000
    run(0x4E7D40, "CameraControl_New", setup, check_eax=True)

    def listener(e):
        RET[0x4EBBA0] = 0x4545
        return 0x1000, task(e, 0x2C, 0x70), 0x77
    run(0x4E7D70, "CameraControl_AddListener", listener, check_eax=True)


# --- viewport ----------------------------------------------------------------

def test_viewport_init():
    def setup(e):
        RET[0x4018F0] = 0x1234
        RET[0x401900] = 0x72
        u32(e, DRAW_EVENT, 0x25)
        u32(e, VP_DRAWN, 3)
        e.write(VP_CALLBACKS, b"\x11" * 64)
    run(0x4E7D90, "Viewport_Init", setup)
    run(0x4E7E20, "Viewport_ResetFrame", setup)
    run(0x4E80D0, "Viewport_Shutdown", lambda e: u16(e, 0x54D98C, 0x72))
    run(0x4E8100, "Viewport_GetType", lambda e: u16(e, 0x54D98C, 0x72), check_eax=True, eax_mask=0xFFFF)


def viewport(e, x=0.1, y=0.2, w=0.7, h=0.9, squeeze=1.0, scale=1.0):
    v = task(e, 200, 0x72)
    f32(e, v + 0x20, x, y, w, h, scale)
    u32(e, v + 0x4C, 0x4C4C, camera(e, squeeze))
    u32(e, v + 0xAC, 3)
    e.write(v + 0xB0, bytes([1, 0, 1, 1, 0, 1, 1, 1, 0]))
    u32(e, v + 0xBC, 0, 1, 0x5050)
    return v


GEOMS = [(0.1, 0.2, 0.7, 0.9, 1.0, 1.0), (0.0, 0.0, 1.0, 1.0, 0.75, 1.5), (0.3333, 0.1, 0.6667, 0.55, 0.8, 0.9),
         (0.25, 0.125, 0.5, 0.625, 1.3, 1.0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("geom", range(len(GEOMS)))
@pytest.mark.parametrize("disp", [(640, 480), (801, 601), (1023, 767)])
def test_viewport_draw(geom, disp, cw):
    def setup(e):
        u32(e, DRAW_EVENT, 0x25)
        display(e, *disp)
        v = viewport(e, *GEOMS[geom])
        children(e, v, [0x41, 0x42, 0x41])
        set_handler(e, 0x25, 0x41, H1)
        set_handler(e, 0x25, 0x42, 0)
        u32(e, VP_CALLBACKS, 0, H2, 0, 0, H3)
        u32(e, VP_CALLBACKS + 14 * 4, H4)
        u32(e, VP_DRAWN, 2)
        return (v,)
    run(0x4E7E30, "ViewportQTask_DrawHandler", setup, cw=cw)


@pytest.mark.parametrize("count", [1, 2])
def test_viewport_delete(count):
    def setup(e):
        u32(e, VP_COUNT, count)
        u8(e, 0xA800C0 + 3, 1)
        return (viewport(e),)
    run(0x4E8090, "Viewport_Delete", setup)


@pytest.mark.parametrize("count", [0, 1])
@pytest.mark.parametrize("slots", [0, 3])
def test_viewport_new(count, slots):
    def setup(e):
        u16(e, 0x54D98C, 0x72)
        u32(e, VP_COUNT, count)
        u8(e, 0xA800C0, *([1] * slots))
        t = task(e, 200, 0x72)
        e.write(t + 0x20, b"\xee" * (200 - 0x20))
        RET[0x4012A0] = t
        RET[0x4B7330] = 0x6161
        fl = [struct.unpack("<I", struct.pack("<f", v))[0] for v in (0.1, 0.2, 0.3, 0.4)]
        return [0x1000] + fl + [0x4C4C, 0x5050, 0x99, 0xA0A0, 0xA4A4, 0xA8A8, 0x3F400000]
    run(0x4E8110, "Viewport_New", setup, check_eax=True)


def test_viewport_accessors():
    def setup(e):
        v = viewport(e)
        u32(e, VP_CURRENT, v)
        return (v,)
    for addr, name in [(0x4E81F0, "Viewport_GetCurrentCamera"), (0x4E8200, "Viewport_GetRect"),
                       (0x4E8210, "Viewport_GetCamera"), (0x4E8240, "Viewport_GetLists"),
                       (0x4E8260, "Viewport_GetDrawWorld"), (0x4E8280, "Viewport_GetCurrent")]:
        run(addr, name, setup, check_eax=True)
    for i, on in [(0, 0), (3, 1), (7, 0x1FF)]:
        run(0x4E8220, "Viewport_SetList", lambda e: setup(e) + (i, on))
    for on in (0, 1, 0xFFFFFF80, 0x100):
        run(0x4E8250, "Viewport_SetDrawWorld", lambda e: setup(e) + (on,))
        run(0x4E8270, "Viewport_SetFog", lambda e: setup(e) + (on,))


@pytest.mark.parametrize("used", [0, 3, 14, 15])
def test_viewport_add_callback(used):
    def setup(e):
        u32(e, VP_CALLBACKS, *([0x1111] * used + [0] * (15 - used)))
        u32(e, VP_CALLBACKS + 4, 0 if used == 14 else e.u32(VP_CALLBACKS + 4))
        return (0x2222,)
    run(0x4E8290, "Viewport_AddCallback", setup)


# --- terrain draw ------------------------------------------------------------

def test_terradraw():
    def setup(e):
        RET[0x4018F0] = 0x1234
        RET[0x401900] = 0x74
        u32(e, DRAW_EVENT, 0x25)
    run(0x4E82C0, "TerraDraw_Init", setup)
    run(0x4E8350, "TerraDraw_Shutdown", lambda e: u16(e, 0x54D9A0, 0x74))


@pytest.mark.parametrize("on", [0, 1, 0x100])
def test_terradraw_draw(on):
    def setup(e):
        u16(e, 0x54D9A2, on)
        u32(e, VP_CURRENT, viewport(e))
        t = task(e, 0x24, 0x74)
        u32(e, t + 0x20, 0x4C4C)
        return (t,)
    run(0x4E8310, "TerraDraw_Draw", setup)
