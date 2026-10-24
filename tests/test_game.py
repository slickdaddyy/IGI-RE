import struct

import pytest

from emu import Emu, ErrorShown, compare
from test_game_stubs import PyFake, d64, f32, fakes, u32


@pytest.fixture(autouse=True, params=[0x37F, 0x7F, 0xC7F, 0x77F], ids=hex)
def cw(request):
    """Every test at the emulator default and at the control words the game uses."""
    Emu.cw = request.param
    yield
    Emu.cw = None


emu = Emu()

G_GAME, MISSION_ID, RAINING, GAME_TYPE = 0x57BABC, 0x539560, 0x57B1A0, 0x57B1A2
EVENTS, LISTS, MUSIC_ON = 0x539540, 0x57BA9C, 0x57BAC0
SPRITES, SPRITE_COUNT, TEXTURES, TEXTURE_COUNT = 0x57B1A8, 0x57BAB8, 0x57B5E8, 0x57BAB4
WTYPES, WCOUNT = 0x56E258, 0x57B18C
HANDLERS, RENDER_FUNCS = 0xA96AE0, 0xA94E84
KEYS, KEYS_PREV, CHEATS = 0xC28C62, 0xC28D62, 0x57B194
UNK_C28C61, UNK_C28F80 = 0xC28C61, 0xC28F80

# python state for the fakes, reset by each setup
S = {}


def counter(name, start, step=1):
    def f(e):
        v = S.get(name, start)
        S[name] = v + step
        return v
    return f


def cstr(i, n=8):
    """log n dwords of the C string at argument i (stack buffers differ in address)"""
    return lambda e: list(struct.unpack("<%dI" % n, (e.read_cstr(e.arg(i)) + b"\0" * 4 * n)[:4 * n]))


def sprintf(e):
    fmt = e.read_cstr(e.arg(1)).decode()
    arg = e.arg(2)
    if "%d" in fmt:
        out = fmt.replace("%d", str(arg))
    else:
        out = fmt.replace("%s", e.read_cstr(arg).decode())
    e.write(e.arg(0), out.encode() + b"\0")


def cutscene_delete(e):
    lst = e.u32(0x57BAB0)
    n = e.u32(lst)
    e.write(lst, u32(n - 1))
    e.write(lst + 8, e.read(lst + 12, 4 * 8))


def clear_pair(e):
    if S.get("clear"):
        e.write(e.arg(0), u32(0))
        e.write(e.arg(1), u32(0))


