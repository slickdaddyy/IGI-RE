import pytest

from test_ui_util import RET, BOX, CHILD, add_child, cstr, make_emu, run, shutdown_test, task, u32, window

emu = make_emu()
emu.fake(0x4B53B0, nargs=17)  # QVertex_DrawQuadEx
emu.baseline = emu.snapshot()


def box(e, count=3, index=1, sprites=True):
    pb = window(e, 0x9FC)
    for i in range(count):
        it = pb + 0x54 + 0x48 * i
        e.write(it, b"pic%d.spr\0" % i)
        u32(e, it + 0x40, 10 * i + 5, 0x6000 + i if sprites else 0)
    u32(e, pb + 0x954, index, count)
    return pb


def test_create():
    run(emu, 0x41B490, "PictureBox_Create", lambda e: (box(e), 0x1234))


def test_delete():
    run(emu, 0x41B500, "PictureBox_Delete", lambda e: (box(e),))


def test_changed():
    run(emu, 0x41B530, "PictureBox_Changed", lambda e: (box(e), 0x4321))


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("value", [5.0, 15.4, 24.6, 25.0, 99.0])
@pytest.mark.parametrize("count", [0, 3])
def test_run(has, value, count):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        return (box(e, count=count),)
    run(emu, 0x41B590, "PictureBox_Run", setup)


@pytest.mark.parametrize("c", [dict(), dict(fade=4), dict(fade=13), dict(count=0), dict(sprites=False),
                               dict(index=0), dict(index=2)])
def test_draw(c):
    def setup(e):
        pb = box(e, count=c.get("count", 3), index=c.get("index", 1), sprites=c.get("sprites", True))
        u32(e, e.u32(pb + 0x44) + 0xD0, c.get("fade", 16))
        return (pb,)
    run(emu, 0x41B5F0, "PictureBox_Draw", setup)


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("count", [0, 3])
@pytest.mark.parametrize("children", [0, 2])
def test_layout(has, count, children):
    def setup(e):
        RET[0x4F16C0] = has
        pb = box(e, count=count)
        for i in range(children):
            add_child(e, pb, task(e, 0x54, CHILD if i % 2 else BOX))
        return (pb,)
    run(emu, 0x41B750, "PictureBox_Layout", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57BBA8, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x41B830, "PictureBox_GetParams", setup)


def test_shutdown():
    shutdown_test(emu, 0x41B840, "PictureBox_Shutdown", 0x53B58C, 0x57BBA8)


@pytest.mark.parametrize("id_", [5, 15, 7])
@pytest.mark.parametrize("count", [0, 2])
def test_add_item(id_, count):
    def setup(e):
        pb = box(e, count=count)
        u32(e, 0x57BBA4, pb)
        return cstr(e, b"pic_%d.spr" % id_), id_
    run(emu, 0x41B870, "PictureBox_AddItem", setup)
