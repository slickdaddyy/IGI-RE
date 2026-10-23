import pytest

from test_explosion_util import (RET, d64, handler, logfake, make_emu, randoms, run, seeded, set_handler, task, u16,
                                 u32, vfake)
from test_menu_util import task_system

CWS = [0x37F, 0x7F]
FLASH, HOLE = 0x2E, 0x2F
FLASH_TYPE = 0x50

e = make_emu()
vfake(e, 0x4012A0, 3, show=lambda e, a: (e.normalize(a[0]), a[1] & 0xFFFF, a[2]),
      effect=lambda e, a: task(e, 0x48, a[1] & 0xFFFF))
logfake(e, 0x4015F0, 1)
vfake(e, 0x4FDF00, 13, show=lambda e, a: tuple(a[:3]) + (e.read(a[3], 12), e.read(a[4], 12), e.read(a[5], 8))
      + tuple(a[6:]), ret=0x7300)
vfake(e, 0x4FE120, 9, show=lambda e, a: (a[0], e.read(a[1], 24), e.read(a[2], 12)) + tuple(a[3:]))
logfake(e, 0x4E6B00, 2, strings=(1,))
logfake(e, 0x4E6C30, 2, derefs={1: 24})
logfake(e, 0x450CE0, 1, derefs={0: 0x1C})
logfake(e, 0x4018C0, 1, masks={0: 0xFF})
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})
for t in range(0x60, 0x64):
    set_handler(e, FLASH, t, handler(e, "flash_%x" % t, 2, derefs={1: 8}))
set_handler(e, HOLE, 0x61, handler(e, "hole", 2, derefs={1: 16}))
e.write(0x540914, bytes([FLASH]))
e.write(0x540904, bytes([HOLE]))
u16(e, 0x5C1144, FLASH_TYPE)
u32(e, 0x5C1148, 0x6100)
u32(e, 0x5C114C, 0x6200)
e.baseline = e.snapshot()

init = make_emu()
logfake(init, 0x4169D0, 1, strings=(0,), ret=0x6100)
logfake(init, 0x416930, 1, strings=(0,), ret=0x6200)
init.baseline = init.snapshot()


def test_init():
    def setup(e):
        task_system(e)
    run(init, 0x47E700, "FlashBang_Init", setup)


def test_create():
    def setup(e):
        return (task(e, 0x48, FLASH_TYPE),)
    run(e, 0x47E790, "FlashBang_Create", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("sound", [0, 0x7700])
@pytest.mark.parametrize("seed", range(3))
def test_run(seed, sound, cw):
    def setup(e):
        rng = seeded(seed)
        randoms(rng)
        t = task(e, 0x48, FLASH_TYPE)
        d64(e, t + 0x20, *[rng.uniform(-1e5, 1e5) for _ in range(3)])
        u32(e, t + 0x38, 0x7300)
        RET[0x4E6B00] = sound
        return (t,)
    run(e, 0x47E820, "FlashBang_Run", setup, cw=cw)


def test_close():
    run(e, 0x47E970, "FlashBang_Close", None)


def test_get_event():
    run(e, 0x47E990, "FlashBang_GetEvent", None, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("humans", [0, 3])
@pytest.mark.parametrize("hit", ["none", "notask", "nohandler", "decal"])
def test_explode(hit, humans):
    def setup(e):
        arr = e.alloc(8 + 4 * humans)
        u32(e, arr, humans)
        for i in range(humans):
            u32(e, arr + 8 + 4 * i, task(e, 0x40, 0x60 + i))
        u32(e, 0x5BDC24, arr)
        pos = e.alloc(24)
        d64(e, pos, 1.5, -2.5, 3.25)
        h = 0
        if hit != "none":
            h = e.alloc(0x70)
            if hit != "notask":
                u32(e, h, task(e, 0x40, 0x61 if hit == "decal" else 0x62))
        return (pos, h, 0x45000000, 0x3F000000)
    run(e, 0x47E9A0, "FlashBang_Explode", setup, check_eax=True)

