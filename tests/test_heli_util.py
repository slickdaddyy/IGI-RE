"""Helpers shared by the test_heli tests."""
import struct

from emu import Emu
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, task, task_system, trap, u16, u32)


def d64(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def floatfake(e, addr, nargs, derefs=None):
    """Replace addr with a logger that returns RET[addr] (default 0.0) in st0."""
    slot = e.fake_top
    code = e.fake_top + 16
    e.fake_top += 48

    def fn(e):
        args = [e.arg(i + 1) for i in range(nargs)]
        shown = [e.read(a, derefs[i]) if derefs and i in derefs else e.normalize(a) for i, a in enumerate(args)]
        if LOG:
            LOG[-1].append((addr,) + tuple(shown))
        v = RET.get(addr, 0.0)
        e.write(slot, struct.pack("<d", v(e, args) if callable(v) else v))
        e.ret(0)
    t = trap(e, "floatfake_%x" % addr, fn)
    e.write(code, b"\xe8" + struct.pack("<i", t - (code + 5)) + b"\xdd\x05" + struct.pack("<I", slot) + b"\xc3")
    e.write(addr, b"\xe9" + struct.pack("<i", code - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)
    e.baseline = e.snapshot()


def vfake(e, addr, nargs, show=None, effect=None, ret=0):
    """Replace addr with a logger of (addr,) + show(e, args) (default: the normalized
    args) that returns effect(e, args), RET[addr] or ret."""
    def fn(e):
        args = [e.arg(i) for i in range(nargs)]
        if LOG:
            LOG[-1].append((addr,) + tuple(show(e, args) if show else [e.normalize(a) for a in args]))
        r = effect(e, args) if effect else RET.get(addr, ret)
        e.ret(r or 0)
    pyfake(e, addr, fn)


def run(e, orig, ours, setup, **kw):
    """check() with normalized memory, eax unchecked unless asked."""
    kw.setdefault("check_eax", False)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def new_emu(fakes=(), floats=()):
    """An Emu with logging fakes: fakes is [(addr, nargs, kw)], floats [(addr, nargs)]."""
    e = Emu()
    for f in fakes:
        logfake(e, f[0], f[1], **(f[2] if len(f) > 2 else {}))
    for addr, n in floats:
        floatfake(e, addr, n)
    e.baseline = e.snapshot()
    return e


def children(e, parent, n, type_, size=0x40):
    """n child tasks of type_ under parent."""
    out = []
    for _ in range(n):
        c = task(e, size, type_)
        add_child(e, parent, c)
        out.append(c)
    return out


def coll_task(e, n=2, type_=0x70):
    """A collision task with n children of type_."""
    t = task(e, 0x40, 0x6F)
    children(e, t, n, type_)
    return t
