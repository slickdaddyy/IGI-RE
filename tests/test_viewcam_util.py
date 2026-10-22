"""Shared helpers for the viewcam tests (Human and HumanView, 0x462BA0 - 0x465910)."""
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import CALL_LOG, compare

HANDLERS = 0xA96AE0
WORLD, RNG, ROOT = 0x5A5A0000, 0x5B5B0000, 0x5C5C0000

f32 = lambda e, p, v: e.write(p, struct.pack("<f", v))
f64 = lambda e, p, *v: e.write(p, struct.pack("<%dd" % len(v), *v))
i32 = lambda e, p, *v: e.write(p, struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v]))
u8 = lambda e, p, v: e.write(p, bytes([v & 0xFF]))
u16 = lambda e, p, v: e.write(p, struct.pack("<H", v))
rf32 = lambda e, p: struct.unpack("<f", e.read(p, 4))[0]


def words(b):
    b = bytes(b) + b"\0" * (-len(b) % 4)
    return list(struct.unpack("<%dI" % (len(b) // 4), b))


def log(e, ws):
    n = e.u32(CALL_LOG)
    e.write(CALL_LOG + 4 + 4 * n, struct.pack("<%dI" % len(ws), *[w & 0xFFFFFFFF for w in ws]))
    e.write(CALL_LOG, struct.pack("<I", n + len(ws)))


class Fakes:
    """Original functions replaced by Python. Each call is logged to the call log as
    [addr, args...]; deref={i: n} logs n bytes behind argument i, deref={i: 's'} the C string."""

    def __init__(self, e):
        self.e = e
        self.fns = {}
        self.seq = {}
        self.rets = {}
        self.tramps = {}
        self.base = e.fake_top
        e.fake_top += 0x8000
        self.next = self.base
        e.uc.hook_add(UC_HOOK_CODE, self._hook, begin=self.base, end=self.base + 0x8000)

    def code(self, data):
        p = self.e.fake_top
        self.e.fake_top += (len(data) + 15) & ~15
        self.e.write(p, data)
        return p

    def _hook(self, uc, addr, size, user):
        fn = self.fns.get(addr)
        if fn:
            fn()

    def add(self, addr, nargs=0, ret=0, masks=(), deref=None, fn=None, st0=None, logged=True):
        """Replace addr (or make a new function when addr is None). ret may be a list,
        used up one value per call (reset it with set_seq in the test setup)."""
        e = self.e
        slot = self.next
        self.next += 16
        e.write(slot, b"\xc3")
        key = addr if addr is not None else slot
        self.rets[key] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code((b"\xd9\x05" if st0 == "f" else b"\xdd\x05") + struct.pack("<I", cell) + b"\xc3") if st0 else None

        def run():
            args = [e.arg(i) for i in range(nargs)]
            args = [a & m for a, m in zip(args, masks)] + args[len(masks):]
            if logged:
                rec = [key]
                for i, a in enumerate(args):
                    d = (deref or {}).get(i)
                    if d == "s":
                        s = e.read_cstr(a)
                        rec += [len(s)] + words(s)
                    elif d:
                        rec += words(e.read(a, d)) if a else [0]
                    else:
                        rec.append(a)
                log(e, rec)
            if fn:
                r = fn(e, args)
            elif key in self.seq:
                r = self.seq[key].pop(0)
            else:
                r = self.rets[key]
            if st0:
                e.write(cell, struct.pack("<f" if st0 == "f" else "<d", r or 0.0))
                e.uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                e.ret(int(r or 0))
        self.fns[slot] = run
        if addr is not None:
            e.write(addr, b"\xe9" + struct.pack("<i", slot - addr - 5))
            e.uc.ctl_remove_cache(addr, addr + 5)
        return slot

    def set_seq(self, addr, values):
        self.seq[addr] = list(values)

    def with_cw(self, target, cw):
        """Entry point that loads an x87 control word and jumps to target."""
        e = self.e
        addr = e.symbols[target] if isinstance(target, str) else target
        key = (addr, cw)
        if key not in self.tramps:
            cell = self.code(struct.pack("<H", cw))
            p = self.code(b"\0" * 16)
            e.write(p, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
            self.tramps[key] = p
        return self.tramps[key]


def check(e, fk, orig, ours, setup=None, cw=None, **kw):
    """compare() with the call log, optionally at x87 control word cw."""
    if cw is not None:
        orig, ours = fk.with_cw(orig, cw), fk.with_cw(ours, cw)

    def s(em):
        fk.seq.clear()
        return setup(em) if setup else None
    return compare(e, orig, ours, setup=s, normalized=True, **kw)


def set_handler(e, slot, type_, fn):
    i32(e, HANDLERS + 4 * (slot * 0x180 + type_), fn)


def new_task(e, size, type_=0, parent=0):
    p = e.alloc(size)
    i32(e, p + 0x14, parent)
    u16(e, p + 0x1C, type_)
    return p


def ref(e, task):
    """A TaskRef cell holding task."""
    p = e.alloc(4)
    i32(e, p, task)
    return p
