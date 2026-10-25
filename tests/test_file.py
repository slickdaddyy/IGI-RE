import os
import struct
import tempfile

import pytest
from emu import Emu, ErrorShown, TRAP_BASE, compare

GAME = os.path.dirname(os.environ.get("IGI_EXE", ""))
SCRATCH = os.path.join(tempfile.gettempdir(), "igi-file-tests")

emu = Emu()

DEVICES = 0x9436B0
DEVICE_SIZE = 0x8C
FAKE = 2
SLOTS = ["init", "shutdown", "open", "close", "read", "write", "seek", "tell", "size", "eof",
         "unk28", "unk2C", "error", "unk34", "load", "enumdir", "gettime", "unk44", "unk48",
         "remove", "unload"]


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def py_func(e, name, fn):
    """Address that runs the Python function fn(emu) when emulated code calls it."""
    trap = TRAP_BASE + len(e.imports) * 4
    e.imports.append(("test", name))
    e.write(trap, b"\xc3")
    e.stub(name, fn)
    return trap


# --- KERNEL32 file functions, backed by real files (read only) --------------

def k_create(e):
    name = e.read_cstr(e.arg(0)).decode()
    access, disp = e.arg(1), e.arg(4)
    e.log.append(("CreateFileA", name, access, disp))
    if access & 0x40000000:
        if not name.startswith(SCRATCH):
            return e.ret(0xFFFFFFFF, 28)
        data = bytearray()
    else:
        try:
            with open(name, "rb") as f:
                data = bytearray(f.read())
        except OSError:
            return e.ret(0xFFFFFFFF, 28)
    h = 0x100 + 4 * len(e.files)
    e.files[h] = [data, 0, name]
    e.ret(h, 28)


def k_read(e):
    h, buf, n, out = e.arg(0), e.arg(1), e.arg(2), e.arg(3)
    data, pos, name = e.files[h]
    if "short" in name:
        n = min(n, 4)
    chunk = bytes(data[pos:pos + n])
    e.files[h][1] = pos + len(chunk)
    e.write(buf, chunk)
    if out:
        e.write(out, u32(len(chunk)))
    e.ret(1, 20)


def k_write(e):
    h, buf, n, out = e.arg(0), e.arg(1), e.arg(2), e.arg(3)
    chunk = e.read(buf, n)
    e.log.append(("WriteFile", chunk))
    f = e.files[h]
    f[0][f[1]:f[1] + n] = chunk
    f[1] += n
    if out:
        e.write(out, u32(n))
    e.ret(1, 20)


def k_seek(e):
    h, off, how = e.arg(0), struct.unpack("<i", u32(e.arg(1)))[0], e.arg(3)
    f = e.files[h]
    f[1] = [0, f[1], len(f[0])][how] + off
    e.ret(f[1], 16)


def k_filetime(e):
    name = e.files[e.arg(0)][2]
    ft = int(os.stat(name).st_mtime) * 10_000_000 + 116444736000000000
    if e.arg(3):
        e.write(e.arg(3), struct.pack("<Q", ft))
    e.ret(1, 16)


def k_delete(e):
    name = e.read_cstr(e.arg(0)).decode()
    e.log.append(("DeleteFileA", name))
    e.ret(1, 4)


for name, fn in [("CreateFileA", k_create), ("ReadFile", k_read), ("WriteFile", k_write),
                 ("SetFilePointer", k_seek), ("GetFileTime", k_filetime), ("DeleteFileA", k_delete),
                 ("GetFileType", lambda e: e.ret(1, 4)), ("CloseHandle", lambda e: e.ret(1, 4)),
                 ("FlushFileBuffers", lambda e: e.ret(1, 4)), ("GetLastError", lambda e: e.ret(2))]:
    emu.stub(name, fn)


# --- a fake device in slot 2 whose callbacks log what they get --------------

def fake_cb(name, ret=0, strings=(), write=None):
    def fn(e):
        args = []
        for i in range(fn.nargs):
            v = e.arg(i)
            args.append(e.read_cstr(v) if i in strings else v)
        e.log.append((name,) + tuple(args))
        if write:
            write(e)
        r = e.fake.get(name, ret)
        if isinstance(r, list):
            r = r.pop(0)
        e.ret(r)
    return fn


