import pytest

from test_ui_util import (RET, BOX, CHILD, WINDOW, add_child, cstr, logfake, make_emu, run, shutdown_test, task,
                          text_new, u32, window)

emu = make_emu()
logfake(emu, 0x4201E0, 2, effect=lambda e, a: task(e, 0x250, CHILD))  # SlideBar_NewVertical
logfake(emu, 0x424B60, 2, effect=lambda e, a: task(e, 0x54, WINDOW))  # MenuWindow_New
logfake(emu, 0x418D70, 1)  # MenuManager_Free
logfake(emu, 0x420240, 6)  # ScrollBar_SetRange
logfake(emu, 0x420290, 1)  # ScrollBar_GetPos
logfake(emu, 0x4202D0, 2)  # ScrollBar_SetPos
logfake(emu, 0x4B4A00, 10)  # Draw_RectA
logfake(emu, 0x4F1270, 2, masks={0: 0}, effect=lambda e, a: e.write(a[0], b"list7\0"))  # Task_GetName
logfake(emu, 0x4B7FA0, 2, strings=(0,))  # Script_RegisterInt2
logfake(emu, 0x4B80A0, 1, strings=(0,))  # Script_RemoveSymbol
emu.baseline = emu.snapshot()

TIMER = 0x567C8C


def box(e, count=5, index=1, text=1, lh=20, h=104, scroll=0, kbd=0, last=-1):
    lb = window(e, 0x13FC, h=h)
    for i in range(count):
        it = lb + 0x54 + 0x48 * i
        u32(e, it, 10 * i + 5)
        e.write(it + 4, b"item%d\0" % i)
        u32(e, it + 0x44, cstr(e, b"Item number %d" % i))
    u32(e, lb + 0x1254, count)
    e.write(lb + 0x125C, b"font1.fnt\0")
    u32(e, lb + 0x129C, index, 10 * index + 5, last)
    e.write(lb + 0x12A9, bytes([scroll, kbd]))
    if text:
        u32(e, lb + 0x12AC, text_new(e, ()))
    u32(e, lb + 0x13F0, lh)
    bar = task(e, 0x250, CHILD)
    u32(e, bar + 0x20, 0, 0, 16, 100)
    frame = task(e, 0x54, WINDOW)
    u32(e, frame + 0x20, 40, 30, 280, h)
    u32(e, frame + 0x44, e.u32(lb + 0x44))
    u32(e, lb + 0x13F4, bar, frame)
    t = e.alloc(0x40)
    u32(e, TIMER, t)
    u32(e, t + 0x34, 5000)
    u32(e, t + 0x3C, 1001)
    return lb


def keys(e, *codes):
    u32(e, 0x5C8B20, *codes)
    u32(e, 0x5C8BA0, 0, len(codes))


def mouse(e, lb, x, y, pressed=0, released=1, select=0):
    mm = e.u32(lb + 0x44)
    e.write(mm + 0x27CC, bytes([1, pressed, released, 0, 0, select, 0, 0]))
    u32(e, mm + 0x27D4, 0, x, y)


def test_create():
    run(emu, 0x41BB10, "ScrollListBox_Create", lambda e: (box(e), 0x1234))


def test_clear():
    run(emu, 0x41BBD0, "ScrollListBox_Clear", lambda e: (box(e),))


@pytest.mark.parametrize("text", [0, 1])
def test_delete(text):
    run(emu, 0x41BC00, "ScrollListBox_Delete", lambda e: (box(e, text=text),))


@pytest.mark.parametrize("found", [0, 1])
@pytest.mark.parametrize("text", [0, 1])
def test_changed(found, text):
    def setup(e):
        RET[0x418E40] = 0x5555 if found else 0
        return box(e, text=text), 0x4321
    run(emu, 0x41BCA0, "ScrollListBox_Changed", setup)


