"""Helpers for the Mine, M203, Grenade, gun part and Gun tests (no tests in here)."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import Emu, ErrorShown, compare

HANDLERS = 0xA96AE0


class Fakes:
    """Python replacements of original functions that log their arguments.
    deref={i: n} logs n bytes behind argument i, fmt(emu, args) builds the record."""

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

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None, mask=None):
        """st0: 'f' or 'd' returns ret as a float in st(0). ret may be callable(emu, args)."""
        e = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code(b"\xd9\x05" + struct.pack("<I", cell) + b"\xc3" if st0 == "f" else
                         b"\xdd\x05" + struct.pack("<I", cell) + b"\xc3") if st0 else None

        def cb(uc, a, size, user):
            em = e.e
            args = [em.arg(i) for i in range(nargs)]
            if mask:
                args = [v & m for v, m in zip(args, mask)] + args[len(mask):]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = [em.read(v, deref[i]) if deref and i in deref else em.normalize(v)
                       for i, v in enumerate(args)]
            e.log.append((a, tuple(rec)))
            r = e.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<f" if st0 == "f" else "<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r or 0)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def handler(self, nargs, deref=None, fmt=None, ret=0):
        """A task handler that only logs (and may write through its arguments in ret)."""
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
    """compare() plus equal logs of the Python fakes. cw: x87 control word to run with."""
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


def check_error(e, fk, orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        args = setup(e)
        fk.log = []
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append((str(err.value), fk.log))
    assert msgs[0] == msgs[1]


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def u32(*v):
    return struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v])


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def new_task(e, type_, size):
    t = e.alloc(size)
    e.write(t + 8, struct.pack("<III", t + 0xC, 0, t + 8))
    e.write(t + 0x1C, struct.pack("<H", type_))
    return t


def add_child(e, parent, child):
    tailpred = e.u32(parent + 0x10)
    e.write(child, struct.pack("<II", parent + 0xC, tailpred))
    e.write(tailpred, struct.pack("<I", child))
    e.write(parent + 0x10, struct.pack("<I", child))
    e.write(child + 0x14, struct.pack("<I", parent))


def ref(e, task):
    """A TaskRef: a cell holding the task."""
    p = e.alloc(8)
    e.write(p, u32(task))
    return p


def rmatrix(rng):
    return f32(*[rng.uniform(-1.5, 1.5) for _ in range(9)]) + struct.pack("<i", rng.randrange(100))


def rvec(rng, scale=5000.0):
    return f64(*[rng.uniform(-scale, scale) for _ in range(3)])


def getter(e, addr):
    """Value a byte/word/dword getter (mov al/ax/eax, [g]; ret) returns from its global."""
    c = e.read(addr, 6)
    if c[0] == 0xA0:
        return struct.unpack("<I", c[1:5])[0], 1
    if c[:2] == b"\x66\xa1":
        return struct.unpack("<I", c[2:6])[0], 2
    if c[0] == 0xA1:
        return struct.unpack("<I", c[1:5])[0], 4
    raise AssertionError(hex(addr))


def set_getter(e, addr, value):
    g, n = getter(e, addr)
    e.write(g, value.to_bytes(4, "little")[:n])


CWS = pytest.mark.parametrize("cw", [0x37F, 0x7F])

__all__ = ["Emu", "ErrorShown", "Fakes", "check", "check_error", "set_handler", "f32", "f64", "u32", "cstr",
           "new_task", "add_child", "ref", "rmatrix", "rvec", "set_getter", "CWS", "with_cw"]
