"""Object lightmap helpers and LightmapInfo (0x521040 - 0x521110, 0x523110 - 0x523900)."""
import random

import pytest

from test_litmap_util import (CWS, LOG, RET, cstr, f32, handler, logfake, new_emu, pyfake, rnd_task, run,
                              set_handler, u16, u32)

TYPE = 0x52
OWNER = 0x53
FREE_EVENT = 0x2A
POOL = 0xA83A00
SCAN_NAME, FOUND = 0xA83B24, 0xA83978

e = new_emu()
logfake(e, 0x4B0D10, 1)  # Mem_Free
logfake(e, 0x4B0C60, 2, effect=lambda e, a: e.alloc(a[0]))  # Mem_Alloc
logfake(e, 0x4B0D60, 1)  # Mem_PushMode
logfake(e, 0x4B0D90, 0)  # Mem_PopMode
logfake(e, 0x4B5B60, 1, strings=(0,))  # Resource_Exists
logfake(e, 0x4B5F00, 2, strings=(0,))  # Resource_Load
logfake(e, 0x4B64E0, 1, strings=(0,))  # Resource_Find
logfake(e, 0x4B6380, 1, strings=(0,))  # Resource_Unload
logfake(e, 0x4B63D0, 1)  # Resource_Flush
logfake(e, 0x4C6890, 3)  # DynCube_ListAdd
logfake(e, 0x4C6840, 3)  # DynCube_ListRemove
logfake(e, 0x523900, 1)
FILES = []


def scan_dir(e):
    """File_ScanDir: logs the folder and marks a file whose name contains the scan name."""
    name = e.read_cstr(e.u32(SCAN_NAME))
    LOG[-1].append(("scan", e.read_cstr(e.arg(0)), e.normalize(e.arg(1)), e.arg(2), name))
    if any(name in f for f in FILES):
        u32(e, FOUND, 1)
    e.ret(0)


pyfake(e, 0x4B1420, scan_dir)
e.write(0x548366, bytes([FREE_EVENT]))
set_handler(e, FREE_EVENT, OWNER, handler(e, "free", 2, derefs={1: 4}))
e.baseline = e.snapshot()
IGNORE = ((SCAN_NAME, SCAN_NAME + 4),)


@pytest.mark.parametrize("mask", [0, 1, 6, 15])
def test_free_buffers(mask):
    def setup(e):
        for i, a in enumerate((0xA839D0, 0xA839F0, 0xA83C54, 0xA839CC)):
            u32(e, a, 0x1000 + i if mask >> i & 1 else 0)
        u32(e, 0xA83D24, 5, 6)
    run(e, 0x521040, "Lightmap_FreeBuffers", setup)


def test_get_dir():
    run(e, 0x5210C0, "Lightmap_GetDir", None, check_eax=True)


def test_get_res_path():
    run(e, 0x5210E0, "Lightmap_GetResPath", None, check_eax=True)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("size", [1, 2, 7, 8, 16, 33])
@pytest.mark.parametrize("maxes", [(0.0, 0.0), (1e9, 1e9), (0.01, 0.002)])
def test_hemicube_new(size, maxes, cw):
    def setup(e):
        f32(e, 0xA83B40, maxes[0])
        f32(e, 0xA83B2C, maxes[1])
        return (size,)
    run(e, 0x523110, "Hemicube_New", setup, cw=cw, check_eax=True)


def test_hemicube_free():
    def setup(e):
        h = e.alloc(0xEC)
        u32(e, h, 8, 4, *range(0x100, 0x10C))
        return (h,)
    run(e, 0x5233A0, "Hemicube_Free", setup)


@pytest.mark.parametrize("name,is_dir", [(b"obj00003_00001.olm", 0), (b"obj00003_00001.olm", 1), (b"obj00004.olm", 0)])
def test_scan_file(name, is_dir):
    def setup(e):
        u32(e, SCAN_NAME, cstr(e, b"obj00003"))
        u32(e, FOUND, 0)
        ent = e.alloc(16)
        u32(e, ent, cstr(e, name), 0x77)
        e.write(ent + 8, bytes([is_dir]))
        return (ent,)
    run(e, 0x523420, "LightmapInfo_ScanFile", setup)