F = fakes(emu, {
    0x48F360: 2, 0x4018F0: (0, 7), 0x401530: 3, 0x4F4870: 1, 0x4B8810: 2, 0x4B8870: 2, 0x4B8890: 4,
    0x4A29E0: 3, 0x4A29C0: 3, 0x4A28B0: 5, 0x4635B0: 1, 0x4635E0: 1, 0x495E70: 2, 0x495F30: 1,
    0x4A5490: (1, 42), 0x402820: 1, 0x4028D0: 0, 0x4028F0: 0, 0x485170: 1, 0x4B12F0: 2,
    0x453270: 0, 0x4D9950: 1, 0x491E70: 3, 0x406390: 0, 0x48A440: (1, 0xAAA0), 0x4C5B20: (1, 0x7700),
    0x408390: 1, 0x4C1780: 1, 0x4B2610: 1, 0x4B25B0: 2, 0x4CEEA0: 0, 0x4CFC00: 1,
    0x48A620: 2, 0x4F0CE0: 1, 0x4B25F0: 0, 0x4CEA60: (0, 3), 0x4CEA70: (0, 4), 0x4E68D0: 1,
    0x4B5F00: (2, 0x5500), 0x46B070: 0, 0x4868A0: 0, 0x402890: (0, 0x2890), 0x485300: (1, 0x4440),
    0x4F0CB0: (1, 0x4C4C), 0x4E6060: 1, 0x4E6550: 2, 0x4E7D40: (3, 0x7D40), 0x401AE0: (1, 0x5454),
    0x4E7D70: 3, 0x4D2D30: 1, 0x4061C0: (13, 0x61C0), 0x4061E0: (1, 0x61E0), 0x4ED5E0: 16, 0x4F80E0: 1,
    0x437500: 1, 0x4EDCC0: 0, 0x4F65B0: 1, 0x4F65D0: 1, 0x485320: (1, 0x5320), 0x4F65C0: 1,
    0x4F0E10: 2, 0x4CE830: 2, 0x4A5350: (2, 0xF11E), 0x4A52A4: 3, 0x4A528A: (1, 0x5E), 0x4A5234: 1,
    0x497450: (1, 0x7450), 0x497410: (1, 0x7410), 0x4974A0: 1, 0x4B0D50: 0, 0x48A5D0: 1,
    0x4B63D0: 1, 0x4C1800: 1, 0x4C1830: 1, 0x4B0D90: 0, 0x489450: (0, 0), 0x406340: 1, 0x405BD0: 1,
    0x4028A0: 1, 0x4E6D60: 1, 0x4027F0: (1, 0x27F0), 0x4E7530: (0, 5), 0x4E75B0: (1, 0x75B0),
    0x46C970: (0, 0x305), 0x47CDA0: (1, 77), 0x47CE00: 3, 0x4B0DB0: (0, 0), 0x4B0D40: 0, 0x4E7130: 0,
    0x4F0E90: 0, 0x401B20: 1, 0x46B310: 0, 0x4868C0: 0, 0x4F0CD0: 0, 0x4E69C0: 1, 0x4D65D0: 0,
    0x4CF9D0: 1, 0x4C5CE0: 1, 0x4B13A0: 1, 0x4B4720: 1, 0x4B47E0: 2, 0x4F1030: (1, 0), 0x484D40: (2, 0x4D40),
    0x484D60: 2, 0x48F250: (0, 0), 0x4F0ED0: (0, 0), 0x4F8FD0: (0, 0x31), 0x487180: (0, 0),
    0x4868D0: 1, 0x401780: 1, 0x468DD0: 0, 0x4B66A0: 1, 0x4B22F0: 1,
})
PyFake(emu, 0x401900, log=(0, 1, ("w", 2), 3), ret=0x1234)
PyFake(emu, 0x401400, log=(("w", 0), ("b", 1), 2))
PyFake(emu, 0x401A20, log=(("w", 0),))
PyFake(emu, 0x4017C0, log=(("w", 0),), ret=counter("event", 0x40))
PyFake(emu, 0x4018C0, log=(("b", 0),))
PyFake(emu, 0x4012A0, log=(0, ("w", 1), 2), ret=0x12A0)
PyFake(emu, 0x4B0D60, log=(("b", 0),))
PyFake(emu, 0x48F210, log=(("b", 0),))
PyFake(emu, 0x4B8A20, log=(0, 1), ret=lambda e: 100 + e.arg(1))
PyFake(emu, 0x4B8A50, log=(0, 1), fret=lambda e: 0.5 + 1.25 * e.arg(1))
PyFake(emu, 0x4062D0, fret=lambda e: 0.1 + 0.2 * counter("music", 1)(e))
PyFake(emu, 0x4062C0, fret=0.35)
PyFake(emu, 0x4A53B3, log=(1, cstr(2, 6)), fn=sprintf)
PyFake(emu, 0x4C5F00, log=(0, cstr(1)))
PyFake(emu, 0x4C6240, log=(0, cstr(1)))
PyFake(emu, 0x4CFBD0, log=(cstr(0),))
PyFake(emu, 0x4CEFA0, log=(0,), ret=lambda e: e.arg(0) + 0x100000)
PyFake(emu, 0x4B24C0, log=(0, 1, cstr(2, 12)))
PyFake(emu, 0x4B6720, log=(0, cstr(1, 12)))
PyFake(emu, 0x491A90, log=((0, 0x47),))
PyFake(emu, 0x4950E0, ret=counter("rm", 0x10))
PyFake(emu, 0x4E7BD0, log=(0, lambda e: [e.arg(1), e.arg(2)] if b"%" in e.read_cstr(e.arg(0)) else []))
PyFake(emu, 0x401F40, log=(2,), fn=clear_pair)
PyFake(emu, 0x4015F0, log=(0,), fn=lambda e: cutscene_delete(e) if S.get("cutscene") else None)
PROFILE = emu.alloc(0x600)
GET_PROFILE = PyFake(emu, 0x406220, ret=PROFILE)
emu.write(PROFILE + 0xC, u32(640) + u32(480) + u32(16))
emu.write(PROFILE + 0x1F, b"Primary Display Driver\0")
emu.write(PROFILE + 0x11F, b"Direct3D HAL\0")

