import random
import struct

import pytest
from unicorn import UC_HOOK_CODE

from emu import Emu, compare

emu = Emu()

LOG = []
STATE = {}
CALL_LOG = None

INTS, REALS = emu.alloc(0x400), emu.alloc(0x800)  # filled per test, see script_args


def pyfake(e, addr, fn):
    """Run fn(e) instead of the code at addr; fn returns eax."""
    def hook(uc, a, size, user):
        e.ret(fn(e) or 0)
    e.uc.hook_add(UC_HOOK_CODE, hook, begin=addr, end=addr)


def name(e, i):
    return e.read_cstr(e.arg(i)).decode("latin1")


def resource_load(e):
    n = name(e, 0)
    data = STATE["res"].get(n)
    LOG.append(("load", n, e.arg(1) != 0))
    if data is None:
        return 0
    if e.arg(1):
        e.write(e.arg(1), struct.pack("<I", len(data[1])))
    return data[0]


def resource_exists(e):
    n = name(e, 0)
    LOG.append(("exists", n))
    return n in STATE["res"]


def mem_alloc(e):
    LOG.append(("alloc", e.arg(0), e.arg(1)))
    return e.alloc(e.arg(0))


def log(tag, *args):
    def fn(e):
        LOG.append((tag,) + tuple(e.arg(i) for i in args))
        return STATE.get(tag, 0)
    return fn


pyfake(emu, 0x4B5F00, resource_load)
pyfake(emu, 0x4B5B60, resource_exists)
pyfake(emu, 0x4B6380, lambda e: LOG.append(("unload", name(e, 0))))
pyfake(emu, 0x4B0C60, mem_alloc)
pyfake(emu, 0x4D11C0, log("4d11c0"))
pyfake(emu, 0x4B2670, log("tmus"))
pyfake(emu, 0x4B2610, log("tmu", 0))
pyfake(emu, 0x4B2450, log("download", 0, 1, 2, 3, 4, 5, 6))
pyfake(emu, 0x4B8890, lambda e: LOG.append(("register", name(e, 0), e.normalize(e.arg(1)), e.arg(2), e.arg(3))))
pyfake(emu, 0x4B8920, lambda e: LOG.append(("unregister", name(e, 0))))
pyfake(emu, 0x4B80B0, lambda e: LOG.append(("script_load", name(e, 0))) or 0x5150)
pyfake(emu, 0x4B85B0, log("script_run", 0))
pyfake(emu, 0x4B83D0, log("script_free", 0))
pyfake(emu, 0x4B8A20, lambda e: LOG.append(("int", e.arg(0), e.arg(1))) or e.u32(INTS + 4 * e.arg(1)))


def run(orig, ours, setup, **kw):
    """compare() plus the log of faked calls."""
    logs = []

    def s(e):
        global LOG
        LOG = []
        logs.append(LOG)
        STATE.clear()
        return setup(e)
    kw.setdefault("check_eax", False)
    compare(emu, orig, ours, setup=s, **kw)
    assert logs[0] == logs[1], "calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return logs[0]


def blob(e, data):
    p = e.alloc(len(data))
    e.write(p, data)
    return p, data


def terrain(e, name=b"maps/lm.lmp"):
    t = e.alloc(0xF00)
    e.write(t + 0xDF4, struct.pack("<I", 0xBEEF00))
    e.write(t + 0xE00, name + b"\0")
    return t


def lmp(rng, sizes):
    out = b""
    for n in sizes:
        out += struct.pack("<i", n) + bytes(rng.randrange(256) for _ in range(n * n))
    return out


@pytest.mark.parametrize("sizes", [[], [4], [0, 8, 0, 2], [16, 1, 0], [0]])
@pytest.mark.parametrize("tmus", [1, 2])
def test_load_lightmaps(sizes, tmus):
    def setup(e):
        t = terrain(e)
        STATE["res"] = {"maps/lm.lmp": blob(e, lmp(random.Random(len(sizes)), sizes))}
        STATE["tmus"] = tmus
        return (t,)
    calls = run(0x4C6160, "Terrain_LoadLightmaps", setup)
    assert ("unload", "maps/lm.lmp") in calls


