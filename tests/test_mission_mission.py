"""Mission records and mission scripts (0x484E60 - 0x4851B0)."""
import pytest

from emu import ErrorShown
from test_mission_util import RET, cstr, make_emu, run, u32

MISSIONS, COUNT, SIZE = 0x5C1590, 0x5C89FC, 0x4A8

emu = make_emu()
OUT = emu.alloc(24)
emu.baseline = emu.snapshot()


def entry(e, name, isdir):
    ent = e.alloc(12)
    u32(e, ent, cstr(e, name), 0)
    e.write(ent + 8, bytes([isdir]))
    return ent


@pytest.mark.parametrize("isdir", [0, 1])
@pytest.mark.parametrize("qvm", [0, 1])
@pytest.mark.parametrize("match", [0, 1])
def test_open(isdir, qvm, match):
    def setup(e):
        RET[0x48F340] = qvm
        RET[0x4B09C0] = match
        return (entry(e, b"LOCAL:missions/Location0/Level1/Mission.QSC", isdir),)
    run(emu, 0x484E60, "MissionOpen", setup)


def test_open_fails():
    msgs = []
    for fn in (0x484E60, "MissionOpen"):
        emu.reset()
        RET.clear()
        RET[0x4B09C0] = 1
        RET[0x4B80B0] = 0
        with pytest.raises(ErrorShown) as err:
            emu.call(fn, entry(emu, b"x/mission.qsc", 0))
        msgs.append((str(err.value), emu.read_cstr(emu.arg(1))))
    assert msgs[0] == msgs[1] == ("Mission_Open(): Couldn't load script: %s", b"mission.qsc")


def test_init():
    run(emu, 0x484F30, "Mission_Init", lambda e: u32(e, COUNT, 5))


def add_missions(e, ids):
    e.write(MISSIONS, b"\x55" * SIZE * (len(ids) + 2))
    for i, m in enumerate(ids):
        u32(e, MISSIONS + i * SIZE, m)
    u32(e, COUNT, len(ids))


@pytest.mark.parametrize("ids", [[], [1, 2, 3]])
def test_define(ids):
    def setup(e):
        add_missions(e, ids)
        return OUT, 0, 0x5A5A0
    ro, rn = run(emu, 0x484F60, "DefineMission", setup, ignore=[(OUT + 4, OUT + 8), (OUT + 20, OUT + 24)],
                 check_eax=True)
    assert ro[0] == OUT


def test_define_not_unique():
    msgs = []
    for fn in (0x484F60, "DefineMission"):
        emu.reset()
        RET.clear()
        add_missions(emu, [4, 1000, 9])
        with pytest.raises(ErrorShown) as err:
            emu.call(fn, OUT, 0, 0x5A5A0)
        msgs.append((str(err.value), emu.arg(1)))
    assert msgs[0] == msgs[1] == ("Mission ID %d not unique", 1000)


def test_close_and_load():
    run(emu, 0x485140, "Mission_Close", lambda e: u32(e, COUNT, 9))
    run(emu, 0x485150, "Mission_LoadAll", lambda e: u32(e, COUNT, 9))


@pytest.mark.parametrize("ids,find", [([], 1), ([1, 2, 3], 1), ([1, 2, 3], 3), ([1, 2, 3], 4), ([7, -1], -1)])
def test_find(ids, find):
    def setup(e):
        add_missions(e, ids)
        return (find,)
    run(emu, 0x485170, "Mission_Find", setup, check_eax=True)
