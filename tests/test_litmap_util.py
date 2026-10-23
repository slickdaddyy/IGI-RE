"""Helpers shared by the test_litmap tests."""
import struct

from emu import Emu
from test_graphnode_util import floatfake  # noqa: F401
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, task, task_system, trap, u16, u32)
from test_patrol_util import d64, run, with_cw  # noqa: F401
from test_vehicle_util import set_events

CWS = [0x37F, 0x7F]
FISTP_CWS = [0x37F, 0x7F, 0xC7F, 0x77F]

# parameter type getters read these; ParamType_New indexes its table with them
PARAM_TYPES = (0xA71880, 0xA71870, 0xA70C68, 0xA70C6C, 0xA71874, 0xA70C7C, 0xA70C9C, 0xA70C88)
# parent type getters: Position, RigidDynCubeObj, root
PARENT_TYPES = (0x54E634, 0x547F42)


def new_emu():
    e = Emu()
    e.baseline = e.snapshot()
    return e


def init_emu():
    """An Emu for the task type registration functions, with the original task system."""
    e = Emu()
    logfake(e, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
    e.baseline = e.snapshot()
    return e


def setup_init(e):
    task_system(e)
    set_events(e)
    for i, a in enumerate(PARAM_TYPES):
        u32(e, a, 2 + i)
    root = e.call(0x4018F0)[0] & 0xFFFF
    for a in PARENT_TYPES:
        t = e.call(0x401900, cstr(e, b"Parent%x" % a), 0xF0, root, 0)[0]
        u16(e, a, t)


def rnd_task(e, size, type_, rng):
    """A task of random bytes with an empty child list."""
    t = e.alloc(size)
    e.write(t, bytes(rng.randrange(256) for _ in range(size)))
    u32(e, t, 0, 0, t + 0xC, 0, t + 8, 0)
    u16(e, t + 0x1C, type_)
    return t