@pytest.mark.parametrize("case", ["noname", "missing", "null"])
def test_load_lightmaps_none(case):
    def setup(e):
        t = terrain(e, b"" if case == "noname" else b"maps/lm.lmp")
        STATE["res"] = {} if case == "missing" else {"maps/lm.lmp": (0, b"")}
        if case == "null":
            STATE["res"]["maps/lm.lmp"] = (0, b"")
        return (t,)
    run(0x4C6160, "Terrain_LoadLightmaps", setup)


def map_file(rng, n_entries, entry_bytes):
    """256 TerrainMap entries followed by their data."""
    head, data = b"", b""
    for i in range(256):
        n = rng.choice([0, 0, 4, 8, 16]) if i < n_entries else 0
        head += struct.pack("<IbxxxI", 0x11111111, 0x55, n)
        data += bytes(rng.randrange(256) for _ in range(entry_bytes(n)))
    return head + data


def ctr_file(rng, n, cmd_len):
    return b"".join(bytes(rng.randrange(256) for _ in range(0x1C)) + struct.pack("<I", rng.randrange(cmd_len))
                    for _ in range(n))


@pytest.mark.parametrize("seed", range(3))
@pytest.mark.parametrize("bit", ["full", "short"])
@pytest.mark.parametrize("hmp", ["full", "short", "missing"])
def test_load(seed, bit, hmp):
    def setup(e):
        rng = random.Random(seed)
        t = terrain(e)
        res = {}
        if bit == "full":
            res["lvl/terrain.bit"] = blob(e, map_file(rng, 40, lambda n: n * n // 8))
        else:
            res["lvl/terrain.bit"] = blob(e, b"\1" * 0x100)
        if hmp == "full":
            res["lvl/terrain.hmp"] = blob(e, map_file(rng, 40, lambda n: (n + 1) * (n + 1) if n else 0))
        elif hmp == "short":
            res["lvl/terrain.hmp"] = blob(e, b"\2" * 0xBFF)
        res["lvl/terrain.cmd"] = blob(e, b"cmd" * 50)
        res["lvl/terrain.ctr"] = blob(e, ctr_file(rng, 5 + seed, 150) + b"\1\2\3")
        STATE["res"] = res
        path = e.alloc(32)
        e.write(path, b"lvl/terrain\0")
        return t, path
    run(0x4C6240, "Terrain_Load", setup)


def script_args(e, ints, reals):
    e.write(INTS, struct.pack("<%dI" % len(ints), *[v & 0xFFFFFFFF for v in ints]))
    e.write(REALS, struct.pack("<%dd" % len(reals), *reals))


# Script_GetArgReal(args, i) returns REALS[i]: mov eax,[esp+8]; fld qword [eax*8+REALS]; ret
emu.write(0x4B8A50, b"\x8b\x44\x24\x08\xdd\x04\xc5" + struct.pack("<I", REALS) + b"\xc3")
emu.baseline = emu.snapshot()


# the original returns a copy of a script value whose bytes 4 and 20 were never set
IGNORE = []


def ret_value(e):
    p = e.alloc(24)
    IGNORE[:] = [(p + 4, p + 8), (p + 20, p + 24)]
    return p


@pytest.mark.parametrize("seed", range(4))
def test_create_material(seed):
    def setup(e):
        rng = random.Random(seed)
        ints = [rng.randrange(0, 40), rng.randrange(-5, 1000)] + [rng.randrange(-100, 100) for _ in range(0xE0)]
        reals = [rng.uniform(-1000, 1000) for _ in range(0xE3)]
        script_args(e, ints, reals)
        return ret_value(e), 0, 0x9999
    run(0x4C6670, "CreateTerrainMaterial", setup, check_eax=True, ignore=IGNORE)


@pytest.mark.parametrize("seed", range(4))
def test_create_tile_map(seed):
    def setup(e):
        rng = random.Random(seed)
        script_args(e, [rng.randrange(0, 200)] + [rng.randrange(-300, 300) for _ in range(64)], [])
        return ret_value(e), 0, 0x9999
    run(0x4C6760, "CreateTerrainTileMap", setup, check_eax=True, ignore=IGNORE)
