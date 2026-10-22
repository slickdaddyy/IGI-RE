import random
import struct

import pytest

from emu import Emu
from test_aiaction_util import Fakes, check, check_error, cstr, f32, f64, new_list, set_game, u32

AI_PTR, EVENTS_PTR = 0x5BDA58, 0x5BDA60

e = Emu()
fk = Fakes(e)
ST = {}  # per test values returned by the fakes

fk.add(0x4B8A20, 2, ret=lambda em, a: ST["ints"][a[1]], fmt=lambda em, a: (a[1],))
fk.add(0x4B8A50, 2, ret=lambda em, a: ST["reals"][a[1]], st0="d", fmt=lambda em, a: (a[1],))


def _get_string(em, a):
    em.write(a[2], ST["strs"][a[1]][:a[3] - 1] + b"\0")
    return a[2]


fk.add(0x4B8A80, 4, ret=_get_string, fmt=lambda em, a: (a[1], a[2], a[3]))
fk.add(0x450E10, 3)   # HumanAI_AddAction
fk.add(0x451030, 1, ret=lambda em, a: ST["data"])
fk.add(0x450FF0, 2)   # HumanAI_RemoveAction
fk.add(0x450D80, 3)   # HumanAI_SendEvent
fk.add(0x401AE0, 1, ret=lambda em, a: a[0] + 0x100)   # TaskRef_Add
fk.add(0x4B85B0, 1)   # Script_Run
fk.add(0x4F9720, 2, ret=lambda em, a: ST["node"])
fk.add(0x4F1030, 1, ret=lambda em, a: ST["find"])
fk.add(0x440C00, 1, ret=lambda em, a: ST["status"])
fk.add(0x475370, 1, ret=lambda em, a: ST["status"])
fk.add(0x4AF810, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1] if b"%" in em.read_cstr(a[0]) else None))
fk.add(0x4C48C0, 0, ret=0x77)
fk.add(0x401CF0, 2, ret=lambda em, a: ST["isa"], fmt=lambda em, a: (a[0] & 0xFFFF, a[1] & 0xFFFF))

OUT = e.alloc(24)
ARGS = 0x5A5A0
DATA = e.alloc(0x40)
AI = e.alloc(0x3ED0)
NODE = e.alloc(0x40)
e.baseline = e.snapshot()
IGNORE = [(OUT + 4, OUT + 8), (OUT + 20, OUT + 24)]


def event(em, type_=5, pos=(1.0, 2.0, 3.0), source=None, target=None):
    n = em.alloc(0x30)
    em.write(n + 8, u32(type_) + u32(0) + f64(*pos))
    if source is not None:
        em.write(n + 0x28, u32(source))
    if target is not None:
        em.write(n + 0x2C, u32(target))
    return n


def ref_to(em, task):
    r = em.alloc(4)
    em.write(r, u32(task))
    return r


def base(em, seed=0, events=None, ints=(0,) * 8, reals=(0.0,) * 8, strs=(b"",) * 4):
    rng = random.Random(seed)
    em.write(AI_PTR, u32(AI))
    em.write(AI, bytes(rng.randrange(256) for _ in range(0x100)))
    em.write(EVENTS_PTR, u32(new_list(em, events if events is not None else [event(em)])))
    set_game(em, seed)
    ST.update(ints=list(ints), reals=list(reals), strs=list(strs), data=DATA, node=NODE, find=0, status=0, isa=0)
    return OUT, 0, ARGS


def run(name, addr, setup, cw=None):
    check(e, fk, addr, name, setup=setup, ignore=IGNORE, cw=cw)


