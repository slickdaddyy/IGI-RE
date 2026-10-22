import struct

import pytest

from emu import Emu, ErrorShown
from test_menu_util import (LOG, RET, add_child, check, cstr, handler, logfake, set_handler, task, u16, u32)

emu = Emu()

MM, PTR, SCREEN, POPUP, ITEM, PLAIN, BGFX = 0x50, 0x51, 0x52, 0x53, 0x54, 0x56, 0x55
EV914, EV918, EV91C, DRAW, DA8, DAC = 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
WINDOW_TYPES = (SCREEN, POPUP, ITEM)
WIN = {}     # window id -> task, filled by the setups
KEYS = []    # menu key queue
POINTER = {}  # what sub_424E50 reports

MASK16 = 0xFFFF


def keys_count(e, args):
    return len(KEYS)


def keys_pop(e, args):
    return KEYS.pop(0)


def pointer(e, args):
    u32(e, args[1], POINTER.get("x", 0))
    u32(e, args[2], POINTER.get("y", 0))
    u32(e, args[3], POINTER.get("buttons", 0))
    u32(e, args[4], POINTER.get("prev", 0))


FAKES = {
    # address: (nargs, options)
    0x4A5350: (2, dict(strings=(0, 1))),  # fopen
    0x4A52A4: (3, {}),  # fseek
    0x4A528A: (1, {}),  # fgetc
    0x4A5234: (1, {}),  # fclose
    0x4015F0: (1, {}),  # Task_Delete
    0x4012A0: (3, dict(masks={1: MASK16}, ret=0x7777)),  # Task_New
    0x401BE0: (2, dict(masks={1: MASK16})),  # Task_FindChild
    0x401CF0: (2, dict(masks={0: MASK16, 1: MASK16},
                       effect=lambda e, a: 1 if a[0] & MASK16 in WINDOW_TYPES and a[1] & MASK16 == 0x40 else 0)),
    0x401D80: (3, dict(masks={1: 0xFF}, derefs={2: 1})),  # Task_Broadcast
    0x402820: (1, {}),
    0x402870: (0, dict(ret=30)),
    0x4028A0: (1, {}),
    0x406220: (0, {}),  # Profile_GetActive
    0x4062E0: (0, dict(ret=1)),  # Profile_GetLanguage
    0x406310: (0, dict(ret=2)),  # Config_GetLanguageCount
    0x416A70: (1, dict(ret=5)),
    0x416D50: (0, {}),
    0x416D60: (1, dict(masks={0: 0xFF})),
    0x417590: (2, {}),
    0x417660: (2, {}),
    0x424E40: (2, {}),
    0x424E50: (5, dict(effect=pointer, masks={3: 0, 4: 0})),  # stack pointers
    0x463170: (1, {}),
    0x489460: (0, {}),
    0x48A8A0: (0, {}),
    0x48A8C0: (0, {}),
    0x48A8D0: (0, dict(effect=keys_pop)),
    0x48A910: (0, dict(effect=keys_count)),
    0x491A30: (0, {}),
    0x491A90: (1, dict(derefs={0: 0x11C})),  # Display_SetMode
    0x491E70: (3, {}),
    0x4950E0: (0, dict(ret=3)),
    0x497410: (1, dict(ret=0x41, strings=(0,))),
    0x497450: (1, dict(ret=0x45, strings=(0,))),
    0x4974A0: (1, {}),
    0x4975F0: (1, {}),
    0x497620: (2, {}),
    0x4B5D80: (3, dict(strings=(0,))),  # Resource_ForEach
    0x4B5F00: (2, dict(strings=(0,), ret=0x5000)),  # Resource_Load
    0x4B63D0: (1, {}),  # Resource_Flush
    0x4B64E0: (1, dict(strings=(0,))),  # Resource_Find
    0x4B66A0: (1, {}),  # Sprite_Free
    0x4B6720: (2, dict(strings=(1,))),  # Sprite_Load
    0x4B6DB0: (1, dict(strings=(0,), ret=0x6000)),  # Text_Load
    0x4B6E30: (1, {}),  # Text_Delete
    0x4B80B0: (1, dict(strings=(0,), ret=0x1234)),  # Script_Load
    0x4B83D0: (1, {}),
    0x4B85B0: (1, {}),
    0x4B8920: (1, {}),  # Script_UnregisterSymbol
    0x4B8930: (2, {}),
    0x4B9120: (1, dict(ret=42)),  # Expr_EvalInt
    0x4E6750: (1, {}),
    0x4E67D0: (1, {}),
    0x4E6D60: (1, {}),
    0x4E7130: (0, {}),
    0x4E7180: (3, {}),  # Sound_Play
    0x4E7540: (2, dict(derefs={0: 0x11C})),
    0x4F0E50: (4, dict(strings=(0,))),  # ScriptInit
    0x4F1030: (1, dict(effect=lambda e, a: WIN.get(a[0], 0))),
    0x4F1050: (1, {}),
    0x4C1830: (1, {}),  # QTaskList_Free
    0x401A20: (1, dict(masks={0: MASK16})),  # TaskType_Free
    0x4C1780: (0, {}),
    0x416D40: (0, {}),  # Game_Get
    0x4B1020: (3, dict(strings=(1,), effect=lambda e, a: e.write(a[0], b"C:\\igi\\" + e.read_cstr(a[1]) + b"\0"))),
    0x4B11A0: (1, dict(strings=(0,))),  # Device_Find
    0x4F1110: (1, {}),
}
for addr, (n, kw) in FAKES.items():
    logfake(emu, addr, n, **kw)

