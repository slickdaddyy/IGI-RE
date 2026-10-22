import struct

import pytest

from emu import Emu, ErrorShown, compare
from test_profile_util import LOG, RET, check, cstr, f32, logfake, pyfake, u32

PROFILES, SIZE = 0xBC2388, 0x564
COUNT, ACTIVE = 0xBC2380, 0xBC2384
DRIVERS, DRIVER_COUNT = 0x567C98, 0x567C90

emu = Emu()
for addr, n in ((0x4B85B0, 1), (0x4B83D0, 1), (0x405300, 0), (0x495E70, 2), (0x495F30, 1), (0x4B1690, 1),
                (0x48F340, 0)):
    logfake(emu, addr, n)
for addr in (0x4B80B0, 0x4B13A0, 0x4B8410, 0x4B1AC0):
    logfake(emu, addr, 1, strings=(0,))
logfake(emu, 0x4B12F0, 2, strings=(0, 1))
logfake(emu, 0x4B1510, 2, strings=(0, 1))


def file_write(e):
    LOG[-1].append(("write", e.arg(0), e.read(e.arg(1), e.arg(2))))
    e.ret(e.arg(2))


pyfake(emu, 0x4B1700, file_write)


def perf_fn(e):
    LOG[-1].append(("perf", e.u32(e.arg(0))))
    e.ret(0)


pyfake(emu, 0x401000, perf_fn)
emu.call(0x4A537B)  # C runtime float formatting, normally set up at startup
emu.baseline = emu.snapshot()


def profile(e, i, difficulty=1, language=0, blood=1, disp=(800, 600, 16), perf=0, device=b"dev0",
            driver=b"drv", gamma=1.25, sfx=(1, 0.5), music=(0, 0.75), speech=(1, 0.25), keys=(), mouse=(1, 2.5),
            name=b"Jones", mission=3):
    p = PROFILES + i * SIZE
    e.write(p, b"\0" * SIZE)
    u32(e, p, difficulty, language)
    e.write(p + 8, bytes([blood]))
    u32(e, p + 0xC, *disp, perf)
    e.write(p + 0x1F, device + b"\0")
    e.write(p + 0x11F, driver + b"\0")
    f32(e, p + 0x220, gamma, sfx[1], music[1], speech[1])
    e.write(p + 0x230, bytes([sfx[0], music[0], speech[0]]))
    for j, k in enumerate(keys):
        u32(e, p + 0x234 + 12 * j, *k)
    u32(e, p + 0x534, len(keys), mouse[0])
    f32(e, p + 0x53C, mouse[1])
    e.write(p + 0x540, name + b"\0")
    u32(e, p + 0x560, mission)
    return p


def profiles(e, n=2, active=0, **kw):
    u32(e, COUNT, n, active)
    for i in range(n):
        profile(e, i, **kw)


def drivers(e, specs):
    u32(e, DRIVER_COUNT, len(specs))
    for i, (name, desc, modes) in enumerate(specs):
        d = DRIVERS + i * 0x404
        e.write(d, name + b"\0")
        e.write(d + 0x80, desc + b"\0")
        for j, m in enumerate(modes):
            u32(e, d + 0x100 + 12 * j, *m)
        u32(e, d + 0x400, len(modes))


DRIVERS_A = [(b"dev0", b"first", [(640, 480, 16), (800, 600, 16), (1024, 768, 32)]),
             (b"dev1", b"second", [(640, 480, 32), (1600, 1200, 32), (800, 600, 32), (800, 600, 32)])]

KEYS = [(0, 0, 30), (5, 1, 0x40000002), (99, 7, 1234), (3, 2, 5)]