ACTIONS = [
    ("AIAction_Patrol", 0x44D0E0, (5, -2, 1)),
    ("AIAction_Dead", 0x44D260, (3,)),
    ("AIAction_Activate", 0x44D420, (7, 0x1FF, 1)),
    ("AIAction_WalkToNode", 0x44D4A0, (9, 1)),
    ("AIAction_RunToNode", 0x44D510, (9, 0)),
    ("AIAction_PlayAnimation", 0x44D6E0, (40, 1)),
    ("AIAction_PlaySound", 0x44D750, (0, 0x101, 1)),
    ("AIAction_MoveToEvent", 0x44D7D0, (0x1AB, 1)),
    ("AIAction_Idle", 0x44DA80, (1,)),
    ("AIFunction_DefaultHandler", 0x44E060, ()),
    ("AIFunction_SetInvulnerability", 0x44E150, (0x301,)),
    ("AIFunction_SetDeathAnimation", 0x44E210, (12,)),
    ("AIFunction_SetAlarmTriggerID", 0x44E270, (3,)),
    ("AIFunction_SetAlarmControlID", 0x44E2D0, (4,)),
    ("AIFunction_SetAlarmAccess", 0x44E330, (1,)),
    ("AIFunction_SetGunnerID", 0x44E390, (6,)),
    ("AIFunction_SetScriptIntegerValue", 0x44E6D0, (3, -77)),
    ("AIFunction_GetScriptIntegerValue", 0x44E740, (2,)),
    ("AIFunction_GetAlarmTriggerID", 0x44E3F0, ()),
    ("AIFunction_GetAlarmControlID", 0x44E430, ()),
    ("AIFunction_GetAlarmAccess", 0x44E470, ()),
    ("AIFunction_GetGunnerID", 0x44E4B0, ()),
]