LANG = [cstr(emu, b"english"), cstr(emu, b"german")]
logfake(emu, 0x406320, 1, effect=lambda e, a: LANG[a[0] % 2])  # Config_GetLanguageName
H = {}
for slot, name in ((1, "run"), (DRAW, "draw"), (EV914, "ev914"), (DAC, "dac")):
    for t in (MM, PTR, SCREEN, POPUP, ITEM, BGFX):
        H[name, t] = handler(emu, "%s_%x" % (name, t))
        set_handler(emu, slot, t, H[name, t])
u16(emu, 0x53B90C, 0x40)
u32(emu, 0x53B914, EV914, EV918, EV91C)
u32(emu, 0x5488C8, DRAW)
u32(emu, 0x54DDA8, DA8, DAC)
emu.write(0x54E0E4, bytes([0x30, 0x31]))
u32(emu, 0xC28B44, 800, 600)
emu.baseline = emu.snapshot()


def menu(e, **f):
    mm = task(e, 0x2898, MM)
    u32(e, mm + 0x27C8, task(e, 0x40, PTR))
    u32(e, mm + 0x26B8, 0xFFFFFFFF)
    for off, v in f.items():
        off = int(off[1:], 16)
        if isinstance(v, bytes):
            e.write(mm + off, v)
        else:
            u32(e, mm + off, v)
    return mm


def window(e, type_=ITEM, selectable=1, key=None):
    w = task(e, 0x54, type_)
    e.write(w + 0x4C, bytes([selectable]))
    if key is not None:
        WIN[key] = w
    return w


def run(orig, ours, setup, **kw):
    KEYS.clear()
    WIN.clear()
    RET.clear()
    return check(emu, orig, ours, setup=setup, normalized=True, **kw)


def set_ret(**r):
    """setup helper: RET values by address"""
    for k, v in r.items():
        RET[int(k[1:], 16)] = v


@pytest.mark.parametrize("owns,flag26be,frames,index,dac", [
    (0, 0, -1, -1, 0), (1, 1, 30, 2, 1), (1, 0, 0, 0, 0), (0, 1, -1, 5, 1)])
def test_delete(owns, flag26be, frames, index, dac):
    def setup(e):
        RET[0x4F1050] = index
        mm = menu(e, o26B8=frames)
        e.write(mm + 0x282C, bytes([owns]))
        e.write(mm + 0x26BE, bytes([flag26be]))
        u32(e, 0xBCADF0, 11, 22, 33)
        if not dac:
            set_handler(e, DAC, MM, 0)
        return (mm,)
    run(0x417A20, "MenuManager_Delete", setup, check_eax=False)


