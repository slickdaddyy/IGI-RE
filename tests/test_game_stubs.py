"""Python fakes shared by the game tests (no tests in here)."""
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX

from emu import CALL_LOG


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def f32(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def d64(v):
    return struct.unpack("<II", struct.pack("<d", v))


def log_words(e, words):
    n = e.u32(CALL_LOG)
    e.write(CALL_LOG + 4 + 4 * n, b"".join(u32(w) for w in words))
    e.write(CALL_LOG, u32(n + len(words)))


class PyFake:
    """Replaces the original function at addr with Python.

    Each call logs addr and the log items to the fake call log: i logs argument i,
    ("b", i) / ("w", i) its low byte / word, (i, n) logs n dwords at pointer
    argument i, a callable logs what it returns.
    ret is the eax value (or a callable taking the emu); with fret the result is
    returned in st(0) as a double instead. fn(emu) runs on each call, before the
    result is taken."""

    def __init__(self, e, addr, log=(), ret=0, fret=None, fn=None):
        self.e, self.addr, self.log, self.ret, self.fret, self.fn = e, addr, log, ret, fret, fn
        self.slot = e.fake_top
        e.fake_top += 16
        code = (b"\xdd\x05" + u32(self.slot) if fret is not None else b"") + b"\xc3"
        e.write(addr, code)
        e.uc.ctl_remove_cache(addr, addr + len(code))
        e.uc.hook_add(UC_HOOK_CODE, self._hook, begin=addr, end=addr)

    def _hook(self, uc, addr, size, user):
        e = self.e
        words = [self.addr]
        for item in self.log:
            if callable(item):
                w = item(e)
                words += w if isinstance(w, list) else [w]
            elif isinstance(item, tuple) and item[0] == "b":
                words.append(e.arg(item[1]) & 0xFF)
            elif isinstance(item, tuple) and item[0] == "w":
                words.append(e.arg(item[1]) & 0xFFFF)
            elif isinstance(item, tuple):
                p = e.arg(item[0])
                words += [e.u32(p + 4 * j) for j in range(item[1])]
            else:
                words.append(e.arg(item))
        log_words(e, words)
        if self.fn:
            self.fn(e)
        if self.fret is not None:
            v = self.fret(e) if callable(self.fret) else self.fret
            e.write(self.slot, struct.pack("<d", v))
        else:
            v = self.ret(e) if callable(self.ret) else self.ret
            uc.reg_write(UC_X86_REG_EAX, v & 0xFFFFFFFF)


def fakes(e, table):
    """{addr: nargs or (nargs, ret)} -> {addr: PyFake} logging the first nargs arguments."""
    out = {}
    for addr, spec in table.items():
        nargs, ret = spec if isinstance(spec, tuple) else (spec, 0)
        out[addr] = PyFake(e, addr, log=tuple(range(nargs)), ret=ret)
    return out