# handlers called through tables
HANDLER = emu.alloc(16)
PyFake(emu, HANDLER, log=(0, (1, 2)))
HANDLER1 = emu.alloc(16)
PyFake(emu, HANDLER1, log=(0,))
RENDER = emu.alloc(16)
PyFake(emu, RENDER, log=(0,))
emu.baseline = emu.snapshot()

OUT = emu.alloc(24)
IGNORE = [(OUT + 4, OUT + 8), (OUT + 20, OUT + 24)]


def reset_state(**kw):
    S.clear()
    S.update(kw)


def new_task(e, size, type_=0):
    t = e.alloc(size)
    e.write(t + 8, u32(t + 0xC) + u32(0) + u32(t + 8))
    e.write(t + 0x1C, struct.pack("<H", type_))
    return t


def add_child(e, parent, child):
    tailpred = e.u32(parent + 0x10)
    e.write(child, u32(parent + 0xC) + u32(tailpred))
    e.write(tailpred, u32(child))
    e.write(parent + 0x10, u32(child))


def new_game(e, players=(0x7101, 0, 0x7103, 0)):
    g = new_task(e, 0x158, 0x55)
    e.write(g + 0x20, u32(0x7700) + u32(0x7724))
    e.write(g + 0x44, u32(0x7744) + u32(0x7748) + u32(0x774C))
    ref = e.alloc(4)
    e.write(ref, u32(0x7754))
    e.write(g + 0x54, u32(ref))
    for i, p in enumerate(players):
        if p:
            r = e.alloc(4)
            e.write(r, u32(p))
            e.write(g + 0xC0 + 4 * i, u32(r))
    e.write(g + 0xE8, u32(0x77E8) + u32(0x77EC) + u32(0x1111) + u32(0x2222) + u32(0x3333) + u32(0x4444))
    e.write(G_GAME, u32(g))
    return g


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), u32(fn))


def test_init():
    compare(emu, 0x415530, "Game_Init", setup=lambda e: reset_state(), check_eax=False, normalized=True)


@pytest.mark.parametrize("addr,name", [(0x4157D0, "Game_DisablePlayerInput"), (0x415830, "Game_EnablePlayerInput")])
@pytest.mark.parametrize("players", [(0x7101, 0, 0x7103, 0), (0, 0, 0, 0), (1, 2, 3, 4)])
def test_player_input(addr, name, players):
    compare(emu, addr, name, args=(OUT,), setup=lambda e: new_game(e, players) and None, ignore=IGNORE,
            normalized=True)


@pytest.mark.parametrize("addr,name", [
    (0x415890, "Game_SetMission"), (0x4158E0, "Game_UpdateVolume"), (0x415950, "Game_SetMusicVolume"),
    (0x4159B0, "Game_SetSFXVolume"), (0x415A10, "Game_EnableMusic"), (0x415A60, "Game_DisableMusic")])
def test_script_functions(addr, name):
    def setup(e):
        reset_state()
        e.write(MUSIC_ON, b"\x07")
    compare(emu, addr, name, args=(OUT, 0x3333, 0x5A00), setup=setup, ignore=IGNORE, normalized=True)


@pytest.mark.parametrize("n", [0, 1, 3])
def test_cutscene_delete(n):
    def setup(e):
        reset_state(cutscene=1)
        lst = e.alloc(48)
        e.write(lst, u32(n) + u32(8) + b"".join(u32(0x6600 + i) for i in range(8)))
        e.write(0x57BAB0, u32(lst))
    compare(emu, 0x415AB0, "Game_CutsceneDelete", args=(OUT,), setup=setup, ignore=IGNORE, normalized=True)


def test_parse_level_arg():
    compare(emu, 0x415B10, "Game_ParseLevelArg", args=(0x5000,), check_eax=False, normalized=True)


def mission(e):
    m = e.alloc(0x500)
    e.write(m + 0x144, b"missions/location0/level1\0")
    e.write(m + 0x244, b"missions/location0/common\0")
    e.write(m + 0x344, b"missions/location0/level1/models\0")
    e.write(m + 0x444, b"LOCAL:models.res\0")
    e.write(m + 0x464, b"LOCAL:objects.res\0")
    e.write(m + 0x4A4, u32(S.get("next", 2)))
    return m


