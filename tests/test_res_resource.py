import random
import struct

import pytest

from emu import Emu, compare
from test_res_util import FILES, GOOD, LOG, blob, cstr, errors_equal, fake, ilff, init, logs_equal, respack, resource, u32

emu = Emu()
rng = random.Random(7)


@pytest.mark.parametrize("s", [b"", b"a", b"data\\Foo.txt", b"LOCAL:models/\xe9x.res", b"Mixed_Case 123"])
def test_string_to_upper(s):
    compare(emu, 0x4B5AF0, "String_ToUpper", setup=lambda e: (e.alloc(64), cstr(e, s)), check_eax=False)


def test_resource_hash():
    for _ in range(20):
        s = bytes(rng.randrange(1, 256) for _ in range(rng.randrange(0, 30)))
        compare(emu, 0x4B5B30, "Resource_Hash", setup=lambda e: (cstr(e, s),))


@pytest.mark.parametrize("data,time,type", [(0, False, 3), (0x1234, True, 2), (0x5678, False, 1)])
def test_resource_create(data, time, type):
    def setup(e):
        init(e)
        resource(e, b"first")
        t = 0
        if time:
            t = e.alloc(8)
            e.write(t, struct.pack("<II", 7, 9))
        return cstr(e, b"Data\\Sub\\File.Txt"), 0x4000, data, 77, type, t
    compare(emu, 0x4B59F0, "Resource_Create", setup=setup)


NAMES = [b"a", b"LOCAL:b.res", b"x\\y\\z.txt", b"MISSION.QSC", b"other"]


@pytest.mark.parametrize("name", NAMES + [b"X\\Y\\Z.TXT", b"missing", b""])
@pytest.mark.parametrize("want", [True, False])
def test_resource_is_loaded(name, want):
    def setup(e):
        init(e)
        for n in NAMES:
            resource(e, n)
        return cstr(e, name), e.alloc(4) if want else 0
    compare(emu, 0x4B5B90, "Resource_IsLoaded", setup=setup)


@pytest.mark.parametrize("name", [b"a", b"X:nofile"])
def test_resource_exists(name):
    def setup(e):
        init(e)
        resource(e, b"a")
        return (cstr(e, name),)
    compare(emu, 0x4B5B60, "Resource_Exists", setup=setup)
    logs_equal()


def pack(e, members=2):
    """A RES_PACK resource with RES_MEMBER children, data owned by the file system."""
    rec = e.call(0x4B19C0, cstr(e, b"pack"))[0]
    data = e.call(0x4B0C60, 64, 4)[0]
    u32(e, rec + 0x18, data)
    u32(e, rec + 0xC, 0)
    p = resource(e, b"pack", data, 0, 0)
    for i in range(members):
        m = resource(e, b"m%d" % i, 0x1000 + i, 4, 1)
        e.call(0x4AF8F0, p + 0x14, m + 8)
    return p


