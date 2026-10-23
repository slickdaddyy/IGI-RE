"""C runtime pieces (0x516770 - 0x516F9D): strlwr, x87 acos/asin, strncat, atof, splitpath, remove,
fread, fwrite, _fltin, _mbsnbcpy."""
import struct

import pytest

from emu import CALL_LOG, Emu, compare_hooked
from test_d3d_util import D3DEmu, check, cstr, pyfake, u32

e = D3DEmu()

LC_CTYPE, MB_CUR_MAX, ISMBCP, MBCTYPE = 0x9361E4, 0x54427C, 0xC3221C, 0xC32320
FASTFLAG = 0x936058

# --- strlwr -------------------------------------------------------------------

LCMAP = {"mode": "ok"}


def lcmap(em):
    src = em.read_cstr(em.arg(2))
    dst, n = em.arg(4), em.arg(5)
    if LCMAP["mode"] == "fail_size":
        return 0
    if dst:
        if LCMAP["mode"] == "fail_map":
            return 0
        em.write(dst, (src.lower() + b"\0")[:n])
    return len(src) + 1


pyfake(e, 0x4AD4D3, "LCMapStringA", 8, before=lcmap)


@pytest.mark.parametrize("text", [b"", b"Hello World", b"ABCxyz@[`{", b"\xc0\xdfMiXeD 123"])
@pytest.mark.parametrize("mode", ["c", "ok", "fail_size", "fail_map"])
def test_strlwr(text, mode):
    def setup(em):
        LCMAP["mode"] = mode
        u32(em, LC_CTYPE, 0 if mode == "c" else 0x409)
        return (cstr(em, text),)
    check(e, 0x516770, "crt_strlwr", setup=setup)


# --- strncat ------------------------------------------------------------------

@pytest.mark.parametrize("dst_off,src_off", [(0, 0), (1, 2), (3, 1), (2, 3)])
@pytest.mark.parametrize("dlen,slen,n", [(0, 0, 5), (3, 10, 0), (5, 10, 4), (5, 10, 10), (5, 10, 11), (7, 9, 100),
                                         (1, 1, 1), (6, 13, 8), (6, 13, 13), (0, 4, 3)])
def test_strncat(dst_off, src_off, dlen, slen, n):
    def setup(em):
        d = em.alloc(64) + dst_off
        s = em.alloc(64) + src_off
        em.write(d, bytes(0x41 + i for i in range(dlen)) + b"\0" + b"\xee" * 30)
        em.write(s, bytes(0x61 + i for i in range(slen)) + b"\0" + b"\xdd" * 8)
        return d, s, n
    check(e, 0x5169B0, "crt_strncat", setup=setup)


# --- atof / _fltin --------------------------------------------------------------

NUMBERS = [b"0", b"1.5", b"  -12.25e3xyz", b"\t\n 3.14159265358979", b"1e400", b"-1e-400", b"abc", b"", b".5",
           b"0x10", b"   +7", b"1e-310", b"123456789012345678901234567890", b"\x85 2"]


@pytest.mark.parametrize("text", NUMBERS)
@pytest.mark.parametrize("mb", [1, 2])
def test_atof(text, mb):
    def setup(em):
        u32(em, MB_CUR_MAX, mb)
        return (cstr(em, text),)
    check(e, 0x516AD3, "crt_atof", setup=setup, check_eax=False, check_st0=True)


@pytest.mark.parametrize("text", NUMBERS)
def test_fltin(text):
    def setup(em):
        s = cstr(em, text)
        return s, len(text), 0, 0
    check(e, 0x516E83, "crt_fltin", setup=setup)


# --- _mbsnbcpy / splitpath ---------------------------------------------------------

def mbcs(em, on):
    u32(em, ISMBCP, on)
    if on:
        for c in range(0x81, 0xA0):
            em.write(MBCTYPE + 1 + c, bytes([em.read(MBCTYPE + 1 + c, 1)[0] | 4]))


@pytest.mark.parametrize("src,n", [(b"abc", 2), (b"abc", 3), (b"abc", 4), (b"abc", 9), (b"", 3), (b"a\x81bc", 2),
                                   (b"a\x81bc", 3), (b"a\x81bc", 7), (b"\x81", 4), (b"\x81", 1), (b"\x82\x83\x84", 2),
                                   (b"x\x81", 2), (b"abcdefgh", 0)])
@pytest.mark.parametrize("on", [0, 1])
def test_mbsnbcpy(src, n, on):
    def setup(em):
        mbcs(em, on)
        d = em.alloc(32)
        em.write(d, b"\xcc" * 32)
        return d, cstr(em, src), n
    check(e, 0x516F14, "crt_mbsnbcpy", setup=setup)


