"""Helpers shared by the test_freecam tests."""
import struct

import pytest

from emu import Emu, ErrorShown
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger,  # noqa: F401
                            pyfake, set_handler, task, task_system, trap, u16, u32)
from test_patrol_util import d64, with_cw  # noqa: F401

CWS = [0x37F, 0x7F]
FISTP_CWS = [0x37F, 0x7F, 0xC7F, 0x77F]

# event slot getters: RigidDynCubeObj_GetEvent0/1/2, MagicObj_GetMoveEvent
EVENTS = {0x4C56D0: (0x547F3C, 0x31), 0x4C56E0: (0x547F3D, 0x32), 0x4C56F0: (0x547F3E, 0x33),
          0x4D9FB0: (0x54864A, 0x34)}


def new_emu():
    e = Emu()
    e.baseline = e.snapshot()
    return e


def set_events(e):
    for cell, v in EVENTS.values():
        e.write(cell, bytes([v]))


def event(getter):
    return EVENTS[getter][1]


def fake_both(e, addr, name, nargs=0, **kw):
    """Log calls of the original at addr and of our rewrite of it under the same tag."""
    logfake(e, addr, nargs, **kw)
    pyfake(e, e.symbols[name], logger(e, addr, nargs, **kw))


def run(e, orig, ours, setup=None, cw=None, **kw):
    """check() with normalized memory, optionally at an x87 control word."""
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def run_error(e, orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        args = setup(e) or ()
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def f32b(*v):
    return struct.pack("<%df" % len(v), *v)


def mem_init(e):
    e.call(0x4B0BA0)
