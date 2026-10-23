"""Helpers for the animtrig tests."""
import struct

from test_menu_util import LOG, check, pyfake


def with_cw(e, target, cw):
    """An entry point that loads x87 control word cw and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    e.cw_tramps = getattr(e, "cw_tramps", {})
    if (addr, cw) not in e.cw_tramps:
        p = e.fake_top
        e.fake_top += 32
        code = b"\xd9\x2d" + struct.pack("<I", p + 16)
        code += b"\xe9" + struct.pack("<i", addr - (p + len(code) + 5))
        e.write(p, code.ljust(16, b"\x90") + struct.pack("<H", cw))
        e.cw_tramps[(addr, cw)] = p
    return e.cw_tramps[(addr, cw)]


def check_cw(e, orig, ours, cw, setup=None, **kw):
    return check(e, with_cw(e, orig, cw), with_cw(e, ours, cw), setup=setup, **kw)


def _strcat(e):
    d = e.arg(0)
    e.write(d + len(e.read_cstr(d)), e.read_cstr(e.arg(1)) + b"\0")
    e.ret(d)


def add_stubs(e):
    e.stub("strcat", _strcat)
    e.baseline = e.snapshot()


def logfn(e, addr, fn, nargs=0, ret=0):
    """Replace addr with a fake that logs (addr, fn(e, args)) and returns ret
    (a callable ret gets (e, args))."""
    def run(e):
        args = [e.arg(i) for i in range(nargs)]
        if LOG:
            LOG[-1].append((addr,) + tuple(fn(e, args)))
        e.ret((ret(e, args) if callable(ret) else ret) or 0)
    pyfake(e, addr, run)


CWS = [0x37F, 0x7F, 0xC7F, 0x77F]
