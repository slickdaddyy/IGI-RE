import struct

import pytest

from emu import compare
from test_core_error import CoreEmu, cstr, escape

emu = CoreEmu()

DEVICES = 0x9436B0
ALIASES = 0x942330


def file_init(e, aliases=()):
    e.call(0x4B0BA0)
    e.call(0x4B1200)
    for name, path in aliases:
        e.call(0x4B12F0, cstr(e, name), cstr(e, path))


def scan_logger(e):
    """scanDir replacement that appends the device number and path to a log."""
    log = e.alloc(0x400)
    ptr = e.alloc(4)
    e.write(ptr, struct.pack("<I", log))
    code = e.alloc(64)
    P = lambda v: struct.pack("<I", v)
    e.write(code, bytes.fromhex("56 57 8b3d") + P(ptr) + bytes.fromhex("8b44240c ab 8b742410 ac aa 84c0 75fa 893d")
            + P(ptr) + bytes.fromhex("5f 5e c3"))
    return code


ALIAS_SET = [(b"MISSION:", b"LOCAL:missions/location0/level1/"),
             (b"LANGUAGE:", b"LOCAL:language/english/;LOCAL:language/default/;LOCAL:x/")]


@pytest.mark.parametrize("path,index", [
    (b"MISSION:objects.qsc", 0),
    (b"LANGUAGE:text.res", 0),
    (b"LANGUAGE:text.res", 1),
    (b"LANGUAGE:text.res", 2),
    (b"LOCAL:weapons", 0),
    (b"nothing", 0),
])
def test_resolve_path(path, index):
    def setup(e):
        file_init(e, ALIAS_SET)
        return e.alloc(256), cstr(e, path), index
    compare(emu, 0x4B1020, "File_ResolvePath", setup=setup)


def test_resolve_path_bad_index():
    def setup(e):
        file_init(e, ALIAS_SET)
        escape(e, 3)
        return e.alloc(256), cstr(e, b"MISSION:a"), 2
    compare(emu, 0x4B1020, "File_ResolvePath", setup=setup, check_eax=False)


def test_device_new():
    compare(emu, 0x4B1180, "Device_New", setup=lambda e: file_init(e))


@pytest.mark.parametrize("path", [b"LOCAL:a", b"LOCA", b"OTHER:a", b"EXTRA:b"])
def test_device_find(path):
    def setup(e):
        file_init(e)
        dev = e.call(0x4B1180)[0]
        e.call(0x4B1290, dev, cstr(e, b"EXTRA:"))
        e.write(DEVICES + dev * 0x8C + 0x58, struct.pack("<I", 1))
        return (cstr(e, path),)
    compare(emu, 0x4B11A0, "Device_Find", setup=setup)


def test_file_init():
    def setup(e):
        e.call(0x4B0BA0)
        e.write(ALIASES, b"\x55" * 0x1700)
        e.write(0x943A18, b"\x55" * 8)
    compare(emu, 0x4B1200, "File_Init", setup=setup, check_eax=False)


@pytest.mark.parametrize("files", [0, 1, 3])
def test_file_shutdown(files):
    def setup(e):
        file_init(e)
        for _ in range(files):
            node = e.call(0x4B0C60, 0x40, 4)[0]
            e.call(0x4AF8F0, 0x943970, node)
    compare(emu, 0x4B1260, "File_Shutdown", setup=setup, check_eax=False)


def test_device_set_name():
    compare(emu, 0x4B1290, "Device_SetName", setup=lambda e: (2, cstr(e, b"CDROM:")), check_eax=False)


def test_alias_clear():
    compare(emu, 0x4B12D0, "Alias_Clear", setup=lambda e: e.write(ALIASES, b"\x55" * 0x1280), check_eax=False)


@pytest.mark.parametrize("name,path,full", [
    (b"MISSION:", b"LOCAL:missions/b/", False),
    (b"NEW:", b"LOCAL:new/", False),
    (b"NEW:", b"NODEV:new/", False),
    (b"NEW:", b"LOCAL:new/", True),
])
def test_alias_set(name, path, full):
    def setup(e):
        file_init(e, ALIAS_SET)
        if full:
            for i in range(32):
                e.write(ALIASES + i * 0x94 + 0x90, struct.pack("<I", 1))
        return cstr(e, name), cstr(e, path)
    compare(emu, 0x4B12F0, "Alias_Set", setup=setup, check_eax=False)


@pytest.mark.parametrize("name", [b"MISSION:", b"LANGUAGE:", b"MISSION", b"NONE:"])
def test_alias_remove(name):
    def setup(e):
        file_init(e, ALIAS_SET)
        return (cstr(e, name),)
    compare(emu, 0x4B13A0, "Alias_Remove", setup=setup, check_eax=False)


@pytest.mark.parametrize("path", [b"LOCAL:weapons", b"LANGUAGE:fonts", b"MISSION:"])
def test_scan_dir(path):
    def setup(e):
        file_init(e, ALIAS_SET)
        e.write(DEVICES + 0x8C + 0x3C, struct.pack("<I", scan_logger(e)))
        return cstr(e, path), 0x401000, 0x1234
    compare(emu, 0x4B1420, "File_ScanDir", setup=setup, check_eax=False)


@pytest.mark.parametrize("case", ["no_device", "not_present"])
def test_scan_dir_errors(case):
    def setup(e):
        file_init(e, ALIAS_SET)
        escape(e, 3)
        if case == "not_present":
            return cstr(e, b"CDROM:x"), 0, 0
        return cstr(e, b"C:/x"), 0, 0
    compare(emu, 0x4B1420, "File_ScanDir", setup=setup, check_eax=False)
