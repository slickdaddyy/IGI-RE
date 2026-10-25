import struct

import pytest

from test_ui_util import (LOG, RET, BOX, CHILD, add_child, cstr, logfake, make_emu, run, shutdown_test, task,
                          text_new, trap, u32, window)

emu = make_emu()
SHOWN = {}


def set_string(e, a):
    SHOWN[a[0]] = e.read_cstr(a[1])


def strip(e):
    """Logs a QVertex strip; the vertex alpha is uninitialised in the original."""
    s = e.arg(0)
    untextured, flags, unk8, count, verts = struct.unpack("<5I", e.read(s, 20))
    vs = [struct.unpack("<9I", e.read(verts + 36 * i, 36)) for i in range(count)]
    LOG[-1].append(("strip", untextured, flags, unk8, count, [v[:5] + v[6:] for v in vs],
                    e.u32(0xA84A50), e.u32(0xA84A58)))
    e.ret(0)


logfake(emu, 0x4B6E90, 2, strings=(1,), effect=set_string)  # Text_SetString
logfake(emu, 0x4B7DF0, 1, effect=lambda e, a: 7 * len(SHOWN.get(a[0], b"")))  # Text_GetWidth
logfake(emu, 0x418FC0, 1)  # MenuManager_Pause
logfake(emu, 0x418FD0, 1)  # MenuManager_Resume
logfake(emu, 0x4F1270, 2, masks={0: 0}, effect=lambda e, a: e.write(a[0], b"input3\0"))  # Task_GetName
logfake(emu, 0x4B8030, 3, strings=(0,))  # Script_RegisterString
logfake(emu, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
u32(emu, 0xA84A54, 5)
u32(emu, 0xA94E84 + 4 * 5, trap(emu, "ui_strip", strip))
u32(emu, 0xA84A50, 0x1111, 5, 0x2222)
emu.baseline = emu.snapshot()

TIMER = 0x567C8C


def box(e, s=b"hello", cursor=2, editing=0, password=0, text=1, maxlen=20, key=0, focused=0, ticks=5000):
    ib = window(e, 0x1A8, h=15)
    e.write(ib + 0x54, s + b"\0")
    e.write(ib + 0x194, bytes([0, editing, password]))
    u32(e, ib + 0x198, 0x777, text_new(e, ()) if text else 0, cursor, maxlen)
    u32(e, ib + 0x50, key)
    e.write(ib + 0x4D, bytes([focused]))
    t = e.alloc(0x40)
    u32(e, TIMER, t)
    u32(e, t + 0x34, ticks)
    u32(e, t + 0x3C, 1000)
    return ib


def keys(e, *codes):
    u32(e, 0x5C8B20, *codes)
    u32(e, 0x5C8BA0, 0, len(codes))


def test_create():
    run(emu, 0x41D290, "InputBox_Create", lambda e: (box(e), 0x1234))


@pytest.mark.parametrize("text", [0, 1])
def test_delete(text):
    run(emu, 0x41D330, "InputBox_Delete", lambda e: (box(e, text=text),))


@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("text", [0, 1])
def test_changed(found, text):
    def setup(e):
        RET[0x418E40] = 0x5555 if found else 0
        return box(e, text=text), 0x4321
    run(emu, 0x41D3A0, "InputBox_Changed", setup)


RUN_CASES = [
    dict(), dict(entered=1, editing=1, key=-7), dict(key=-7), dict(click=1), dict(click=1, s=b""),
    dict(editing=1), dict(editing=1, keys=(-10,)), dict(editing=1, keys=(-9,)), dict(editing=1, keys=(-8, 65)),
    dict(editing=1, keys=(-7,)), dict(editing=1, keys=(-7,), has=0), dict(editing=1, keys=(-6, -6)),
    dict(editing=1, keys=(-6,), cursor=5), dict(editing=1, keys=(-5, -5, -5)), dict(editing=1, keys=(-4, -3, -11)),
    dict(editing=1, keys=(-2, -2, -2, -2)), dict(editing=1, keys=(-1, -1, -1)), dict(editing=1, keys=(65, 66, 0x22, 0)),
    dict(editing=1, keys=(0x161, 0x122, -100, 200)), dict(editing=1, keys=(97,) * 20, maxlen=12),
    dict(editing=1, keys=(-5,), cursor=0), dict(editing=1, keys=(-12, 49)),
]


@pytest.mark.parametrize("c", RUN_CASES)
def test_run(c):
    def setup(e):
        RET[0x4F16C0] = c.get("has", 1)
        ib = box(e, s=c.get("s", b"hello"), cursor=c.get("cursor", 2), editing=c.get("editing", 0),
                 key=c.get("key", 0), maxlen=c.get("maxlen", 20))
        mm = e.u32(ib + 0x44)
        e.write(mm + 0x27CC, bytes([1, 0, 1, c.get("entered", 0)]))
        u32(e, mm + 0x27D8, 50, 35)
        if c.get("click"):
            e.write(ib + 0x4A, b"\x01")
        keys(e, *c.get("keys", ()))
        return (ib,)
    run(emu, 0x41D450, "InputBox_Run", setup)


@pytest.mark.parametrize("pos", [0, 2, 4, 5])
def test_delete_char(pos):
    run(emu, 0x41D660, "InputBox_DeleteChar", lambda e: (box(e), pos))


@pytest.mark.parametrize("pos,c", [(0, 65), (2, 0x7A), (5, 0x30), (3, -3 & 0xFF), (1, 0x180)])
def test_insert_char(pos, c):
    run(emu, 0x41D6A0, "InputBox_InsertChar", lambda e: (box(e), pos, c))


DRAW_CASES = [dict(), dict(fade=3), dict(text=0), dict(password=1), dict(password=1, s=b""), dict(editing=1),
              dict(editing=1, ticks=5600), dict(editing=1, ticks=5600, password=1, cursor=4),
              dict(editing=1, ticks=5600, cursor=0, focused=1), dict(editing=1, ticks=0x7FFFFFF0)]


@pytest.mark.parametrize("c", DRAW_CASES)
def test_draw(c):
    def setup(e):
        ib = box(e, s=c.get("s", b"hello"), cursor=c.get("cursor", 2), editing=c.get("editing", 0),
                 password=c.get("password", 0), text=c.get("text", 1), focused=c.get("focused", 0),
                 ticks=c.get("ticks", 5000))
        u32(e, e.u32(ib + 0x44) + 0xD0, c.get("fade", 16))
        return (ib,)
    run(emu, 0x41D720, "InputBox_Draw", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57BBC4, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x41D9D0, "InputBox_GetParams", setup)


@pytest.mark.parametrize("orig,ours", [(0x41D9E0, "InputBox_Layout"), (0x41DA90, "InputBox_Save")])
@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("children", [0, 2])
def test_layout(orig, ours, has, children):
    def setup(e):
        RET[0x4F16C0] = has
        ib = box(e)
        for i in range(children):
            add_child(e, ib, task(e, 0x54, CHILD if i % 2 else BOX))
        return (ib,)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("orig,ours", [(0x41DB30, "InputBox_Activate"), (0x41DB90, "InputBox_Deactivate")])
@pytest.mark.parametrize("reg", [0, 1])
def test_activate(orig, ours, reg):
    def setup(e):
        ib = box(e)
        e.write(ib + 0x194, bytes([reg]))
        return (ib,)
    run(emu, orig, ours, setup)


def test_shutdown():
    shutdown_test(emu, 0x41DBF0, "InputBox_Shutdown", 0x53B640, 0x57BBC4)


@pytest.mark.parametrize("s", [b"", b"Player One", b"x" * 39])
def test_set_text(s):
    def setup(e):
        ib = box(e)
        u32(e, 0x57BBCC, ib)
        return (cstr(e, s),)
    run(emu, 0x41DC20, "InputBox_SetText", setup)
