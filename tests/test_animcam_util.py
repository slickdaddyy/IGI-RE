"""Helpers shared by the test_animcam tests."""
import re
import struct

from emu import Emu, compare
from test_dxinit_util import with_cw
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, task, task_system, trap, u16, u32)
from test_vehicle_util import d64, floatfake  # noqa: F401

CWS = [0x37F, 0x7F]
FISTP_CWS = [0x37F, 0x7F, 0xC7F, 0x77F]
NAN = float("nan")
SNAN = 0x7F800001
NO_KEY = -999.0

TABLE_EVENT, CHANGED_EVENT = 0x30, 0x31
CONTROL_EVENT, INFO_EVENT, SIZE_EVENT, SAVE_EVENT, LOAD_EVENT, KEYS_EVENT = 0x33, 0x34, 0x35, 0x36, 0x37, 0x38
EVENTS = {0x54E0E4: TABLE_EVENT, 0x54E0E5: CHANGED_EVENT, 0x54E1DA: CONTROL_EVENT, 0x54E1DC: INFO_EVENT,
          0x54E1DD: SIZE_EVENT, 0x54E1DE: SAVE_EVENT, 0x54E1DF: LOAD_EVENT, 0x54E1E0: KEYS_EVENT}

KEYS, KEYS_PREV = 0xC28C62, 0xC28D62


def bits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def fval(v):
    """A float argument as pushed on the stack; ints are raw bits."""
    return v if isinstance(v, int) else bits(v)


def floats(e, p, n):
    return struct.unpack("<%df" % n, e.read(p, 4 * n))


def set_events(e):
    for a, v in EVENTS.items():
        e.write(a, bytes([v]))


def run(e, orig, ours, setup, cw=None, **kw):
    """check() with normalized memory and call logs, optionally at x87 control word cw."""
    RET.clear()
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def printf_fake(e, addr, tag=None):
    """A printf style function that logs its formatted text."""
    def fn(e):
        fmt = e.read_cstr(e.arg(0)).decode("latin1")
        i, vals = 1, []
        for conv in re.findall(r"%[-+ #0-9.]*([sdifxc%])", fmt):
            if conv == "%":
                continue
            if conv == "s":
                vals.append(e.read_cstr(e.arg(i)).decode("latin1"))
            elif conv == "f":
                vals.append(struct.unpack("<d", struct.pack("<II", e.arg(i), e.arg(i + 1)))[0])
                i += 1
            else:
                vals.append(struct.unpack("<i", struct.pack("<I", e.arg(i)))[0])
            i += 1
        text = fmt % tuple(vals)
        if LOG:
            LOG[-1].append((tag or addr, text))
        e.ret(len(text))
    pyfake(e, addr, fn)


def chain_fakes(e, chain):
    """Task_FindChild / Task_FindNext walk the list chain (filled by the test)."""
    def child(e):
        if LOG:
            LOG[-1].append(("child", e.normalize(e.arg(0)), e.arg(1) & 0xFFFF))
        e.ret(chain[0] if chain else 0)

    def next_(e):
        t = e.arg(0)
        if LOG:
            LOG[-1].append(("next", e.normalize(t), e.arg(1) & 0xFFFF))
        i = chain.index(t) + 1 if t in chain else len(chain)
        e.ret(chain[i] if i < len(chain) else 0)
    pyfake(e, 0x401BE0, child)
    pyfake(e, 0x401C40, next_)


def find_by_id_fake(e, tasks):
    """Task_FindById returns tasks[id] (filled by the test)."""
    def fn(e):
        i = struct.unpack("<i", struct.pack("<I", e.arg(0)))[0]
        if LOG:
            LOG[-1].append(("find", i))
        e.ret(tasks.get(i, 0))
    pyfake(e, 0x4F1030, fn)


def info_handler(e, infos):
    """A slot 7 (GetInfo) handler that fills the info from infos[task]."""
    def fn(e):
        t, out = e.arg(0), e.arg(1)
        if LOG:
            LOG[-1].append(("info", e.normalize(t), e.u32(out + 0x50)))
        pos, mat, unk = infos[t]
        d64(e, out, *pos)
        f32(e, out + 0x24, *mat)
        u32(e, out + 0x48, unk)
        e.ret(0)
    return trap(e, "info", fn)


PARAM_GLOBALS = list(range(0xA70C60, 0xA70CAC, 4)) + list(range(0xA71860, 0xA71890, 4)) + [0xA75890, 0xA75898]


def setup_open(e):
    """The original task system, the events and parameter types, and the parent types."""
    task_system(e)
    set_events(e)
    for i, a in enumerate(PARAM_GLOBALS):
        u32(e, a, 0x100 + i)
    root = e.call(0x4018F0)[0] & 0xFFFF
    for g, name, size in ((0x54F768, b"CamTask", 0x88), (0x547F42, b"RigidDynCubeObj", 0xF0),
                          (0x54E1D8, b"AnimTask", 0x20160)):
        u16(e, g, e.call(0x401900, cstr(e, name), size, root, 0)[0] & 0xFFFF)
    u32(e, 0x54E1E4, 0x1EE)


def open_emu():
    e = Emu()
    logfake(e, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
    e.baseline = e.snapshot()
    return e


def compare_open(e, orig, ours):
    compare(e, orig, ours, setup=setup_open, check_eax=False, normalized=True)
