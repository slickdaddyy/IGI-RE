"""Helpers shared by the test_editobj tests."""
import random
import struct

from test_dxinit_util import with_cw
from test_elevator_util import all_events, setup_types as elevator_types
from test_vehicle_util import (LOG, RET, add_child, check, cstr, d64, f32, floatfake, handler, init_emu, logfake,  # noqa: F401
                               pyfake, set_handler, task, u16, u32, vfake)

CWS = [0x37F, 0x7F]
FISTP_CWS = [0x37F, 0x7F, 0xC7F, 0x77F]

# event getters this range needs that the other helpers leave unset: global, value, width
EVENTS = [(0x54DDA4, 0x39, 1), (0x54E830, 0x3A, 1), (0x54E831, 0x3B, 1), (0x5488C8, 0x3C, 4), (0x54E70A, 0x3D, 1),
          (0x54E70B, 0x3E, 1), (0x54E71C, 0x3F, 1), (0x54E71D, 0x40, 1), (0x54E71E, 0x41, 1)]

CHAIN = []


def set_events(e):
    all_events(e)
    for a, v, n in EVENTS:
        e.write(a, v.to_bytes(n, "little"))


def setup_types(e):
    """The original task system with the parent types of this range."""
    elevator_types(e)
    set_events(e)
    u16(e, 0xA774B0, e.call(0x401900, cstr(e, b"PhysicsObj"), 0x720, e.u32(0x547F42) & 0xFFFF, 0)[0] & 0xFFFF)


def run(e, orig, ours, setup, cw=None, **kw):
    """check() with normalized memory and call logs, optionally at x87 control word cw."""
    RET.clear()
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def chain_fakes(e):
    """Task_FindChild / Task_FindNext walk the task list CHAIN."""
    def child(e):
        LOG[-1].append(("child", e.normalize(e.arg(0)), e.arg(1) & 0xFFFF))
        e.ret(CHAIN[0] if CHAIN else 0)

    def next_(e):
        t = e.arg(0)
        LOG[-1].append(("next", e.normalize(t), e.arg(1) & 0xFFFF))
        i = CHAIN.index(t) + 1 if t in CHAIN else len(CHAIN)
        e.ret(CHAIN[i] if i < len(CHAIN) else 0)
    pyfake(e, 0x401BE0, child)
    pyfake(e, 0x401C40, next_)


def mat(e, p, rng, unk=3):
    f32(e, p, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, p + 0x24, unk)


def obj(e, size, type_, rng, scale=(1.0, 2.0, 1.5), parent=0):
    """A RigidDynCubeObj style task with position, model, matrix and scale set."""
    t = task(e, size, type_, parent)
    d64(e, t + 0x20, rng.uniform(-5000, 5000), rng.uniform(-5000, 5000), rng.uniform(-500, 500))
    u32(e, t + 0x68, 0x6868, 0x6C6C)
    mat(e, t + 0x70, rng)
    f32(e, t + 0x98, *scale)
    return t


def strz(e, p, s):
    e.write(p, s + b"\0")


def rnd(seed):
    return random.Random(seed)


def f32v(e, p, n):
    return struct.unpack("<%df" % n, e.read(p, 4 * n))


def sprintf_fake(e):
    """crt sprintf done in Python (the CRT float formatting needs imports), logging the text."""
    import re

    def fn(e):
        buf, fmt = e.arg(0), e.read_cstr(e.arg(1)).decode("latin1")
        i, vals = 2, []
        for conv in re.findall(r"%[-+ #0-9.]*([sdifxc%])", fmt):
            if conv == "%":
                continue
            if conv == "s":
                vals.append(e.read_cstr(e.arg(i)).decode("latin1"))
            elif conv == "f":
                vals.append(struct.unpack("<d", struct.pack("<II", e.arg(i), e.arg(i + 1)))[0])
                i += 1
            else:
                vals.append(e.arg(i))
            i += 1
        text = (fmt % tuple(vals)).encode("latin1")
        if LOG:
            LOG[-1].append(("sprintf", fmt, tuple(vals)))
        e.write(buf, text + b"\0")
        e.ret(len(text))
    pyfake(e, 0x4A53B3, fn)