@pytest.mark.parametrize("loaded", [0, 0x12340])
def test_read(loaded):
    def setup(e):
        RET[0x4B80B0] = loaded
        profiles(e, 1)
        drivers(e, DRIVERS_A)
        return (cstr(e, b"LOCAL:config.qsc"),)
    check(emu, 0x405850, "Config_Read", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("keys", [[], KEYS, [(i, 0, i) for i in range(27)], [(0x3F, 0, 1), (0x40, 0, 2)]])
@pytest.mark.parametrize("active", [0, 1])
def test_add_missing_keys(keys, active):
    def setup(e):
        profiles(e, 2, active, keys=keys)
    compare(emu, 0x4058A0, "Config_AddMissingKeys", setup=setup, check_eax=False)


@pytest.mark.parametrize("device,disp", [(b"dev0", (800, 600, 16)), (b"dev1", (801, 599, 24)),
                                         (b"nope", (1600, 1200, 32)), (b"dev1", (-5, 0x7FFFFFFF, 99)),
                                         (b"dev1", (0x80000000, 0, 32))])
def test_fix_display_modes(device, disp):
    def setup(e):
        profiles(e, 3, device=device, disp=disp)
        drivers(e, DRIVERS_A)
    compare(emu, 0x405980, "Config_FixDisplayModes", setup=setup, check_eax=False)


def test_fix_display_modes_no_driver():
    for fn in (0x405980, "Config_FixDisplayModes"):
        emu.reset()
        profiles(emu, 1)
        u32(emu, DRIVER_COUNT, 0)
        with pytest.raises(ErrorShown):
            emu.call(fn)


@pytest.mark.parametrize("set_before", [0, 1])
def test_apply(set_before):
    def setup(e):
        profiles(e, 2, 1, language=3, music=(1, 0.3), sfx=(1, 0.6))
        e.write(0x537B90, bytes([set_before]))
    check(emu, 0x405B30, "Config_Apply", setup=setup, check_eax=False, normalized=True)


KEYS_ODD = [(i % 30, i % 4, 0x400000 * (i % 3) + i) for i in range(40)]


@pytest.mark.parametrize("qvm", [0, 1])
@pytest.mark.parametrize("count", [0, 1, 3])
def test_write(qvm, count):
    def setup(e):
        RET[0x4B1510] = 0x5555
        RET[0x48F340] = qvm
        profiles(e, count, keys=KEYS)
        e.write(0xBC79C8, b"secret\0")
        return (cstr(e, b"LOCAL:config.qsc"),)
    check(emu, 0x405BD0, "Config_Write", setup=setup, check_eax=False, normalized=True)


def test_write_fails():
    for fn in (0x405BD0, "Config_Write"):
        emu.reset()
        RET[0x4B1510] = 0
        LOG.append([])
        with pytest.raises(ErrorShown):
            emu.call(fn, cstr(emu, b"x"))


@pytest.mark.parametrize("orig,ours,args", [
    (0x405C50, "Config_WriteStart", (7,)),
    (0x406170, "Config_WriteContentControl", (7,)),
    (0x405C90, "Config_WritePlayer", (7, 1)),
    (0x405D40, "Config_WriteGame", (7, 1)),
    (0x405E00, "Config_WriteSound", (7, 1)),
    (0x405EC0, "Config_WriteGfx", (7, 1)),
    (0x405F90, "Config_WriteInput", (7, 1)),
])
@pytest.mark.parametrize("variant", [0, 1])
def test_write_parts(orig, ours, args, variant):
    def setup(e):
        if variant:
            profiles(e, 2, 1, difficulty=2, language=5, blood=0, sfx=(0xFF, -1.5), music=(1, 1e10),
                     speech=(0x80, 0.001), keys=KEYS_ODD, mouse=(0, 0.1), name=b"A" * 31, perf=0x1FF)
        else:
            profiles(e, 2, 1, keys=KEYS)
        e.write(0xBC79C8, b"pw\0")
    check(emu, orig, ours, args=args, setup=setup, check_eax=False)


@pytest.mark.parametrize("orig,ours,args", [
    (0x4061B0, "Config_GetDefaultKeys", ()),
    (0x4061C0, "Profile_GetKeys", ()),
    (0x4061E0, "Profile_GetKeyCount", ()),
    (0x406200, "Profile_SetKeyCount", (17,)),
    (0x406220, "Profile_GetActive", ()),
    (0x406270, "Config_GetKeyCount", ()),
    (0x4062B0, "Config_GetActionCount", ()),
    (0x4062E0, "Profile_GetLanguage", ()),
    (0x406310, "Config_GetLanguageCount", ()),
    (0x406320, "Config_GetLanguageName", (4,)),
    (0x406330, "Config_GetDifficultyName", (2,)),
])
def test_small(orig, ours, args):
    compare(emu, orig, ours, args=args, setup=lambda e: profiles(e, 3, 2, language=4, keys=KEYS),
            check_eax=ours != "Profile_SetKeyCount")


@pytest.mark.parametrize("orig,ours", [(0x4062C0, "Profile_GetSfxVolume"), (0x4062D0, "Profile_GetMusicVolume")])
def test_volume(orig, ours):
    compare(emu, orig, ours, setup=lambda e: profiles(e, 3, 1, sfx=(1, 0.7), music=(1, 0.2)),
            check_eax=False, check_st0=True)


@pytest.mark.parametrize("orig,ours", [(0x406240, "Config_GetKey"), (0x406280, "Config_GetAction")])
@pytest.mark.parametrize("value,name", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_get_named(orig, ours, value, name):
    def setup(e):
        p = e.alloc(8)
        return 5, p if value else 0, p + 4 if name else 0
    compare(emu, orig, ours, setup=setup, check_eax=False)


def test_set_language():
    def setup(e):
        profiles(e, 2, 1)
        return (2,)
    check(emu, 0x4062F0, "Profile_SetLanguage", setup=setup, check_eax=False, normalized=True)


def missions(e, chain):
    """chain: list of (id, next) records in the mission table."""
    u32(e, 0x5C89FC, len(chain))
    for i, (mid, nxt) in enumerate(chain):
        u32(e, 0x5C1590 + 0x4A8 * i, mid)
        u32(e, 0x5C1590 + 0x4A8 * i + 0x4A4, nxt)


@pytest.mark.parametrize("active,target", [(1, 4), (2, 4), (4, 4), (3, 2), (9, 4), (2, 99)])
def test_advance_mission(active, target):
    def setup(e):
        profiles(e, 2, 1, mission=active)
        missions(e, [(1, 2), (2, 3), (3, 4), (4, 5), (5, -1)])
        return (target,)
    compare(emu, 0x406340, "Profile_AdvanceMission", setup=setup, check_eax=False)


@pytest.mark.parametrize("flags", [0, 1, 2, 3, 0x1FC, 0x10, 0x20, 0x30, 0x40, 0x80, 0xC0, 0x100, 0xFFFFFFFF])
def test_apply_performance(flags):
    def setup(e):
        profiles(e, 2, 1, perf=flags)
        u32(e, 0x546F90, 3)
        u32(e, 0xA94E84 + 12, 0x401000)
    check(emu, 0x406390, "Profile_ApplyPerformance", setup=setup, check_eax=False)
