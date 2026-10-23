"""Loading screen progress bar and the menu key queue (src/loadscreen/loadscreen.c, keys.c)."""
import pytest

from emu import Emu
from test_loadscreen_util import Fakes, check, w32

e = Emu()
fk = Fakes(e)
USA = [0]
fk.add(0x4B0FD0, 1)
fk.add(0x4B0F90, 0)
fk.add(0x4B5F00, 2, ret=0x5151)
fk.add(0x48F2D0, 0, ret=lambda em, a: USA[0])
TEXT = e.alloc(0x40)
fk.add(0x4B6DB0, 1, ret=TEXT)
fk.add(0x4B6E60, 1, deref={0: 0x28})
fk.add(0x4B6E30, 1)
fk.add(0x4B63D0, 1)
fk.add(0x491D70, 1, deref={0: 40})
fk.add(0x491E70, 3)
fk.add(0x491A30, 0)
fk.add(0x48F490, 1)
fk.add(0x48F4F0, 1)
e.baseline = e.snapshot()

DISPLAY = 0xC28B40


@pytest.mark.parametrize("usa", [0, 1])
@pytest.mark.parametrize("w,h", [(640, 480), (1024, 768), (801, 601)])
@pytest.mark.parametrize("steps", [1, 7])
def test_new(usa, w, h, steps):
    def setup(em):
        USA[0] = usa
        w32(em, DISPLAY + 4, w, h)
        return (steps,)
    check(e, fk, 0x48A440, "LoadingScreen_New", setup=setup)


def screen(em, total, done):
    ls = em.call(0x4B0C60, 0x1C, 4)[0]
    pix = em.call(0x4B0C60, 560 * 10 * 4, 4)[0]
    w32(em, ls, total, done, 40, 440, 560, 10, pix)
    return ls


@pytest.mark.parametrize("total,done,steps", [(10, 0, 0), (10, 3, 2), (7, 6, 1), (3, 0, 5), (10, 2, -5)])
def test_step(total, done, steps):
    check(e, fk, 0x48A620, "LoadingScreen_Step", setup=lambda em: (screen(em, total, done), steps), check_eax=False)


def test_delete():
    def setup(em):
        w32(em, 0xBCADF0, 10, 20, 30)
        return (screen(em, 5, 5),)
    check(e, fk, 0x48A5D0, "LoadingScreen_Delete", setup=setup, check_eax=False)


def keys(em, start, count):
    w32(em, 0x5C8BA0, start, count)
    for i in range(32):
        w32(em, 0x5C8B20 + 4 * i, 1000 + i)


@pytest.mark.parametrize("start,count", [(0, 0), (5, 3), (30, 5), (0, 30), (0, 31), (7, 40), (31, 1), (-5, 2), (-40, 1)])
def test_push(start, count):
    check(e, fk, 0x48A6D0, "Keys_Push", setup=lambda em: (keys(em, start, count), (77,))[1], check_eax=False)


@pytest.mark.parametrize("start,count", [(0, 0), (5, 3), (31, 1), (31, 0), (0, 32)])
def test_pop(start, count):
    check(e, fk, 0x48A8D0, "Keys_Pop", setup=lambda em: keys(em, start, count))


@pytest.mark.parametrize("count", [0, 1, 9])
def test_get_count(count):
    check(e, fk, 0x48A910, "Keys_GetCount", setup=lambda em: keys(em, 3, count))


@pytest.mark.parametrize("addr,name", [(0x48A710, "Keys_Install"), (0x48A8A0, "Keys_Uninstall"),
                                       (0x48A8C0, "Keys_Clear")])
def test_install(addr, name):
    def setup(em):
        keys(em, 3, 4)
        em.write(0x5C8BA8, b"\x05")
    check(e, fk, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("msg", [0x100, 0x101, 0x102])
@pytest.mark.parametrize("key", [8, 9, 0xD, 0x1B, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2E, 0x41, 0x61, 7, 0x30])
def test_wndproc(msg, key):
    check(e, fk, 0x48A730, "Keys_WndProc", setup=lambda em: (keys(em, 2, 3), (0, msg, key))[1])
