import os
import struct

import pytest

from emu import Emu, ErrorShown, TRAP_BASE, compare

GAME = os.path.dirname(os.environ.get("IGI_EXE", ""))

emu = Emu()
emu.fake(0x509910)
emu.fake(0x509920)

RENDER_FUNCS = 0xA94E84
DEVICE_FUNCS = 0xA950A4
RENDER_DEVICE = 0xA94A64
FROM_FILE = 0x943B3C
IDS = {"SetLOD": 0x546F90, "GetTextureSize": 0x546F94, "ReloadTexture": 0x546F98,
       "RefreshTexture": 0x546F9C, "BeginTextureList": 0x546FA0, "EndTextureList": 0x546FA4,
       "AllocTexture": 0x546FA8, "DeAllocTexture": 0x546FAC, "DownloadTexture": 0x546FB0,
       "GetTMUs": 0x546FB4, "SetActiveTMU": 0x546FB8, "GetActiveTMU": 0x546FBC}


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


def render_cb(name):
    """A render device function: logs the 12 word texture description it is given."""
    def fn(e):
        p = e.arg(0)
        words = struct.unpack("<12I", e.read(p, 48)) if p else None
        if words and name == "DownloadTexture_nomemset":
            words = words[:4] + words[6:]
        e.log.append((name, words))
        if p and name in ("GetActiveTMU", "GetTMUs"):
            e.write(p, u32(e.fake.get(name, 3)))
        e.ret(0)
    return py_func(e_global, "render_" + name, fn)


e_global = emu
CB = {name: render_cb(name) for name in IDS}
CB["DownloadTexture_nomemset"] = render_cb("DownloadTexture_nomemset")


def resource_load(e):
    name = e.read_cstr(e.arg(0)).decode()
    e.log.append(("Resource_Load", name, e.arg(1)))
    with open(name, "rb") as f:
        data = f.read()
    p = e.alloc(len(data))
    e.write(p, data)
    e.ret(p)


def resource_unload(e):
    e.log.append(("Resource_Unload", e.read_cstr(e.arg(0))))
    e.ret(0)


for addr, name, fn in [(0x4B5F00, "Resource_Load", resource_load), (0x4B6380, "Resource_Unload", resource_unload)]:
    trap = py_func(emu, name, fn)
    emu.write(addr, b"\xe9" + struct.pack("<i", trap - addr - 5))
emu.baseline = emu.snapshot()


def render_setup(e, present=True, download=CB["DownloadTexture"]):
    """Give each texture function the id of its name and a device function."""
    e.write(RENDER_DEVICE, u32(2))
    for i, (name, addr) in enumerate(sorted(IDS.items())):
        e.write(addr, u32(10 + i))
        fn = CB[name] if present else 0
        if name == "DownloadTexture":
            fn = download
        e.write(RENDER_FUNCS + 4 * (10 + i), u32(fn))
        e.write(DEVICE_FUNCS + 4 * (10 + i + 2 * 0x82), u32(fn))


def check(orig, ours, setup=None, args=(), **kw):
    logs = []

    def s(e):
        e.fake = {}
        e.log = []
        r = setup(e) if setup else None
        e.log = []
        logs.append(e.log)
        return r
    compare(emu, orig, ours, args=args, setup=s, normalized=True, **kw)
    assert logs[0] == logs[1], "calls differ:\n%s\n%s" % (logs[0], logs[1])
    return logs[0]


def test_init():
    check(0x4B2180, "Texture_Init", check_eax=False)


def test_shutdown():
    def setup(e):
        e.call(0x4B2180)
    check(0x4B2250, "Texture_Shutdown", setup=setup, check_eax=False)


