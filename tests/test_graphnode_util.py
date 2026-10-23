"""Helpers shared by the test_graphnode tests."""
import struct

from emu import Emu
from test_menu_util import (LOG, RET, check, cstr, f32, handler, logfake, logger, pyfake,  # noqa: F401
                            set_handler, trap, u16, u32)
from test_patrol_util import d64, run, with_cw  # noqa: F401

CWS = [0x37F, 0x7F]
FISTP_CWS = [0x37F, 0x7F, 0xC7F, 0x77F]


def floatfake(e, addr, nargs, effect=None):
    """addr returns effect(e, args) or RET[addr] (a double) in st0, logging its args."""
    slot = e.fake_top
    code = e.fake_top + 16
    e.fake_top += 48

    def fn(e):
        args = [e.arg(i + 1) for i in range(nargs)]
        if LOG:
            LOG[-1].append((addr,) + tuple(e.normalize(a) for a in args))
        v = effect(e, args) if effect else RET.get(addr, 0.0)
        e.write(slot, struct.pack("<d", v))
        e.ret(0)
    t = trap(e, "floatfake_%x" % addr, fn)
    e.write(code, b"\xe8" + struct.pack("<i", t - (code + 5)) + b"\xdd\x05" + struct.pack("<I", slot) + b"\xc3")
    e.write(addr, b"\xe9" + struct.pack("<i", code - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)
    e.baseline = e.snapshot()


def alloc_fake(e, addr=0x4B0C60):
    """Mem_Alloc from the emulator heap, logging size and alignment."""
    logfake(e, addr, 2, effect=lambda e, a: e.alloc(a[0]))


class Files:
    """In-memory files behind the File_* functions; every call goes to LOG."""
    HANDLE = 0x5F000010

    def __init__(self, e):
        self.e = e
        self.data = {}
        self.pos = 0
        self.cur = None
        pyfake(e, 0x4B1510, self.open)
        pyfake(e, 0x4B16D0, self.read)
        pyfake(e, 0x4B1700, self.write)
        pyfake(e, 0x4B1730, self.seek)
        pyfake(e, 0x4B1760, self.eof)
        pyfake(e, 0x4B17C0, lambda e: self.log(e, "flush", 1, 0))
        pyfake(e, 0x4B1690, lambda e: self.log(e, "close", 1, 0))

    def set(self, data, name=b"graph.dat"):
        """Call from a setup: the next File_Open of name returns data."""
        self.data = {name: data}
        self.pos = 0
        self.cur = None

    def log(self, e, what, nargs, r, *extra):
        if LOG:
            LOG[-1].append((what,) + tuple(e.arg(i) for i in range(nargs)) + extra)
        e.ret(r)

    def open(self, e):
        name, mode = e.read_cstr(e.arg(0)), e.read_cstr(e.arg(1))
        self.cur = self.data.get(name)
        self.pos = 0
        if LOG:
            LOG[-1].append(("open", name, mode))
        e.ret(self.HANDLE if self.cur is not None else 0)

    def read(self, e):
        f, buf, n = e.arg(0), e.arg(1), e.arg(2)
        chunk = self.cur[self.pos:self.pos + n]
        e.write(buf, chunk + b"\0" * (n - len(chunk)))
        self.pos += n
        self.log(e, "read", 1, n, n)

    def write(self, e):
        f, buf, n = e.arg(0), e.arg(1), e.arg(2)
        self.log(e, "write", 1, n, e.read(buf, n))

    def seek(self, e):
        off, whence = e.arg(1), e.arg(2)
        off = off - (1 << 32) if off & 0x80000000 else off
        self.pos = off if whence == 0 else self.pos + off
        self.log(e, "seek", 3, 0)

    def eof(self, e):
        self.log(e, "eof", 1, int(self.pos >= len(self.cur)))


def stream(e, handle=Files.HANDLE):
    """A GraphStream on the open file."""
    s = e.alloc(8)
    u32(e, s, 0x1234, handle)
    return s


# tagged values as the GraphStream_Write functions store them
def t_u8(v, tag=0):
    return bytes([tag, v & 0xFF])


def t_u16(v, tag=2):
    return bytes([tag]) + struct.pack("<H", v & 0xFFFF)


def t_int(v, tag=5):
    return bytes([tag]) + struct.pack("<I", v & 0xFFFFFFFF)


def t_float(v):
    return b"\x06" + struct.pack("<f", v)


def t_double(v):
    return b"\x07" + struct.pack("<d", v)


def t_vec(*v):
    return b"\x08" + struct.pack("<3d", *v)


def t_str(s):
    return b"\x09" + bytes([len(s) + 1]) + s + b"\0"


def t_blob(data):
    return t_u8(10) + t_int(len(data)) + data


def new_emu():
    e = Emu()
    e.baseline = e.snapshot()
    return e