def add_entries(e, base, size, count_addr, names):
    for i, n in enumerate(names):
        e.write(base + i * size, n.ljust(0x18, b"\0"))
    e.write(count_addr, u32(len(names)))


@pytest.mark.parametrize("render", [0, 1])
@pytest.mark.parametrize("keys,joy", [(0, 0), (1, 0), (1, 1)])
@pytest.mark.parametrize("disc", [0x5E, 0x5D])
def test_level_start(render, keys, joy, disc):
    def setup(e):
        reset_state()
        F[0x485170].ret = mission(e)
        F[0x4A528A].ret = disc
        e.write(MISSION_ID, u32(3))
        e.write(RENDER_FUNCS + 4 * 3, u32(RENDER if render else 0))
        e.write(UNK_C28C61, bytes([keys]))
        e.write(UNK_C28F80, bytes([joy]))
        add_entries(e, SPRITES, 0x30, SPRITE_COUNT, [b"cross.spr", b"radar.spr"])
        add_entries(e, TEXTURES, 0x3C, TEXTURE_COUNT, [b"sky.tex"])
        g = new_task(e, 0x158, 0x55)
        e.write(G_GAME, u32(0x1234))
        return (g,)
    if disc == 0x5E:
        compare(emu, 0x415B30, "LevelStart", setup=setup, check_eax=False, normalized=True)
    else:
        for fn in (0x415B30, "LevelStart"):
            emu.reset()
            args = setup(emu)
            with pytest.raises(ErrorShown, match="0x000001"):
                emu.call(fn, *args)


@pytest.mark.parametrize("fopen", [0, 0xF11E])
def test_level_start_errors(fopen):
    for fn in (0x415B30, "LevelStart"):
        emu.reset()
        reset_state()
        F[0x485170].ret = 0
        e = emu
        e.write(MISSION_ID, u32(77))
        with pytest.raises(ErrorShown, match="The mission ID %d does not exist."):
            emu.call(fn, new_task(e, 0x158))
    for fn in (0x415B30, "LevelStart"):
        emu.reset()
        reset_state()
        F[0x485170].ret = mission(emu)
        F[0x4A5350].ret = 0
        with pytest.raises(ErrorShown, match="0x000001"):
            emu.call(fn, new_task(emu, 0x158))
    F[0x4A5350].ret = 0xF11E


def test_set_display_mode():
    compare(emu, 0x416010, "Game_SetDisplayMode", setup=lambda e: reset_state(), check_eax=False, normalized=True)


