"""Texture set files (0x5175A0 - 0x51783D)."""
import struct

import pytest

from test_d3d_util import D3DEmu, check, check_raises, cstr, pyfake

e = D3DEmu()

RES = {"ptr": 0}


def resource(em):
    return RES["ptr"]


pyfake(e, 0x4B5F00, "Resource_Load", 2, before=resource)
pyfake(e, 0x4B6380, "Resource_Unload", 1, before=lambda em: (None, em.read_cstr(em.arg(0))))
pyfake(e, 0x4B23B0, "Texture_Create", 9)
pyfake(e, 0x4B7CF0, "Palette_Create", 3, before=lambda em: (None, em.read(em.arg(1), 16)))
pyfake(e, 0x4B22F0, "Texture_Free", 1)
pyfake(e, 0x4B7D30, "Palette_Free", 1)


def loop_file(version, count, palette=True, magic=b"LOOP", flags=-3):
    head = bytearray(0x34)
    head[0:4] = magic
    struct.pack_into("<i", head, 4, version)
    struct.pack_into("<h", head, 0x12, flags)
    struct.pack_into("<I", head, 0x20, count)
    size = 0x28 if version == 7 else 0x20
    entries = bytearray(size * count)
    data_at = 0x34 + len(entries)
    for i in range(count):
        if version == 7:
            struct.pack_into("<5I", entries, i * size, data_at + 0x40 * i, 64 + i, 32 << (i & 1), 16 + i, 3 + i)
        else:
            struct.pack_into("<2I3H", entries, i * size, data_at + 0x40 * i, 5 + i, 0x8000 + i, 64 + i, 0xFFFF - i)
    body = bytes(head) + bytes(entries) + bytes(range(256)) * count
    if palette:
        struct.pack_into("<I", head, 0x18, len(body))
        body = bytes(head) + bytes(entries) + bytes(range(256)) * count
        body += b"\0" * 0xC + struct.pack("<I", 0x100 + count) + bytes(range(16)) * 4
    return body


def setup_file(data):
    def setup(em):
        RES["ptr"] = 0
        if data is not None:
            RES["ptr"] = em.alloc(len(data) + 16)
            em.write(RES["ptr"], data)
        return (cstr(em, b"textures\\terrain.tex"),)
    return setup


@pytest.mark.parametrize("version", [7, 9])
@pytest.mark.parametrize("count", [0, 1, 3])
@pytest.mark.parametrize("palette", [False, True])
def test_load(version, count, palette):
    log = check(e, 0x5175A0, "TexSet_Load", setup=setup_file(loop_file(version, count, palette)))
    assert len([x for x in log if x[0] == "Texture_Create"]) == count


@pytest.mark.parametrize("data", [None, loop_file(8, 1), loop_file(7, 1, magic=b"LOOQ"), loop_file(9, 1, magic=b"XOOP"),
                                  loop_file(9, 1, magic=b"LOPP"), loop_file(7, 1, magic=b"LOOp")])
def test_load_bad(data):
    msg = check_raises(e, 0x5175A0, "TexSet_Load", setup=setup_file(data))
    assert ("Couldn't load" if data is None else "not of required type") in msg


@pytest.mark.parametrize("version", [7, 9])
@pytest.mark.parametrize("count", [0, 1, 3])
@pytest.mark.parametrize("palette", [False, True])
def test_free(version, count, palette):
    def setup(em):
        name = setup_file(loop_file(version, count, palette))(em)
        return (em.call(0x5175A0, *name)[0],)
    log = check(e, 0x5177F0, "TexSet_Free", setup=setup, check_eax=False)
    assert len([x for x in log if x[0] in ("Texture_Free", "Palette_Free")]) == count + palette