def fake_read(e):
    e.write(e.arg(2), bytes(range(e.arg(3) & 0xFF)))


def fake_load(e):
    e.write(e.arg(2), u32(77))
    e.write(e.arg(3) + 0x18, u32(e.fake.get("load", 0)))


def fake_time(e):
    e.write(e.arg(1), b"\x11" * 8)


CALLBACKS = {
    "init": (1, fake_cb("init", ret=FAKE, strings=(0,))),
    "shutdown": (1, fake_cb("shutdown")),
    "open": (3, fake_cb("open", strings=(1,))),
    "close": (2, fake_cb("close")),
    "read": (4, fake_cb("read", ret=0x55, write=fake_read)),
    "write": (4, fake_cb("write", ret=0x66)),
    "seek": (4, fake_cb("seek", ret=0x77)),
    "size": (2, fake_cb("size", ret=40)),
    "eof": (2, fake_cb("eof", ret=1)),
    "error": (2, fake_cb("error", ret=0)),
    "unk34": (3, fake_cb("unk34", ret=0x34)),
    "load": (4, fake_cb("load", write=fake_load)),
    "gettime": (3, fake_cb("gettime", strings=(2,), write=fake_time)),
    "remove": (2, fake_cb("remove", strings=(1,))),
    "unload": (2, fake_cb("unload")),
}
FAKE_ADDR = {}
for slot, (nargs, fn) in CALLBACKS.items():
    fn.nargs = nargs
    FAKE_ADDR[slot] = py_func(emu, "fake_" + slot, fn)
# for the stack pointer argument of load
CALLBACKS["load"][1].nargs = 2


def device(dev):
    return DEVICES + dev * DEVICE_SIZE


def install_fake(e, name=b"FAKE:", **slots):
    base = device(FAKE)
    e.write(base, b"\0" * DEVICE_SIZE)
    for slot, addr in FAKE_ADDR.items():
        addr = slots.get(slot, addr)
        e.write(base + 4 * SLOTS.index(slot), u32(addr))
    e.write(base + 0x54, u32(FAKE))
    e.write(base + 0x58, u32(1))
    e.write(base + 0x7C, name + b"\0")


def fake_file(e):
    f = e.alloc(0xAC)
    e.write(f, u32(FAKE))
    return f


def base(e):
    e.call(0x4B0BA0)
    piob = e.alloc(80)
    e.write(0xC311DC, u32(piob))
    e.write(0xC321E0, u32(20))
    e.call(0x4B1200)
    install_fake(e)


def check(orig, ours, setup=None, args=(), **kw):
    logs = []

    def s(e):
        e.files = {}
        e.fake = {}
        e.log = []
        r = setup(e) if setup else None
        e.log = []
        logs.append(e.log)
        return r
    compare(emu, orig, ours, args=args, setup=s, **kw)
    assert logs[0] == logs[1], "calls differ:\n%s\n%s" % (logs[0], logs[1])
    return logs[0]


def check_error(orig, ours, setup):
    """Both versions must stop in Error_Show with the same message after the same calls."""
    out = []
    for target in (orig, ours):
        emu.reset()
        emu.files, emu.fake, emu.log = {}, {}, []
        args = setup(emu)
        emu.log = []
        with pytest.raises(ErrorShown) as ex:
            emu.call(target, *args)
        out.append((str(ex.value), emu.log))
    assert out[0] == out[1]
    return out[0][0]


def add_alias(e, name, paths):
    e.call(0x4B12F0, cstr(e, name), cstr(e, paths))


os.makedirs(SCRATCH + "/d1", exist_ok=True)
os.makedirs(SCRATCH + "/d2", exist_ok=True)
with open(SCRATCH + "/d2/two.txt", "wb") as f:
    f.write(b"second directory\n")
with open(SCRATCH + "/hello.txt", "wb") as f:
    f.write(b"hello world 12345")
