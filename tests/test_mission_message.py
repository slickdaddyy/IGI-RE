"""StatusMessage (0x485330 - 0x485A40) and StatusMessageScreen (0x485A40 - 0x486900)."""
import random

import pytest

from test_mission_util import (RET, cstr, display, f32, handler, make_emu, run, set_handler, strip_logger, task,
                               tick_info, u16, u32)

MSG_TYPE, PARAMS, SCREEN_TYPE, CUR_SCREEN = 0x540A8C, 0x5C8A38, 0x540B08, 0x5C8A48
MSG, LINE = 0x63, 0x102C
EV_DAC = 0x32

emu = make_emu()
set_handler(emu, EV_DAC, MSG, handler(emu, "dac", 1))
strip_logger(emu)
OUT = emu.alloc(24)
emu.baseline = emu.snapshot()


def message(e, text=b"MSG_01", portrait=b"", sound=b"", once=0, shown=0, plain=0, tick_sent=-100000,
            sent=0, registered=0, finished=0, duration=2.0):
    m = task(e, 0x1E0, MSG)
    e.write(m + 0x20, bytes(range(0x40)))
    u32(e, m + 0xB0, cstr(e, text) if text is not None else 0)
    e.write(m + 0xB4, portrait + b"\0")
    e.write(m + 0x1B4, sound + b"\0")
    e.write(m + 0x1C4, bytes([once, shown, plain]))
    u32(e, m + 0x1C8, tick_sent, 77, 5)
    e.write(m + 0x1D4, bytes([sent, registered, finished]))
    f32(e, m + 0x1D8, duration)
    return m


def msg_screen(e):
    s = task(e, 0x18440, 0x64)
    for i in range(24):
        e.write(s + 0x20 + LINE * i, b"\xA5" * 0x2C)
        e.write(s + 0x40 + LINE * i, b"\0")
    return s


def status_screen(e, messages):
    ss = task(e, 0x4F0, 0x46)
    u32(e, ss + 0x4E4, messages)
    u32(e, CUR_SCREEN, ss)
    return ss


def line(e, s, i, msg, ticks=0, shown=0, text=b"", typed=None, portrait=0, width=0.0, height=0.0):
    p = s + 0x20 + LINE * i
    ref = e.alloc(4)
    u32(e, ref, msg)
    texts = [e.alloc(0x28) for _ in range(3)]
    u32(e, p, ref, ticks, shown, p + 0x2C + (len(text) if typed is None else typed), *texts)
    u32(e, p + 0x1C, portrait)
    e.write(p + 0x20, b"\1")
    f32(e, p + 0x24, width, height)
    e.write(p + 0x2C, text + b"\0")
    return p


@pytest.mark.parametrize("orig,ours", [(0x485330, "StatusMessage_Init"), (0x485950, "StatusMessage_Close"),
                                       (0x485A40, "StatusMessageScreen_Init"),
                                       (0x486610, "StatusMessageScreen_Close"),
                                       (0x4868A0, "StatusMessages_Load"), (0x4868C0, "StatusMessages_Free")])
def test_init_close(orig, ours):
    def setup(e):
        u16(e, MSG_TYPE, 0x63)
        u16(e, SCREEN_TYPE, 0x64)
        u32(e, PARAMS, 0x3838)
        u32(e, 0x5C8A40, 0x8A40)
    run(emu, orig, ours, setup)


def test_create():
    run(emu, 0x485570, "StatusMessage_Create", lambda e: (message(e, once=1, shown=1, plain=1, sent=1), 0))


@pytest.mark.parametrize("finished", [0, 1])
@pytest.mark.parametrize("once,sent,shown", [(0, 0, 0), (1, 1, 0), (1, 0, 0), (0, 1, 0), (1, 1, 1), (0, 0, 1)])
@pytest.mark.parametrize("tick_sent", [-100000, 790, 791])
@pytest.mark.parametrize("cond", [None, 0.0, -0.0, 1.0, float("nan")])
def test_run(finished, once, sent, shown, tick_sent, cond):
    def setup(e):
        tick_info(e, ticks=1000)
        s = msg_screen(e)
        status_screen(e, s)
        for i in range(3):
            line(e, s, i, 0x5555)
        m = message(e, once=once, sent=sent, shown=shown, finished=finished, tick_sent=tick_sent)
        if cond is not None:
            RET[("has", m + 0x60)] = 1
            RET[("val", m + 0x60)] = cond
        return (m,)
    run(emu, 0x4855F0, "StatusMessage_Run", setup)


@pytest.mark.parametrize("cid", [-1, 0, 3])
@pytest.mark.parametrize("text", [None, b"abc"])
def test_delete(cid, text):
    def setup(e):
        RET[0x4F1050] = cid
        return (message(e, text=text),)
    run(emu, 0x4856D0, "StatusMessage_Delete", setup)