PATHS = [b"", b"c", b"c:", b"c:\\dir\\file.txt", b"d:/a/b/c", b"name.ext", b"dir.d/name", b"a.b.c", b"\\\\srv\\x.",
         b".hidden", b"/", b"c:file", b"x" * 300 + b".e", b"d\\" + b"y" * 300, b"a\\b." + b"z" * 300,
         b"\x81\\dir\\f\x81.x", b"a\x81", b"k:\x81.\x81/z.q"]


@pytest.mark.parametrize("path", PATHS)
@pytest.mark.parametrize("mask", [0x1F, 0x00, 0x15, 0x0A])
@pytest.mark.parametrize("on", [0, 1])
def test_splitpath(path, mask, on):
    def setup(em):
        mbcs(em, on)
        outs = []
        for bit in range(4):
            p = em.alloc(0x110)
            em.write(p, b"\xcc" * 0x110)
            outs.append(p if mask >> bit & 1 else 0)
        return (cstr(em, path),) + tuple(outs)
    check(e, 0x516B20, "crt_splitpath", setup=setup, check_eax=False)


# --- remove -----------------------------------------------------------------------

REMOVE = {"ok": 1, "err": 2}


def stub_imports(em):
    em.stub("DeleteFileA", lambda em: em.ret(REMOVE["ok"], 4))
    em.stub("GetLastError", lambda em: em.ret(REMOVE["err"]))


stub_imports(e)


@pytest.mark.parametrize("ok,err", [(1, 0), (0, 2), (0, 5), (0, 0), (0, 0x12345)])
def test_remove(ok, err):
    def setup(em):
        REMOVE["ok"], REMOVE["err"] = ok, err
        return (cstr(em, b"some.file"),)
    check(e, 0x516C67, "crt_remove", setup=setup)


# --- fread / fwrite -----------------------------------------------------------------

IO = {}


def io_read(em):
    n = em.arg(2)
    r = IO["read"].pop(0) if IO["read"] else n
    if r == "all":
        r = n
    if r > 0:
        em.write(em.arg(1), bytes((IO["byte"] + i) & 0xFF for i in range(r)))
        IO["byte"] += r
    return r


def io_filbuf(em):
    fp = em.arg(0)
    r = IO["filbuf"].pop(0) if IO["filbuf"] else -1
    if r == "refill":
        # refill the buffer like the runtime: return the first byte, leave the rest
        base = em.u32(fp + 8)
        size = em.u32(fp + 0x18)
        em.write(base, bytes((IO["byte"] + i) & 0xFF for i in range(size)))
        IO["byte"] += size
        u32(em, fp, base + 1)
        u32(em, fp + 4, size - 1)
        return em.read(base, 1)[0]
    return r


def io_write(em):
    n = em.arg(2)
    r = IO["write"].pop(0) if IO["write"] else n
    if r == "all":
        r = n
    return (r, em.read(em.arg(1), n))


def io_flsbuf(em):
    r = IO["flsbuf"].pop(0) if IO["flsbuf"] else em.arg(0) & 0xFF
    return r


pyfake(e, 0x4AC12D, "_read", 3, before=io_read)
pyfake(e, 0x4A6CA4, "_filbuf", 1, before=io_filbuf)
pyfake(e, 0x4ABEC7, "_write", 3, before=io_write)
pyfake(e, 0x4A760B, "_flsbuf", 2, before=io_flsbuf)
pyfake(e, 0x4A6BD2, "_flush", 1, before=lambda em: IO["flush"].pop(0) if IO["flush"] else 0)


def make_file(em, flag, cnt, bufsiz):
    buf = em.alloc(max(bufsiz, 1) + 16)
    em.write(buf, bytes((0x30 + i) & 0xFF for i in range(max(bufsiz, 1))))
    fp = em.alloc(0x20)
    em.write(fp, struct.pack("<IiIiiiiI", buf + (bufsiz - cnt if bufsiz >= cnt else 0), cnt, buf, flag, 3, 0,
                             bufsiz, 0))
    return fp


