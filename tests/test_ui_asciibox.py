import pytest

from test_ui_util import RET, BOX, CHILD, add_child, cstr, logfake, make_emu, run, shutdown_test, task, text_new, u32, window

emu = make_emu()
SHOWN = {}


def set_string(e, a):
    SHOWN[a[0]] = e.read_cstr(a[1])


def read_file(e, a):
    data = RET.get("file", b"")[:a[2]]
    e.write(a[1], data)
    return len(data)


logfake(emu, 0x4B6E90, 2, strings=(1,), effect=set_string)  # Text_SetString
logfake(emu, 0x4B7DF0, 1, effect=lambda e, a: 7 * len(SHOWN.get(a[0], b"")))  # Text_GetWidth
logfake(emu, 0x4201E0, 2, effect=lambda e, a: task(e, 0x250, CHILD))  # ScrollBar_New
logfake(emu, 0x418D70, 1)  # MenuManager_Free
logfake(emu, 0x420240, 6)  # ScrollBar_SetRange
logfake(emu, 0x420290, 1)  # ScrollBar_GetPos
logfake(emu, 0x4B0C60, 2, effect=lambda e, a: e.alloc(a[0]))  # Mem_Alloc
logfake(emu, 0x4B0D10, 1)  # Mem_Free
logfake(emu, 0x4B0D60, 1)  # Mem_PushMode
logfake(emu, 0x4B0D90, 0)  # Mem_PopMode
logfake(emu, 0x4B1510, 2, strings=(0, 1), effect=lambda e, a: RET.get("open", 0))  # File_Open
logfake(emu, 0x4B1690, 1)  # File_Close
logfake(emu, 0x4B16D0, 3, effect=read_file)  # File_Read
logfake(emu, 0x4B1780, 1, effect=lambda e, a: len(RET.get("file", b"")))  # File_GetSize
emu.baseline = emu.snapshot()

TEXT = (b"IGI is a tactical shooter.\nYou play David Jones, a former SAS soldier.\n\n"
        b"Your\tmission\tbegins in Estonia with a long walk through the forest towards the base camp.")
LINES = [b"First line", b"second", b"", b"fourth line here", b"five", b"6"]


def box(e, lines=LINES, idx=0, sprite=1, text=1, buf=True, lh=12, h=60):
    ab = window(e, 0xF8, h=h)
    e.write(ab + 0x54, b"font1.fnt\0")
    e.write(ab + 0x94, b"readme.txt\0")
    if sprite:
        u32(e, ab + 0xD4, 0x777)
    if text:
        u32(e, ab + 0xD8, text_new(e, ()))
    if buf:
        data = b"\0" + b"\0".join(lines) + b"\0"
        p = e.alloc(len(data) + 4)
        e.write(p, data)
        line = p + 1
        for _ in range(idx):
            line += len(e.read_cstr(line)) + 1
        u32(e, ab + 0xDC, p, line, len(lines), idx, lh)
    bar = task(e, 0x250, CHILD)
    u32(e, ab + 0xF4, bar)
    return ab


def test_create():
    run(emu, 0x41C8D0, "AsciiBox_Create", lambda e: (box(e), 0x1234))


@pytest.mark.parametrize("text", [0, 1])
@pytest.mark.parametrize("buf", [False, True])
def test_delete(text, buf):
    run(emu, 0x41C940, "AsciiBox_Delete", lambda e: (box(e, text=text, buf=buf),))


@pytest.mark.parametrize("c", [dict(), dict(buf=False), dict(menu=0), dict(sprite=0), dict(open=0),
                               dict(children=2), dict(file=b""), dict(w=120)])
def test_reload(c):
    def setup(e):
        RET["open"] = c.get("open", 0x4444)
        RET["file"] = c.get("file", TEXT)
        ab = box(e, buf=c.get("buf", True), sprite=c.get("sprite", 1))
        u32(e, ab + 0x28, c.get("w", 300))
        if c.get("menu", 1) == 0:
            u32(e, ab + 0x44, 0)
        for i in range(c.get("children", 0)):
            add_child(e, ab, task(e, 0x54, CHILD if i % 2 else BOX))
        return (ab,)
    run(emu, 0x41C9D0, "AsciiBox_Reload", setup)


@pytest.mark.parametrize("s", [TEXT, b"", b"one", b"a b c d e f g h i j k", b"word\n\nnext\n", b"averyveryverylongword x",
                               b"trailing space ", b"\ttab start"])
@pytest.mark.parametrize("width", [0, 30, 70, 1000])
def test_wrap(s, width):
    def setup(e):
        return text_new(e, ()), 0x777, cstr(e, s), width
    run(emu, 0x41CB40, "AsciiBox_Wrap", setup, check_eax=True)


def test_wrap_null():
    run(emu, 0x41CB40, "AsciiBox_Wrap", lambda e: (text_new(e, ()), 0x777, 0, 50), check_eax=True)


@pytest.mark.parametrize("menu", [0, 1])
@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("text", [0, 1])
def test_changed(menu, found, text):
    def setup(e):
        RET[0x418E40] = 0x5555 if found else 0
        RET["open"] = 0x4444
        RET["file"] = TEXT
        ab = box(e, text=text)
        if not menu:
            u32(e, ab + 0x44, 0)
        return ab, 0x4321
    run(emu, 0x41CC40, "AsciiBox_Changed", setup)


@pytest.mark.parametrize("idx,pos", [(0, 0), (0, 3), (3, 0), (4, 2), (2, 2), (1, 5), (5, 1)])
@pytest.mark.parametrize("buf", [False, True])
def test_run(idx, pos, buf):
    def setup(e):
        RET[0x420290] = pos
        return (box(e, idx=idx, buf=buf),)
    run(emu, 0x41CD20, "AsciiBox_Run", setup)


@pytest.mark.parametrize("c", [dict(), dict(fade=3), dict(idx=4), dict(idx=6), dict(h=20), dict(h=200),
                               dict(sprite=0), dict(buf=False)])
def test_draw(c):
    def setup(e):
        ab = box(e, idx=c.get("idx", 1), sprite=c.get("sprite", 1), buf=c.get("buf", True), h=c.get("h", 60))
        u32(e, e.u32(ab + 0x44) + 0xD0, c.get("fade", 16))
        return (ab,)
    run(emu, 0x41CDF0, "AsciiBox_Draw", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57BBBC, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x41D000, "AsciiBox_GetParams", setup)


@pytest.mark.parametrize("orig,ours", [(0x41D010, "AsciiBox_Activate"), (0x41D030, "AsciiBox_Deactivate")])
@pytest.mark.parametrize("active", [0, 1])
def test_activate(orig, ours, active):
    def setup(e):
        ab = box(e)
        u32(e, ab + 0xF0, active)
        return (ab,)
    run(emu, orig, ours, setup)


def test_shutdown():
    shutdown_test(emu, 0x41D050, "AsciiBox_Shutdown", 0x53B5F4, 0x57BBBC)
