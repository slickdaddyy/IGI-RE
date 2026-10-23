"""Helpers for the sector and shadow tests (no tests in here)."""
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import compare

HANDLERS = 0xA96AE0
CWS = [0x37F, 0x7F, 0xC7F, 0x77F]


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

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None, key=None):
        """deref: {arg index: byte count} logs the bytes instead of the pointer.
        st0: 'f' or 'd' to return a float in st(0); the value comes from ret.
        ret may be a function (emu, args) -> value. key: what to log instead of addr."""
        e = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code((b"\xd9\x05" if st0 == "f" else b"\xdd\x05") + struct.pack("<I", cell) + b"\xc3") \
            if st0 else None

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
            e.log.append((key or a, tuple(rec)))
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

    def handler(self, nargs, deref=None):
        """A task handler that only logs."""
        return self.add(self.code(b"\xc3"), nargs, deref=deref)


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
        fk.log = []
        r = setup(em) if setup else None
        fk.log = []
        logs.append(fk.log)
        return r
    kw.setdefault("check_eax", False)
    res = compare(e, orig, ours, setup=s, normalized=normalized, **kw)
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return res, logs[0]


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def i32(e, p, v):
    e.write(p, struct.pack("<I", v & 0xFFFFFFFF))


def u16(e, p, v):
    e.write(p, struct.pack("<H", v & 0xFFFF))


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


def new_task(e, type_, size=0x40):
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


def with_children(e, t, slot, handler, n=2, type0=0x40, size=0x40):
    kids = []
    for i in range(n):
        c = new_task(e, type0 + i, size)
        add_child(e, t, c)
        set_handler(e, slot, type0 + i, handler)
        kids.append(c)
    return kids


def pool_init(e, p, stride, count):
    e.call(0x4B0B00, p, stride, count, 0)


def pool_take(e, p):
    data, slots, used, cap, stride, off = struct.unpack("<6I", e.read(p, 24))
    item = data + e.u32(slots + 4 * used) * stride
    i32(e, item + off, used)
    i32(e, p + 8, used + 1)
    return item


def rmatrix(rng, lo=-1.5, hi=1.5):
    return f32(*[rng.uniform(lo, hi) for _ in range(9)]) + struct.pack("<i", rng.randrange(1200))


def rvec(rng, scale=5000.0):
    return f64(*[rng.uniform(-scale, scale) for _ in range(3)])
