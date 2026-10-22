"""Helpers shared by the test_menu tests."""
import struct

from emu import TRAP_BASE, compare

LOG = []
RET = {}
HANDLERS = 0xA96AE0


def trap(e, name, fn):
    """A callable address that runs fn(e); fn returns with e.ret()."""
    if name not in e.stubs:
        t = TRAP_BASE + len(e.imports) * 4
        e.imports.append(("test", name))
        e.write(t, b"\xc3")
        e.trap_addr = getattr(e, "trap_addr", {})
        e.trap_addr[name] = t
    e.stub(name, fn)
    e.baseline = e.snapshot()
    return e.trap_addr[name]


def pyfake(e, addr, fn):
    """Replace the original function at addr with fn(e)."""
    t = trap(e, "pyfake_%x" % addr, fn)
    e.write(addr, b"\xe9" + struct.pack("<i", t - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)
    e.baseline = e.snapshot()


def logger(e, tag, nargs=0, strings=(), ret=0, derefs=None, effect=None, masks=None):
    """fn(e) that logs (tag, args...) to LOG[-1]. Args in strings are logged as text,
    derefs maps an arg index to a number of bytes logged from that address,
    masks maps an arg index to a mask (for u16 args pushed with junk above).
    The return value is RET[tag] when set, or effect(e, args) when given."""
    def fn(e):
        raw = [e.arg(i) for i in range(nargs)]
        args = [v & masks.get(i, 0xFFFFFFFF) for i, v in enumerate(raw)] if masks else raw
        shown = []
        for i, v in enumerate(args):
            if i in strings:
                v = e.read_cstr(v) if v else None
            elif derefs and i in derefs:
                v = e.read(v, derefs[i]) if v else None
            else:
                v = e.normalize(v)
            shown.append(v)
        if LOG:
            LOG[-1].append((tag,) + tuple(shown))
        r = effect(e, raw) if effect else RET.get(tag, ret)
        e.ret(r or 0)
    return fn


def logfake(e, addr, nargs=0, **kw):
    pyfake(e, addr, logger(e, addr, nargs, **kw))


def handler(e, tag, nargs=1, **kw):
    """A task handler that logs its calls."""
    return trap(e, "handler_%s" % tag, logger(e, tag, nargs, **kw))


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


def strip_logger(e):
    """Route QVertex strips to a logger of the strip and its vertices."""
    def fn(e):
        s = e.arg(0)
        untextured, flags, unk8, count, verts = struct.unpack("<5I", e.read(s, 20))
        n = count if 1 < count < 64 else 0  # a single vertex is left unset by DrawCircle
        LOG[-1].append(("strip", untextured, flags, unk8, count, e.read(verts, 36 * n),
                        e.u32(0xA84A50), e.u32(0xA84A58)))
        e.ret(0)
    t = trap(e, "strip", fn)
    e.write(0xA94E84, struct.pack("<I", t))
    e.write(0xA84A54, struct.pack("<I", 0))
    e.baseline = e.snapshot()


def check(e, orig, ours, setup=None, **kw):
    """compare() plus the LOG of both runs."""
    runs = []

    def s(e):
        LOG.append([])
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


def u16(e, p, *v):
    e.write(p, struct.pack("<%dH" % len(v), *v))


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def task(e, size, type_=0, parent=0):
    """A task with an empty child list."""
    t = e.alloc(size)
    u32(e, t + 8, t + 0xC, 0, t + 8)
    u32(e, t + 0x14, parent)
    u16(e, t + 0x1C, type_)
    return t


def add_child(e, parent, child):
    last = e.u32(parent + 0x10)
    u32(e, child, parent + 0xC, last)
    u32(e, last, child)
    u32(e, parent + 0x10, child)
    u32(e, child + 0x14, parent)


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
