"""Helpers shared by the test_dirlight tests."""
import struct

import pytest

from emu import ErrorShown
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, task, trap, u16, u32)
from test_patrol_util import run, with_cw  # noqa: F401

CWS = [0x37F, 0x7F, 0xC7F, 0x77F]

MATCH_EVENT, LOAD_EVENT, EVENT4 = 0x548365, 0x548367, 0x548368
TABLE_EVENT, CHANGED_EVENT = 0x54E0E4, 0x54E0E5
LIGHTSOURCE_POOL = 0xA83CF8
DIRLIGHT_POOL = 0xBA1E00


def u8(e, p, *v):
    e.write(p, bytes(v))


def rd_f32(e, p, n=1):
    return struct.unpack("<%df" % n, e.read(p, 4 * n))


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def pools(e):
    """Mem and the two light source pools, with original code."""
    e.call(0x4B0BA0)
    e.call(0x4B0B00, LIGHTSOURCE_POOL, 4, 100, 0)
    e.call(0x4B0B00, DIRLIGHT_POOL, 4, 3, 4)


def check_error(e, orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        args = setup(e)
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]
