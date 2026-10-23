"""Helpers shared by the test_physobj tests."""
import re
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP

from emu import ErrorShown, compare

HANDLERS = 0xA96AE0
CWS = [0x37F, 0x7F, 0xC7F, 0x77F]


class Fakes:
    """Python replacements of original functions that log their arguments."""

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

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None, effect=None, masks=None):
        """deref: {arg: byte count} logs bytes instead of the pointer; masks: {arg: mask}.
        st0='d' returns ret as a double in st(0). effect(emu, args) runs before returning."""
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code(b"\xdd\x05" + struct.pack("<I", cell) + b"\xc3") if st0 else None

        def cb(uc, a, size, user):
            em = self.e
            args = [em.arg(i) for i in range(nargs)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = []
                for i, v in enumerate(args):
                    if deref and i in deref:
                        rec.append(em.read(v, deref[i]))
                    else:
                        rec.append(em.normalize(v & (masks or {}).get(i, 0xFFFFFFFF)))
            self.log.append((a, tuple(rec)))
            if effect:
                effect(em, args)
            r = self.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def handler(self, nargs=2, deref=None, fmt=None, effect=None):
        """A task handler that only logs."""
        return self.add(self.code(b"\xc3"), nargs, deref=deref, fmt=fmt, effect=effect)


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
        args = setup(e)
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]
    return msgs[0]


def u32(e, p, *v):
    e.write(p, struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v]))


def f32(e, p, *v):
    e.write(p, struct.pack("<%df" % len(v), *v))


def d64(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def cstr(e, s, size=None):
    p = e.alloc(size or len(s) + 1)
    e.write(p, s + b"\0")
    return p


def set_handler(e, slot, type_, fn):
    u32(e, HANDLERS + 4 * (slot * 0x180 + type_), fn)


def c_format(em, fmt, argp):
    """Formats like sprintf with %s, %d and %f; returns (text, logged args)."""
    out, logged = b"", []
    parts = re.split(rb"(%[-0-9.]*[sdf%])", fmt)
    for p in parts:
        if not p.startswith(b"%") or len(p) < 2:
            out += p
        elif p == b"%%":
            out += b"%"
        elif p.endswith(b"s"):
            s = em.read_cstr(em.u32(argp))
            argp += 4
            logged.append(s)
            out += (p.decode() % s.decode("latin1")).encode("latin1")
        elif p.endswith(b"d"):
            v = struct.unpack("<i", em.read(argp, 4))[0]
            argp += 4
            logged.append(v)
            out += (p.decode() % v).encode()
        else:
            raw = em.read(argp, 8)
            argp += 8
            logged.append(raw)
            out += (p.decode() % struct.unpack("<d", raw)[0]).encode()
    return out, logged


def fake_sprintf(fk):
    """crt_sprintf (0x4A53B3) in Python; logs the format and the argument values."""
    def ret(em, a):
        esp_args = em.uc.reg_read(UC_X86_REG_ESP) + 12
        text, _ = c_format(em, em.read_cstr(a[1]), esp_args)
        em.write(a[0], text + b"\0")
        return len(text)

    def log(em, a):
        esp_args = em.uc.reg_read(UC_X86_REG_ESP) + 12
        return (em.read_cstr(a[1]), tuple(c_format(em, em.read_cstr(a[1]), esp_args)[1]))
    return fk.add(0x4A53B3, 2, ret=ret, fmt=log)