def resources(e, mm, sprites=2, pics=3, fonts=1, strings=2, sounds=1, lang=2):
    e.call(0x4B0BA0)
    u16(e, mm + 0x2664, sprites, fonts, pics)
    for i in range(pics):
        u32(e, mm + 0x1DE4 + 0x44 * i + 0x40, 0x100 + i)
    prev = 0
    for i in range(strings):
        s = e.call(0x4B0C60, 0x88, 4)[0]
        u32(e, s + 0x84, prev)
        prev = s
    u32(e, mm + 0x260, prev)
    e.write(mm + 0x26BF, bytes([sounds]))
    u32(e, mm + 0x1D4, 0x200 if sounds else 0, 0x300)
    u32(e, mm + 0x25C, lang)
    for i in range(lang):
        u32(e, mm + 0x1DC + 4 * i, 0x400 + i)


@pytest.mark.parametrize("counts", [(0, 0, 0, 0, 0, 0), (2, 3, 1, 2, 1, 2), (5, 0, 4, 1, 0, 1)])
def test_free_resources(counts):
    def setup(e):
        mm = menu(e)
        resources(e, mm, *counts)
        return (mm,)
    run(0x417B80, "MenuManager_FreeResources", setup, check_eax=False)


@pytest.mark.parametrize("reload", [None, b"", b"\x01"])
@pytest.mark.parametrize("path", [b"LOCAL:menusystem/menu.res", b"nodir.res"])
def test_update(reload, path):
    def setup(e):
        mm = menu(e, o20=7)
        resources(e, mm)
        WIN[7] = window(e, SCREEN)
        e.write(mm + 0xD4, path + b"\0")
        e.write(mm + 0x7E4 + 0x58, b"POINTER.SPR\0")
        return mm, cstr(e, reload) if reload is not None else 0
    run(0x417CF0, "MenuManager_Update", setup, check_eax=False)


@pytest.mark.parametrize("name", [b"LOCAL:menusystem/sprites/arrow.spr", b"menusystem:pic/back.PIC", b"a\\b\\font1.fnt",
                                  b"sounds/x.snd", b"readme.txt", b"x.spr"])
def test_add_resource(name):
    def setup(e):
        mm = menu(e)
        u16(e, mm + 0x2664, 3, 2, 1)
        info = e.alloc(8)
        u32(e, info, cstr(e, name))
        return info, mm
    run(0x417E20, "MenuManager_AddResource", setup, check_eax=False)


@pytest.mark.parametrize("count", [0, 1, 3])
def test_load_language_res(count):
    def setup(e):
        RET[0x406310] = count
        return menu(e, o25C=2), cstr(e, b"menusystem.res")
    run(0x417FF0, "MenuManager_LoadLanguageRes", setup, check_eax=False)


def test_get_params():
    def setup(e):
        u32(e, 0x57BAE0, 0xABCD)
        return 0, e.alloc(4)
    run(0x418070, "MenuManager_GetParams", setup, check_eax=False)


def screen_with_items(e, mm, n=3, key=None, type_=SCREEN):
    s = window(e, type_, 0, key)
    group = window(e, PLAIN, 0)
    add_child(e, s, group)
    for i in range(n):
        add_child(e, group if i % 2 else s, window(e, ITEM, 1))
    add_child(e, s, window(e, ITEM, 0))
    return s