READ_CASES = [
    # flag, cnt, bufsiz, size, count, read results, filbuf results
    (0x01 | 0x08, 10, 16, 1, 5, [], []),
    (0x01 | 0x08, 10, 16, 1, 10, [], []),
    (0x01 | 0x08, 10, 16, 1, 12, [], ["refill", "refill"]),
    (0x01 | 0x08, 3, 16, 4, 10, [], []),
    (0x01 | 0x08, 3, 16, 4, 10, ["all"], ["refill"]),
    (0x01 | 0x08, 0, 16, 1, 40, [5, "all"], ["refill"] * 4),
    (0x01 | 0x08, 0, 16, 1, 40, [0], []),
    (0x01 | 0x08, 0, 16, 2, 40, [-1], []),
    (0x01 | 0x08, 2, 16, 3, 3, [], [-1]),
    (0x01, 0, 0, 1, 0x1800, ["all"], ["refill"] * 3),
    (0x01, 0, 0, 1, 0x800, [], [0x41, 0x42, -1]),
    (0x01 | 0x100, 0, 0, 1, 7, [], [0x41, 0x42, -1]),
    (0x01 | 0x100, 0, 0, 1, 7, ["all"], [0x41, 0x42, 1, 2, 3, 4, 5]),
    (0x01 | 0x08, 4, 16, 0, 5, [], []),
    (0x01 | 0x08, 4, 16, 5, 0, [], []),
    (0x01 | 0x08, 0, 16, 7, 9, [10, 20, 33], []),
]


@pytest.mark.parametrize("case", range(len(READ_CASES)))
def test_fread(case):
    flag, cnt, bufsiz, size, count, reads, fills = READ_CASES[case]

    def setup(em):
        IO.update(read=list(reads), filbuf=list(fills), byte=0x80)
        fp = make_file(em, flag, cnt, bufsiz)
        dst = em.alloc(size * count + 16)
        return dst, size, count, fp
    check(e, 0x516C91, "crt_fread", setup=setup)


WRITE_CASES = [
    # flag, cnt, bufsiz, size, count, write results, flsbuf results, flush results
    (0x02 | 0x08, 10, 16, 1, 5, [], [], []),
    (0x02 | 0x08, 10, 16, 1, 10, [], [], []),
    (0x02 | 0x08, 10, 16, 1, 12, [], [], []),
    (0x02 | 0x08, 3, 16, 4, 10, [], [], []),
    (0x02 | 0x08, 0, 16, 1, 40, [], [], [0]),
    (0x02 | 0x08, 0, 16, 1, 40, [], [], [1]),
    (0x02 | 0x08, 0, 16, 1, 40, [-1], [], [0]),
    (0x02 | 0x08, 0, 16, 1, 40, [17], [], [0]),
    (0x02 | 0x08, 0, 16, 2, 9, ["all"], [-1], [0]),
    (0x02, 0, 0, 1, 0x1800, ["all"], [], []),
    (0x02, 0, 0, 1, 0x800, [], [0x41, 0x42, -1], []),
    (0x02 | 0x100, 0, 0, 1, 7, [], [1, 2, 3, -1], []),
    (0x02 | 0x100, 0, 0, 3, 7, ["all"], [], []),
    (0x02 | 0x04, 0, 0, 1, 3, [], [], []),
    (0x02 | 0x08, 4, 16, 0, 5, [], [], []),
    (0x02 | 0x08, 0, 16, 7, 9, [10, 20, 33], [], [0, 0, 0]),
]


@pytest.mark.parametrize("case", range(len(WRITE_CASES)))
def test_fwrite(case):
    flag, cnt, bufsiz, size, count, writes, flsbufs, flushes = WRITE_CASES[case]

    def setup(em):
        IO.update(write=list(writes), flsbuf=list(flsbufs), flush=list(flushes), byte=0)
        fp = make_file(em, flag, cnt, bufsiz)
        src = em.alloc(size * count + 16)
        em.write(src, bytes((0x90 + i * 7) & 0xFF for i in range(size * count)))
        return src, size, count, fp
    check(e, 0x516D79, "crt_fwrite", setup=setup)


# --- x87 acos / asin ------------------------------------------------------------------

CELLS = CALL_LOG + 0x40000


def ext(v):
    """80 bit extended encoding of a python float, or raw (mantissa, exp_sign) tuple."""
    if isinstance(v, tuple):
        return struct.pack("<QH", *v)
    if v != v:
        return struct.pack("<QH", 0xC000000000000000, 0x7FFF)
    if v in (float("inf"), float("-inf")):
        return struct.pack("<QH", 0x8000000000000000, 0x7FFF | (0x8000 if v < 0 else 0))
    if v == 0:
        return struct.pack("<QH", 0, 0x8000 if str(v).startswith("-") else 0)
    import math
    m, ex = math.frexp(abs(v))
    mant = int(m * (1 << 64)) & 0xFFFFFFFFFFFFFFFF
    return struct.pack("<QH", mant, (ex - 1 + 16383) | (0x8000 if v < 0 else 0))


