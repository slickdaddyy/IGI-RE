"""Helpers shared by the test_profile tests."""
import struct

from emu import TRAP_BASE, compare

LOG = []
RET = {}


def pyfake(e, addr, fn):
    """Replace the original function at addr with fn(e), which returns with e.ret()."""
    name = "pyfake_%x" % addr
    if name not in e.stubs:
        trap = TRAP_BASE + len(e.imports) * 4
        e.imports.append(("test", name))
        e.write(trap, b"\xc3")
        e.write(addr, b"\xe9" + struct.pack("<i", trap - addr - 5))
        e.uc.ctl_remove_cache(addr, addr + 5)
    e.stub(name, fn)
    e.baseline = e.snapshot()


def logfake(e, addr, nargs=0, masks=(), strings=(), ret=0):
    """Log (addr, args...) to LOG[-1]. Args in strings are logged as the C string they point to.
    The return value comes from RET[addr] when set."""
    def fn(e):
        args = []
        for i in range(nargs):
            v = e.arg(i)
            if i in strings:
                v = e.read_cstr(v) if v else None
            else:
                v = e.normalize(v & (masks[i] if i < len(masks) else 0xFFFFFFFF))
            args.append(v)
        if LOG:
            LOG[-1].append((addr,) + tuple(args))
        e.ret(RET.get(addr, ret))
    pyfake(e, addr, fn)


def check(e, orig, ours, setup=None, **kw):
    """compare() plus the LOG of both runs."""
    runs = []

    def s(e):
        LOG.append([])  # calls made by the setup itself
        r = setup(e) if setup else None
        LOG.append([])
        runs.append(LOG[-1])
        return r
    r = compare(e, orig, ours, setup=s, **kw)
    assert runs[0] == runs[1], "calls differ:\n orig %s\n ours %s" % (runs[0], runs[1])
    return r


def u32(e, p, *v):
    e.write(p, struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v]))


def f32(e, p, *v):
    e.write(p, struct.pack("<%df" % len(v), *v))


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def script_table(e):
    e.call(0x4B0BA0)
    buckets = e.alloc(12 * 97)
    for i in range(97):
        b = buckets + 12 * i
        e.write(b, struct.pack("<III", b + 4, 0, b))
    table = e.alloc(8)
    e.write(table, struct.pack("<II", buckets, 97))
    u32(e, 0xA43E88, table)


def task_system(e):
    """Mem, script table, tasks and task parameters, all original code."""
    script_table(e)
    e.call(0x4010D0)
    e.call(0x4F1840)
