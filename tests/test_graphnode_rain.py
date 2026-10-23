"""The end of the RainDynCubeObj module (0x4F80B0 - 0x4F8190)."""
import pytest

from test_graphnode_util import RET, d64, logfake, new_emu, run, u16, u32

e = new_emu()
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free

RAINS = 0xA76CA4
CELLS = {}


def find_cell(e, a):
    return CELLS.get(a[0], 0)


logfake(e, 0x4F65E0, 2, effect=find_cell, derefs={1: 24})


def test_shutdown():
    def setup(e):
        u32(e, 0xA76CCC, 0x1234)
        u16(e, 0x54E2CC, 0x77)
    run(e, 0x4F80B0, "RainDynCubeObj_Shutdown", setup)


def test_set_callback():
    run(e, 0x4F80E0, "RainDynCubeObj_SetCallback", None, args=(0x401234,))


# rain slots present -> flag of its cell (None: no cell)
RAIN_CASES = [{}, {1: None, 4: None}, {4: 0}, {4: 1}, {1: None, 4: 0, 7: 1}, {i: (2 if i == 7 else None) for i in range(8)}]


@pytest.mark.parametrize("case", RAIN_CASES)
def test_is_raining_at(case):
    def setup(e):
        CELLS.clear()
        rains = [0] * 8
        for i, flag in case.items():
            r = e.alloc(16)
            rains[i] = r
            if flag is not None:
                c = e.alloc(0x20)
                u32(e, c + 0x1C, flag)
                CELLS[r] = c
        u32(e, RAINS, *rains)
        p = e.alloc(24)
        d64(e, p, 1.0, 2.0, 3.0)
        return (p,)
    run(e, 0x4F80F0, "RainDynCubeObj_IsRainingAt", setup, check_st0=True)


@pytest.mark.parametrize("mask", [0, 0x81, 0xFF, 0x26])
def test_get_hits(mask):
    def setup(e):
        rains = []
        for i in range(8):
            if mask >> i & 1:
                r = e.alloc(0x4290)
                u32(e, r + 0x4288, 10 + i)
                rains.append(r)
            else:
                rains.append(0)
        u32(e, RAINS, *rains)
        out = e.alloc(0x50)
        u32(e, out + 0x40, 99)
        return out, out + 0x20, out + 0x40
    run(e, 0x4F8140, "RainDynCubeObj_GetHits", setup)
    RET.clear()
