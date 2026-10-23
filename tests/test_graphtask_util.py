"""Shared helpers for the graphtask tests (no tests in here)."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import ErrorShown, compare

HANDLERS = 0xA96AE0
CWS = [0x37F, 0x7F, 0xC7F, 0x77F]


class Fakes:
    """Original functions replaced by Python code that logs its arguments.
    deref={arg: bytes} logs the bytes behind a pointer instead of the pointer."""

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

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None, skip=()):
        """st0: 'f' or 'd' returns ret as a float in st(0). ret may be fn(emu, args).
        skip: argument indexes left out of the log."""
        me = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = (self.code((b"\xd9\x05" if st0 == "f" else b"\xdd\x05") + struct.pack("<I", cell) + b"\xc3")
                if st0 else None)

        def cb(uc, a, size, user):
            em = me.e
            args = [em.arg(i) for i in range(nargs)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = []
                for i, v in enumerate(args):
                    if i in skip:
                        continue
                    if deref and i in deref:
                        rec.append(em.read(v, deref[i]))
                    else:
                        rec.append(em.normalize(v))
            me.log.append((a, tuple(rec)))
            r = me.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<f" if st0 == "f" else "<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r or 0)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def handler(self, nargs, deref=None, ret=0, fmt=None):
        """A fresh function (task handler, callback) that only logs."""
        return self.add(self.code(b"\xc3"), nargs, ret=ret, deref=deref, fmt=fmt)


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
        args = setup(e) or ()
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


def i32(*v):
    return struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v])


def task(e, size, type_, parent=None):
    """A zeroed task with an empty child list, appended to parent's children."""
    p = e.alloc(size)
    e.write(p + 8, i32(p + 0xC, 0, p + 8))
    e.write(p + 0x1C, struct.pack("<H", type_))
    if parent:
        lst = parent + 8
        last = e.u32(lst + 8)
        e.write(p, i32(lst + 4, last))
        e.write(last, i32(p))
        e.write(lst + 8, i32(p))
        e.write(p + 0x14, i32(parent))
    return p