@pytest.mark.parametrize("type", [0, 1, 2, 3, 4, 5])
@pytest.mark.parametrize("which", [0, 2])
def test_resource_free(type, which):
    def setup(e):
        init(e)
        rs = [resource(e, b"r%d" % i) for i in range(3)]
        p = pack(e)
        if type == 0:
            rs[which] = p
        elif type == 2:
            rec = e.call(0x4B19C0, cstr(e, b"file"))[0]
            data = e.call(0x4B0C60, 16, 4)[0]
            u32(e, rec + 0x18, data)
            rs[which] = resource(e, b"file", data, 16, 2)
        elif type in (1, 4):
            rs[which] = resource(e, b"member", 0x2000, 4, type)
            e.call(0x4AF8F0, p + 0x14, rs[which] + 8)
        else:
            u32(e, rs[which] + 0x24, type)
        return (rs[which],)
    compare(emu, 0x4B5C90, "Resource_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("name", [b"list", b"X:dir/*.txt", b"X:single"])
def test_resource_for_each(name):
    def setup(e):
        init(e)
        fn = fake(e, "foreach_cb", log_cb)
        for n, size in ((b"one", 11), (b"three", 33), (b"X:single", 5)):
            resource(e, n, 0, size)
        paths = cstr(e, b"one;two;three;;one")
        resource(e, b"list", paths, 0, 4)
        return cstr(e, name), fn, 0x55AA
    compare(emu, 0x4B5D80, "Resource_ForEach", setup=setup, check_eax=False)
    logs_equal()


def log_cb(e):
    info = e.arg(0)
    LOG[-1].append((e.read_cstr(e.u32(info)), e.u32(info + 4), e.read(info + 8, 1), e.arg(1)))
    e.ret(0)


def test_resource_init():
    compare(emu, 0x4B5E70, "Resource_Init", setup=lambda e: e.call(0x4B0BA0) and None, check_eax=False)


def test_resource_free_all_and_shutdown():
    def setup(e):
        init(e)
        for i in range(4):
            resource(e, b"r%d" % i)
        pack(e)
        resource(e, b"user")
    compare(emu, 0x4B5EE0, "Resource_FreeAll", setup=setup, check_eax=False)
    compare(emu, 0x4B5EC0, "Resource_Shutdown", setup=setup, check_eax=False)


def test_resource_add_chunks():
    def setup(e):
        init(e)
        data = blob(e, GOOD)
        p = resource(e, b"X:pack.res", data, 0, 0)
        t = e.alloc(8)
        e.write(t, struct.pack("<II", 3, 4))
        return cstr(e, b"X:pack.res"), p, data, t
    compare(emu, 0x4B60F0, "Resource_AddChunks", setup=setup)


@pytest.mark.parametrize("data", [GOOD, respack([(b"a.txt", b"hello"), (b"Dir\\B.BIN", b"x" * 40)]), respack([]),
                                  b"plain text file", b"RESOURC", b""])
def test_resource_add(data):
    def setup(e):
        init(e)
        return cstr(e, b"X:file.res"), blob(e, data), len(data)
    compare(emu, 0x4B5F80, "Resource_Add", setup=setup)


@pytest.mark.parametrize("name", [b"loaded", b"X:new.txt", b"X:new.res"])
@pytest.mark.parametrize("want", [True, False])
def test_resource_load(name, want):
    FILES[b"new.txt"] = b"some text"
    FILES[b"new.res"] = GOOD

    def setup(e):
        init(e)
        resource(e, b"loaded", 0x1234, 99)
        return cstr(e, name), e.alloc(4) if want else 0
    compare(emu, 0x4B5F00, "Resource_Load", setup=setup)
    logs_equal()


@pytest.mark.parametrize("name", [b"user", b"file", b"pack", b"member"])
def test_resource_unload(name):
    def setup(e):
        init(e)
        resource(e, b"user")
        rec = e.call(0x4B19C0, cstr(e, b"file"))[0]
        data = e.call(0x4B0C60, 16, 4)[0]
        u32(e, rec + 0x18, data)
        resource(e, b"file", data, 16, 2)
        pack(e)
        p = resource(e, b"member", 0x2000, 4, 1)
        e.call(0x4AF8F0, e.u32(0x943E40) + 0x14, p + 8)
        return (cstr(e, name),)
    compare(emu, 0x4B6380, "Resource_Unload", setup=setup, check_eax=False)


@pytest.mark.parametrize("which", [0, 1, 2])
def test_resource_flush(which):
    def setup(e):
        init(e)
        rec = e.call(0x4B19C0, cstr(e, b"file"))[0]
        data = e.call(0x4B0C60, 16, 4)[0]
        u32(e, rec + 0x18, data)
        resource(e, b"file", data, 16, 2)
        resource(e, b"user", 0x3000, 4, 3)
        p = pack(e)
        return ((data, 0x3000, e.u32(p + 0x34))[which],)
    compare(emu, 0x4B63D0, "Resource_Flush", setup=setup, check_eax=False)


def test_resource_register():
    def setup(e):
        init(e)
        return cstr(e, b"Mem\\Block"), 64, 0x4444
    compare(emu, 0x4B6420, "Resource_Register", setup=setup)


@pytest.mark.parametrize("name", [b"user", b"X:other"])
def test_resource_get_time(name):
    def setup(e):
        init(e)
        t = e.alloc(8)
        e.write(t, struct.pack("<II", 5, 6))
        resource(e, b"user", 0, 0, 3, t)
        return e.alloc(8), cstr(e, name)
    compare(emu, 0x4B6440, "Resource_GetTime", setup=setup)


@pytest.mark.parametrize("name", [b"user", b"pack", b"missing"])
def test_resource_release(name):
    def setup(e):
        init(e)
        resource(e, b"user")
        pack(e)
        return (cstr(e, name),)
    compare(emu, 0x4B64B0, "Resource_Release", setup=setup, check_eax=False)


@pytest.mark.parametrize("name", [b"user", b"missing"])
def test_resource_find(name):
    def setup(e):
        init(e)
        resource(e, b"user")
        return (cstr(e, name),)
    compare(emu, 0x4B64E0, "Resource_Find", setup=setup)


def test_resource_compare_time():
    def setup(e):
        e.stub("CompareFileTime", lambda e: e.ret(e.u32(e.arg(0)) - e.u32(e.arg(1)), 8))
        a, b = e.alloc(8), e.alloc(8)
        e.write(a, struct.pack("<II", 5, 0))
        e.write(b, struct.pack("<II", 3, 0))
        return a, b
    compare(emu, 0x4B6490, "Resource_CompareTime", setup=setup)


def test_resource_errors():
    def load(e):
        init(e)
        return cstr(e, b"X:missing"), 0
    assert "Failed" in errors_equal(emu, 0x4B5F00, "Resource_Load", load)

    def unload(e):
        init(e)
        return (cstr(e, b"nothere"),)
    errors_equal(emu, 0x4B6380, "Resource_Unload", unload)

    def flush(e):
        init(e)
        resource(e, b"user", 0x100)
        return (0x200,)
    errors_equal(emu, 0x4B63D0, "Resource_Flush", flush)


@pytest.mark.parametrize("kids", [
    [(b"NAME", b"x\0")],
    [(b"NAME", b"x\0"), (b"JUNK", b"1234")],
    [(b"JUNK", b"1234")],
    [(b"ILFF", b"JUNK", [(b"CSTR", b"a\0")])],
    [(b"NAME", b"sub\0"), (b"ILFF", b"IRES", [(b"NAME", b"y\0"), (b"JUNK", b"1")])],
])
def test_resource_add_chunks_errors(kids):
    def setup(e):
        init(e)
        data = blob(e, ilff(b"IRES", kids))
        p = resource(e, b"X:bad.res", data, 0, 0)
        return cstr(e, b"X:bad.res"), p, data, 0
    assert "Error in resource" in errors_equal(emu, 0x4B60F0, "Resource_AddChunks", setup)