RUN_CASES = [
    dict(state=0), dict(state=0, lang=2), dict(state=1), dict(state=0, noscreen=1),
    dict(state=2), dict(state=2, popup=1), dict(state=2, keys=[-4, -4, -3, -11, -8, -7, -1, -2, -5]),
    dict(state=2, keys=[-3], popup=1, horizontal=1), dict(state=2, keys=[-1, -4], popup=1, horizontal=1),
    dict(state=3), dict(state=4), dict(state=5, fade=4), dict(state=5, fade=16, shown=16), dict(state=7, fade=3, fast=1),
    dict(state=6, fade=8), dict(state=6, fade=0, shown=0), dict(state=8, fade=5, fast=1),
    dict(state=9), dict(state=9, esc=1), dict(state=9, esc=1, d50=1), dict(state=9, d50=1),
    dict(state=9, demo=1, timer=150), dict(state=9, demo=1, timer=151), dict(state=9, cfg=1, space=1),
    dict(state=9, cfg=1, enter=1, d50=1), dict(state=9, cfg=1, demo=1, timer=200),
    dict(state=10), dict(state=11), dict(state=12), dict(state=13), dict(state=14), dict(state=14, nopopup=1),
    dict(state=15), dict(state=16), dict(state=17), dict(state=18), dict(state=18, popup=1),
    dict(state=2, idle=1), dict(state=2, idle=1, same=1), dict(state=2, idle=1, same=1, timer=3601),
    dict(state=2, idle=1, same=1, timer=3600), dict(state=2, idle=1, same=1, busy=1),
    dict(state=2, idle=1, same=1, depth=1), dict(state=2, idle=1, same=1, keys=[-5]),
    dict(state=3, owns=1, sounds=1, bgfx=1), dict(state=3, owns=1, sounds=0, bgfx=1),
]


@pytest.mark.parametrize("c", RUN_CASES)
def test_run(c):
    def setup(e):
        RET[0x4062E0] = c.get("lang", 1)
        RET[0x416D50] = c.get("d50", 0)
        RET[0x489460] = c.get("demo", 0)
        RET[0x463170] = c.get("cfg", 0)
        KEYS[:] = c.get("keys", [])
        POINTER.update(x=100, y=50, buttons=c.get("buttons", 1), prev=0)
        mm = menu(e, o20=7, o24=8, o28=c["state"], o2C=4, oCC=c.get("fade", 0), oC8=c.get("shown", 3),
                  oC4=5, o27E4=1)
        e.write(mm + 0x26C1, bytes([c.get("fast", 0), 1, c.get("idle", 0), c.get("busy", 0)]))
        e.write(mm + 0x282C, bytes([c.get("owns", 0)]))
        e.write(mm + 0x26BF, bytes([c.get("sounds", 0)]))
        u32(e, mm + 0x2838, 1)
        u32(e, mm + 0x27E8, c.get("depth", 0))
        if c.get("bgfx"):
            u32(e, mm + 0x27E0, task(e, 0x24, BGFX))
        if not c.get("noscreen"):
            s = screen_with_items(e, mm, key=7)
            u32(e, mm + 0x30, s)
        if not c.get("nopopup"):
            p = screen_with_items(e, mm, 2, key=8, type_=POPUP)
            e.write(p + 0x59, bytes([c.get("horizontal", 0)]))
            if c.get("popup"):
                u32(e, mm + 0x34, p)
                e.call(0x4187F0, mm, p)
            elif e.u32(mm + 0x30):
                e.call(0x4187F0, mm, e.u32(mm + 0x30))
        u32(e, 0x57BAE8, c.get("timer", 0))
        u32(e, 0x57BADC, c.get("timer", 0))
        if c.get("same"):
            e.write(0x57BAC8, e.read(mm + 0x27CC, 0x14))
            u32(e, 0x57BAEC, e.u32(mm + 0x38))
            u32(e, 0x57BAE4, e.u32(mm + 0x30))
        for k, name in ((1, "esc"), (0x39, "space"), (0x1C, "enter")):
            e.write(0xC28C62 + k, bytes([0x80 if c.get(name) else 0]))
        return (mm,)
    run(0x418080, "MenuManager_Run", setup, check_eax=False)


@pytest.mark.parametrize("started", [0, 1])
def test_play_music(started):
    def setup(e):
        mm = menu(e)
        u32(e, mm + 0x2838, started)
        return (mm,)
    run(0x418520, "MenuManager_PlayMusic", setup, check_eax=False)


@pytest.mark.parametrize("buttons", [0, 0x100, 0x200, 0x300])
@pytest.mark.parametrize("enabled", [0, 1])
def test_read_input(buttons, enabled):
    def setup(e):
        KEYS[:] = [-3, -3, -4, -11, -8, -7, -1, -2, -9, 5]
        mm = menu(e)
        s = screen_with_items(e, mm, 4, key=7)
        u32(e, mm + 0x30, s)
        e.call(0x4187F0, mm, s)
        e.write(mm + 0x26C2, bytes([enabled]))
        u32(e, 0xC28F8C, buttons)
        return (mm,)
    run(0x418580, "MenuManager_ReadInput", setup, check_eax=False)