def test_free():
    def setup(e):
        render_setup(e)
        tex = e.alloc(36)
        e.write(tex, u32(2) + b"\x55" * 32)
        return (tex,)
    assert check(0x4B22F0, "Texture_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("from_file", [0, 1, 0xFF])
def test_alloc_create(from_file):
    def setup(e):
        render_setup(e)
        e.write(FROM_FILE, bytes([from_file]))
        return (e.alloc(36), 1, 2, 3, 4, 5, 6, 7, 8)
    assert len(check(0x4B2320, "Texture_Alloc", setup=setup, check_eax=False)) == 1
    assert len(check(0x4B23B0, "Texture_Create", setup=setup, check_eax=False)) == 2


def test_download():
    def setup(e):
        render_setup(e, download=CB["DownloadTexture_nomemset"])
        e.write(FROM_FILE, b"\x01")
        return (e.alloc(36), 1, 2, 3, 4, 5, 6)
    assert check(0x4B2450, "Texture_Download", setup=setup, check_eax=False)


TEX_V2 = GAME + "/common/textures/flare07.tex"
TEX_V11 = GAME + "/missions/location0/common/textures/sky_layer2.tex"


@pytest.mark.parametrize("path", [TEX_V2, TEX_V11])
def test_load_file(path):
    def setup(e):
        render_setup(e)
        return e.alloc(36), 9, cstr(e, path.encode())
    log = check(0x4B24C0, "Texture_LoadFile", setup=setup, check_eax=False)
    assert [x[0] for x in log] == ["Resource_Load", "AllocTexture", "DownloadTexture", "Resource_Unload"]


@pytest.mark.parametrize("path", [GAME + "/config.qvm", GAME + "/computer/computer.res"])
def test_load_file_invalid(path):
    msgs = []
    for target in (0x4B24C0, "Texture_LoadFile"):
        emu.reset()
        emu.log, emu.fake = [], {}
        render_setup(emu)
        args = (emu.alloc(36), 9, cstr(emu, path.encode()))
        with pytest.raises(ErrorShown) as ex:
            emu.call(target, *args)
        msgs.append((str(ex.value), emu.log))
    assert msgs[0] == msgs[1]
    assert msgs[0][0] == "'%s' is not a valid Texture file."


def chunk(cid, data=b"", align=4):
    return [cid, data, align]


def ilff(chunks):
    """Build a resource file: 'ILFF' header, form type, then 16 byte chunk headers."""
    out = bytearray(b"ILFF" + u32(0) + u32(4) + u32(0) + b"IRES")
    for i, (cid, data, al) in enumerate(chunks):
        start = len(out)
        size = (len(data) + 3) & ~3
        nxt = 16 + size if i < len(chunks) - 1 else 0
        out += cid + u32(len(data)) + u32(al) + u32(nxt) + data + b"\0" * (size - len(data))
        assert len(out) - start == 16 + size
    out[4:8] = u32(len(out))
    return bytes(out)


TEXH = struct.pack("<3I6H", 3, 7, 0, 5, 256, 128, 256, 128, 32)
CASES = {
    "two": ilff([chunk(b"NAME", b"x\0"), chunk(b"TEXH", TEXH), chunk(b"BODY", b"\1" * 8),
                 chunk(b"PAD ", b"z"), chunk(b"TEXH", TEXH), chunk(b"BODY", b"\2" * 8)]),
    "no_body": ilff([chunk(b"TEXH", TEXH), chunk(b"NAME", b"x")]),
    "empty_body": ilff([chunk(b"TEXH", TEXH), chunk(b"BODY")]),
    "empty_header": ilff([chunk(b"TEXH"), chunk(b"BODY", b"1234")]),
    "last": ilff([chunk(b"NAME", b"x"), chunk(b"TEXH", TEXH)]),
    "none": ilff([chunk(b"NAME", b"x")]),
}


def res_file(e, data):
    p = e.alloc(len(data))
    e.write(p, data)
    return p


@pytest.mark.parametrize("case", sorted(CASES))
def test_texres_count(case):
    check(0x4B1FF0, "TexRes_Count", setup=lambda e: (res_file(e, CASES[case]),))


def font_res():
    """The first resource in computer.res is a font: an ILFF file with a TEXH and BODY chunk."""
    with open(GAME + "/computer/computer.res", "rb") as f:
        data = f.read()
    size = struct.unpack("<I", data[0x44:0x48])[0]
    return data[0x50:0x50 + size]


def test_texres_count_real():
    data = font_res()
    check(0x4B1FF0, "TexRes_Count", setup=lambda e: (res_file(e, data),))


@pytest.mark.parametrize("case", sorted(CASES))
@pytest.mark.parametrize("max", [0, 1, 5])
def test_texres_load(case, max):
    def setup(e):
        render_setup(e)
        return res_file(e, CASES[case]), e.alloc(36 * 5), max, 4
    check(0x4B2070, "TexRes_Load", setup=setup)


def test_texres_load_real():
    data = font_res()

    def setup(e):
        render_setup(e)
        return res_file(e, data), e.alloc(36 * 8), 8, 0
    assert check(0x4B2070, "TexRes_Load", setup=setup)


@pytest.mark.parametrize("present", [True, False])
def test_list_and_tmu(present):
    def setup(e):
        render_setup(e, present)
    check(0x4B25B0, "Texture_BeginList", setup=setup, args=(5, 6), check_eax=False)
    check(0x4B25F0, "Texture_EndList", setup=setup, check_eax=False)
    check(0x4B2610, "Texture_SetActiveTMU", setup=setup, args=(2,), check_eax=False)
    check(0x4B2640, "Texture_GetActiveTMU", setup=setup)
    check(0x4B2670, "Texture_GetTMUs", setup=setup)
    check(0x4B26C0, "Texture_ReloadAll", setup=setup, check_eax=False)


@pytest.mark.parametrize("addr,name", [(0x4B26E0, "Texture_GetRefreshId"), (0x4B26F0, "Texture_GetReloadId"),
                                       (0x4B2700, "Texture_GetSizeId"), (0x4B2710, "Texture_GetSetLODId")])
def test_ids(addr, name):
    check(addr, name, setup=render_setup)


def test_from_file_flag():
    check(0x4B26A0, "Texture_BeginFileLoad", check_eax=False)
    check(0x4B26B0, "Texture_EndFileLoad", setup=lambda e: e.write(FROM_FILE, b"\x01"), check_eax=False)


def test_params():
    def setup(e):
        p = e.alloc(28)
        e.write(p, bytes(range(1, 29)))
        return (p,)
    check(0x4B2720, "sub_4B2720", setup=setup, check_eax=False)
    check(0x4B2740, "sub_4B2740", setup=lambda e: (e.alloc(28),), check_eax=False)
