"""Helpers shared by the test_loadscreen tests."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import STACK_BASE, STACK_SIZE, ErrorShown, compare

CWS = [0x37F, 0x7F]


def norm(em, v):
    """A logged word: stack addresses differ between the two runs, so they all read 'stack'."""
    if STACK_BASE <= v < STACK_BASE + STACK_SIZE:
        return "stack"
    return em.normalize(v)


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

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None, effect=None):
        """deref: {arg index: byte count} logs the bytes instead of the pointer.
        st0: 'f' or 'd' to return a float in st(0); the value comes from ret.
        ret may be callable(emu, args); effect(emu, args) runs before returning."""
        e = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code(b"\xd9\x05" + struct.pack("<I", cell) + b"\xc3" if st0 == "f" else
                         b"\xdd\x05" + struct.pack("<I", cell) + b"\xc3") if st0 else None

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
                        rec.append(norm(em, v))
            e.log.append((a, tuple(rec)))
            if effect:
                effect(em, args)
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


def check_error(e, orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        args = setup(e)
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]


def w32(e, p, *v):
    e.write(p, struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v]))


def wf(e, p, *v):
    e.write(p, struct.pack("<%df" % len(v), *v))


def wd(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def list_new(e, nodes, lst=None):
    """An Amiga style list (12 bytes, at lst or new) holding the given node addresses."""
    lst = lst or e.alloc(12)
    prev = lst
    for n in nodes:
        w32(e, prev, n)
        w32(e, n + 4, prev)
        prev = n
    w32(e, prev, lst + 4)
    w32(e, lst + 4, 0)
    w32(e, lst + 8, prev)
    return lst
