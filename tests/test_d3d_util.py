"""Helpers for the Direct3D and sound tests: logged imports, fake COM objects, readable Error_Show messages."""
import re
import struct

from emu import STACK_BASE, STACK_SIZE, TRAP_BASE, Emu, ErrorShown, compare

LOG = []


class D3DEmu(Emu):
    """Error_Show stops the call with the fully formatted message."""

    def _error_show(self, uc, addr, size, user):
        fmt = self.read_cstr(self.arg(0)).decode("latin1")
        args, i = [], 1
        for spec in re.findall(r"%[-+ #0-9.]*([sdxXfu])", fmt):
            v = self.arg(i)
            args.append(self.read_cstr(v).decode("latin1") if spec == "s" else v)
            i += 1
        self.import_error = ErrorShown("%s | %s" % (fmt, args))
        uc.emu_stop()


def norm(e, v):
    if STACK_BASE <= v < STACK_BASE + STACK_SIZE:
        return "stack"
    return e.normalize(v)


def trap(e, name, fn):
    """A trap address that runs fn(e) when called; fn returns with e.ret()."""
    e.traps = getattr(e, "traps", {})
    if name not in e.traps:
        addr = TRAP_BASE + len(e.imports) * 4
        e.imports.append(("test", name))
        e.write(addr, b"\xc3")
        e.traps[name] = addr
    e.stubs[name] = fn
    return e.traps[name]


def logged(name, nargs, ret=0, pop=None, before=None):
    """Stub that logs (name, args) and returns ret. before(e) may return a value, or (value, extra)
    where extra is logged too. pop defaults to stdcall cleanup of nargs."""
    def fn(e):
        args = tuple(norm(e, e.arg(i)) for i in range(nargs))
        entry = [name, args]
        r = ret
        if before:
            got = before(e)
            if isinstance(got, tuple):
                r, extra = got
                r = ret if r is None else r
                entry.append(extra)
            elif got is not None:
                r = got
        if LOG:
            LOG[-1].append(tuple(entry))
        e.ret(r, 4 * nargs if pop is None else pop)
    return fn


def api(e, name, nargs, ret=0, before=None):
    e.stub(name, logged(name, nargs, ret, before=before))


def pyfake(e, addr, name, nargs, ret=0, before=None):
    """Replace the cdecl original function at addr by a logged python stub."""
    t = trap(e, name, logged(name, nargs, ret, pop=0, before=before))
    e.write(addr, b"\xe9" + struct.pack("<i", t - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)


# argument counts (with this) of the COM methods the code calls
DDRAW = {0: 3, 1: 1, 2: 1, 4: 4, 6: 4, 8: 5, 11: 3, 19: 1, 20: 3, 21: 6, 22: 3, 25: 1}
D3D = {0: 3, 1: 1, 2: 1, 3: 3, 4: 4, 5: 4, 6: 4, 7: 1}
SURFACE = {0: 3, 1: 1, 2: 1, 3: 2, 5: 6, 8: 3, 11: 3, 12: 3, 13: 2, 21: 2, 25: 5, 28: 2, 32: 2}
DEVICE = {0: 3, 1: 1, 2: 1, 3: 2, 5: 1, 6: 1, 10: 7, 13: 2, 20: 3, 27: 2, 35: 3}
VBUF = {0: 3, 1: 1, 2: 1, 3: 4, 4: 1}
CLIPPER = {0: 3, 1: 1, 2: 1, 8: 3}
DSOUND = {0: 3, 1: 1, 2: 1, 3: 4, 4: 2, 5: 3, 6: 3, 7: 1, 8: 2, 9: 2, 10: 2}
DSBUFFER = {0: 3, 1: 1, 2: 1, 3: 2, 4: 3, 5: 2, 6: 2, 7: 2, 8: 2, 9: 2, 10: 3, 11: 8, 12: 4, 13: 2, 14: 2,
            15: 2, 16: 2, 17: 2, 18: 1, 19: 5}


def com(e, name, nargs, methods=None, size=48):
    """A fake COM object whose vtable slots are logged traps "name.slot".
    methods maps slot to ret, (ret, before) or an address of hand written code."""
    methods = methods or {}
    obj = e.alloc(8)
    vt = e.alloc(4 * size)
    e.write(obj, struct.pack("<I", vt))
    for i in range(size):
        m = methods.get(i, 0)
        if isinstance(m, Code):
            ret, before = m.ret, m.before
        elif isinstance(m, tuple):
            ret, before = m
        else:
            ret, before = m, None
        addr = trap(e, "%s.%d" % (name, i), logged("%s.%d" % (name, i), nargs.get(i, 1), ret, before=before))
        if isinstance(m, Code):
            addr = m.build(e, addr)
        e.write(vt + 4 * i, struct.pack("<I", addr))
    return obj


class Code:
    """Machine code for a vtable slot that calls a stdcall callback(item, ctx) for each item,
    then jumps to the logged trap. Offsets are from esp at entry."""

    def __init__(self, items, ctx_off, cb_off, ret=0, before=None):
        self.items, self.ctx_off, self.cb_off, self.ret, self.before = items, ctx_off, cb_off, ret, before

    def build(self, e, trap_addr):
        code = b""
        for item in self.items:
            code += b"\xff\x74\x24" + bytes([self.ctx_off]) + b"\x68" + struct.pack("<I", item)
            code += b"\xff\x54\x24" + bytes([self.cb_off + 8])
        addr = e.alloc(len(code) + 5)
        code += b"\xe9" + struct.pack("<i", trap_addr - (addr + len(code) + 5))
        e.write(addr, code)
        return addr


def out(i, value, extra=None):
    """before() that writes value to the pointer in argument i."""
    def fn(e):
        e.write(e.arg(i), struct.pack("<I", value(e) if callable(value) else value))
        return (None, extra(e)) if extra else None
    return fn


def u32(e, p, v):
    e.write(p, struct.pack("<I", v & 0xFFFFFFFF))


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def check(emu, orig, ours, setup=None, **kw):
    """compare() plus the logged import/COM calls must match. Returns the call log."""
    def wrapped(e):
        LOG.append([])
        return setup(e) if setup else None
    kw.setdefault("normalized", True)
    compare(emu, orig, ours, setup=wrapped, **kw)
    assert LOG[-2] == LOG[-1], "calls differ:\n orig %s\n ours %s" % (LOG[-2], LOG[-1])
    return LOG[-1]


def check_raises(emu, orig, ours, setup=None, exc=ErrorShown, args=()):
    """Both versions stop with the same exception and the same calls. Returns the message."""
    msgs = []
    for target in (orig, ours):
        emu.reset()
        LOG.append([])
        call_args = (setup(emu) if setup else None) or args
        try:
            emu.call(target, *call_args)
        except exc as ex:
            msgs.append(str(ex))
        else:
            raise AssertionError("no %s from %s" % (exc.__name__, target))
    assert msgs[0] == msgs[1], msgs
    assert LOG[-2] == LOG[-1], "calls differ:\n orig %s\n ours %s" % (LOG[-2], LOG[-1])
    return msgs[0]
