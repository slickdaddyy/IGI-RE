"""The task type Open and Close functions of 0x5183D0 - 0x51C090."""
import pytest

from emu import Emu
from test_nodereg_util import logfake, logger, pyfake, reg_fakes, run, u8, u16, u32

emu = Emu()
reg_fakes(emu)

# type and table globals the Close functions read
GLOBALS16 = (0x54EC9C, 0xA81014, 0xBA2028, 0x54ED3C, 0x54ED4C, 0x54ED4E, 0x54ED50, 0xA83764, 0xA8376C, 0xA83774,
             0x54EE3C)
GLOBALS32 = (0xA801FC, 0xA80300, 0xA80304, 0xA83744, 0xA8374C, 0xA83754, 0xA83760, 0xA83778)


def state(e):
    for i, a in enumerate(GLOBALS16):
        u16(e, a, 0x70 + i)
    for i, a in enumerate(GLOBALS32):
        u32(e, a, 0x10000 + 0x100 * i)
    u8(e, 0x54ED52, 0x51)
    u8(e, 0x54ED53, 0x52)


OPENS = [
    (0x518760, "GraphNode_Open"), (0x518900, "GraphNode_Close"),
    (0x518EF0, "Water_Open"), (0x519650, "Water_Close"),
    (0x519710, "GFXCapsViewer_Open"), (0x519750, "GFXCapsViewer_Close"),
    (0x519760, "ShadowTask_Open"), (0x519820, "ShadowTask_Close"),
    (0x519850, "SplinePathNode_Open"), (0x519BA0, "SplinePathNode_Close"),
    (0x51A1D0, "SplinePath_Open"), (0x51A260, "SplinePath_Close"),
    (0x51B140, "SplinePathGuide_Open"), (0x51B3A0, "SplinePathGuide_Close"),
    (0x51B400, "AnimSound_Open"), (0x51B530, "AnimSound_Close"),
    (0x51B960, "EditorMagicObj_Open"), (0x51BAB0, "EditorMagicObj_Close"),
    (0x51BAC0, "LevelTimer_Open"), (0x51BF90, "LevelTimer_Close"),
    (0x51BFB0, "SoundManager_Open"),
]


@pytest.mark.parametrize("orig,ours", OPENS)
def test_open_close(orig, ours):
    run(emu, orig, ours, state)


def test_task_types():
    """TaskTypes_Open and TaskTypes_Close with every callee replaced by a logger."""
    e = Emu()
    for a, ours in OPENS:
        logfake(e, a)
        pyfake(e, e.symbols[ours], logger(e, a))
    for orig, ours, stop in ((0x5183D0, "TaskTypes_Open", 0x518550), (0x518550, "TaskTypes_Close", 0x5186C0)):
        for a in callees(orig, stop):
            if a not in dict(OPENS):
                logfake(e, a)
        e.baseline = e.snapshot()
        run(e, orig, ours)


def callees(start, stop):
    """Targets of the call and jmp rel32 instructions in start..stop."""
    import pefile
    from emu import GAME_EXE
    pe = pefile.PE(GAME_EXE, fast_load=True)
    data = pe.get_data(start - pe.OPTIONAL_HEADER.ImageBase, stop - start)
    out = []
    i = 0
    while i + 5 <= len(data):
        if data[i] in (0xE8, 0xE9):
            out.append((start + i + 5 + int.from_bytes(data[i + 1:i + 5], "little", signed=True)) & 0xFFFFFFFF)
            i += 5
        else:
            i += 1
    return sorted(set(out))