def test_get_params():
    def setup(e):
        u32(e, PARAMS, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x485740, "StatusMessage_GetParams", setup)


def test_changed():
    run(emu, 0x485750, "StatusMessage_Changed", lambda e: (message(e), 0))


@pytest.mark.parametrize("orig,ours", [(0x485770, "StatusMessage_Activate"), (0x485870, "StatusMessage_Deactivate")])
@pytest.mark.parametrize("registered", [0, 1])
def test_activate(orig, ours, registered):
    run(emu, orig, ours, lambda e: (message(e, registered=registered),))


@pytest.mark.parametrize("text", [b"", b"MSG_FOUND_KEY", b"x" * 300])
def test_show(text):
    def setup(e):
        RET["task_size"] = 0x1E0
        u16(e, MSG_TYPE, 0x63)
        return 0x4444, cstr(e, text), cstr(e, b"STATUSSCREEN:anya.spr"), cstr(e, b"beep")
    run(emu, 0x485970, "StatusMessageShow", setup, check_eax=True)


def full_screen(e, used=(0, 3, 23)):
    s = msg_screen(e)
    status_screen(e, s)
    for i in used:
        line(e, s, i, message(e), text=b"hello")
    return s


def test_clear_messages():
    def setup(e):
        tick_info(e)
        full_screen(e)
        return OUT, 0, 0
    ro, rn = run(emu, 0x485AD0, "StatusMessageScreen_ClearMessages", setup,
                 ignore=[(OUT + 4, OUT + 8), (OUT + 20, OUT + 24)], check_eax=True)
    assert ro[0] == OUT


def test_screen_create():
    run(emu, 0x485B20, "StatusMessageScreen_Create", lambda e: (full_screen(e),))


@pytest.mark.parametrize("paused", [0, 1])
def test_screen_run(paused):
    def setup(e):
        tick_info(e)
        display(e)
        RET[0x416D50] = paused
        s = msg_screen(e)
        line(e, s, 2, message(e), ticks=3, text=b"abc")
        line(e, s, 5, 0)
        line(e, s, 9, message(e, plain=1), ticks=12, text=b"hello world")
        return (s,)
    run(emu, 0x485B40, "StatusMessageScreen_Run", setup)


TEXT = b"Find the radar room and destroy the computer. Then\nleave by the north gate."


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("plain", [0, 1])
@pytest.mark.parametrize("ticks", [0, 1, 2, 8, 9, 10, 11, 40])
@pytest.mark.parametrize("typed", [0, 5, len(TEXT)])
@pytest.mark.parametrize("shown,duration", [(0, 2.0), (59, 2.0), (60, 2.0), (61, 2.0), (0, 0.0166), (7, 0.25)])
def test_update_line(plain, ticks, typed, shown, duration, cw):
    def setup(e):
        tick_info(e)
        display(e, 800, 600)
        s = msg_screen(e)
        line(e, s, 4, message(e, plain=plain, duration=duration), ticks=ticks, shown=shown, text=TEXT, typed=typed,
             width=0.5, height=0.25)
        return s, 4
    run(emu, 0x485B80, "StatusMessageScreen_UpdateLine", setup, cw=cw)


def test_update_line_dead():
    def setup(e):
        tick_info(e)
        s = msg_screen(e)
        line(e, s, 4, 0, ticks=20)
        return s, 4
    run(emu, 0x485B80, "StatusMessageScreen_UpdateLine", setup)


def test_get_message():
    def setup(e):
        s = msg_screen(e)
        line(e, s, 7, 0xBEEF)
        return s, 7
    run(emu, 0x485D00, "StatusMessageScreen_GetMessage", setup, check_eax=True)


@pytest.mark.parametrize("msg,tick", [(0, 1), (1, 1), (1, 0)])
def test_remove_line(msg, tick):
    def setup(e):
        if tick:
            tick_info(e, ticks=4321)
        else:
            u32(e, 0xBC2360, 0)
        s = msg_screen(e)
        line(e, s, 11, message(e) if msg else 0)
        return s, 11
    run(emu, 0x485D20, "StatusMessageScreen_RemoveLine", setup)


WRAP_TEXTS = [b"", b"a", b"short", TEXT, b"one two three four five six seven eight nine ten eleven twelve " * 4,
              b"averyveryveryverylongwordwithoutanyspacesatallthatkeepsgoing and more",
              b"line\nbreaks\n\neverywhere\n", b"  leading spaces and trailing  ", b"word " * 60]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("text", range(len(WRAP_TEXTS)))
