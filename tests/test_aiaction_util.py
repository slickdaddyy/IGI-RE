"""Helpers shared by the aiaction tests (no tests in here)."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import ErrorShown, compare

GAME_PTR = 0x57BABC


class Fakes:
    """Python replacements of original functions that log their arguments.
    fmt(emu, args) may turn the arguments into what gets logged; ret may be a
    callable(emu, args)."""

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

    def add(self, addr, nargs=0, ret=0, st0=None, fmt=None):
        """st0: 'f' or 'd' returns ret as a float in st(0)."""
        fk = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = None
        if st0:
            stub = self.code((b"\xd9\x05" if st0 == "f" else b"\xdd\x05") + struct.pack("<I", cell) + b"\xc3")

        def cb(uc, a, size, user):
            em = fk.e
            args = [em.arg(i) for i in range(nargs)]
            rec = fmt(em, args) if fmt else [em.normalize(v) for v in args]
            fk.log.append((a, tuple(rec)))
            r = fk.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<f" if st0 == "f" else "<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r)
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


def check(e, fk, orig, ours, setup=None, cw=None, normalized=True, **kw):
    """compare() plus equal logs of the Python fakes. cw: x87 control word
    (0x7F is the single precision the game runs at)."""
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
    out = []
    for target in (orig, ours):
        e.reset()
        args = setup(e)
        fk.log = []
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        out.append((str(err.value), fk.log))
    assert out[0] == out[1]


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


def new_list(e, nodes=()):
    """An exec style list holding the given node addresses."""
    lst = e.alloc(12)
    prev = lst
    for n in nodes:
        e.write(n, u32(0) + u32(prev))
        e.write(prev, u32(n))
        prev = n
    e.write(prev, u32(lst + 4))
    e.write(lst + 4, u32(0) + u32(prev))
    return lst


def set_game(e, seed):
    """A game object whose random state (Game_GetRandom) depends on seed."""
    g = e.alloc(0x100)
    e.write(GAME_PTR, u32(g))
    e.write(g + 0x2C, struct.pack("<III", seed * 2654435761 & 0xFFFFFFFF, seed * 40503 + 7, seed ^ 0x5A5A1234))
    return g