with open(SCRATCH + "/short.txt", "wb") as f:
    f.write(b"this read comes back short")
HELLO = ("LOCAL:" + SCRATCH + "/hello.txt").encode()
SHORT = ("LOCAL:" + SCRATCH + "/short.txt").encode()
MISSING = ("LOCAL:" + SCRATCH + "/missing.txt").encode()
GAMEFILE = ("LOCAL:" + GAME + "/config.qvm").encode()


# --- File_* -------------------------------------------------------------

@pytest.mark.parametrize("mode", [b"r", b"r+", b"w", b"w+", b"rb", b"", b"x"])
def test_parse_mode(mode):
    check(0x4B1620, "File_ParseMode", setup=lambda e: (cstr(e, mode),))


@pytest.mark.parametrize("path,mode", [(HELLO, b"r"), (GAMEFILE, b"r"), (MISSING, b"r"),
                                       (("LOCAL:" + SCRATCH + "/new.txt").encode(), b"w")])
def test_open_local(path, mode):
    log = check(0x4B1510, "File_Open", setup=lambda e: (base(e), cstr(e, path), cstr(e, mode))[1:])
    assert log[0][0] == "CreateFileA"


@pytest.mark.parametrize("path", [b"DATA:two.txt", b"DATA:nothere.txt"])
def test_open_alias(path):
    def setup(e):
        base(e)
        add_alias(e, b"DATA:", ("LOCAL:%s/d1/;LOCAL:%s/d2/" % (SCRATCH, SCRATCH)).encode())
        return cstr(e, path), cstr(e, b"r")
    log = check(0x4B1510, "File_Open", setup=setup)
    assert len([x for x in log if x[0] == "CreateFileA"]) == 2


def test_open_fake():
    def setup(e):
        base(e)
        e.fake["open"] = fake_file(e)
        return cstr(e, b"FAKE:dir/x.bin"), cstr(e, b"w+")
    check(0x4B1510, "File_Open", setup=setup)


@pytest.mark.parametrize("path", [b"NOPE:x", b"x.txt"])
def test_open_bad_device(path):
    msg = check_error(0x4B1510, "File_Open", lambda e: (base(e), cstr(e, path), cstr(e, b"r"))[1:])
    assert msg == 'Illegal device name in path: "%s".'


def open_local(e, path=HELLO):
    base(e)
    f = e.call(0x4B1510, cstr(e, path), cstr(e, b"r"))[0]
    assert f
    return f


def test_close_local():
    check(0x4B1690, "File_Close", setup=lambda e: (open_local(e),), check_eax=False)


def test_close_null_file():
    def setup(e):
        e.write(0x943988, u32(5))
        return (0x943980,)
    check(0x4B1690, "File_Close", setup=setup, check_eax=False)


@pytest.mark.parametrize("orig,ours,extra", [
    (0x4B1690, "File_Close", ()),
    (0x4B16D0, "File_Read", (0, 9)),
    (0x4B1700, "File_Write", (0, 3)),
    (0x4B1730, "File_Seek", (-4, 2)),
    (0x4B1760, "File_Eof", ()),
    (0x4B1780, "File_GetSize", ()),
    (0x4B17A0, "File_GetError", ()),
    (0x4B17C0, "File_Flush", ()),
    (0x4B17F0, "File_Control", (5,)),
])
def test_dispatch_fake(orig, ours, extra):
    def setup(e):
        base(e)
        f = fake_file(e)
        args = [f] + list(extra)
        if ours in ("File_Read", "File_Write"):
            args[1] = cstr(e, b"abcdefghijk")
        return tuple(args)
    log = check(orig, ours, setup=setup, check_eax=ours != "File_Close")
    assert len(log) == 1


def test_read_size_local():
    def setup(e):
        f = open_local(e)
        return f, e.alloc(32), 32
    check(0x4B16D0, "File_Read", setup=setup, check_eax=False)
    check(0x4B1780, "File_GetSize", setup=lambda e: (open_local(e),))
    check(0x4B1760, "File_Eof", setup=lambda e: (open_local(e),))
    check(0x4B17A0, "File_GetError", setup=lambda e: (open_local(e),))