@pytest.mark.parametrize("weather", [b"\x00\x00", b"\x00\x01", b"\x00\xff"])
def test_set_raining(weather):
    def setup(e):
        p = e.alloc(4)
        e.write(p, weather)
        return (p,)
    compare(emu, 0x416110, "Game_SetRaining", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("names", [[], [b"a"], [b"sky.tex", b"water.tex", b"twentythree_chars_name\x00"]])
@pytest.mark.parametrize("addr,name,base,size,count", [
    (0x416130, "Game_LoadTextures", TEXTURES, 0x3C, TEXTURE_COUNT),
    (0x4161D0, "Game_LoadSprites", SPRITES, 0x30, SPRITE_COUNT),
    (0x416660, "Game_FreeTextures", TEXTURES, 0x3C, TEXTURE_COUNT),
    (0x416690, "Game_FreeSprites", SPRITES, 0x30, SPRITE_COUNT)])
def test_textures_sprites(addr, name, base, size, count, names):
    compare(emu, addr, name, setup=lambda e: add_entries(e, base, size, count, names), check_eax=False,
            normalized=True)


@pytest.mark.parametrize("addr,name,base,size,count", [
    (0x416930, "Game_AddTexture", TEXTURES, 0x3C, TEXTURE_COUNT),
    (0x4169D0, "Game_AddSprite", SPRITES, 0x30, SPRITE_COUNT)])
@pytest.mark.parametrize("key", [b"sky", b"water", b"sk", b"skyy", b"", b"new_one"])
def test_add(addr, name, base, size, count, key):
    def setup(e):
        add_entries(e, base, size, count, [b"sky", b"water", b""][:3 if key != b"" else 2])
        p = e.alloc(32)
        e.write(p, key + b"\0")
        return (p,)
    compare(emu, addr, name, setup=setup)


def run_setup(e, paused=0, music=1, quit=0, f12=0, kids=(0x21, 0x22, 0x21)):
    reset_state(clear=1)
    F[0x489450].ret = quit
    e.write(MUSIC_ON, bytes([music]))
    e.write(KEYS + 0x58, b"\x80" if f12 else b"\x00")
    e.write(KEYS + 0x1D, b"\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80")
    e.write(PROFILE + 0x560, u32(14))
    e.write(CHEATS, u32(1))
    g = new_game(e)
    e.write(g + 0xE0, bytes([paused]))
    for t in kids:
        add_child(e, g, new_task(e, 0x20, t))
    set_handler(e, 1, 0x21, HANDLER1)
    return (g,)


@pytest.mark.parametrize("paused", [0, 1])
@pytest.mark.parametrize("music", [0, 1])
@pytest.mark.parametrize("kids", [(), (0x21,), (0x21, 0x22, 0x21)])
def test_run(paused, music, kids):
    compare(emu, 0x416260, "Game_RunHandler", setup=lambda e: run_setup(e, paused, music, kids=kids),
            check_eax=False, normalized=True)


@pytest.mark.parametrize("quit,f12", [(1, 0), (0, 1)])
@pytest.mark.parametrize("next_", [5, -1])
def test_run_mission_done(quit, f12, next_):
    def setup(e):
        args = run_setup(e, quit=quit, f12=f12)
        S["next"] = next_
        F[0x485170].ret = mission(e)
        F[0x4A528A].ret = 0x40
        return args
    compare(emu, 0x416260, "Game_RunHandler", setup=setup, check_eax=False, normalized=True)
    F[0x4A528A].ret = 0x5E


@pytest.mark.parametrize("fopen,byte", [(0, 0x40), (0xF11E, 0x41)])
def test_run_disc_check(fopen, byte):
    for fn in (0x416260, "Game_RunHandler"):
        emu.reset()
        args = run_setup(emu, quit=1)
        F[0x485170].ret = mission(emu)
        F[0x4A5350].ret = fopen
        F[0x4A528A].ret = byte
        with pytest.raises(ErrorShown, match="0x000000"):
            emu.call(fn, *args)
    F[0x4A5350].ret = 0xF11E
    F[0x4A528A].ret = 0x5E


@pytest.mark.parametrize("clear", [0, 1])
@pytest.mark.parametrize("keys", [b"", b"P", b"PR", b"I", b"RI", b"PRI"])
@pytest.mark.parametrize("player,handler", [(0x7101, 1), (0x7101, 0), (0, 1)])
def test_update_cheats(clear, keys, player, handler):
    def setup(e):
        reset_state(clear=clear)
        g = new_game(e, (player, 0x7102, 0, 0))
        e.write(g + 0xE1, b"\x01")
        if player:
            pl = new_task(e, 0x400, 0x66)
            e.write(e.u32(g + 0xC0), u32(pl))
        e.write(KEYS + 0x1D, b"\x80")
        e.write(KEYS + 0x2A, b"\x80")
        for k, code in ((b"P", 0x19), (b"R", 0x13), (b"I", 0x17)):
            e.write(KEYS + code, b"\x80" if k in keys else b"\x00")
        e.write(PROFILE + 0x560, u32(14))
        e.write(CHEATS, u32(1))
        e.write(WCOUNT, u32(3))
        for i, (wid, ammo) in enumerate([(5, 30), (7, 0), (9, -4)]):
            e.write(WTYPES + i * 0x6E8, u32(wid))
            e.write(WTYPES + i * 0x6E8 + 0x648, u32(ammo))
        set_handler(e, 5, 0x66, HANDLER if handler else 0)
    compare(emu, 0x4163E0, "Game_UpdateCheats", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("mode", [0, 1])
@pytest.mark.parametrize("unk48", [0, 0x7748])
@pytest.mark.parametrize("render", [0, 1])
def test_level_quit(mode, unk48, render):
    def setup(e):
        reset_state()
        F[0x4B0DB0].ret = mode
        g = new_game(e)
        e.write(g + 0x48, u32(unk48))
        e.write(RENDER_FUNCS + 4 * 4, u32(RENDER if render else 0))
        add_entries(e, SPRITES, 0x30, SPRITE_COUNT, [b"a", b"b"])
        add_entries(e, TEXTURES, 0x3C, TEXTURE_COUNT, [b"c"])
    compare(emu, 0x416550, "LevelQuit", setup=setup, check_eax=False, normalized=True)


def test_shutdown():
    def setup(e):
        e.write(EVENTS, bytes(range(0x60, 0x6F)))
        e.write(GAME_TYPE, struct.pack("<H", 0x99))
        e.write(LISTS, b"".join(u32(0x8800 + i) for i in range(6)))
    compare(emu, 0x4166C0, "Game_Close", setup=setup, check_eax=False, normalized=True)


GETTERS = [(0x4167D0, "Game_GetEvent0"), (0x4167E0, "Game_GetEvent1"), (0x4167F0, "Game_GetEvent2"),
           (0x416800, "Game_GetEvent4"), (0x416810, "Game_GetEvent3"), (0x416820, "Game_GetEvent6"),
           (0x416830, "Game_GetEvent7"), (0x416840, "Game_GetEvent9"), (0x416850, "Game_GetEvent8"),
           (0x416860, "Game_GetEvent11"), (0x416870, "Game_GetEvent12"), (0x416880, "Game_GetEvent13"),
           (0x416890, "Game_GetEvent14")]
LIST_GETTERS = [(0x4168A0, "Game_GetTaskList0"), (0x4168B0, "Game_GetTaskList1"), (0x4168C0, "Game_GetTaskList2"),
                (0x4168D0, "Game_GetTaskList3"), (0x4168E0, "Game_GetTaskList4"),
                (0x4168F0, "Game_GetCutsceneList"), (0x416920, "Game_GetWorld"), (0x416D20, "Game_GetRandom"),
                (0x416D30, "Game_GetRandom2"), (0x416D40, "Game_Get"), (0x416FB0, "Game_IsRaining"),
                (0x417090, "Mission_GetFirst")]


@pytest.mark.parametrize("addr,name", GETTERS)
def test_event_getters(addr, name):
    compare(emu, addr, name, setup=lambda e: e.write(EVENTS, bytes(range(0x81, 0x90))), eax_mask=0xFF)


@pytest.mark.parametrize("addr,name", LIST_GETTERS)
@pytest.mark.parametrize("rain", [0, 1, 0x80])
def test_getters(addr, name, rain):
    def setup(e):
        new_game(e)
        e.write(LISTS, b"".join(u32(0x8800 + i) for i in range(6)))
        e.write(RAINING, bytes([rain]))
    compare(emu, addr, name, setup=setup)


@pytest.mark.parametrize("addr,name,args", [(0x416D50, "Game_IsPaused", ()), (0x416D80, "PlayerXPHit", ()),
                                            (0x416D60, "Game_SetPaused", (0x101,)), (0x416FC0, "Game_CountHuman", (0,))])
def test_flags(addr, name, args):
    def setup(e):
        g = new_game(e)
        e.write(g + 0xE0, b"\x01\x00")
        e.write(g + 0xE4, u32(41))
    compare(emu, addr, name, args=args, setup=setup, eax_mask=0xFF, check_eax=not args)


def test_count_human():
    def setup(e):
        g = new_game(e)
        e.write(g + 0xE4, u32(0x12345))
    compare(emu, 0x416FC0, "Game_CountHuman", setup=setup)


def test_new():
    def setup(e):
        new_game(e)
        e.write(GAME_TYPE, struct.pack("<H", 0x99))
    compare(emu, 0x416900, "Game_New", args=(0x4400,), setup=setup, normalized=True)


@pytest.mark.parametrize("i", [0, 1, 2, 3])
def test_get_player(i):
    compare(emu, 0x416A70, "Game_GetPlayer", args=(i,), setup=lambda e: new_game(e) and None)


@pytest.mark.parametrize("item", [1, 2, 3, 4, 0])
def test_filters(item):
    def setup(e):
        ctx = e.alloc(16)
        e.write(ctx, u32(0x55) + u32(1) + u32(2) + u32(3))
        return item, ctx
    compare(emu, 0x416A90, "Game_CollFilterSkip3", setup=setup, check_eax=False)
    compare(emu, 0x416AB0, "Game_CollFilterSkip", setup=setup, check_eax=False)


def view_query(e, best=(1.0, 1.0), rng=5000.0):
    q = e.alloc(0x88)
    e.write(q + 8, struct.pack("<3d", 100.0, 200.0, 10.0))
    # a rotation of 30 degrees around z
    e.write(q + 0x20, struct.pack("<9f", 0.8660254, -0.5, 0.0, 0.5, 0.8660254, 0.0, 0.0, 0.0, 1.0))
    e.write(q + 0x70, struct.pack("<ff", *best) + struct.pack("<d", rng) + u32(0x4321))
    return q


POSITIONS = [(100, 1000, 10), (300, 600, 50), (-200, 900, -100), (100, 200, 10), (100, 150, 10),
             (900, 200, 10), (101, 6000, 10), (130, 1200, 900), (100, 1000.0000001, 10)]


@pytest.mark.parametrize("pos", POSITIONS)
@pytest.mark.parametrize("best", [(1.0, 1.0), (0.05, 3.0), (3.0, 0.05), (0.0, 0.0)])
def test_view_query(pos, best):
    def setup(e):
        p = e.alloc(24)
        e.write(p, struct.pack("<3d", *pos))
        return view_query(e, best), 0x9999, p
    compare(emu, 0x416AD0, "UseQuery_Consider", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("pos", [(0, 0, 0), (3, 4, 0), (30, 40, 0), (60, 80, 0), (100, 200, 300), (5, 0, 0)])
@pytest.mark.parametrize("radius", [0.0, 2.5, -10.0])
def test_falloff(pos, radius):
    def setup(e):
        s = e.alloc(0x14)
        p = e.alloc(24)
        e.write(p, struct.pack("<3d", 1.0, 2.0, 3.0))
        e.write(s, u32(0) + u32(p) + struct.pack("<3f", 0.75, 4.0, 60.0))
        q = e.alloc(24)
        e.write(q, struct.pack("<3d", *pos))
        return s, 0x77, q, f32(radius)
    compare(emu, 0x416C90, "Game_BlastFalloff", setup=setup, check_eax=False, check_st0=True)


def tree(e, type_):
    root = new_task(e, 0x20)
    a = new_task(e, 0x20, 0x31)
    b = new_task(e, 0x20, 0x32)
    c = new_task(e, 0x20, 0x31)
    d = new_task(e, 0x20, 0x33)
    for p, k in ((root, a), (root, b), (b, c), (b, d), (root, new_task(e, 0x20, 0x31))):
        add_child(e, p, k)
    F[0x4F8FD0].ret = type_
    return root


@pytest.mark.parametrize("player", [0, 0x7777])
@pytest.mark.parametrize("lightmaps", [0, 1])
def test_reset_state(player, lightmaps):
    def setup(e):
        reset_state()
        g = new_game(e)
        e.write(g + 0xD0, b"\x11" * 0x14)
        F[0x4F1030].ret = player
        F[0x48F250].ret = lightmaps
        F[0x4F0ED0].ret = tree(e, 0x31)
    compare(emu, 0x416D90, "Game_ResetState", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("type_", [0x31, 0x32, 0x33, 0x99])
def test_delete_tasks_of_type(type_):
    compare(emu, 0x416EF0, "Game_DeleteTasksOfType", setup=lambda e: (tree(e, type_),), check_eax=False,
            normalized=True)


@pytest.mark.parametrize("cam", [0, 1])
@pytest.mark.parametrize("ref", [0, 0x7754])
def test_clear_players(cam, ref):
    def setup(e):
        g = new_game(e, (0x7101, 0x7102, 0, 0x7104))
        e.write(e.u32(g + 0x54), u32(ref))
        if cam:
            c = e.alloc(0x500)
            e.write(c + 0x4E4, u32(0x4E4))
            F[0x487180].ret = c
        else:
            F[0x487180].ret = 0
    compare(emu, 0x416F40, "Game_ClearPlayers", setup=setup, check_eax=False, normalized=True)


def test_level_restart():
    def setup(e):
        new_game(e)
        c = e.alloc(0x500)
        e.write(c + 0x4E4, u32(0x4E4))
        F[0x487180].ret = c
        e.write(0x57B568, b"LOCAL:missions/location0/level1/\0")
    compare(emu, 0x416FE0, "LevelRestart", setup=setup, check_eax=False, normalized=True)
