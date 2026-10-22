"""Helpers shared by the test_patrol tests."""
import struct

from test_menu_util import LOG, RET, check, logfake, pyfake, u32  # noqa: F401


def with_cw(e, target, cw):
    """Entry point that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    tramps = e.__dict__.setdefault("cw_tramps", {})
    if (addr, cw) not in tramps:
        p = e.fake_top
        e.fake_top += 32
        e.write(p + 16, struct.pack("<H", cw))
        e.write(p, b"\xd9\x2d" + struct.pack("<I", p + 16) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        e.baseline = e.snapshot()
        tramps[(addr, cw)] = p
    return tramps[(addr, cw)]


def run(e, orig, ours, setup, cw=None, **kw):
    """check() with normalized memory, optionally at an x87 control word."""
    RET.clear()
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, orig, cw), with_cw(e, ours, cw)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


def list_init(e, lst):
    u32(e, lst, lst + 4, 0, lst)


def list_add(e, lst, node):
    last = e.u32(lst + 8)
    u32(e, node, lst + 4, last)
    u32(e, last, node)
    u32(e, lst + 8, node)


def f32(e, p, *v):
    e.write(p, struct.pack("<%df" % len(v), *v))


def d64(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def fake(e, addr, nargs, skip=(), derefs=None, effect=None, ret=0):
    """Replace addr with a logger of its args; args in skip (stack out pointers) are not logged,
    derefs maps an arg to a number of bytes logged from where it points."""
    def fn(e):
        args = [e.arg(i) for i in range(nargs)]
        shown = [e.read(a, derefs[i]) if derefs and i in derefs else e.normalize(a)
                 for i, a in enumerate(args) if i not in skip]
        if LOG:
            LOG[-1].append((addr,) + tuple(shown))
        r = effect(e, args) if effect else RET.get(addr, ret)
        e.ret(r or 0)
    pyfake(e, addr, fn)