@pytest.mark.parametrize("old,new", [(0, 0), (1, 2), (1, 5)])
def test_set_focus(old, new):
    def setup(e):
        mm = menu(e)
        items = [window(e) for _ in range(3)] + [0, 0, 0]
        u32(e, mm + 0x3C, *items)
        if old:
            u32(e, mm + 0x38, items[old])
        return mm, new
    run(0x4186E0, "MenuManager_SetFocus", setup, check_eax=False)


@pytest.mark.parametrize("found,check_lang,lang", [(0, 1, 1), (1, 0, 1), (1, 1, 1), (1, 1, 2)])
def test_open_screen(found, check_lang, lang):
    def setup(e):
        RET[0x4062E0] = lang
        mm = menu(e, o27E4=1)
        if found:
            screen_with_items(e, mm, key=9)
        return mm, 9, check_lang
    run(0x418710, "MenuManager_OpenScreen", setup, check_eax=False)


@pytest.mark.parametrize("has_handler", [0, 1])
@pytest.mark.parametrize("lang", [1, 3])
def test_check_language(has_handler, lang):
    def setup(e):
        RET[0x4062E0] = lang
        mm = menu(e, o27E4=1)
        u32(e, mm + 0x30, screen_with_items(e, mm))
        if not has_handler:
            set_handler(e, EV914, SCREEN, 0)
        return (mm,)
    run(0x418790, "MenuManager_CheckLanguage", setup, check_eax=False)


@pytest.mark.parametrize("n,screen", [(0, 1), (1, 1), (5, 1), (3, 0)])
def test_collect_items(n, screen):
    def setup(e):
        mm = menu(e)
        s = screen_with_items(e, mm, n)
        if screen:
            u32(e, mm + 0x30, s)
        return mm, s
    run(0x4187F0, "MenuManager_CollectItems", setup, check_eax=False)


@pytest.mark.parametrize("found,has_handler", [(0, 1), (1, 1), (1, 0)])
def test_open_popup(found, has_handler):
    def setup(e):
        mm = menu(e, o24=8)
        if found:
            screen_with_items(e, mm, 2, key=8, type_=POPUP)
        if not has_handler:
            set_handler(e, EV914, POPUP, 0)
        return (mm,)
    run(0x4188B0, "MenuManager_OpenPopup", setup, check_eax=False)


@pytest.mark.parametrize("popup,screen", [(0, 1), (1, 0), (1, 1)])
def test_close_popup(popup, screen):
    def setup(e):
        mm = menu(e)
        if popup:
            u32(e, mm + 0x34, screen_with_items(e, mm, 1, type_=POPUP))
        if screen:
            u32(e, mm + 0x30, screen_with_items(e, mm, 3))
        return (mm,)
    run(0x418940, "MenuManager_ClosePopup", setup, check_eax=False)


@pytest.mark.parametrize("state", [2, 3, 7, 8, 9, 15])
@pytest.mark.parametrize("screen,popup", [(0, 0), (1, 0), (1, 1)])
def test_draw(state, screen, popup):
    def setup(e):
        mm = menu(e, o28=state, oCC=6, oC4=4, oC8=2)
        if screen:
            u32(e, mm + 0x30, window(e, SCREEN))
        if popup:
            u32(e, mm + 0x34, window(e, POPUP))
        return (mm,)
    run(0x418970, "MenuManager_Draw", setup, check_eax=False)


@pytest.mark.parametrize("orig,ours", [(0x418A80, "MenuManager_Activate"), (0x418AA0, "MenuManager_Deactivate")])
@pytest.mark.parametrize("active", [0, 1])
def test_activate(orig, ours, active):
    def setup(e):
        mm = menu(e)
        e.write(mm + 0x26BD, bytes([active]))
        return (mm,)
    run(orig, ours, setup, check_eax=False)


