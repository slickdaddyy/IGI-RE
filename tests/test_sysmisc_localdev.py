import struct

import pytest

from emu import Emu
from test_sysmisc_util import Fakes, check, check_error, cstr, u32

FILES = 0xA7B640
NAMES = {0x4B1BE0: "init", 0x4B1C00: "shutdown", 0x4B1C20: "open", 0x4B1C40: "close", 0x4B1C60: "read",
         0x4B1C80: "write", 0x4B1CA0: "seek", 0x4B1CC0: "tell", 0x4B1CE0: "size", 0x4B1D00: "error",
         0x4B1D20: "unk2c", 0x4B1D40: "eof", 0x4B1D60: "unk34", 0x4B1D80: "delete", 0x4B1DA0: "enumdir",
         0x4B1DC0: "gettime"}

e = Emu()
fk = Fakes(e)
ST = {"fopen": 0x7000, "n": 0, "tell": [], "find": [], "handle": 0x1234, "create": 0x88, "remove": 0}

fk.add(0x4B1180, 0, ret=3)
for _a in NAMES:
    fk.add(_a, 2)
fk.add(0x4B1290, 2, fmt=lambda em, a: (a[0], em.read_cstr(a[1])))
fk.add(0x4B1B90, 2, fmt=lambda em, a: (em.read_cstr(a[0]), em.normalize(a[1])))
fk.add(0x4B11A0, 1, ret=2, fmt=lambda em, a: (em.read_cstr(a[0]),))
fk.add(0x4B1BC0, 1)
fk.add(0x4B1DE0, 1, ret=lambda em, a: ST["devname"])
fk.add(0x4A5350, 2, ret=lambda em, a: ST["fopen"], fmt=lambda em, a: (em.read_cstr(a[0]), em.read_cstr(a[1])))
fk.add(0x4A5234, 1, ret=0x55)
fk.add(0x516C91, 4, ret=lambda em, a: ST["n"])
fk.add(0x516D79, 4, ret=lambda em, a: ST["n"])
fk.add(0x4A52A4, 3, ret=0x66)
fk.add(0x4A6E17, 1, ret=lambda em, a: ST["tell"].pop(0))
fk.add(0x516C67, 1, ret=lambda em, a: ST["remove"], fmt=lambda em, a: (em.read_cstr(a[0]),))


def _find_fill(em, fd):
    attr, size, name = ST["find"].pop(0)
    em.write(fd, u32(attr) + b"\x11" * 0x1C + u32(size) + b"\x22" * 8 + name + b"\0")


def _find_first(em):
    fk.log.append(("FindFirstFileA", em.read_cstr(em.arg(0))))
    if ST["handle"] != 0xFFFFFFFF:
        _find_fill(em, em.arg(1))
    em.ret(ST["handle"], 8)


def _find_next(em):
    fk.log.append(("FindNextFileA", em.arg(0)))
    if ST["find"]:
        _find_fill(em, em.arg(1))
        em.ret(1, 8)
    else:
        em.ret(0, 8)


def _create(em):
    fk.log.append(("CreateFileA", em.read_cstr(em.arg(0))) + tuple(em.arg(i) for i in range(1, 7)))
    em.ret(ST["create"], 28)


def _get_time(em):
    fk.log.append(("GetFileTime",) + tuple(em.arg(i) for i in range(4)))
    em.write(em.arg(3), u32(0x01020304) + u32(0x05060708))
    em.ret(1, 16)


def _compare(em):
    a = struct.unpack("<Q", em.read(em.arg(0), 8))[0]
    b = struct.unpack("<Q", em.read(em.arg(1), 8))[0]
    em.ret((a > b) - (a < b), 8)


def _strcat(em):
    d = em.arg(0)
    em.write(d + len(em.read_cstr(d)), em.read_cstr(em.arg(1)) + b"\0")
    em.ret(d)


e.stub("strcat", _strcat)
e.stub("FindFirstFileA", _find_first)
e.stub("FindNextFileA", _find_next)
e.stub("FindClose", lambda em: (fk.log.append(("FindClose", em.arg(0))), em.ret(1, 4)))
e.stub("CreateFileA", _create)
e.stub("GetFileTime", _get_time)
e.stub("CloseHandle", lambda em: (fk.log.append(("CloseHandle", em.arg(0))), em.ret(1, 4)))
e.stub("CompareFileTime", _compare)

ENTRY = fk.add(fk.code(b"\xc3"), 2, fmt=lambda em, a: (em.read_cstr(em.u32(a[0])), em.u32(a[0] + 4),
                                                        em.read(a[0] + 8, 1), a[1]))


def test_register():
    check(e, fk, 0x5098B0, "LocalDevice_Register", check_eax=False)


def test_unregister():
    check(e, fk, 0x5098D0, "LocalDevice_Unregister", check_eax=False)


