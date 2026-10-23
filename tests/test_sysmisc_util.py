"""Helpers for the sysmisc tests (no tests in here)."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import STACK_BASE, STACK_SIZE, ErrorShown, compare


class Fakes:
    """Python replacements of original functions that log their arguments.
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

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None, pop=0, fn=None):
        """deref: {arg index: byte count} logs the bytes instead of the pointer.
        st0: 'f' or 'd' to return a float in st(0); the value comes from ret.
        pop: bytes of arguments to pop (stdcall). fn(emu, args) runs before returning."""
        e = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = None
        if st0:
            stub = self.code((b"\xd9\x05" if st0 == "f" else b"\xdd\x05") + struct.pack("<I", cell) + b"\xc3")

        def cb(uc, a, size, user):
            em = e.e
            args = [em.arg(i) for i in range(nargs)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = []
                for i, v in enumerate(args):
                    if deref and i in deref:
                        rec.append(em.read(v, deref[i]))
                    else:
                        rec.append(em.normalize(v))
            e.log.append((a, tuple(rec)))
            if fn:
                fn(em, args)
            r = e.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<f" if st0 == "f" else "<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r, pop)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def handler(self, nargs, deref=None, pop=0):
        """A fresh function address that only logs."""
        return self.add(self.code(b"\xc3"), nargs, deref=deref, pop=pop)


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
    """compare() plus equal logs of the Python fakes."""
    logs = []
    if cw is not None:
        orig, ours = with_cw(e, fk, orig, cw), with_cw(e, fk, ours, cw)

    def s(em):
        fk.log = []
        logs.append(fk.log)
        return setup(em) if setup else None
    res = compare(e, orig, ours, setup=s, normalized=normalized, **kw)
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return res


def check_error(e, fk, orig, ours, setup=None, args=()):
    """Both stop in Error_Show with the same message and the same fake calls."""
    msgs, logs = [], []
    for target in (orig, ours):
        e.reset()
        fk.log = []
        logs.append(fk.log)
        call_args = (setup(e) if setup else None) or args
        with pytest.raises(ErrorShown) as err:
            e.call(target, *call_args)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return msgs[0]


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def norm(em, v):
    """Like emu.normalize, but stack addresses become "stack"."""
    return "stack" if STACK_BASE <= v < STACK_BASE + STACK_SIZE else em.normalize(v)


class Com:
    """Fake COM methods (stdcall) that log to fk: (stub, slot, args...). Stack pointers are
    logged as "stack", or as the bytes behind them with derefs={slot: {arg: size or fn(em, ptr)}}.
    rets / fns map a slot to a return value (or fn(em, args)) / a function run first."""

    def __init__(self, fk, nargs, size=48, derefs=None):
        self.rets, self.fns, self.slots = {}, {}, []
        derefs = derefs or {}
        for i in range(size):
            n = nargs.get(i, 1)

            def fmt(em, a, i=i):
                d = derefs.get(i, {})
                out = [i]
                for j, v in enumerate(a):
                    n = d.get(j)
                    if callable(n):
                        n = n(em, v)
                    out.append(em.read(v, n) if n else norm(em, v))
                return tuple(out)

            def ret(em, a, i=i):
                r = self.rets.get(i, 0)
                return r(em, a) if callable(r) else r

            def fn(em, a, i=i):
                if i in self.fns:
                    self.fns[i](em, a)
            self.slots.append(fk.add(fk.code(b"\xc3"), n, ret=ret, fmt=fmt, pop=4 * n, fn=fn))

    def new(self, em):
        vt = em.alloc(4 * len(self.slots))
        em.write(vt, b"".join(u32(s) for s in self.slots))
        obj = em.alloc(16)
        em.write(obj, u32(vt))
        return obj