def x87_tramp(em, target, cells):
    """fldcw cw; fld tbyte x; call target; fstp tbyte res; fnstsw; fnstcw; ret"""
    addr = em.symbols[target] if isinstance(target, str) else target
    key = ("x87", addr, cells)
    em.x87 = getattr(em, "x87", {})
    if key not in em.x87:
        p = em.fake_top
        em.fake_top += 64
        c = b"\xd9\x2d" + struct.pack("<I", cells)
        c += b"\xdb\x2d" + struct.pack("<I", cells + 0x10)
        c += b"\xb8" + struct.pack("<I", addr) + b"\xff\xd0"
        c += b"\xdb\x3d" + struct.pack("<I", cells + 0x20)
        c += b"\xdd\x3d" + struct.pack("<I", cells + 0x30)
        c += b"\xd9\x3d" + struct.pack("<I", cells + 0x34)
        c += b"\xc3"
        em.write(p, c)
        em.x87[key] = p
    return em.x87[key]


def cdecl_tramp(em, target, cells):
    """fldcw cw; push double; call target; add esp, 8; fstp tbyte res; fnstsw; fnstcw; ret"""
    key = ("cdecl", target, cells)
    em.x87 = getattr(em, "x87", {})
    if key not in em.x87:
        p = em.fake_top
        em.fake_top += 64
        c = b"\xd9\x2d" + struct.pack("<I", cells)
        c += b"\xff\x35" + struct.pack("<I", cells + 0x44) + b"\xff\x35" + struct.pack("<I", cells + 0x40)
        c += b"\xb8" + struct.pack("<I", target) + b"\xff\xd0" + b"\x83\xc4\x08"
        c += b"\xdb\x3d" + struct.pack("<I", cells + 0x20)
        c += b"\xdd\x3d" + struct.pack("<I", cells + 0x30)
        c += b"\xd9\x3d" + struct.pack("<I", cells + 0x34)
        c += b"\xc3"
        em.write(p, c)
        em.x87[key] = p
    return em.x87[key]


XS = [0.0, -0.0, 0.5, -0.5, 0.25, 0.9999999, -0.9999999, 1.0, -1.0, 1.0000001, -1.5, 2.0, 1e300, 5e-324,
      float("inf"), float("-inf"), float("nan"), (0xC90FDAA22168C235, 0x3FFE), (0xFFFFFFFFFFFFFFFF, 0x3FFE),
      (0x8000000000000001, 0x3FFF), (0x8000000000000001, 0xBFFF), (0x8000000000000000, 0x3FFF),
      (0xA000000000000000, 0x7FFF), (0xE000000000000000, 0xFFFF), (0x8000000000000400, 0x3FFE)]
CWS = [0x37F, 0x7F, 0xC7F, 0x77F, 0x27F, 0x17F]


def x87_setup(em, x, cw, fast):
    em.write(CELLS, struct.pack("<H", cw))
    em.write(CELLS + 0x10, ext(x))
    u32(em, FASTFLAG, fast)


@pytest.mark.parametrize("fast", [0, 1])
@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("x", range(len(XS)))
@pytest.mark.parametrize("orig,ours", [(0x516810, "crt_acos87"), (0x5168E0, "crt_asin87")])
def test_ci_acos_asin(orig, ours, x, cw, fast):
    check(e, x87_tramp(e, orig, CELLS), x87_tramp(e, ours, CELLS), setup=lambda em: x87_setup(em, XS[x], cw, fast))


plain = Emu()
hooked = Emu()
hooked.install_hooks(only={0x51682D, 0x5168FD})


def dbl(x):
    if isinstance(x, tuple):
        return None
    return struct.pack("<d", x)


@pytest.mark.parametrize("fast", [0, 1])
@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("x", [i for i, v in enumerate(XS) if not isinstance(v, tuple)] +
                         ["snan", "qnan_neg", "max"])
@pytest.mark.parametrize("entry", [0x516824, 0x5168F4])
def test_acos_asin_core(entry, x, cw, fast):
    raw = {"snan": struct.pack("<Q", 0x7FF0000000000001), "qnan_neg": struct.pack("<Q", 0xFFF8000000000123),
           "max": struct.pack("<Q", 0x3FEFFFFFFFFFFFFF)}.get(x) or dbl(XS[x])

    def setup(em):
        em.write(CELLS, struct.pack("<H", cw))
        em.write(CELLS + 0x40, raw)
        u32(em, FASTFLAG, fast)
    t = cdecl_tramp(plain, entry, CELLS)
    assert cdecl_tramp(hooked, entry, CELLS) == t
    compare_hooked(plain, hooked, t, setup=setup)
