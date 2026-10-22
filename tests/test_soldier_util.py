"""Helpers shared by the test_soldier tests."""
import random
import struct

from emu import Emu
from test_menu_util import LOG, RET, check, logger, pyfake, trap, u32  # noqa: F401

SOLDIER = 0xD50
NAN = float("nan")


def d64(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def f32(e, p, *v):
    e.write(p, struct.pack("<%df" % len(v), *v))


def u8(e, p, *v):
    e.write(p, bytes(v))


def fake(e, addr, nargs=0, strings=(), derefs=None, ret=0, effect=None, masks=None, also=None):
    """Replace addr (and our function `also`, when given) with a logger."""
    fn = logger(e, addr, nargs, strings=strings, derefs=derefs, ret=ret, effect=effect, masks=masks)
    pyfake(e, addr, fn)
    if also:
        pyfake(e, e.symbols[also], fn)


def float_fake(e, addr, nargs=0, derefs=None, value=None, also=None):
    """A logger returning RET[addr] (or value(e, args)) in st0."""
    slot = e.fake_top
    code = e.fake_top + 16
    e.fake_top += 48

    def fn(e):
        args = [e.arg(i + 1) for i in range(nargs)]
        shown = [e.read(a, derefs[i]) if derefs and i in derefs else e.normalize(a) for i, a in enumerate(args)]
        if LOG:
            LOG[-1].append((addr,) + tuple(shown))
        v = value(e, args) if value else RET.get(addr, 0.0)
        e.write(slot, struct.pack("<d", v))
        e.ret(0)
    t = trap(e, "floatfake_%x" % addr, fn)
    e.write(code, b"\xe8" + struct.pack("<i", t - (code + 5)) + b"\xdd\x05" + struct.pack("<I", slot) + b"\xc3")
    for a in [addr] + ([e.symbols[also]] if also else []):
        e.write(a, b"\xe9" + struct.pack("<i", code - a - 5))
        e.uc.ctl_remove_cache(a, a + 5)
    e.baseline = e.snapshot()


def with_cw(e, target, cw, cache={}):
    """Entry point that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    key = (id(e), addr, cw)
    if key not in cache:
        p = e.fake_top
        e.fake_top += 32
        e.write(p, struct.pack("<H", cw))
        e.write(p + 4, b"\xd9\x2d" + struct.pack("<I", p) + b"\xe9" + struct.pack("<i", addr - (p + 15)))
        cache[key] = p + 4
        e.baseline = e.snapshot()
    return cache[key]


def run(e, orig, ours, setup, cw=None, **kw):
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    kw.setdefault("check_eax", False)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def junk(e, p, n, seed=1):
    r = random.Random(seed)
    e.write(p, bytes(r.randrange(256) for _ in range(n)))


def state_table(e, n=38):
    """A state table of logging handlers, one per state."""
    t = e.alloc(4 * n)
    for i in range(n):
        u32(e, t + 4 * i, e.state_handlers[i])
    return t


def soldier(e, bits=0, state=5, seed=1):
    """A soldier filled with junk, with its state machine using logging states."""
    h = e.alloc(SOLDIER)
    junk(e, h, SOLDIER, seed)
    sm = h + 0x2EC
    u32(e, sm + 4, state_table(e))
    u32(e, sm + 8, bits)
    u32(e, sm + 0x10, state)
    return h, sm


def new_emu():
    e = Emu()
    e.state_handlers = [trap(e, "state_%d" % i, logger(e, "state_%d" % i, 2)) for i in range(38)]
    e.baseline = e.snapshot()
    return e
