import pytest

from test_ui_util import RET, BOX, CHILD, WINDOW, add_child, logfake, make_emu, run, shutdown_test, task, text_new, u32, window

emu = make_emu()
logfake(emu, 0x4012A0, 3, masks={1: 0xFFFF}, effect=lambda e, a: task(e, 0x54, WINDOW))  # Task_New
logfake(emu, 0x4015F0, 1)  # Task_Delete
logfake(emu, 0x4B4AF0, 11)  # Draw_RectAZ
logfake(emu, 0x4B7DF0, 1, effect=lambda e, a: 31 if e.u32(a[0] + 0x20) == 0x5A5A else 17)  # Text_GetWidth
emu.baseline = emu.snapshot()


def box(e, on=1, texts=1, key=0, focused=0, w=50, h=15):
    tb = window(e, 0x14C, w=w, h=h)
    mm = e.u32(tb + 0x44)
    left, right = task(e, 0x54, WINDOW), task(e, 0x54, WINDOW)
    for t in (left, right):
        u32(e, t + 0x44, mm)
        u32(e, t + 0x20, 1, 2, 3, 4)
    u32(e, tb + 0x94, left, right)
    if texts:
        yes, no = text_new(e, ()), text_new(e, ())
        u32(e, no + 0x20, 0x6B6B)
        u32(e, tb + 0x9C, yes, no)
    u32(e, tb + 0xA4, on)
    u32(e, tb + 0x50, key)
    e.write(tb + 0x4D, bytes([focused]))
    return tb


def test_create():
    run(emu, 0x41DDD0, "ToggleBox_Create", lambda e: (box(e), 0x1234))


@pytest.mark.parametrize("texts", [0, 1])
def test_delete(texts):
    run(emu, 0x41DEB0, "ToggleBox_Delete", lambda e: (box(e, texts=texts),))


@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("texts", [0, 1])
def test_changed(found, texts):
    def setup(e):
        RET[0x418E40] = 0x5555 if found else 0
        return box(e, texts=texts), 0x4321
    run(emu, 0x41DF30, "ToggleBox_Changed", setup)


@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("c", [dict(), dict(key=-1), dict(key=-2), dict(key=-7), dict(click=(45, 35)),
                               dict(click=(80, 35)), dict(click=(200, 35)), dict(w=51)])
def test_run(on, c):
    def setup(e):
        tb = box(e, on=on, key=c.get("key", 0), w=c.get("w", 50))
        if "click" in c:
            mm = e.u32(tb + 0x44)
            e.write(mm + 0x27CC, bytes([1, 0, 1, 0]))
            u32(e, mm + 0x27D8, *c["click"])
            for t in (e.u32(tb + 0x94), e.u32(tb + 0x98)):
                e.write(t + 0x4A, b"\x01")
        return (tb,)
    run(emu, 0x41E020, "ToggleBox_Run", setup)


@pytest.mark.parametrize("c", [dict(), dict(on=0), dict(fade=2), dict(texts=0), dict(focused=1), dict(w=-33, h=-7),
                               dict(w=101, h=20)])
def test_draw(c):
    def setup(e):
        RET[0x4B7E00] = 9
        tb = box(e, on=c.get("on", 1), texts=c.get("texts", 1), focused=c.get("focused", 0), w=c.get("w", 50),
                 h=c.get("h", 15))
        u32(e, e.u32(tb + 0x44) + 0xD0, c.get("fade", 16))
        return (tb,)
    run(emu, 0x41E130, "ToggleBox_Draw", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57BBD0, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x41E410, "ToggleBox_GetParams", setup)


@pytest.mark.parametrize("orig,ours", [(0x41E420, "ToggleBox_Layout"), (0x41E500, "ToggleBox_Forward918")])
@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("value", [0, 1, 7])
@pytest.mark.parametrize("children", [0, 2])
def test_layout(orig, ours, has, value, children):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x419070] = value
        tb = box(e, on=2)
        for i in range(children):
            add_child(e, tb, task(e, 0x54, CHILD if i % 2 else BOX))
        return (tb,)
    run(emu, orig, ours, setup)
