"""MoveRigidObj_Changed and MoveRigidObj_Shutdown (0x51FD60 - 0x51FEF0)."""
import random
import struct

import pytest

from test_litmap_util import add_child, cstr, d64, f32, handler, logfake, new_emu, rnd_task, run, set_handler, u16, u32

TYPE = 0x52
CHILD = 0x53

e = new_emu()
logfake(e, 0x4CEC10, 1, strings=(0,), ret=0x7700)  # VirModel_Find
logfake(e, 0x4C48D0, 2)  # RigidDynCubeObj_SetModel
logfake(e, 0x51FBA0, 3)
logfake(e, 0x401780, 1)  # Task_DeleteChildren
logfake(e, 0x4C4950, 3)
logfake(e, 0x4F1400, 2)  # VarString_Bind
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
e.write(0x54E0E5, bytes([0x31]))
set_handler(e, 0x31, CHILD, handler(e, "changed", 2))
e.baseline = e.snapshot()

NAN = struct.unpack("<f", b"\x00\x00\xc0\x7f")[0]


def obj(e, changes, children, rng):
    m = rnd_task(e, 0x1E0, TYPE, rng)
    u32(e, m + 0xE8, 0x5150)
    e.write(m + 0x180, b"model\0")
    for i in range(2):
        d64(e, m + 0xF0 + 24 * i, 1.0 + i, 2.0, 3.0)
        d64(e, m + 0x120 + 24 * i, 1.0 + i, 2.0, 3.0)
        for k in range(3):
            f32(e, m + 0x150 + 8 * k + 4 * i, 0.5 * k + i)
            f32(e, m + 0x168 + 8 * k + 4 * i, 0.5 * k + i)
    for i, what in changes:
        if what == "pos":
            d64(e, m + 0x120 + 24 * i + 8, 2.5)
        elif what == "nan":
            f32(e, m + 0x150 + 4 * i, NAN)
            f32(e, m + 0x168 + 4 * i, NAN)
        else:
            f32(e, m + 0x168 + 8 * what + 4 * i, -7.0)
    for i in range(children):
        add_child(e, m, rnd_task(e, 0x40, CHILD if i != 1 else CHILD + 1, rng))
    return m


@pytest.mark.parametrize("changes", [[], [(0, "pos")], [(1, "pos")], [(0, 0)], [(1, 1)], [(1, 2)], [(0, 2), (1, 0)],
                                     [(0, "nan")], [(1, "nan")]])
@pytest.mark.parametrize("children,arg", [(0, None), (3, None), (3, b""), (3, b"x")])
def test_changed(changes, children, arg):
    def setup(e):
        rng = random.Random(len(changes) * 7 + children)
        m = obj(e, changes, children, rng)
        return m, cstr(e, arg) if arg is not None else 0
    run(e, 0x51FD60, "MoveRigidObj_Changed", setup)


def test_shutdown():
    def setup(e):
        u32(e, 0xA837D0, 0x1234)
        u16(e, 0xA837CC, 0x77)
    run(e, 0x51FED0, "MoveRigidObj_Shutdown", setup)