RUN_CASES = [
    dict(), dict(scroll=1, pos=2), dict(click=(50, 75)), dict(click=(50, 55)), dict(click=(50, 300)),
    dict(click=(50, 55), last=4900), dict(click=(50, 55), last=4000), dict(click=(50, 55), last=4900, has=0),
    dict(click=(50, 55), last=4900, scroll=1, pos=1), dict(click=(500, 55)),
    dict(kbd=1, keys=(-4, -4, -3)), dict(kbd=1, keys=(-3, -3, -3)), dict(kbd=1, keys=(-4,) * 6),
    dict(kbd=1, keys=(-4, -4, -4), scroll=1, pos=0, index=3), dict(kbd=1, keys=(-3,), scroll=1, pos=2, index=2),
    dict(kbd=1, keys=(-7, -5, -11, -4)), dict(kbd=1, keys=(-7,), has=0), dict(keys=(-4,)), dict(select=1),
    dict(kbd=1, keys=(-11,), select=1), dict(kbd=1, keys=(-4, -4), scroll=1, pos=0, index=2, h=44),
]


@pytest.mark.parametrize("c", RUN_CASES)
def test_run(c):
    def setup(e):
        RET[0x4F16C0] = c.get("has", 1)
        RET[0x420290] = c.get("pos", 0)
        lb = box(e, index=c.get("index", 1), scroll=c.get("scroll", 0), kbd=c.get("kbd", 0), last=c.get("last", -1),
                 h=c.get("h", 104))
        mm = e.u32(lb + 0x44)
        if "click" in c:
            u32(e, e.u32(lb + 0x13F8) + 0x48, 0x10000)  # held
            mouse(e, lb, *c["click"])
        mouse_sel = c.get("select", 0)
        e.write(mm + 0x27D1, bytes([mouse_sel]))
        keys(e, *c.get("keys", ()))
        return (lb,)
    run(emu, 0x41BDD0, "ScrollListBox_Run", setup)


DRAW_CASES = [dict(), dict(fade=5), dict(count=0), dict(count=3, index=2), dict(count=12, scroll=1, pos=3, index=5),
              dict(count=12, scroll=1, pos=3, index=0), dict(h=30), dict(kbd=1)]


@pytest.mark.parametrize("c", DRAW_CASES)
def test_draw(c):
    def setup(e):
        RET[0x420290] = c.get("pos", 0)
        lb = box(e, count=c.get("count", 5), index=c.get("index", 1), h=c.get("h", 104), scroll=c.get("scroll", 0),
                 kbd=c.get("kbd", 0))
        u32(e, e.u32(lb + 0x44) + 0xD0, c.get("fade", 16))
        return (lb,)
    run(emu, 0x41C080, "ScrollListBox_Draw", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57BBB4, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x41C300, "ScrollListBox_GetParams", setup)


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("value", [5.0, 25.4, 99.0])
@pytest.mark.parametrize("count", [0, 3, 8])
@pytest.mark.parametrize("children", [0, 2])
def test_layout(has, value, count, children):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        lb = box(e, count=count)
        for i in range(children):
            add_child(e, lb, task(e, 0x54, CHILD if i % 2 else BOX))
        return (lb,)
    run(emu, 0x41C310, "ScrollListBox_Layout", setup)


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("children", [0, 2])
def test_forward(has, children):
    def setup(e):
        RET[0x4F16C0] = has
        lb = box(e)
        for i in range(children):
            add_child(e, lb, task(e, 0x54, CHILD if i % 2 else BOX))
        return (lb,)
    run(emu, 0x41C500, "ScrollListBox_Forward918", setup)


@pytest.mark.parametrize("orig,ours", [(0x41C5A0, "ScrollListBox_Activate"), (0x41C600, "ScrollListBox_Deactivate")])
@pytest.mark.parametrize("reg", [0, 1])
def test_activate(orig, ours, reg):
    def setup(e):
        lb = box(e)
        e.write(lb + 0x12A8, bytes([reg]))
        return (lb,)
    run(emu, orig, ours, setup)


def test_shutdown():
    shutdown_test(emu, 0x41C660, "ScrollListBox_Shutdown", 0x53B59C, 0x57BBB4)


@pytest.mark.parametrize("id_", [5, 15, 7])
@pytest.mark.parametrize("count", [0, 2])
def test_add_item(id_, count):
    def setup(e):
        lb = box(e, count=count)
        u32(e, 0x57BBB0, lb)
        return cstr(e, b"Profile %d" % id_), id_
    run(emu, 0x41C690, "ScrollListBox_AddItem", setup)
