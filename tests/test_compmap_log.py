import pytest

from test_compmap_util import (DRAW_SLOT, MAP_ENABLED, add_child, check, cstr, make_emu, make_menu, set_handler, task,
                               u16, u32)

LOG, COUNT, LINES = 0x5BDD68, 0x5BDD6C, 0x5BDC9C

e, fk = make_emu()
DRAWN = fk.handler(1)
fk.add(0x401A20, 1)  # TaskType_Free


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("enabled", [0, 1])
@pytest.mark.parametrize("size", [(800, 600), (1023, 767)])
def test_create(enabled, size, cw):
    def setup(e):
        make_menu(e)
        u32(e, MAP_ENABLED, enabled)
        u32(e, 0xC28B44, *size)
        return (task(e, 0x24),)
    check(e, fk, 0x468920, "ComputerLog_Create", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("addr,name", [(0x468B00, "ComputerLog_OnTab")])
@pytest.mark.parametrize("pressed", [0, 0x10, 0x30, 0x20])
@pytest.mark.parametrize("id_", [0, 1, 2, 3])
def test_on_tab(addr, name, pressed, id_):
    def setup(e):
        m = make_menu(e)
        u32(e, m + 0x34, pressed)
        p = e.alloc(4)
        u32(e, p, id_)
        return task(e, 0x90), p
    check(e, fk, addr, name, setup, check_eax=False)


@pytest.mark.parametrize("id_", [3, 4, 5])
@pytest.mark.parametrize("match", [True, False])
@pytest.mark.parametrize("buttons", [0, 0x10])
@pytest.mark.parametrize("was", [0, 0x10])
@pytest.mark.parametrize("scroll,count", [(0, 10), (2, 30), (15, 30), (14, 30), (0, 16)])
def test_on_scroll(id_, match, buttons, was, scroll, count):
    def setup(e):
        m = make_menu(e)
        u32(e, m + 0x30, buttons)
        log = task(e, 0x24)
        u32(e, log + 0x20, scroll)
        u32(e, LOG, log)
        u32(e, COUNT, count)
        b = task(e, 0x94)
        u32(e, b + 0x38, id_)
        e.write(b + 0x80, bytes([was, 0x77]))
        p = e.alloc(4)
        u32(e, p, id_ if match else id_ + 1)
        return b, p
    check(e, fk, 0x468B30, "ComputerLog_OnScroll", setup, check_eax=False)


def test_small():
    def setup(e):
        u32(e, LOG, 0x1234)
        u32(e, COUNT, 7)
        u16(e, 0x5BDC98, 0x55)
    check(e, fk, 0x468BB0, "ComputerLog_Delete", setup, check_eax=False)
    check(e, fk, 0x468DD0, "ComputerLog_Clear", setup, check_eax=False)
    check(e, fk, 0x468DA0, "ComputerLog_Shutdown", setup, check_eax=False)
    check(e, fk, 0x468DB0, "ComputerLog_New", setup, args=(0x4444,))


@pytest.mark.parametrize("count", [0, 1, 49, 50, 51])
def test_add(count):
    def setup(e):
        u32(e, COUNT, count)
        for i in range(52):
            u32(e, LINES + 4 * i, 0x1000 + i)
        return (0xABCD,)
    check(e, fk, 0x468DE0, "ComputerLog_Add", setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("count,scroll", [(0, 0), (1, 0), (5, 0), (20, 3), (20, 5)])
@pytest.mark.parametrize("children", [0, 2])
@pytest.mark.parametrize("size", [(800, 600), (1023, 767)])
def test_draw(count, scroll, children, size, cw):
    def setup(e):
        u32(e, 0xC28B44, *size)
        u32(e, COUNT, count)
        for i in range(count):
            u32(e, LINES + 4 * i, cstr(e, b"line %d with words" % i))
        log = task(e, 0x24)
        u32(e, log + 0x20, scroll)
        for i in range(children):
            c = task(e, 0x40, 0x31 + i)
            add_child(e, log, c)
        set_handler(e, DRAW_SLOT, 0x31, DRAWN)
        return (log,)
    check(e, fk, 0x468BC0, "ComputerLog_Draw", setup, check_eax=False, cw=cw)