def test_write_local():
    def setup(e):
        base(e)
        f = e.call(0x4B1510, cstr(e, ("LOCAL:" + SCRATCH + "/out.txt").encode()), cstr(e, b"w"))[0]
        e.call(0x4B1700, f, cstr(e, b"written"), 7)
        return (f,)
    log = check(0x4B1690, "File_Close", setup=setup, check_eax=False)
    assert ("WriteFile", b"written") in log


@pytest.mark.parametrize("path", [HELLO, MISSING, b"FAKE:file"])
def test_get_time(path):
    log = check(0x4B1820, "File_GetTime", setup=lambda e: (base(e), e.alloc(8), cstr(e, path))[1:])
    assert log


def test_get_time_alias():
    def setup(e):
        base(e)
        add_alias(e, b"T:", b"FAKE:a/;FAKE:b/;FAKE:c/")
        e.fake["gettime"] = [-1, -1, 0]
        return e.alloc(8), cstr(e, b"T:f")
    assert len(check(0x4B1820, "File_GetTime", setup=setup)) == 3

    def setup_fail(e):
        r = setup(e)
        e.fake["gettime"] = [-1, -1, -1]
        return r
    assert len(check(0x4B1820, "File_GetTime", setup=setup_fail)) == 3


@pytest.mark.parametrize("path", [HELLO, GAMEFILE, MISSING, SHORT])
@pytest.mark.parametrize("want_size", [True, False])
def test_load_local(path, want_size):
    check(0x4B18B0, "File_Load", setup=lambda e: (base(e), cstr(e, path), e.alloc(4) if want_size else 0)[1:])


@pytest.mark.parametrize("want_size", [True, False])
def test_load_fake(want_size):
    def setup(e):
        base(e)
        e.fake["open"] = fake_file(e)
        e.fake["load"] = 0x12345678
        return cstr(e, b"FAKE:big.res"), e.alloc(4) if want_size else 0
    check(0x4B18B0, "File_Load", setup=setup)


def test_add_loaded():
    check(0x4B19C0, "File_AddLoaded", setup=lambda e: (base(e), cstr(e, b"LOCAL:some/file.res"))[1:])


def loaded(e):
    base(e)
    sz = e.alloc(4)
    datas = [e.call(0x4B18B0, cstr(e, p), sz)[0] for p in (HELLO, GAMEFILE)]
    assert all(datas)
    return datas


@pytest.mark.parametrize("which", [0, 1])
def test_find_loaded(which):
    check(0x4B1A90, "File_FindLoaded", setup=lambda e: (loaded(e)[which],))


def test_find_loaded_missing():
    msg = check_error(0x4B1A90, "File_FindLoaded", lambda e: (loaded(e)[0] + 1,))
    assert msg == "No files loaded at address: %p"


@pytest.mark.parametrize("which", [0, 1])
def test_unload(which):
    check(0x4B1A30, "File_Unload", setup=lambda e: (loaded(e)[which],), check_eax=False)


def test_unload_fake():
    def setup(e):
        base(e)
        e.fake["open"] = fake_file(e)
        e.fake["load"] = 0x12345678
        return (e.call(0x4B18B0, cstr(e, b"FAKE:big.res"), 0)[0],)
    log = check(0x4B1A30, "File_Unload", setup=setup, check_eax=False)
    assert log[0][0] == "unload"


def test_delete_bad_device():
    msg = check_error(0x4B1AC0, "File_Delete", lambda e: (base(e), cstr(e, b"NOPE:x"))[1:])
    assert msg == 'Illegal device name in path: "%s".'


@pytest.mark.parametrize("path", [HELLO, b"FAKE:gone.txt"])
def test_delete(path):
    check(0x4B1AC0, "File_Delete", setup=lambda e: (base(e), cstr(e, path))[1:], check_eax=False)


def test_delete_alias():
    def setup(e):
        base(e)
        add_alias(e, b"T:", b"FAKE:a/;LOCAL:b/;FAKE:c/")
        return (cstr(e, b"T:f"),)
    log = check(0x4B1AC0, "File_Delete", setup=setup, check_eax=False)
    assert len(log) == 3


