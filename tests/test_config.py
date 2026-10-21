import random
import struct

import pytest

from emu import CALL_LOG, Emu, compare

emu = Emu()

COUNT, ACTIVE, PROFILES, PSIZE = 0xBC2380, 0xBC2384, 0xBC2388, 0x564
PASSWORD = 0xBC79C8
DEV_COUNT, DEVICES, DSIZE = 0x567C90, 0x567C98, 0x404
ARGS = 0x5A5A0
# inputs for the stubs below; this part of the fake area is never compared
INTS = emu.fake_top + 0x8000
REALS = INTS + 0x100
STRS = INTS + 0x200
DEVICE_ENUM_RET = INTS + 0x400
RET = emu.alloc(24)
MISSIONS = emu.alloc(0x4A8 * 8)
LISTBOX = emu.alloc(0xC00)
COUNTRY = emu.alloc(16)
MODE = emu.alloc(0x120)
IGNORE = [(RET + 4, RET + 8), (RET + 20, RET + 24)]


def stub(e, addr, body, log_args):
    """Replace addr with code that logs (addr, chosen args) to the fake call log, then runs body."""
    count, log = CALL_LOG, CALL_LOG + 4
    code = b"\x8b\x0d" + struct.pack("<I", count)
    code += b"\xc7\x04\x8d" + struct.pack("<II", log, addr) + b"\x41"
    for i in log_args:
        code += b"\x8b\x44\x24" + bytes([4 + 4 * i]) + b"\x89\x04\x8d" + struct.pack("<I", log) + b"\x41"
    code += b"\x89\x0d" + struct.pack("<I", count) + body
    p = e.fake_top
    e.fake_top += (len(code) + 15) & ~15
    e.write(p, code)
    e.write(addr, b"\xe9" + struct.pack("<i", p - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)


# Script_GetArgInt / GetArgReal / GetArgString return INTS[i], REALS[i], STRS[i]
stub(emu, 0x4B8A20, b"\x8b\x44\x24\x08\x8b\x04\x85" + struct.pack("<I", INTS) + b"\xc3", (0, 1))
stub(emu, 0x4B8A50, b"\x8b\x44\x24\x08\xdd\x04\xc5" + struct.pack("<I", REALS) + b"\xc3", (0, 1))
stub(emu, 0x4B8A80, b"\x56\x57\x8b\x44\x24\x10\xc1\xe0\x06\x8d\xb0" + struct.pack("<I", STRS)
     + b"\x8b\x7c\x24\x14\xac\xaa\x84\xc0\x75\xfa\x8b\x44\x24\x14\x5f\x5e\xc3", (0, 1, 3))
# mission lookup: record of mission id is at MISSIONS + id * 0x4A8
stub(emu, 0x485170, b"\x8b\x44\x24\x04\x69\xc0\xa8\x04\x00\x00\x05" + struct.pack("<I", MISSIONS) + b"\xc3", (0,))
# device enumeration fills a stack buffer: log only the max count
stub(emu, 0x491EA0, b"\xa1" + struct.pack("<I", DEVICE_ENUM_RET) + b"\xc3", (1,))

FAKES = {  # address: (nargs, return value)
    0x4B8810: (2, 0), 0x4B8890: (4, 0), 0x4B88F0: (4, 0), 0x4B0FD0: (1, 0), 0x4B0F90: (0, 0),
    0x4B13A0: (1, 0), 0x417090: (0, 1), 0x48F2C0: (0, 0), 0x48F2F0: (0, COUNTRY), 0x4950E0: (0, 7),
    0x491CF0: (0, 0xC28B40), 0x491980: (3, 0), 0x41DC20: (1, 0), 0x41C690: (2, 0), 0x41B870: (2, 0),
    0x41B290: (2, 0),
}
for addr, (nargs, ret) in FAKES.items():
    emu.fake(addr, ret, nargs)
# list box object used by the real ListBox add function at 0x41F2D0
emu.write(0x57BBD8, struct.pack("<I", LISTBOX))
emu.baseline = emu.snapshot()


def fake_ret(e, addr, v):
    e.write(e.fake_ret_at[addr], struct.pack("<I", v & 0xFFFFFFFF))
    e.uc.ctl_remove_cache(e.fake_ret_at[addr], e.fake_ret_at[addr] + 4)


def fill_profiles(e, count, active, seed=1):
    rnd = random.Random(seed)
    data = bytearray(rnd.getrandbits(8) for _ in range(16 * PSIZE))
    for i in range(16):
        b = i * PSIZE
        data[b + 0x540:b + 0x560] = (b"Player%d" % i).ljust(32, b"\0")
        data[b + 0x534:b + 0x538] = struct.pack("<i", rnd.randrange(0, 60))
        for off in (0x220, 0x224, 0x228, 0x22C, 0x53C):  # gamma, volumes, mouse sensitivity
            data[b + off:b + off + 4] = struct.pack("<f", rnd.choice([0.25, 1.5, 2.5, 3.75, -0.5]))
        data[b + 0x1F:b + 0x11F] = (b"dev%d" % (i % 4)).ljust(0x100, b"\0")
    e.write(PROFILES, bytes(data))
    e.write(COUNT, struct.pack("<ii", count, active))


def fill_devices(e, n, modes=((640, 480, 16), (800, 600, 32), (1024, 768, 32), (1600, 1200, 16))):
    e.write(DEV_COUNT, struct.pack("<i", n))
    for i in range(5):
        b = DEVICES + i * DSIZE
        e.write(b, (b"dev%d" % i).ljust(0x80, b"\0") + (b"Device %d" % i).ljust(0x80, b"\0"))
        ms = modes[: len(modes) - i] or modes[:1]
        e.write(b + 0x100, b"".join(struct.pack("<3i", *m) for m in ms))
        e.write(b + 0x400, struct.pack("<i", len(ms)))


def set_args(e, ints=(), reals=(), strs=()):
    e.write(INTS, struct.pack("<8I", *[v & 0xFFFFFFFF for v in list(ints) + [0] * (8 - len(ints))]))
    e.write(REALS, struct.pack("<8d", *(list(reals) + [0.0] * (8 - len(reals)))))
    for i, s in enumerate(list(strs) + [b""] * (4 - len(strs))):
        e.write(STRS + 64 * i, s + b"\0")


def run(orig, ours, ints=(), reals=(), strs=(), count=3, active=1, prep=None, fakes=None, **kw):
    def setup(e):
        for addr, v in (fakes or {}).items():
            fake_ret(e, addr, v)
        fill_profiles(e, count, active)
        fill_devices(e, 3)
        set_args(e, ints, reals, strs)
        e.write(LISTBOX + 0xBE0, b"\0" * 4)
        e.write(COUNTRY, b"EU\0")
        if prep:
            prep(e)
        return RET, 0, ARGS
    try:
        return compare(emu, orig, ours, setup=setup, normalized=True, ignore=IGNORE, **kw)
    finally:
        for addr, (nargs, v) in FAKES.items():
            fake_ret(emu, addr, v)


def result(e):
    i = struct.unpack("<i", e.read(RET, 4))[0]
    d = struct.unpack("<d", e.read(RET + 8, 8))[0]
    return i, d, e.u32(RET + 16)


@pytest.mark.parametrize("devices", [0, 2, 5])
def test_register_script_symbols(devices):
    def prep(e):
        e.write(DEVICE_ENUM_RET, struct.pack("<i", devices))
        e.write(PASSWORD, b"secret\0")
    run(0x402FF0, "Config_RegisterScriptSymbols", prep=prep, check_eax=False, stack_fill=0x41)


GO_CASES = [
    (0x403660, "GOStart", dict(ints=[2])),
    (0x4036B0, "GOPlayer", dict(strs=[b"Jones"])),
    (0x403750, "GOActiveMission", dict(ints=[9])),
    (0x4037C0, "GOInRemap", dict(ints=[4, 1, -1])),
    (0x4037C0, "GOInRemap", dict(ints=[4, 1, 0x4000ABCD])),
    (0x403870, "GOInMouInv", dict(ints=[1])),
    (0x4038E0, "GOInMouSens", dict(reals=[0.7])),
    (0x403950, "GOGfxDisp", dict(ints=[800, 600, 16])),
    (0x403AC0, "GOGameDiff", dict(ints=[2])),
    (0x403C10, "GOContentControlPW", dict(strs=[b"pw123"])),
    (0x403DF0, "GOIsBlood", dict(ints=[0x101])),
    (0x403E60, "GOSoundSpeech", dict(ints=[1], reals=[0, 0.3])),
    (0x403EF0, "GOSoundMusic", dict(ints=[0], reals=[0, 0.6])),
    (0x403F80, "GOSoundFX", dict(ints=[1], reals=[0, 1.1])),
    (0x404010, "GOGfxDevice", dict(strs=[b"Direct3D HAL"])),
    (0x404080, "GOGfxGamma", dict(reals=[1.25])),
    (0x4040F0, "GOGfxPerformance", dict(ints=[0x1AE])),
]


@pytest.mark.parametrize("orig,ours,args", GO_CASES)
@pytest.mark.parametrize("count", [1, 3])
def test_go(orig, ours, args, count):
    run(orig, ours, count=count, **args)


@pytest.mark.parametrize("country,lang", [(b"USA", 2), (b"usa", 5), (b"EU", 2), (b"EU", 5)])
def test_go_game_lang(country, lang):
    run(0x403A00, "GOGameLang", ints=[lang], prep=lambda e: e.write(COUNTRY, country + b"\0"))


@pytest.mark.parametrize("german", [0, 1])
def test_is_germany(german):
    run(0x403B30, "Config_IsGermany", fakes={0x48F2C0: german})


@pytest.mark.parametrize("active", [0, 2])
def test_reset_graphic_options(active):
    run(0x403B70, "Config_ResetGraphicOptions", active=active)


@pytest.mark.parametrize("pw,guess", [(b"", b""), (b"abc", b"abc"), (b"abc", b"abd"), (b"abc", b"ab"),
                                      (b"ab", b"abc"), (b"Abc", b"abc")])
def test_content_control(pw, guess):
    prep = lambda e: e.write(PASSWORD, pw + b"\0")
    run(0x403C70, "Config_GetContentControlPassword", prep=prep)
    run(0x403CC0, "Config_VerifyContentControlPassword", strs=[guess], prep=prep)
    run(0x403D90, "Config_IsContentControlPasswordEnabled", prep=prep)


SIMPLE = [
    (0x4041D0, "Config_GameOptionsInputGetInvertMouse"),
    (0x404290, "Config_GameOptionsInputGetMouseSensitivity"),
    (0x404340, "Config_GameOptionsInputGetBloodEnabled"),
    (0x4046A0, "Config_FillRenderDeviceListBox"),
    (0x404910, "Config_GraphicOptionsGetGamma"),
    (0x4049C0, "Config_GraphicOptionsGetTransparency"),
    (0x404A90, "Config_SoundOptionsGetSoundsEffects"),
    (0x404B40, "Config_SoundOptionsGetSoundsEffectsVolume"),
    (0x404BF0, "Config_SoundOptionsGetMusic"),
    (0x404CA0, "Config_SoundOptionsGetMusicVolume"),
    (0x404D50, "Config_SoundOptionsGetSpeech"),
    (0x404E00, "Config_SoundOptionsGetSpeechVolume"),
    (0x404EB0, "Config_SoundOptionsGetReverseStereo"),
    (0x404F60, "Config_GraphicOptionsGetPerformanceFlags"),
    (0x404FA0, "Config_FillPlayerProfileListBox"),
    (0x405010, "Config_GetActivePlayerProfileIndex"),
    (0x4054F0, "Config_GetNumberOfPlayerProfiles"),
    (0x405620, "Config_GraphicOptionsNumPerfLevels"),
    (0x404590, "Config_GetGraphicOptions"),
    (0x404A70, "Config_GetSoundOptions"),
]


@pytest.mark.parametrize("orig,ours", SIMPLE)
@pytest.mark.parametrize("active", [0, 1, 2])
def test_getters(orig, ours, active):
    def prep(e):
        # invert flag 0 for profile 0, blood negative for profile 2
        e.write(PROFILES + 0x538, b"\0" * 4)
        e.write(PROFILES + 2 * PSIZE + 8, b"\xf0")
    run(orig, ours, active=active, prep=prep)


SETTERS = [
    (0x404160, "Config_GameOptionsInputSetInvertMouse", dict(reals=[0.0])),
    (0x404160, "Config_GameOptionsInputSetInvertMouse", dict(reals=[1.0])),
    (0x404230, "Config_GameOptionsInputSetMouseSensitivity", dict(reals=[0.55])),
    (0x4042E0, "Config_GameOptionsInputSetBloodEnabled", dict(reals=[1.0])),
    (0x4042E0, "Config_GameOptionsInputSetBloodEnabled", dict(reals=[2.5])),
    (0x4042E0, "Config_GameOptionsInputSetBloodEnabled", dict(reals=[300.6])),
    (0x404510, "Config_GraphicOptionsSetResolution", dict(ints=[(32 << 26) | (1024 << 13) | 768])),
    (0x404510, "Config_GraphicOptionsSetResolution", dict(ints=[-1])),
    (0x4047F0, "Config_GraphicOptionsSetDevice", dict(ints=[2])),
    (0x4048B0, "Config_GraphicOptionsSetGamma", dict(reals=[0.8])),
    (0x404960, "Config_GraphicOptionsSetTransparency", dict(ints=[0x201])),
    (0x404A10, "Config_SoundOptionsSetSoundsEffects", dict(ints=[0])),
    (0x404AE0, "Config_SoundOptionsSetSoundsEffectsVolume", dict(reals=[0.45])),
    (0x404B90, "Config_SoundOptionsSetMusic", dict(ints=[1])),
    (0x404C40, "Config_SoundOptionsSetMusicVolume", dict(reals=[0.15])),
    (0x404CF0, "Config_SoundOptionsSetSpeech", dict(ints=[0x7F])),
    (0x404DA0, "Config_SoundOptionsSetSpeechVolume", dict(reals=[0.95])),
    (0x404E50, "Config_SoundOptionsSetReverseStereo", dict(ints=[1])),
    (0x404F00, "Config_GraphicOptionsSetPerformanceFlags", dict(ints=[0x60])),
    (0x405050, "Config_SetActivePlayerProfileIndex", dict(ints=[2])),
    (0x4055C0, "Config_GraphicOptionsGetPerfFlagsFromLevel", dict(ints=[3])),
    (0x4057D0, "Config_PlayerGetActiveMission", dict(ints=[2])),
    (0x404450, "Config_FillScreenResolutionListBox", dict(ints=[0])),
    (0x404450, "Config_FillScreenResolutionListBox", dict(ints=[2])),
]


@pytest.mark.parametrize("orig,ours,args", SETTERS)
@pytest.mark.parametrize("active", [0, 2])
def test_setters(orig, ours, args, active):
    run(orig, ours, active=active, **args)


@pytest.mark.parametrize("flags", [0, 0x19E, 0x1AF, 0xD, 0x60, 0x4D, 0x41, 0xFFFFFFFF, 0x12345678, 0x1000])
def test_perf_level_from_flags(flags):
    run(0x405530, "Config_GraphicOptionsGetPerfLevelFromFlags", ints=[flags])


@pytest.mark.parametrize("dev", [0, 1, 2])
@pytest.mark.parametrize("cur", [(800, 600, 32), (640, 480, 32), (2000, 100, 16), (1024, 768, 8), (0, 0, 0),
                                 (-70000, -70000, 32), (800, 600, 200)])
def test_get_resolution(dev, cur):
    def prep(e):
        e.write(PROFILES + 0x564 + 0xC, struct.pack("<3i", *cur))
    run(0x4045B0, "Config_GraphicOptionsGetResolution", ints=[dev], prep=prep)


@pytest.mark.parametrize("dev", ["dev0", "dev2", "dev3", "none"])
@pytest.mark.parametrize("ndev", [0, 3])
def test_get_device(dev, ndev):
    def prep(e):
        e.write(DEV_COUNT, struct.pack("<i", ndev))
        e.write(PROFILES + 0x564 + 0x1F, dev.encode() + b"\0")
    run(0x404710, "Config_GraphicOptionsGetDevice", prep=prep)


@pytest.mark.parametrize("name", [b"dev1", b"dev4", b"Dev1", b"dev", b"dev10"])
@pytest.mark.parametrize("ndev", [0, 1, 3])
@pytest.mark.parametrize("size", [(640, 480), (639, 480), (640, 479), (1280, 1024)])
def test_add_display_mode(name, ndev, size):
    def setup(e):
        fill_devices(e, ndev)
        e.write(MODE, struct.pack("<8i", 0, size[0], size[1], 0, 24, 0, 0, 0)[:0x1C] + name + b"\0")
        return (MODE,)
    compare(emu, 0x404390, "Config_AddDisplayMode", setup=setup, normalized=True, check_eax=False)


def test_add_display_mode_null_name():
    def setup(e):
        fill_devices(e, 3)
        return (0xFFFFFFE4,)
    compare(emu, 0x404390, "Config_AddDisplayMode", setup=setup, normalized=True, check_eax=False)


@pytest.mark.parametrize("name", [b"Jane", b"Jane  \t ", b"   ", b"", b"player1", b"PLAYER2 ", b"\tX"])
@pytest.mark.parametrize("count,active", [(0, 0), (1, 0), (3, 1), (3, 2), (15, 4), (16, 0), (17, 0)])
def test_create_profile(name, count, active):
    run(0x4050A0, "Config_CreateNewPlayerProfile", strs=[name], ints=[0, 2], count=count, active=active)


@pytest.mark.parametrize("country,german", [(b"USA", 0), (b"EU", 0), (b"EU", 7)])
def test_create_first_profile(country, german):
    run(0x4050A0, "Config_CreateNewPlayerProfile", strs=[b"Ann"], ints=[0, 3], count=0, active=4,
        prep=lambda e: e.write(COUNTRY, country + b"\0"), fakes={0x48F2C0: german})


@pytest.mark.parametrize("country,german", [(b"USA", 1), (b"EU", 0), (b"EU", 1)])
def test_reset_profiles(country, german):
    run(0x405300, "Config_ResetProfiles", prep=lambda e: e.write(COUNTRY, country + b"\0"),
        fakes={0x48F2C0: german, 0x417090: 4}, check_eax=False)


@pytest.mark.parametrize("index", [-1, 0, 1, 2, 3, 4])
@pytest.mark.parametrize("active", [0, 1, 2])
def test_delete_profile(index, active):
    run(0x405400, "Config_DeletePlayerProfile", ints=[index], count=4, active=active)


def missions(e, chain):
    """chain: list of mission ids, each record points to the next, the last to -1."""
    for i, mid in enumerate(chain):
        b = MISSIONS + mid * 0x4A8
        nxt = chain[i + 1] if i + 1 < len(chain) else -1
        e.write(b + 0x4A4, struct.pack("<i", nxt))


@pytest.mark.parametrize("first,chain,current", [
    (-1, [], 1), (1, [1, 2, 3, 4], 3), (1, [1, 2, 3, 4], 7), (1, [1], 1), (2, [2, 5, 1], 2),
])
@pytest.mark.parametrize("orig,ours", [
    (0x405660, "Config_FillMissionSelectionBox"), (0x4056F0, "Config_FillMissionPictureBox"),
    (0x405760, "Config_FillMissionTextBox"),
])
def test_fill_missions(first, chain, current, orig, ours):
    def prep(e):
        missions(e, chain)
        e.write(PROFILES + 0x564 + 0x560, struct.pack("<i", current))
    run(orig, ours, prep=prep, fakes={0x417090: first})


@pytest.mark.parametrize("flag", [0, 1])
def test_remove_language_alias(flag):
    run(0x405830, "Config_RemoveLanguageAlias", prep=lambda e: e.write(0x537B90, bytes([flag])),
        check_eax=False)


def test_result_values():
    for target in (0x404290, "Config_GameOptionsInputGetMouseSensitivity"):
        emu.reset()
        fill_profiles(emu, 2, 1)
        emu.write(PROFILES + PSIZE + 0x53C, struct.pack("<f", 2.5))
        assert emu.call(target, RET)[0] == RET
        assert result(emu) == (2, 2.5, 0x567C74)
    for target in (0x4050A0, "Config_CreateNewPlayerProfile"):
        emu.reset()
        fill_profiles(emu, 2, 1)
        set_args(emu, ints=[0, 3], strs=[b"New"])
        emu.call(target, RET, 0, ARGS)
        assert result(emu) == (1, 1.0, 0x567C74)
        assert emu.read(COUNT, 8) == struct.pack("<ii", 3, 2)
        assert emu.read(PROFILES + 2 * PSIZE + 0x540, 4) == b"New\0"
