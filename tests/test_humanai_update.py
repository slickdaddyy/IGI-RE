"""HumanAI update, destroy, params and module shutdown (src/humanai/humanai.c)."""
import struct

import pytest

from emu import Emu, compare
from test_humanai import (AI_TYPE, HUMAN_TYPE, SCRIPT_GLOBALS, Fakes, add_event, add_tail, check, f32, f64,
                          fake_common, new_action, new_ai, new_human, new_list, p32, push_action, ref,
                          set_handler)

CTRL_SLOT, WORLD_SLOT, DAC_SLOT, EVENT_SLOT = 0x31, 0x32, 0x33, 0x30
WEAPON_TYPE = 0x80

U = Emu()
UF = Fakes(U)
fake_common(UF)
UF.add(0x4B4770, 1, ret=0.5, st0="f")
UF.add(0x416D20, 0, ret=0x1234)
UF.add(0x415020, 1, ret=1)
OFFSET = UF.code(f32(3.0, 4.0, 150.25))
UF.add(0x48A340, 3, ret=OFFSET, fmt=lambda e, a: tuple(a[1:]))
UF.add(0x465800, 2)
UF.add(0x465880, 2)
UF.add(0x465920, 3)
UF.add(0x4B85B0, 1)
UF.add(0x489750, 4, deref={1: 0x78})
UF.add(0x489BE0, 2, ret=0)
UF.add(0x489B90, 1)
UF.add(0x4F1030, 1, ret=0)
UF.add(0x440C00, 1, ret=0)
UF.add(0x440C10, 1, ret=0)
UF.add(0x4B47C0, 2, ret=6)
UF.add(0x453390, 1)
UF.add(0x452C10, 3)
UF.add(0x4028C0, 0, ret=100)
UF.add(0x416920, 0, ret=0x777)
UF.add(0x477BF0, 0, ret=WEAPON_TYPE)
UF.add(0x401CF0, 2, ret=lambda e, a: (a[0] & 0xFFFF) == WEAPON_TYPE)
UF.add(0x4F2070, 3)
UF.add(0x4B83D0, 1)
UF.add(0x413C80, 1, ret=0)
UF.add(0x4EE140, 1, ret=UF.code(b"\0" * 0x2D4 + p32(UF.code(f32(2.37)))))
CTRL = UF.handler(2, deref={1: 8})
WORLD = UF.handler(2, deref={1: 4})
DAC = UF.handler(1)
# message fields without the team and radius SendSimple leaves unset
SEND = UF.add(UF.code(b"\xc3"), 2, fmt=lambda e, a: (e.u32(a[1]), e.u32(a[1] + 8), e.u32(a[1] + 0x10),
                                                     e.u32(a[1] + 0x14), e.u32(a[1] + 0x18)))


def events(e):
    e.write(0x54E1DA, bytes([CTRL_SLOT]))
    e.write(0x548177, bytes([WORLD_SLOT]))
    e.write(0x54DDAC, p32(DAC_SLOT))
    e.write(0x53C922, bytes([EVENT_SLOT]))
    set_handler(e, CTRL_SLOT, HUMAN_TYPE, CTRL)
    set_handler(e, WORLD_SLOT, HUMAN_TYPE, WORLD)
    set_handler(e, DAC_SLOT, AI_TYPE, DAC)
    set_handler(e, EVENT_SLOT, AI_TYPE, SEND)


def seen(e, lst, task, new):
    n = e.alloc(0x30)
    e.write(n + 8, p32(ref(e, task)))
    e.write(n + 0x29, bytes([new]))
    add_tail(e, lst, n)


def ready_ai(e, dead=0):
    events(e)
    h = new_human(e, dead=dead)
    ai = new_ai(e, h)
    e.write(ai + 0x3C70, p32(0x5555))
    e.write(ai + 0x3B6C, p32(0x6666))
    e.write(ai + 0x81C, p32(4, 2, 22, 23))
    e.write(ai + 0x8DC, p32(4))
    new_list(e, ai + 0x1610)
    new_list(e, ai + 0x161C)
    return ai, h


@pytest.mark.parametrize("case", ["noscript", "nograph", "create"])
def test_update_start(case):
    def setup(e):
        ai, h = ready_ai(e)
        if case == "noscript":
            e.write(ai + 0x3C70, p32(0))
        if case == "nograph":
            e.write(ai + 0x3B6C, p32(0))
        return (ai, 0)
    check(U, UF, 0x44F410, "HumanAI_Update", setup=setup, ignore=SCRIPT_GLOBALS)