def test_shutdown():
    def setup(e):
        u32(e, 0x57BAE0, 0x1357)
        u16(e, 0x539824, 0x66)
    run(0x418AC0, "MenuManager_Shutdown", setup, check_eax=False)


def test_get_type():
    run(0x418AF0, "MenuManager_GetType", lambda e: u16(e, 0x539824, 0x66), eax_mask=0xFFFF)


NEW_CASES = [
    dict(owns=0, start=1, idle=0), dict(owns=1, start=0, idle=1), dict(owns=1, start=1, idle=0, keep=1),
    dict(owns=0, start=0, idle=1, nofile=1), dict(owns=0, start=1, idle=0, byte=0x41),
    dict(owns=0, start=1, idle=0, noscript=1),
]


@pytest.mark.parametrize("c", NEW_CASES)
def test_new(c):
    def setup(e):
        parent = task(e, 0x20, PLAIN)
        mm = menu(e, o20=5)
        profile = e.alloc(0x300)
        e.write(profile + 0x1F, b"Direct3D HAL\0")
        e.write(profile + 0x11F, b"Primary Display Driver\0")
        RET[0x406220] = profile
        RET[0x401BE0] = mm
        RET[0x4B80B0] = 0 if c.get("noscript") else 0x1234
        RET[0x4A5350] = 0 if c.get("nofile") else 0x2468
        RET[0x4A528A] = c.get("byte", 0x40)
        u32(e, 0x539828, c.get("keep", 0))
        return parent, cstr(e, b"LOCAL:menusystem/mainmenu.qsc"), c["owns"], c["start"], c["idle"]
    if c.get("nofile") or c.get("noscript") or c.get("byte"):
        run_error(0x418B00, "MenuManager_New", setup)
    else:
        run(0x418B00, "MenuManager_New", setup)


def run_error(orig, ours, setup):
    """Both versions end in Error_Show with the same message after the same calls."""
    seen = []
    for target in (orig, ours):
        emu.reset()
        KEYS.clear()
        WIN.clear()
        RET.clear()
        LOG.append([])
        args = setup(emu)
        LOG.append([])
        with pytest.raises(ErrorShown) as ex:
            emu.call(target, *args)
        seen.append((str(ex.value), LOG[-1]))
    assert seen[0] == seen[1]


def test_free():
    run(0x418D70, "MenuManager_Free", lambda e: (0x4321,), check_eax=False)


def named(e, base, stride, names, value_off=None):
    for i, n in enumerate(names):
        e.write(base + stride * i, n + b"\0")
        if value_off is not None:
            u32(e, base + stride * i + value_off, 0x700 + i)


NAMES = [b"pointer.spr", b"Back.spr", b"font1.fnt", b"x"]


@pytest.mark.parametrize("name", [b"POINTER.SPR", b"back.spr", b"x", b"missing", b""])
@pytest.mark.parametrize("which", [(0x418D80, "MenuManager_FindPic", 0x1DE4, 0x44, 0x2668),
                                   (0x418DE0, "MenuManager_FindSprite", 0x7E4, 0x58, 0x2664),
                                   (0x418E40, "MenuManager_FindFont", 0x264, 0x58, 0x2666)])
def test_find(name, which):
    orig, ours, base, stride, count = which

    def setup(e):
        mm = menu(e)
        named(e, mm + base, stride, NAMES, 0x40)
        u16(e, mm + count, 3)
        return mm, cstr(e, name)
    run(orig, ours, setup)


@pytest.mark.parametrize("found,known", [(1, 0), (0, 0), (0, 1), (0, 2)])
def test_get_string(found, known):
    def setup(e):
        e.call(0x4B0BA0)
        RET[0x4B64E0] = found
        mm = menu(e, o27E4=1)
        for i, n in enumerate([b"LOCAL:german/other", b"LOCAL:GERMAN/Title_Main"][:known]):
            s = e.call(0x4B0C60, 0x88, 4)[0]
            e.write(s, n + b"\0")
            u32(e, s + 0x84, e.u32(mm + 0x260))
            u32(e, mm + 0x260, s)
        return mm, cstr(e, b"title_main")
    run(0x418EA0, "MenuManager_GetString", setup)