@pytest.mark.parametrize("a,b", [(5, 5), (1 << 40, 3), (3, 1 << 40)])
def test_compare_time(a, b):
    def setup(em):
        p = em.alloc(16)
        em.write(p, struct.pack("<QQ", a, b))
        return p, p + 8
    check(e, fk, 0x5098F0, "File_CompareTime", setup=setup)


def test_init():
    def setup(em):
        for i in range(16):
            em.write(FILES + 0xAC * i + 0xA8, u32(1))
        return (cstr(em, b"LOCAL:"),)
    check(e, fk, 0x509F50, "LocalDevice_Init", setup=setup)


@pytest.mark.parametrize("ret", [0, -1])
def test_delete(ret):
    def setup(em):
        ST["remove"] = ret
        return 1, cstr(em, b"c:/x.txt")
    check(e, fk, 0x50A040, "LocalDevice_Delete", setup=setup)


@pytest.mark.parametrize("path", [b"c:/games/igi/", b"c:/games/igi", b"x"])
@pytest.mark.parametrize("files", [None, [], [(0x10, 0, b".")], [(0x20, 123, b"a.txt"), (0x10, 0, b"..")],
                                   [(0x30, 9, b"dir"), (0x80, 7, b"b.res"), (0x11, 1, b".hidden")]])
def test_enum_dir(path, files):
    def setup(em):
        ST["devname"] = cstr(em, b"LOCAL:")
        ST["handle"] = 0xFFFFFFFF if files is None else 0x1234
        ST["find"] = list(files or [(0x20, 1, b"first")])
        if files == []:
            ST["find"] = [(0x20, 1, b"first")]
        return 1, cstr(em, path), ENTRY, 0x4321
    check(e, fk, 0x50A050, "LocalDevice_EnumDir", setup=setup, check_eax=False)


@pytest.mark.parametrize("mode", [1, 2, 3])
@pytest.mark.parametrize("free", [0, 5, None])
@pytest.mark.parametrize("ok", [True, False])
def test_open(mode, free, ok):
    def setup(em):
        ST["fopen"] = 0x7000 if ok else 0
        for i in range(16):
            em.write(FILES + 0xAC * i + 0xA8, u32(0 if i == free else 1))
        p = em.alloc(0x100)
        em.write(p, b"c:/f.dat\0")
        return 1, p, mode
    check(e, fk, 0x50A370, "LocalDevice_Open", setup=setup)


@pytest.mark.parametrize("mode", [0, 4, -1])
def test_open_bad_mode(mode):
    def setup(em):
        return 1, cstr(em, b"c:/f.dat"), mode
    check_error(e, fk, 0x50A370, "LocalDevice_Open", setup=setup)


def lfile(em, error=0):
    f = FILES + 0xAC * 2
    em.write(f + 0x98, u32(0x7000) + u32(error) + u32(0x77) + u32(1) + u32(1))
    return f


def test_close():
    check(e, fk, 0x50A430, "LocalDevice_Close", setup=lambda em: (1, lfile(em)))


@pytest.mark.parametrize("n,size", [(10, 10), (3, 10), (0, 0), (5, -1)])
def test_read_write(n, size):
    def setup(em):
        ST["n"] = n
        return 1, lfile(em), 0x5000, size
    check(e, fk, 0x50A450, "LocalDevice_Read", setup=setup)
    check(e, fk, 0x50A490, "LocalDevice_Write", setup=setup)


@pytest.mark.parametrize("whence", [0, 1, 2, 3, -1])
def test_seek(whence):
    check(e, fk, 0x50A4D0, "LocalDevice_Seek", setup=lambda em: (1, lfile(em), 1234, whence))


def test_size_tell():
    def setup(em):
        ST["tell"] = [40, 100]
        return 1, lfile(em)
    check(e, fk, 0x50A510, "LocalDevice_Size", setup=setup)
    check(e, fk, 0x50A550, "LocalDevice_Tell", setup=setup)


@pytest.mark.parametrize("pos,size", [(5, 10), (10, 10), (11, 10)])
def test_eof(pos, size):
    def setup(em):
        ST["tell"] = [pos, pos, size]
        return 1, lfile(em)
    check(e, fk, 0x50A570, "LocalDevice_Eof", setup=setup)


def test_getters():
    for orig, ours in ((0x50A5A0, "LocalDevice_GetUnk2C"), (0x50A5B0, "LocalDevice_GetError")):
        check(e, fk, orig, ours, setup=lambda em: (1, lfile(em, 1)))
    for arg in (0, 1, 2):
        check(e, fk, 0x50A5C0, "LocalDevice_Control", args=(1, 0, arg))


@pytest.mark.parametrize("handle", [0x88, 0xFFFFFFFF])
def test_get_time(handle):
    def setup(em):
        ST["create"] = handle
        t = em.alloc(8)
        em.write(t, b"\xff" * 8)
        return 1, t, cstr(em, b"c:/f.dat")
    check(e, fk, 0x50A5D0, "LocalDevice_GetTime", setup=setup)
