"""Stub builders shared by the weapon tests (no tests in here)."""
import struct

from emu import CALL_LOG, FAKE_BASE

LOG = CALL_LOG + 4
# per stub call counters for float sequences; inside the compared fake area, so reset with the emu
COUNTERS = FAKE_BASE + 0xF0000


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def _store():
    """mov [LOG+ecx*4],eax; inc ecx"""
    return b"\x89\x04\x8d" + u32(LOG) + b"\x41"


class Stub:
    pass


def _data(e, data):
    p = e.fake_top
    e.fake_top += (len(data) + 15) & ~15
    e.write(p, data)
    return p


def stub(e, addr=None, log=(), ret=0, frets=None, out=(), body=None, tag=None):
    """Code that logs tag (default addr) and some arguments to the fake call log.

    log items: i logs argument i, (i, n) logs n dwords at pointer argument i.
    out items: (i, bytes) copies bytes (a multiple of 4) to pointer argument i.
    frets: floats returned in st(0) one after the other on each call.
    body: machine code to run instead of 'mov eax, ret; ret'.
    With addr, the original function is patched to jump here. Returns a Stub with
    .code (address to use as a function pointer) and .set_ret(v)."""
    s = Stub()
    tag = addr if tag is None else tag
    c = b"\x8b\x0d" + u32(CALL_LOG)
    c += b"\xc7\x04\x8d" + u32(LOG) + u32(tag) + b"\x41"
    for item in log:
        if isinstance(item, int):
            c += b"\x8b\x44\x24" + bytes([4 + 4 * item]) + _store()
        else:
            i, n = item
            c += b"\x8b\x54\x24" + bytes([4 + 4 * i])
            for j in range(n):
                c += b"\x8b\x82" + u32(4 * j) + _store()
    c += b"\x89\x0d" + u32(CALL_LOG)
    for i, data in out:
        src = _data(e, data)
        c += b"\x8b\x54\x24" + bytes([4 + 4 * i])
        for j in range(0, len(data), 4):
            c += b"\xa1" + u32(src + j) + b"\x89\x82" + u32(j)
    if frets is not None:
        table = _data(e, b"".join(struct.pack("<d", v) for v in frets))
        counter = COUNTERS + 4 * len(getattr(e, "_weapon_counters", []))
        e._weapon_counters = getattr(e, "_weapon_counters", []) + [counter]
        c += b"\xa1" + u32(counter) + b"\xdd\x04\xc5" + u32(table) + b"\x40\xa3" + u32(counter)
    if body is None:
        s.ret_at = None
        c += b"\xb8"
        ret_off = len(c)
        c += u32(ret) + b"\xc3"
    else:
        ret_off = None
        c += body
    s.code = _data(e, c)
    s.ret_at = s.code + ret_off if ret_off is not None else None
    if addr is not None:
        e.write(addr, b"\xe9" + struct.pack("<i", s.code - addr - 5))
        e.uc.ctl_remove_cache(addr, addr + 5)

    def set_ret(v):
        e.write(s.ret_at, u32(v))
        e.uc.ctl_remove_cache(s.code, s.code + len(c))
    s.set_ret = set_ret
    return s


def fakes(e, table):
    """{addr: nargs} or {addr: (nargs, ret)} -> stubs logging the first nargs arguments."""
    out = {}
    for addr, v in table.items():
        nargs, ret = v if isinstance(v, tuple) else (v, 0)
        out[addr] = stub(e, addr, log=range(nargs), ret=ret)
    return out
