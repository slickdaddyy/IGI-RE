"""Helpers for the haze tests (no tests in here)."""
import random
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import ErrorShown, compare

HANDLERS = 0xA96AE0
CWS = [0x37F, 0x7F]
ROUND_CWS = [0x37F, 0x7F, 0xC7F, 0x77F]


class Fakes:
    """Python-side replacements of original functions that log their arguments.
    Pointer arguments can be logged by content, so stack addresses compare equal."""

    def __init__(self, e):
        self.e = e
        self.log = []
        self.rets = {}
        self.tramps = {}

    def code(self, data):
        p = self.e.fake_top
        self.e.fake_top += (len(data) + 15) & ~15
        self.e.write(p, data)
        return p

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None):
        """deref: {arg index: byte count} logs the bytes instead of the pointer.
        st0: 'f' to return a float in st(0); the value comes from ret.
        ret may be a function (emu, args) -> value."""
        e = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code(b"\xd9\x05" + struct.pack("<I", cell) + b"\xc3") if st0 else None

        def cb(uc, a, size, user):
            em = e.e
            args = [em.arg(i) for i in range(nargs)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = [em.read(v, deref[i]) if deref and i in deref else em.normalize(v) for i, v in enumerate(args)]
            e.log.append((a, tuple(rec)))
            r = e.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<f", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def handler(self, nargs, deref=None, fmt=None):
        """A task handler that only logs."""
        return self.add(self.code(b"\xc3"), nargs, deref=deref, fmt=fmt)


def with_cw(e, fk, target, cw):
    """Entry point that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    key = (addr, cw)
    if key not in fk.tramps:
        cell = fk.code(struct.pack("<H", cw))
        p = fk.code(b"\0" * 16)
        e.write(p, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        fk.tramps[key] = p
    return fk.tramps[key]


def check(e, fk, orig, ours, setup=None, normalized=True, cw=None, **kw):
    """compare() plus equal logs of the Python fakes, optionally at x87 control word cw."""
    logs = []
    if cw is not None:
        orig, ours = with_cw(e, fk, orig, cw), with_cw(e, fk, ours, cw)

    def s(em):
        r = setup(em) if setup else None
        fk.log = []
        logs.append(fk.log)
        return r
    res = compare(e, orig, ours, setup=s, normalized=normalized, **kw)
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return res


def check_error(e, orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        args = setup(e)
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def rbytes(n, seed):
    r = random.Random(seed)
    return bytes(r.randrange(256) for _ in range(n))


def new_task(e, type_, size, fill=True):
    t = e.alloc(size)
    if fill:
        e.write(t, rbytes(size, t))
    e.write(t + 8, struct.pack("<III", t + 0xC, 0, t + 8))
    e.write(t + 0x1C, struct.pack("<H", type_))
    return t


def add_child(e, parent, child):
    tailpred = e.u32(parent + 0x10)
    e.write(child, struct.pack("<II", parent + 0xC, tailpred))
    e.write(tailpred, struct.pack("<I", child))
    e.write(parent + 0x10, struct.pack("<I", child))
    e.write(child + 0x14, struct.pack("<I", parent))


def with_children(e, t, slot, handler, types):
    for ty in types:
        c = new_task(e, ty, 0x40)
        add_child(e, t, c)
        set_handler(e, slot, ty, handler)


def rmatrix(rng):
    return f32(*[rng.uniform(-1.5, 1.5) for _ in range(9)]) + struct.pack("<i", rng.randrange(100))


def task_system(e):
    """Memory, script symbol table, task system and the DynCubeObj type."""
    e.call(0x4B0BA0)
    buckets = e.alloc(12 * 97)
    for i in range(97):
        b = buckets + 12 * i
        e.write(b, struct.pack("<III", b + 4, 0, b))
    table = e.alloc(8)
    e.write(table, struct.pack("<II", buckets, 97))
    e.write(0xA43E88, struct.pack("<I", table))
    e.call(0x4010D0)
    e.call(0x4E5370)
    e.write(0xA5E408, b"\x20")
    e.write(0x548176, b"\x21")