ALIVE = ["plain", "seen", "alarm_missing", "alarm_on", "alarm_last", "anim0", "anim3", "target_seen",
         "target_lost", "target_gone", "alarmed", "busy"]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", ALIVE)
def test_update_alive(case, cw):
    UF.rets[0x489BE0] = 0x4444 if case == "target_seen" else 0
    UF.rets[0x4F1030] = 0 if case == "alarm_missing" else 0x9999
    UF.rets[0x440C00] = case == "alarm_on"
    UF.rets[0x440C10] = case == "alarm_last"

    def setup(e):
        ai, h = ready_ai(e)
        e.write(ai + 0x3D14, b"\1")
        e.write(ai + 0xA0, f64(0.7, 0.8, 1000.0, 2000.0, 0.9, 1.1, 3000.0, 4000.0))
        e.write(ai + 0x3ABB, b"\1" if case == "alarmed" else b"\0")
        if case == "seen":
            enemy, friend, deadf = new_human(e), new_human(e), new_human(e, dead=1)
            seen(e, ai + 0x1610, enemy, 1)
            seen(e, ai + 0x1610, enemy, 0)
            seen(e, ai + 0x1610, new_human(e, dead=1), 1)
            seen(e, ai + 0x161C, friend, 1)
            seen(e, ai + 0x161C, deadf, 1)
            seen(e, ai + 0x161C, deadf, 0)
        if case.startswith("alarm_"):
            e.write(ai + 0x3AC4, p32(12))
        if case.startswith("anim"):
            e.write(ai + 0x3B58, p32(40, 9, int(case[4]), 1))
        if case.startswith("target"):
            t = 0x57D838
            e.write(t, b"\0" * 0x130)
            e.write(t + 8, b"\1")
            e.write(t + 0xC, p32(ref(e, 0 if case == "target_gone" else new_human(e))))
            e.write(t + 0x10, p32(h, 0x1111))
            e.write(t + 0x90, p32(3, 4))
            e.write(t + 0x128, p32(2))
            e.write(ai + 0x3AA8, p32(t))
        if case == "busy":
            push_action(e, ai, 2)
        return (ai, 0)
    check(U, UF, 0x44F410, "HumanAI_Update", setup=setup, ignore=SCRIPT_GLOBALS, cw=cw)


DEAD = ["first", "done", "end_frame", "end_reset", "end_parent", "end_count", "end_inactive"]


@pytest.mark.parametrize("case", DEAD)
def test_update_dead(case):
    def setup(e):
        ai, h = ready_ai(e, dead=1)
        e.write(ai + 0x3D14, b"\1")
        e.write(ai + 0x3ABA, b"\0" if case == "first" else b"\1")
        push_action(e, ai, 1, end=0)
        if case.startswith("end"):
            e.write(h + 0x33C, b"\1")
            e.write(ai + 0x3AB0, p32(0x123, 0x456))
            e.write(ai + 0x3EA0, p32(0x789, 4))
            e.write(ai + 0x3E10, p32(4 if case == "end_count" else 0xFFFFFFFF))
            e.write(h + 0x7C4, p32({"end_frame": 98, "end_reset": 0xFFFFFFFF}.get(case, 99)))
            if case == "end_parent":
                e.write(0x541094, struct.pack("<H", 0x70))
            if case == "end_inactive":
                e.write(h + 0xCF6, b"\0")
        return (ai, 0)
    check(U, UF, 0x44F410, "HumanAI_Update", setup=setup, ignore=SCRIPT_GLOBALS)


@pytest.mark.parametrize("kinds", [[], [None], [0], [2, 4], [3, 1, 4], [5]])
def test_find_weapons(kinds):
    def setup(e):
        ai, h = ready_ai(e)
        infos = {}
        prev = h + 8
        for i, k in enumerate(kinds):
            w = e.alloc(0x40)
            e.write(w + 0x1C, struct.pack("<H", WEAPON_TYPE if i != 1 else 0x81))
            e.write(prev, p32(w))
            prev = w
            if k is not None:
                info = e.alloc(0x700)
                e.write(info, p32(100 + i))
                e.write(info + 0x610, p32(k))
                infos[w] = info
        e.write(prev, p32(h + 0xC))
        UF.rets[0x413C80] = lambda em, a: infos.get(a[0], 0)
        return (ai,)
    check(U, UF, 0x44FBA0, "HumanAI_FindWeapons", setup=setup)


