"""Helpers shared by the test_bonemesh tests."""
import struct

from emu import STACK_BASE, STACK_SIZE, Emu

from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, task, trap, u16, u32)
from test_vehicle_util import d64, floatfake, run, vfake  # noqa: F401

CW = {}


def with_cw(e, target, cw):
    """Entry that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    key = (id(e), addr, cw)
    if key not in CW:
        p = e.fake_top
        e.fake_top += 32
        e.write(p + 16, struct.pack("<H", cw))
        e.write(p, b"\xd9\x2d" + struct.pack("<I", p + 16) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        e.baseline = e.snapshot()
        CW[key] = p
    return CW[key]


def make_emu():
    e = Emu()
    e.baseline = e.snapshot()
    return e


def com(e, name, nargs, effects=None, derefs=None, size=64):
    """A fake COM object: vtable slot i logs (name.i, args) and pops nargs[i] (with this, default 1).
    effects[i](e, args) may write outputs and return the HRESULT; derefs[i] maps an argument
    index to a byte count, or to a function (e, args) -> value, logged instead of the pointer."""
    effects = effects or {}
    derefs = derefs or {}
    obj = e.alloc(8)
    vt = e.alloc(4 * size)
    u32(e, obj, vt)
    for i in range(size):
        def fn(e, i=i):
            n = nargs.get(i, 1)
            args = [e.arg(k) for k in range(n)]
            if LOG:
                d = derefs.get(i, {})
                shown = []
                for k, a in enumerate(args):
                    if k in d:
                        shown.append(d[k](e, args) if callable(d[k]) else e.read(a, d[k]))
                    elif STACK_BASE <= a < STACK_BASE + STACK_SIZE:
                        shown.append("stack")
                    else:
                        shown.append(e.normalize(a))
                LOG[-1].append(("%s.%d" % (name, i),) + tuple(shown))
            r = effects[i](e, args) if i in effects else 0
            e.ret(r or 0, 4 * n)
        u32(e, vt + 4 * i, trap(e, "com_%s_%d" % (name, i), fn))
    e.baseline = e.snapshot()
    return obj


def f32s(e, p, n):
    return struct.unpack("<%df" % n, e.read(p, 4 * n))