@pytest.mark.parametrize("width", [229.0, 100.0, 40.5, 0.0, 2000.0])
@pytest.mark.parametrize("plain", [0, 1])
def test_wrap_text(text, width, plain, cw):
    t = WRAP_TEXTS[text]
    typed = len(t) if plain else len(t) * 2 // 3

    def setup(e):
        tick_info(e)
        s = msg_screen(e)
        line(e, s, 6, message(e, plain=plain), text=t, typed=typed)
        return s, 6, struct_f(width)
    run(emu, 0x485DB0, "StatusMessageScreen_WrapText", setup, cw=cw)


def struct_f(v):
    import struct
    return struct.unpack("<I", struct.pack("<f", v))[0]


@pytest.mark.parametrize("msg,plain", [(0, 0), (1, 0), (1, 1)])
def test_get_font(msg, plain):
    def setup(e):
        tick_info(e)
        s = msg_screen(e)
        line(e, s, 1, message(e, plain=plain) if msg else 0)
        return s, 1
    run(emu, 0x485FB0, "StatusMessageScreen_GetFont", setup, check_eax=True)


@pytest.mark.parametrize("orig,ours", [(0x486010, "StatusMessageScreen_Delete"),
                                       (0x4868D0, "StatusMessageScreen_Clear")])
def test_clear(orig, ours):
    def setup(e):
        tick_info(e)
        return (full_screen(e),)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("size", [(640, 480), (1023, 767), (1600, 1200)])
def test_draw(size, cw):
    def setup(e):
        tick_info(e)
        display(e, *size)
        RET[0x416D40] = 0x41600
        s = msg_screen(e)
        rng = random.Random(size[0])
        for i in (0, 2, 3, 7, 20):
            line(e, s, i, message(e, plain=int(i == 3)), ticks=rng.randrange(20), text=b"abc",
                 portrait=rng.choice([0, e.alloc(0x18)]), width=rng.random(), height=rng.random())
        return (s,)
    run(emu, 0x486020, "StatusMessageScreen_Draw", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(10))
def test_draw_line(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        RET[0x4B6CE0] = rng.randrange(-5, 80)
        RET[0x4B6D00] = rng.randrange(0, 80)
        RET[0x4B7E00] = rng.randrange(0, 30)
        s = msg_screen(e)
        line(e, s, 5, 0x5555, ticks=rng.choice([-3, 0, 4, 9, 10, 30]), portrait=rng.choice([0, e.alloc(0x18)]),
             width=rng.uniform(0, 1.2), height=rng.uniform(0, 1.2))
        return s, 5, struct_f(rng.uniform(-50, 700)), struct_f(rng.uniform(-50, 500))
    run(emu, 0x4860F0, "StatusMessageScreen_DrawLine", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(16))
def test_draw_box(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        RET[0x4B6D00] = rng.randrange(-10, 80)
        s = msg_screen(e)
        line(e, s, 2, 0x5555, ticks=rng.choice([-20, -9, 0, 1, 4, 8, 9, 10, 30]),
             portrait=rng.choice([0, e.alloc(0x18)]), width=rng.uniform(0, 1.2), height=rng.uniform(0, 1.2))
        return s, 2, struct_f(rng.uniform(-50, 700)), struct_f(rng.uniform(-50, 500)), rng.choice([0, 1, 0x40])
    run(emu, 0x486210, "StatusMessageScreen_DrawBox", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_draw_plain_line(cw):
    def setup(e):
        RET[0x4B7E00] = 19
        s = msg_screen(e)
        line(e, s, 3, 0x5555)
        return s, 3, struct_f(10.25), struct_f(415.3)
    run(emu, 0x4865B0, "StatusMessageScreen_DrawPlainLine", setup, cw=cw)


def test_screen_new():
    def setup(e):
        u16(e, SCREEN_TYPE, 0x64)
        return (0x4444,)
    run(emu, 0x486620, "StatusMessageScreen_New", setup, check_eax=True)


@pytest.mark.parametrize("used", [(), (0, 1), tuple(range(23)), tuple(range(24))])
@pytest.mark.parametrize("portrait", [b"", b"STATUSSCREEN:note.spr", b"STATUSSCREEN:weapon.spr", b"anya"])
@pytest.mark.parametrize("sound", [b"", b"beep"])
@pytest.mark.parametrize("found,text", [(0, b"MSG_1"), (1, b"MSG_1"), (1, b"M" * 127), (1, b"M" * 128)])
@pytest.mark.parametrize("plain", [0, 1])
def test_add_message(used, portrait, sound, found, text, plain):
    def setup(e):
        tick_info(e)
        s = msg_screen(e)
        ss = status_screen(e, s)
        u32(e, CUR_SCREEN, ss)
        for i in used:
            line(e, s, i, 0x5555)
        RET[0x4B64E0] = found
        RET[("res", text)] = b"The text of " + text[:40]
        return s, message(e, text=text, portrait=portrait, sound=sound, plain=plain)
    run(emu, 0x486650, "StatusMessageScreen_AddMessage", setup)