@pytest.mark.parametrize("case", ["plain", "noid"])
def test_destroy(case):
    def setup(e):
        ai, h = ready_ai(e)
        e.write(ai + 0x18, p32(0xFFFFFFFF if case == "noid" else 5))
        e.write(ai + 0x3E4C, p32(ref(e, h)))
        e.write(ai + 0x3E4A, b"\1")
        e.write(ai + 0x3EA0, p32(0x789, 4))
        e.write(ai + 0x3AB0, p32(0x123, 0x456))
        e.write(ai + 0x3A28, b"\1")
        add_event(e, ai, 4)
        push_action(e, ai, 3)
        new_list(e, 0x58A9C8)
        add_tail(e, ai + 0xE0, e.alloc(0x28))
        return (ai, 0)
    check(U, UF, 0x44FFC0, "HumanAI_Destroy", setup=setup)


P = Emu()
PF = Fakes(P)
fake_common(PF)
PF.add(0x4B8770, 1, ret=11)
PF.add(0x4B83D0, 1)
PF.add(0x4B80B0, 1, fmt=lambda e, a: (e.read_cstr(a[0]),), ret=0x5151)
PF.add(0x4F9290, 1, ret=0)
NEAR = PF.code(p32(17))
PF.add(0x5096F0, 3, ret=0, fmt=lambda e, a: tuple(a[1:]))
PF.add(0x452D20, 6, ret=0x8888)
PF.add(0x464250, 0, ret=0x3F)
PF.add(0x4896C0, 6)
PF.add(0x406F70, 1)


@pytest.mark.parametrize("case", ["noid", "nograph", "missing", "nonear", "samenode", "path"])
def test_params_changed(case):
    PF.rets[0x4F9290] = 0 if case == "missing" else 0x4242
    PF.rets[0x5096F0] = (lambda e, a: e.write(a[0], p32(0))) if case == "nonear" else \
        (lambda e, a: e.write(a[0], p32(NEAR)))

    def setup(e):
        ai = new_ai(e)
        e.write(ai + 0x3D15, b"AITYPE_X\0")
        e.write(ai + 0x3C70, p32(0x4343))
        e.write(ai + 0x18, p32(0xFFFFFFFF if case == "noid" else 21))
        e.write(ai + 0x3B68, p32(0xFFFFFFFF if case == "nograph" else 3))
        e.write(ai + 0x3E2C, p32(17 if case == "samenode" else 2))
        return (ai,)
    check(P, PF, 0x4505D0, "HumanAI_ParamsChanged", setup=setup)


SY = Emu()
SYF = Fakes(SY)
SYF.add(0x4F1270, 2, ret=lambda e, a: e.write(a[0], b"HumanAI_7\0"))
SYF.add(0x4B7EE0, 2, fmt=lambda e, a: (e.read_cstr(a[0]), e.normalize(a[1])))
SYF.add(0x4B80A0, 1, fmt=lambda e, a: (e.read_cstr(a[0]),))
SYF.add(0x4B8920, 1)
SYF.add(0x4C1830, 1)
SYF.add(0x401A20, 1)
SYF.add(0x4018C0, 1)
SYF.add(0x401B20, 1)
SYF.add(0x4B83D0, 1)


@pytest.mark.parametrize("added", [0, 1])
def test_symbols(added):
    def setup(e):
        ai = e.alloc(0x3ED0)
        e.write(ai + 0x3AB8, bytes([added]))
        return (ai,)
    check(SY, SYF, 0x450770, "HumanAI_AddSymbols", setup=setup)
    check(SY, SYF, 0x4507D0, "HumanAI_RemoveSymbols", setup=setup)


def test_unregister():
    check(SY, SYF, 0x4508E0, "HumanAI_UnregisterFunctions")
    check(SY, SYF, 0x450B20, "HumanAI_UnregisterConstants")


@pytest.mark.parametrize("slots", [b"\xff\xff\xff", b"\x20\xff\x22"])
def test_shutdown(slots):
    def setup(e):
        e.write(0x58A9D4, p32(0x3131))
        e.write(0x53C920, struct.pack("<H", 0x60) + slots)
        for i in range(8):
            e.write(0x57D838 + 0x130 * i + 8, bytes([i % 3 == 0]))
            e.write(0x57D838 + 0x130 * i + 0xC, p32(0x1000 + i))
        e.write(0x58A9F4, p32(*[0x2000 + i if i % 2 else 0 for i in range(9)]))
    check(SY, SYF, 0x450830, "HumanAI_Shutdown", setup=setup)