def pool(e, names, owner_type=OWNER, rng=None):
    """The LightmapInfo pool with infos named names, slots used in a shuffled order."""
    rng = rng or random.Random(1)
    count, stride, size = 8, 12, 8
    data = e.alloc(count * stride)
    order = list(range(count))
    rng.shuffle(order)
    owner = rnd_task(e, 0x40, owner_type, rng)
    infos = []
    for i, n in enumerate(names):
        li = rnd_task(e, 0xE0, TYPE, rng)
        e.write(li + 0x78, n + b"\0")
        u32(e, li + 0x14, owner)
        u32(e, li + 0x88, 0)
        u32(e, li + 0xC8, 0)
        item = data + order[i] * stride
        u32(e, item, li)
        u32(e, item + size, i)
        u32(e, li + 0x74, item)
        infos.append(li)
    o = e.alloc(count * 4)
    u32(e, o, *order)
    u32(e, POOL, data, o, len(names), count, stride, size)
    return infos, owner


CASES = [([], []), ([b"obj00000"], []), ([], [b"obj00000_00001.olm"]),
         ([b"obj00000", b"obj00002"], [b"obj00001_00000.olm", b"obj00003.olm"])]


@pytest.mark.parametrize("names,files", CASES)
def test_make_name(names, files):
    def setup(e):
        FILES[:] = files
        pool(e, names)
        li = rnd_task(e, 0xE0, TYPE, random.Random(2))
        return (li,)
    run(e, 0x5235A0, "LightmapInfo_MakeName", setup, ignore=IGNORE)


@pytest.mark.parametrize("names,files", CASES)
@pytest.mark.parametrize("exists", [0, 1])
def test_create(names, files, exists):
    def setup(e):
        FILES[:] = files
        RET[0x4B5B60] = exists
        u32(e, 0xA44344, 0x4444)
        pool(e, names)
        li = rnd_task(e, 0xE0, TYPE, random.Random(3))
        u32(e, li + 0x14, 0x1414)
        return (li,)
    run(e, 0x523450, "LightmapInfo_Create", setup, ignore=IGNORE)


def vertex_lights(e, n, rng):
    d = e.alloc(4 + 12 * n)
    u32(e, d, n)
    for i in range(n):
        u32(e, d + 4 + 12 * i, rng.choice([0, 0x100 + i]), 0x200 + i, 0x300 + i)
    return d


@pytest.mark.parametrize("n", [0, 1, 5])
def test_vertex_lights_free(n):
    run(e, 0x523670, "VertexLights_Free", lambda e: (vertex_lights(e, n, random.Random(n)),))


def lightmaps(e, count, recs, res):
    o = e.alloc(0x3C + 0x2C * count)
    u32(e, o + 0x24, res)
    u32(e, o + 0x2C, count)
    if recs:
        r = e.alloc(0x28 * count)
        u32(e, r, *range(0x1000, 0x1000 + 10 * count))
        u32(e, o + 0x38, r)
    for i in range(count):
        u32(e, o + 0x3C + 0x2C * i + 0x24, 0x2000 + i)
    return o


@pytest.mark.parametrize("count", [0, 1, 3])
@pytest.mark.parametrize("recs", [0, 1])
@pytest.mark.parametrize("res", [0, 1])
def test_lightmaps_free(count, recs, res):
    run(e, 0x5237E0, "Lightmaps_Free", lambda e: (lightmaps(e, count, recs, res),))


# which info of three is deleted, its model lightmaps (None or mesh flags), its data, last one
@pytest.mark.parametrize("which", [0, 1, 2])
@pytest.mark.parametrize("meshes", [None, [], [1, 0, 1]])
@pytest.mark.parametrize("data", [0, 2])
@pytest.mark.parametrize("found", [0, 1])
def test_delete(which, meshes, data, found):
    def setup(e):
        RET[0x4B64E0] = found
        u32(e, 0xA44344, 0x4444)
        rng = random.Random(which)
        infos, owner = pool(e, [b"obj00000", b"obj00001", b"obj00002"][:3 if which else 1], rng=rng)
        li = infos[which if which < len(infos) else 0]
        if meshes is not None:
            m = e.alloc(4 + 0x24 * len(meshes))
            u32(e, m, len(meshes))
            for i, f in enumerate(meshes):
                u32(e, m + 4 + 0x24 * i + 0x14, lightmaps(e, 2, 1, i & 1) if f else 0)
            u32(e, li + 0x88, m)
        if data:
            u32(e, li + 0xC8, vertex_lights(e, data, rng))
        return (li,)
    run(e, 0x5236C0, "LightmapInfo_Delete", setup)
