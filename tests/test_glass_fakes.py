"""Helpers for the GenericTBA, Building, RadarDish, Siren and Glass tests (no tests in here)."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import ErrorShown, compare

HANDLERS = 0xA96AE0
TICK = 0x5000

# getters of event slots, task types and param types; set_events gives each a fixed value
GETTERS = [0x4F1A60, 0x4F1A70, 0x4F1210, 0x4F1220, 0x4167F0, 0x416800, 0x416810, 0x416820, 0x416840, 0x416860,
           0x416880, 0x481340, 0x481350, 0x4D9FA0, 0x4DEE80, 0x489610, 0x4CEA10, 0x4CEA20, 0x4C56D0, 0x4C56E0,
           0x467D90, 0x4E6610, 0x4FC860, 0x4018F0, 0x4168A0, 0x4168D0, 0x4F0EE0, 0x4F0F00, 0x4F0F50, 0x4F0F70,
           0x4F0FB0, 0x4F0FC0, 0x4F0FD0, 0x4F0FE0, 0x4F1000, 0x4F1010, 0x487180]


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
        r = setup(em) if setup else None
        fk.log = []
        logs.append(fk.log)
        return r
    kw.setdefault("check_eax", False)
    res = compare(e, orig, ours, setup=s, normalized=normalized, **kw)
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return res, logs[0]


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


def masked(*masks):
    """fmt for Fakes.add: the args ANDed with masks (missing masks keep the arg)."""
    def fmt(em, args):
        return tuple(em.normalize(a & (masks[i] if i < len(masks) else 0xFFFFFFFF)) for i, a in enumerate(args))
    return fmt


def param_add(em, a):
    """ParamTable_Add: only the field offset matters."""
    return (a[0], (a[2] - a[1]) & 0xFFFFFFFF, a[3], a[4], a[5])


def common(e, fk, types=None):
    """Fakes for task type setup and parameter tables."""
    types = types or {}
    fk.add(0x401900, 4, ret=lambda em, a: types.get(em.read_cstr(a[0]), 0x7777), fmt=masked(~0, ~0, 0xFFFF))
    fk.add(0x401400, 3, fmt=masked(0xFFFF, 0xFF))
    fk.add(0x401530, 3, fmt=masked(0xFFFF))
    fk.add(0x401A20, 1, fmt=masked(0xFFFF))
    fk.add(0x4F1950, 1, ret=0x1950)
    fk.add(0x4F1980, 2, ret=0x1980)
    fk.add(0x4F1A80, 6, fmt=param_add)
    fk.add(0x4C1830, 1)
    fk.add(0x4B8810, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))


def set_events(e):
    for i, g in enumerate(GETTERS):
        c = e.read(g, 6)
        if c[0] == 0xA0:
            e.write(struct.unpack("<I", c[1:5])[0], bytes([0x20 + i]))
        elif c[:2] == b"\x66\xa1":
            e.write(struct.unpack("<I", c[2:6])[0], struct.pack("<H", 0x20 + i))
        elif c[0] == 0xA1:
            e.write(struct.unpack("<I", c[1:5])[0], struct.pack("<I", 0x20 + i))
        else:
            raise AssertionError(hex(g))


def event(getter):
    return 0x20 + GETTERS.index(getter)


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


def base(e):
    """Fixed event slots and a frame counter."""
    set_events(e)
    info = e.alloc(0x80)
    i32(e, info + 0x68, TICK)
    i32(e, 0xBC2360, info)


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def i32(e, p, v):
    e.write(p, struct.pack("<I", v & 0xFFFFFFFF))


def u8(e, p, v):
    e.write(p, bytes([v]))


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


def rmatrix(rng):
    return f32(*[rng.uniform(-1.5, 1.5) for _ in range(9)]) + struct.pack("<i", rng.randrange(100))


def rvec(rng, scale=5000.0):
    return f64(*[rng.uniform(-scale, scale) for _ in range(3)])


def with_children(e, t, slot, handler, n=2, type0=0x40):
    for i in range(n):
        c = new_task(e, type0 + i, 0x40)
        add_child(e, t, c)
        set_handler(e, slot, type0 + i, handler)
