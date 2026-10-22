"""Shared helpers for the Missile and Train tests (no tests in here)."""
import struct

from unicorn import UC_HOOK_CODE

from emu import CALL_LOG, FAKE_BASE

R = {}  # values the fakes return or write, set by each test's setup


def log(e, words):
    n = e.u32(CALL_LOG)
    e.write(CALL_LOG + 4 + 4 * n, struct.pack("<%dI" % len(words), *[w & 0xFFFFFFFF for w in words]))
    e.write(CALL_LOG, struct.pack("<I", n + len(words)))


def words(e, p, n):
    return list(struct.unpack("<%dI" % n, e.read(p, 4 * n)))


f32 = lambda e, p, v: e.write(p, struct.pack("<f", v))
f64 = lambda e, p, v: e.write(p, struct.pack("<d", v))
i32 = lambda e, p, v: e.write(p, struct.pack("<I", v & 0xFFFFFFFF))
u16 = lambda e, p, v: e.write(p, struct.pack("<H", v))
u8 = lambda e, p, v: e.write(p, bytes([v]))


class PyFakes:
    """Callees emulated in Python. Each fake is a slot in the fake area that the
    original function jumps to."""

    def __init__(self, e, base=FAKE_BASE + 0xC0000):
        self.e, self.next, self.fns = e, base, {}
        e.uc.hook_add(UC_HOOK_CODE, self._hook, begin=base, end=base + 0x10000)

    def _hook(self, uc, addr, size, user):
        fn = self.fns.get(addr)
        if fn:
            fn(self.e)

    def slot(self, fn, code=b"\xc3"):
        t = self.next
        self.next += 32
        self.e.write(t, code)
        self.fns[t] = fn
        return t

    def patch(self, addr, t):
        self.e.write(addr, b"\xe9" + struct.pack("<i", t - addr - 5))
        self.e.uc.ctl_remove_cache(addr, addr + 5)

    def fake(self, addr, nargs=0, ret=0, masks=(), fn=None, logged=True):
        """Logs [addr, args...] (masked) and returns R.get(addr, ret), or fn(e, args)."""
        def run(e):
            args = [e.arg(i) for i in range(nargs)]
            shown = [a & m for a, m in zip(args, masks)] + args[len(masks):]
            if logged:
                log(e, [addr] + shown)
            r = fn(e, args) if fn else R.get(addr, ret)
            e.ret(r or 0)
        t = self.slot(run)
        if addr is not None:
            self.patch(addr, t)
        return t

    def fake_float(self, addr, nargs, value_addr):
        """Logs [addr, args...] and returns the double at value_addr in st(0)."""
        def run(e):
            log(e, [addr] + [e.arg(i) for i in range(nargs)])
        t = self.slot(run, b"\xdd\x05" + struct.pack("<I", value_addr) + b"\xc3")
        self.patch(addr, t)
        return t

    def logger(self, nargs, deref=None):
        """A handler that logs its own address and args; deref=(arg, n) logs n words there instead."""
        def run(e):
            args = [e.arg(i) for i in range(nargs)]
            if deref:
                args[deref[0]:deref[0] + 1] = words(e, args[deref[0]], deref[1])
            log(e, [t] + args)
            e.ret(0)
        t = self.slot(run)
        return t


def task(e, size, type_, parent=None):
    """A zeroed task with an empty child list, appended to parent's children."""
    p = e.alloc(size)
    i32(e, p + 8, p + 0xC)
    i32(e, p + 0x10, p + 8)
    u16(e, p + 0x1C, type_)
    if parent:
        add_child(e, parent, p)
    return p


def add_child(e, parent, c):
    """List_AddTail on the parent's child list."""
    lst = parent + 8
    last = e.u32(lst + 8)
    i32(e, c, lst + 4)
    i32(e, c + 4, last)
    i32(e, last, c)
    i32(e, lst + 8, c)
    i32(e, c + 0x14, parent)


def set_handler(e, slot, type_, fn):
    i32(e, 0xA96AE0 + 4 * (slot * 0x180 + type_), fn)


def model(e, radius, kind=0):
    """A VirModel (array of LOD pointers) with the radius VirModel_GetRadius reads and the
    word Missile_EmitSmoke switches on."""
    m, lod, a, b = e.alloc(8), e.alloc(0x20), e.alloc(0x20), e.alloc(0x100)
    i32(e, m, lod)
    i32(e, lod + 0xC, a)
    i32(e, a + 0xC, b)
    f32(e, b + 0x78, radius)
    u16(e, b + 0x7C, kind)
    return m
