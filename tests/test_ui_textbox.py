import pytest

from test_ui_util import RET, BOX, CHILD, add_child, cstr, logfake, make_emu, run, shutdown_test, task, text_new, u32, window

emu = make_emu()
logfake(emu, 0x4B7050, 2, strings=(1,), effect=lambda e, a: 7 * len(e.read_cstr(a[1])))  # Sprite_StringWidth
logfake(emu, 0x4B70F0, 2, strings=(1,), effect=lambda e, a: 10 + len(e.read_cstr(a[1])))  # Sprite_GetHeight
emu.baseline = emu.snapshot()

TEXTS = [b"Hello world, this is a fairly long text that wraps", b"Short", b"", b"  two  spaces "]


def box(e, count=3, index=1, text=1, sprite=1, strs=True):
    tb = window(e, 0x2244)
    for i in range(count):
        it = tb + 0x9C + 0x108 * i
        e.write(it, b"item%d\0" % i)
        u32(e, it + 0x100, 10 * i + 5)
        if strs:
            u32(e, it + 0x104, cstr(e, TEXTS[i % len(TEXTS)]))
    u32(e, tb + 0x219C, index, count)
    if text:
        u32(e, tb + 0x94, text_new(e, ()))
    if sprite:
        u32(e, tb + 0x98, 0x777)
    e.write(tb + 0x54, b"font2.fnt\0")
    return tb


def test_create():
    run(emu, 0x41ACA0, "TextBox_Create", lambda e: (box(e), 0x1234))


@pytest.mark.parametrize("text", [0, 1])
def test_delete(text):
    run(emu, 0x41AD20, "TextBox_Delete", lambda e: (box(e, text=text),))


@pytest.mark.parametrize("menu", [0, 1])
@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("text", [0, 1])
def test_changed(menu, found, text):
    def setup(e):
        RET[0x418E40] = 0x5555 if found else 0
        tb = box(e, text=text)
        if not menu:
            u32(e, tb + 0x44, 0)
        return tb, 0x4321
    run(emu, 0x41AD70, "TextBox_Changed", setup)


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("value", [5.0, 15.4, 24.6, 25.0, 99.0, -3.0])
def test_run(has, value):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        return (box(e),)
    run(emu, 0x41AE20, "TextBox_Run", setup)


@pytest.mark.parametrize("c", [dict(), dict(fade=4), dict(fade=11), dict(count=0), dict(text=0), dict(sprite=0),
                               dict(strs=False), dict(index=0), dict(index=2), dict(index=3, count=4)])
def test_draw(c):
    def setup(e):
        tb = box(e, count=c.get("count", 3), index=c.get("index", 1), text=c.get("text", 1),
                 sprite=c.get("sprite", 1), strs=c.get("strs", True))
        u32(e, e.u32(tb + 0x44) + 0xD0, c.get("fade", 16))
        return (tb,)
    run(emu, 0x41AEA0, "TextBox_Draw", setup)


@pytest.mark.parametrize("s", TEXTS + [b"a b c d e f g h", b"x" * 60])
@pytest.mark.parametrize("width", [0, 40, 100, 400])
def test_draw_wrapped(s, width):
    def setup(e):
        t = text_new(e, ())
        return (12, 34, 0x3F800000, width, 77, cstr(e, s), 0x777, t, 0x3F000000)
    run(emu, 0x41AFB0, "TextBox_DrawWrapped", setup, check_eax=True)


def test_get_params():
    def setup(e):
        u32(e, 0x57BB98, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x41B160, "TextBox_GetParams", setup)


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("count", [0, 3])
@pytest.mark.parametrize("children", [0, 2])
def test_layout(has, count, children):
    def setup(e):
        RET[0x4F16C0] = has
        tb = box(e, count=count, strs=False)
        for i in range(children):
            add_child(e, tb, task(e, 0x54, CHILD if i % 2 else BOX))
        return (tb,)
    run(emu, 0x41B170, "TextBox_Layout", setup)


def test_shutdown():
    shutdown_test(emu, 0x41B260, "TextBox_Shutdown", 0x53B580, 0x57BB98)


@pytest.mark.parametrize("id_", [5, 15, 7])
@pytest.mark.parametrize("count", [0, 2])
def test_add_item(id_, count):
    def setup(e):
        tb = box(e, count=count)
        u32(e, 0x57BBA0, tb)
        return cstr(e, b"Mission text %d" % id_), id_
    run(emu, 0x41B290, "TextBox_AddItem", setup)
