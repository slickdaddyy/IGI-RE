"""Helpers for the Human base type tests (no tests in here)."""
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import compare

HANDLERS = 0xA96AE0


class Fakes:
    """Original functions replaced by Python code that logs the arguments.
    deref={i: n} logs n bytes behind pointer argument i instead of the pointer."""

    def __init__(self, e):
        self.e = e
        self.log = []
        self.rets = {}
        self.tramps = {}
        self.added = set()

    def code(self, data):
        p = self.e.fake_top
        self.e.fake_top += (len(data) + 15) & ~15
        self.e.write(p, data)
        return p

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None, act=None):
        """st0: 'f' or 'd' returns ret in st(0). act(e, args) runs before returning."""
        assert addr not in self.added, hex(addr)
        self.added.add(addr)
        f = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = None
        if st0:
            stub = self.code((b"\xd9\x05" if st0 == "f" else b"\xdd\x05") + struct.pack("<I", cell) + b"\xc3")

        def cb(uc, a, size, user):
            em = f.e
            args = [em.arg(i) for i in range(nargs)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = [em.read(v, deref[i]) if deref and i in deref else em.normalize(v) for i, v in enumerate(args)]
            f.log.append((a, tuple(rec)))
            if act:
                act(em, args)
            r = f.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<f" if st0 == "f" else "<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def handler(self, nargs=2, deref=None, act=None):
        """A task handler that only logs."""
        return self.add(self.code(b"\xc3"), nargs, deref=deref, act=act)


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
    """compare() plus equal logs of the Python fakes. cw: x87 control word to run with
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
    return res, logs[0]


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


def w32(e, p, v):
    e.write(p, struct.pack("<I", v & 0xFFFFFFFF))


def w16(e, p, v):
    e.write(p, struct.pack("<H", v))


def w8(e, p, v):
    e.write(p, bytes([v & 0xFF]))


def wf(e, p, *v):
    e.write(p, struct.pack("<%df" % len(v), *v))


def wd(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def rf(e, p, n=1):
    v = struct.unpack("<%df" % n, e.read(p, 4 * n))
    return v[0] if n == 1 else v


def rd(e, p, n=1):
    v = struct.unpack("<%dd" % n, e.read(p, 8 * n))
    return v[0] if n == 1 else v
