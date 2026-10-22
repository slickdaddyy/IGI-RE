"""Helpers shared by the test_win tests: logged Windows imports, fake COM objects."""
import struct

from emu import STACK_BASE, STACK_SIZE, TRAP_BASE, ErrorShown, compare

LOG = []


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
    """Stub that logs (name, args) and returns ret (or before(e) if it gives a value).
    pop defaults to stdcall cleanup of nargs."""
    def fn(e):
        args = tuple(norm(e, e.arg(i)) for i in range(nargs))
        entry = [name, args]
        r = ret
        if before:
            got = before(e)
            if isinstance(got, tuple):
                r, extra = got
                entry.append(extra)
            elif got is not None:
                r = got
        if LOG:
            LOG[-1].append(tuple(entry))
        e.ret(r() if callable(r) else r, 4 * nargs if pop is None else pop)
    return fn


def api(e, name, nargs, ret=0, before=None):
    e.stub(name, logged(name, nargs, ret, before=before))


def com(e, name, methods, size=48):
    """A fake COM object: a pointer to a vtable whose slots are logged traps.
    methods maps slot index to (nargs including this, ret[, before])."""
    obj = e.alloc(8)
    vt = e.alloc(4 * size)
    e.write(obj, struct.pack("<I", vt))
    for i in range(size):
        nargs, ret, before = (methods.get(i, (1, 0)) + (None,))[:3]
        addr = trap(e, "%s.%d" % (name, i), logged("%s.%d" % (name, i), nargs, ret, before=before))
        e.write(vt + 4 * i, struct.pack("<I", addr))
    return obj


def pyfake(e, addr, name, nargs, ret=0, before=None, stdcall=False):
    """Replace the original function at addr by a logged python stub."""
    t = trap(e, name, logged(name, nargs, ret, pop=4 * nargs if stdcall else 0, before=before))
    e.write(addr, b"\xe9" + struct.pack("<i", t - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)


def u32(e, p, v):
    e.write(p, struct.pack("<I", v & 0xFFFFFFFF))


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def check(emu, orig, ours, setup=None, **kw):
    """compare() plus the logged import/COM calls must match."""
    def wrapped(e):
        LOG.append([])
        return setup(e) if setup else None
    kw.setdefault("normalized", True)
    r = compare(emu, orig, ours, setup=wrapped, **kw)
    assert LOG[-2] == LOG[-1], "calls differ:\n orig %s\n ours %s" % (LOG[-2], LOG[-1])
    return r


def check_raises(emu, orig, ours, setup=None, exc=ErrorShown, args=()):
    """Both versions stop with the same exception and the same calls."""
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
    assert msgs[0] == msgs[1]
    assert LOG[-2] == LOG[-1], "calls differ:\n orig %s\n ours %s" % (LOG[-2], LOG[-1])
    return msgs[0]
