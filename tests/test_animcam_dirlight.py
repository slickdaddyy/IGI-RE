"""The end of the Dirlight module (0x527460 - 0x5274D0)."""

import pytest

from emu import Emu
from test_animcam_util import NAN, SNAN, bits, fval, logfake, run, task, u16, u32

e = Emu()
logfake(e, 0x4B0B70, 1)  # Pool_Free
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(e, 0x5269C0, 1)
e.baseline = e.snapshot()

POOL = 0xBA1E00


def test_keyframe_get_params():
    def setup(e):
        u32(e, 0xA83D48, 0x4848)
        return task(e, 0x54), e.alloc(4)
    run(e, 0x527460, "DirlightKeyframe_GetParams", setup)


def test_keyframe_delete():
    def setup(e):
        d = task(e, 0x64)
        k = task(e, 0x54, parent=d)
        return (k,)
    run(e, 0x527470, "DirlightKeyframe_Delete", setup)


def test_close():
    def setup(e):
        u32(e, 0xA83D48, 0x4848)
        u32(e, 0xA83D30, 0x3030)
        u32(e, 0xA83D4C, 0x12340066)
        u32(e, 0xA83D34, 0x56780065)
    run(e, 0x527480, "Dirlight_Close", setup)


def test_get_type():
    run(e, 0x5274C0, "Dirlight_GetType", lambda e: u32(e, 0xA83D34, 0xABCD0065), check_eax=True, eax_mask=0xFFFF)


@pytest.mark.parametrize("time", [0.0, 1.5, -2.25, NAN, SNAN])
@pytest.mark.parametrize("count", [0, 1, 3])
def test_set_time(time, count):
    times = [1.5, NAN, -2.25, 0.0]

    def setup(e):
        lights = []
        for t in times:
            d = task(e, 0x64)
            u32(e, d + 0x5C, bits(t))
            lights.append(d)
        data = e.alloc(4 * 8)
        for i, d in enumerate(lights):
            u32(e, data + 4 * (2 * i + 1), d)
        used = e.alloc(4 * 8)
        u32(e, used, 3, 1, 5, 7)
        u32(e, POOL, data, used, count, 8, 4, 4)
        return (fval(time),)
    run(e, 0x5274D0, "Dirlight_SetTime", setup)