@pytest.mark.parametrize("orig,ours,check_eax", [
    (0x418FB0, "MenuManager_GetInput", True), (0x418FC0, "MenuManager_Pause", False),
    (0x418FD0, "MenuManager_Resume", False), (0x419030, "MenuManager_GetFade", True),
    (0x419040, "MenuManager_GetBackgroundFX", True), (0x419050, "MenuManager_Reset", False),
    (0x4190B0, "MenuManager_DisableInput", False), (0x4190C0, "MenuManager_EnableInput", False),
    (0x4190D0, "MenuManager_MarkBusy", False)])
def test_small(orig, ours, check_eax):
    def setup(e):
        mm = menu(e, o28=6, o20=3, oD0=9, o27E0=0x5555)
        e.write(mm + 0x26C2, b"\x07\x00\x00")
        return (mm,)
    run(orig, ours, setup, check_eax=check_eax)


@pytest.mark.parametrize("item,index", [(0, 0), (1, 0), (1, 1), (2, 1), (9, 0)])
def test_focus_item(item, index):
    def setup(e):
        mm = menu(e, oBC=3)
        items = [window(e) for _ in range(3)]
        u32(e, mm + 0x3C, *items)
        e.call(0x4186E0, mm, index)
        return mm, items[item] if item < 3 else 0x9999
    run(0x418FE0, "MenuManager_FocusItem", setup, check_eax=False)


@pytest.mark.parametrize("expr", [0, 0x3456])
def test_expr_eval(expr):
    def setup(e):
        p = e.alloc(12)
        u32(e, p + 8, expr)
        return (p,)
    run(0x419070, "MenuExpr_Eval", setup)


@pytest.mark.parametrize("screen", [0, 0x8888])
def test_refresh_screen(screen):
    run(0x419090, "MenuManager_RefreshScreen", lambda e: (menu(e, o30=screen),), check_eax=False)


def test_loader_create():
    def setup(e):
        ml = task(e, 0x124)
        e.write(ml + 0x20, b"\x55" * 0x104)
        return (ml,)
    run(0x4191F0, "MenuLoader_Create", setup, check_eax=False)


def test_loader_get_params():
    def setup(e):
        u32(e, 0x57BAF4, 0xABCD)
        return 0, e.alloc(4)
    run(0x4192A0, "MenuLoader_GetParams", setup, check_eax=False)


@pytest.mark.parametrize("load,unload,device,found", [
    (0, 0, 1, 1), (1, 0, 0, 0), (1, 0, 1, 0), (0, 1, 0, 0), (0, 1, 0, 1), (1, 1, 1, 1)])
def test_loader_changed(load, unload, device, found):
    def setup(e):
        game = e.alloc(0x100)
        root = task(e, 0x20, PLAIN)
        u32(e, game + 0x44, root)
        RET[0x416D40] = game
        RET[0x4B11A0] = device
        mm = menu(e)
        RET[0x401BE0] = mm if found or device else 0
        RET[0x4A5350] = 0x2468
        RET[0x4A528A] = 0x40
        ml = task(e, 0x124)
        e.write(ml + 0x20, b"menusystem/ingame.qsc\0")
        e.write(ml + 0x120, bytes([load, unload]))
        return (ml,)
    run(0x419210, "MenuLoader_Changed", setup, check_eax=False)


@pytest.mark.parametrize("path", [b"LOCAL:menusystem/menu.res", b"nodir.res"])
def test_load_resources(path):
    def setup(e):
        mm = menu(e)
        resources(e, mm)
        e.write(mm + 0xD4, path + b"\0")
        return (mm,)
    run(0x417D50, "MenuManager_LoadResources", setup, check_eax=False)


@pytest.mark.parametrize("n", [0, 1, 4])
@pytest.mark.parametrize("root_type", [SCREEN, PLAIN])
def test_add_items(n, root_type):
    def setup(e):
        mm = menu(e, oBC=1)
        s = screen_with_items(e, mm, n, type_=root_type)
        e.write(s + 0x4C, b"\x01")
        return mm, s
    run(0x418840, "MenuManager_AddItems", setup, check_eax=False)