def test_delete_no_callback():
    def setup(e):
        base(e)
        install_fake(e, remove=0)
        return (cstr(e, b"FAKE:f"),)
    assert check(0x4B1AC0, "File_Delete", setup=setup, check_eax=False) == []


@pytest.mark.parametrize("path", [HELLO, MISSING])
def test_exists(path):
    check(0x4B1FC0, "File_Exists", setup=lambda e: (base(e), cstr(e, path))[1:])


def test_get_expanded_path():
    check(0x4B1FB0, "File_GetExpandedPath")


# --- Device_* -------------------------------------------------------------

@pytest.mark.parametrize("ret", [0, FAKE])
def test_device_register(ret):
    def setup(e):
        base(e)
        e.write(device(FAKE) + 0x58, u32(0))
        e.fake["init"] = ret
        return cstr(e, b"FAKE:"), FAKE_ADDR["init"]
    check(0x4B1B90, "Device_Register", setup=setup)


def test_device_shutdown():
    check(0x4B1BC0, "Device_Shutdown", setup=lambda e: (base(e), FAKE)[1:], check_eax=False)


@pytest.mark.parametrize("addr,name", [
    (0x4B1BE0, "Device_SetInit"), (0x4B1C00, "Device_SetShutdown"), (0x4B1C20, "Device_SetOpen"),
    (0x4B1C40, "Device_SetClose"), (0x4B1C60, "Device_SetRead"), (0x4B1C80, "Device_SetWrite"),
    (0x4B1CA0, "Device_SetSeek"), (0x4B1CC0, "Device_SetTell"), (0x4B1CE0, "Device_SetSize"),
    (0x4B1D00, "Device_SetError"), (0x4B1D20, "Device_SetUnused"), (0x4B1D40, "Device_SetEof"),
    (0x4B1D60, "Device_SetControl"), (0x4B1D80, "Device_SetDelete"), (0x4B1DA0, "Device_SetEnumDir"),
    (0x4B1DC0, "Device_SetGetTime"),
])
def test_device_setters(addr, name):
    check(addr, name, args=(3, 0xDEADBEEF), check_eax=False)


@pytest.mark.parametrize("dev", [1, 2, 4])
def test_device_get_name(dev):
    check(0x4B1DE0, "Device_GetName", setup=lambda e: (base(e), dev)[1:])


# --- Path_* -------------------------------------------------------------------

PATHS = [b"LOCAL:dir/sub/file.tex", b"file.tex", b"", b"a:b:c", b"dir\\x.y.z", b"noext", b".hidden",
         b"LOCAL:", b"dir/file.", b"c:\\dir.d\\file"]


@pytest.mark.parametrize("path", PATHS)
@pytest.mark.parametrize("size", [0, 4, 64])
def test_copy_no_device(path, size):
    check(0x4B1E00, "Path_CopyNoDevice", setup=lambda e: (e.alloc(64) if size else 0, size, cstr(e, path)))


@pytest.mark.parametrize("path", PATHS)
def test_skip_device(path):
    check(0x4B1E70, "Path_SkipDevice", setup=lambda e: (cstr(e, path),))


@pytest.mark.parametrize("path", PATHS)
@pytest.mark.parametrize("size", [0, 3, 64])
def test_get_extension(path, size):
    check(0x4B1E90, "Path_GetExtension", setup=lambda e: (e.alloc(64) if size else 0, size, cstr(e, path)))


@pytest.mark.parametrize("path", PATHS)
@pytest.mark.parametrize("size", [-1, 0, 5, 64])
def test_get_dir(path, size):
    check(0x4B1EE0, "Path_GetDir", setup=lambda e: (e.alloc(64), size, cstr(e, path)))


@pytest.mark.parametrize("path", PATHS)
def test_strip_extension(path):
    def setup(e):
        out = e.alloc(64)
        e.write(out, b"\xAA" * 64)
        return out, 64, cstr(e, path)
    check(0x4B1F30, "Path_StripExtension", setup=setup)