@pytest.mark.parametrize("name,addr,ints", ACTIONS)
def test_simple(name, addr, ints):
    def setup(em):
        r = base(em, 1, ints=ints, strs=[b"a_rather_long_sound_name_over_32_bytes"])
        em.write(AI + 0x3AC0, struct.pack("<4i", 11, -1, 12, 13))
        em.write(AI + 0x3AD4, struct.pack("<5i", 1, 2, 3, 4, 5))
        em.write(AI + 0x3D38, u32(9))
        return r
    run(name, addr, setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("name,addr,reals", [
    ("AIAction_LookAtEvent", 0x44D870, (1.2345678912,)),
    ("AIAction_Stunned", 0x44D8F0, (2.7182818284,)),
    ("AIAction_RunPanicking", 0x44DA00, (3.3333333333,)),
    ("AIAction_FireAtNode", 0x44D580, (0.0, 0.51666, 1.0499999)),
    ("AIAction_FireAtTask", 0x44D630, (0.0, 2.18333333, 3.5)),
    ("AIFunction_SetScriptRealValue", 0x44E790, (0, 1.2345678901234)),
    ("AIFunction_SetAlarmViewLength", 0x44DCD0, (1.1,)),
    ("AIFunction_SetSecondaryAlarmViewLength", 0x44DE80, (1.3,)),
    ("AIFunction_SetViewAlpha", 0x44DD30, (33.3,)),
    ("AIFunction_SetViewGamma", 0x44DD90, (47.1,)),
    ("AIFunction_SetSecondaryViewAlpha", 0x44DEE0, (12.7,)),
    ("AIFunction_SetSecondaryViewGamma", 0x44DF40, (-5.9,)),
])
def test_reals(name, addr, reals, cw):
    run(name, addr, lambda em: base(em, 2, ints=(4, 1, 2, 0x180, 1), reals=reals), cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("length", [0.5, 1.0, 2.0])
@pytest.mark.parametrize("name,addr,off", [("AIFunction_SetViewLength", 0x44DC40, 0xB0),
                                           ("AIFunction_SetSecondaryViewLength", 0x44DDF0, 0xD0)])
def test_view_length(name, addr, off, length, cw):
    def setup(em):
        r = base(em, 3, reals=(length,))
        em.write(AI + off + 8, f64(4096.0))
        return r
    run(name, addr, setup, cw=cw)


@pytest.mark.parametrize("ai_type", list(range(29)) + [-1, 29])
def test_combat(ai_type):
    def setup(em):
        r = base(em, 4, ints=(1,))
        em.write(AI + 0x3D38, u32(ai_type))
        return r
    run("AIAction_Combat", 0x44D160, setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", ["flag", "no_target", "dead_target", "near", "far", "nan"])
def test_fall_flat(case, cw):
    def setup(em):
        human = em.alloc(0x40)
        em.write(human + 0x20, f64(100.0, 200.0, 300.0))
        tgt = em.alloc(0x40)
        em.write(tgt + 0x20, f64(140.0, 230.0, 300.0) if case != "near" else f64(110.0, 205.0, 301.0))
        target = None if case == "no_target" else ref_to(em, 0 if case == "dead_target" else tgt)
        r = base(em, 5, events=[event(em, target=target or 0)], ints=(0, 1), reals=(1.75,))
        em.write(AI + 0x14, u32(human))
        em.write(AI + 0x3B6C, u32(0x1234))
        em.write(AI + 0x3E2C, u32(17))
        em.write(AI + 0x3DB8, f64(float("nan") if case == "nan" else 20.0))
        em.write(NODE + 0x30, u32(6))
        em.write(0xA7B2D8, u32(2 if case == "flag" else 9))
        return r
    run("AIAction_FallFlat", 0x44D2C0, setup, cw=cw)


@pytest.mark.parametrize("case", ["source", "nosource"])
def test_kick_grenade(case):
    def setup(em):
        t = em.alloc(0x40)
        return base(em, 6, events=[event(em, pos=(5.5, 6.5, 7.5), source=ref_to(em, t if case == "source" else 0))],
                    ints=(1,))
    run("AIAction_KickGrenade", 0x44D960, setup)


@pytest.mark.parametrize("case", ["none", "null_target_ref", "dead_target", "target", "both", "dead_source"])
def test_set_combat(case):
    def setup(em):
        a, b = em.alloc(0x40), em.alloc(0x40)
        if case == "none":
            return base(em, 7, events=[], ints=(1,))
        src = {"both": ref_to(em, a), "dead_source": ref_to(em, 0)}.get(case, 0)
        tgt = {"null_target_ref": 0, "dead_target": ref_to(em, 0)}.get(case, ref_to(em, b))
        return base(em, 7, events=[event(em, pos=(9.0, 8.0, 7.0), source=src, target=tgt)], ints=(1,))
    run("AIAction_SetCombat", 0x44DAE0, setup)


@pytest.mark.parametrize("name,addr", [("AIAction_MoveToEvent", 0x44D7D0), ("AIAction_LookAtEvent", 0x44D870),
                                       ("AIAction_RunPanicking", 0x44DA00)])
def test_event_pos_empty(name, addr):
    run(name, addr, lambda em: base(em, 8, events=[], ints=(1, 1), reals=(2.5,)))


@pytest.mark.parametrize("empty", [False, True])
def test_get_pos(empty):
    def setup(em):
        base(em, 9, events=[] if empty else [event(em, pos=(1.5, -2.5, 3.25))])
        return (DATA,)
    check(e, fk, 0x44D850, "AIEvent_GetPos", setup=setup, check_eax=False)


@pytest.mark.parametrize("empty", [False, True])
def test_get_current(empty):
    check(e, fk, 0x44C8C0, "AIEvent_GetCurrent", setup=lambda em: base(em, 9, events=[] if empty else None) and ())


def test_remove_alarm_actions():
    def setup(em):
        r = base(em, 10)
        nodes = []
        for tag in (3, 5, 3, 3, 7, 3):
            n = em.alloc(0x30)
            em.write(n + 8, u32(tag))
            nodes.append(n)
        lst = new_list(em, nodes)
        em.write(AI + 0x8FC, em.read(lst, 12))
        em.write(nodes[0] + 4, u32(AI + 0x8FC))
        em.write(nodes[-1], u32(AI + 0x900))
        em.write(AI + 0x83C, u32(3))
        return r
    run("AIFunction_RemoveAlarmActions", 0x44DBC0, setup)


def test_remove_alarm_actions_empty():
    def setup(em):
        r = base(em, 10)
        em.write(AI + 0x8FC, u32(AI + 0x900) + u32(0) + u32(AI + 0x8FC))
        return r
    run("AIFunction_RemoveAlarmActions", 0x44DBC0, setup)


@pytest.mark.parametrize("type_", [0, 7, 23])
def test_current_event_type(type_):
    run("AIFunction_GetCurrentEventType", 0x44DFA0, lambda em: base(em, 11, events=[event(em, type_)]))


def test_current_event_type_empty():
    check_error(e, fk, 0x44DFA0, "AIFunction_GetCurrentEventType", lambda em: base(em, 11, events=[]))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(6))
def test_random_value(seed, cw):
    run("AIFunction_GetRandomValue", 0x44E000, lambda em: base(em, seed, reals=(10.0 + seed * 3.7,)), cw=cw)


@pytest.mark.parametrize("ai_type", [0, 6, 7, 28])
def test_default_handler(ai_type):
    def setup(em):
        r = base(em, 12)
        em.write(AI + 0x3D38, u32(ai_type))
        em.write(0x58A9F4, b"".join(u32(0x7000 + i) for i in range(9)))
        return r
    run("AIFunction_DefaultHandler", 0x44E060, setup)


@pytest.mark.parametrize("used", [0, 1])
def test_event_priority(used):
    def setup(em):
        r = base(em, 13, ints=(5,))
        em.write(AI + 0x8DC, u32(3))
        em.write(AI + 0x8E4 + 5, bytes([used]))
        return r
    run("AIFunction_SetEventPriority", 0x44E0C0, setup)


@pytest.mark.parametrize("arg", [0, 1, 0x100, -1])
def test_instant_death(arg):
    run("AIFunction_SetInstantDeath", 0x44E1B0, lambda em: base(em, 14, ints=(arg,)))


@pytest.mark.parametrize("case", ["none", "missing", "off", "on", "neg"])
@pytest.mark.parametrize("name,addr,off", [("AIFunction_GetAlarmControlStatus", 0x44E4F0, 0x3AC4),
                                           ("AIFunction_GetGunnerStatus", 0x44E5E0, 0x3AC8)])
def test_status(name, addr, off, case):
    def setup(em):
        r = base(em, 15)
        em.write(AI + off, u32(-1 if case == "none" else 42))
        ST.update(find=0 if case == "missing" else em.alloc(0x40),
                  status={"off": 0, "on": 1, "neg": 0x12345680}.get(case, 0))
        return r
    run(name, addr, setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("v", [2.5, 3.5, -1.7, 1e9])
def test_get_script_real(v, cw):
    def setup(em):
        r = base(em, 16, ints=(1,))
        em.write(AI + 0x3AF0, f64(v))
        return r
    run("AIFunction_GetScriptRealValue", 0x44E800, setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
def test_event_behind(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        evs = [event(em, pos=tuple(rng.uniform(-1000, 1000) for _ in range(3))) for _ in range(seed % 4)]
        r = base(em, seed, events=evs)
        em.write(AI + 0x38, f64(*[rng.uniform(-1000, 1000) for _ in range(3)]))
        for off in (0x7C, 0x88, 0x94):
            em.write(AI + off, f32(rng.uniform(-1, 1)))
        return r
    run("AIFunction_IsEventBehind", 0x44E860, setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
def test_event_distance(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        r = base(em, seed, events=[event(em, pos=tuple(rng.uniform(-9000, 9000) for _ in range(3)))])
        em.write(AI + 0x38, f64(*[rng.uniform(-9000, 9000) for _ in range(3)]))
        return r
    run("AIFunction_GetEventDistance", 0x44E930, setup, cw=cw)


def test_event_distance_empty():
    check_error(e, fk, 0x44E930, "AIFunction_GetEventDistance", lambda em: base(em, 1, events=[]))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", ["none", "missing", "wrong_type", "ok"])
def test_alarm_trigger_distance(case, cw):
    def setup(em):
        r = base(em, 17)
        em.write(AI + 0x3AC0, u32(-1 if case == "none" else 8))
        em.write(AI + 0x20, f64(10.0, 20.0, 30.0))
        t = em.alloc(0x40)
        em.write(t + 0x1C, struct.pack("<H", 0x55))
        em.write(t + 0x20, f64(1234.5, -77.25, 3.0))
        ST.update(find=0 if case == "missing" else t, isa=1 if case == "ok" else 0)
        return r
    run("AIFunction_GetAlarmTriggerDistance", 0x44E9B0, setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(4))
def test_animation_interval(seed, cw):
    run("AIFunction_SetAnimationInterval", 0x44EAF0,
        lambda em: base(em, seed, reals=(1.5 + seed, 0.7 + seed * 2.3)), cw=cw)


@pytest.mark.parametrize("count", [0, 4, 5, 6])
def test_add_animation_entry(count):
    def setup(em):
        r = base(em, 18, ints=(33,), reals=(0.0, 0.25))
        em.write(AI + 0x3B64, u32(count))
        return r
    run("AIFunction_AddAnimationEntry", 0x44EBB0, setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("weights", [(), (1.0,), (0.2, 0.3, 0.5), (0.001, 0.002), (0.0, 0.0, 5.0), (3.0, 0.0, 1.0, 2.0, 4.0),
                                     (0.004, 0.003, 0.003)])
@pytest.mark.parametrize("seed", range(3))
def test_animation_to_play(weights, seed, cw):
    def setup(em):
        r = base(em, seed)
        for i, w in enumerate(weights):
            em.write(AI + 0x3B08 + 16 * i, u32(100 + i) + u32(0) + f64(w))
        em.write(AI + 0x3B64, u32(len(weights)))
        return r
    run("AIFunction_GetAnimationToPlay", 0x44ECB0, setup, cw=cw)


def test_send_response():
    def setup(em):
        t = em.alloc(0x40)
        return base(em, 19, events=[event(em, source=ref_to(em, t))])
    run("AIFunction_SendResponse", 0x44EE40, setup)


def test_send_response_empty():
    check_error(e, fk, 0x44EE40, "AIFunction_SendResponse", lambda em: base(em, 19, events=[]))


# --- registration ------------------------------------------------------------

r = Emu()
rf = Fakes(r)
NAME_FN = lambda em, a: (em.read_cstr(a[0]), em.normalize(a[1]), a[2], a[3])
for _a in (0x4B8890, 0x4B88C0, 0x4B88F0):
    rf.add(_a, 4, fmt=NAME_FN)
rf.add(0x4B8810, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))
rf.add(0x4018F0, 0, ret=0x21)
rf.add(0x401900, 4, ret=0x1C0, fmt=lambda em, a: (em.read_cstr(a[0]), a[1], a[2] & 0xFFFF, a[3]))
rf.add(0x401400, 3, fmt=lambda em, a: (a[0] & 0xFFFF, a[1] & 0xFF, em.normalize(a[2])))
rf.add(0x401530, 3, fmt=lambda em, a: (a[0] & 0xFFFF, a[1], em.normalize(a[2])))
for _a, _v in ((0x450C80, 0x31), (0x4F1A60, 0x32), (0x4F1A70, 0x33), (0x4F1210, 0x34), (0x4F1220, 0x35),
               (0x4F1950, 0x7A0000), (0x4F0FE0, 0x41), (0x4F0FA0, 0x42), (0x4F0F10, 0x43)):
    rf.add(_a, 0, ret=_v)
rf.add(0x4F1A80, 6, fmt=lambda em, a: (a[0], a[2] - a[1], a[3], a[4], a[5]))
rf.add(0x4B80B0, 1, ret=lambda em, a: a[0] ^ 0x1000000, fmt=lambda em, a: (em.read_cstr(a[0]),))


@pytest.mark.parametrize("addr,name", [(0x44CCA0, "HumanAI_RegisterFunctions"),
                                       (0x44EEB0, "HumanAI_RegisterConstants"),
                                       (0x44C8D0, "HumanAI_Init")])
def test_register(addr, name):
    check(r, rf, addr, name, check_eax=False)
