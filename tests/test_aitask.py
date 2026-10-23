"""AI action handlers and combat moves (src/aitask/aitask.c)."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import Emu, compare

POSE_MAP = 0x541658
CHANNEL_MAP = 0x54151C


class Fakes:
    """Python replacements of original functions that log their arguments."""

    def __init__(self, e):
        self.e = e
        self.log = []
        self.tramps = {}

    def code(self, data):
        p = self.e.fake_top
        self.e.fake_top += (len(data) + 15) & ~15
        self.e.write(p, data)
        return p

    def add(self, addr, nargs=0, ret=0, st0=False, fmt=None):
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code(b"\xdd\x05" + struct.pack("<I", cell) + b"\xc3") if st0 else None

        def cb(uc, a, size, user):
            em = self.e
            args = [em.arg(i) for i in range(nargs)]
            rec = fmt(em, args) if fmt else tuple(em.normalize(v) for v in args)
            self.log.append((a, rec))
            r = ret(em, args) if callable(ret) else ret
            if st0:
                em.write(cell, struct.pack("<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r or 0)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)

    def handler(self, nargs, ret=0, fmt=None):
        p = self.code(b"\xc3")
        self.add(p, nargs, ret=ret, fmt=fmt)
        return p


def with_cw(e, fk, target, cw):
    addr = e.symbols[target] if isinstance(target, str) else target
    key = (addr, cw)
    if key not in fk.tramps:
        cell = fk.code(struct.pack("<H", cw))
        p = fk.code(b"\0" * 16)
        e.write(p, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        fk.tramps[key] = p
    return fk.tramps[key]


def p32(*v):
    return struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v])


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def printf(e, a):
    text = e.read_cstr(a[0])
    out = [text]
    for i in range(text.count(b"%")):
        v = a[1 + i]
        out.append(e.read_cstr(v) if b"%s" in text and i == 1 else v)
    return tuple(out)


E = Emu()
F = Fakes(E)
S = {}  # what the fakes return, set by each test


def st(key, default=0):
    return lambda e, a: S.get(key, default)


def write_ammo(e, a):
    e.write(a[1], p32(S.get("ammo", 0), 0x55))


def nearest(e, a):
    e.write(a[0], p32(S.get("nearest", 0)))


def node_pos(e, a):
    e.write(a[0], f64(*S.get("nodepos", {}).get(a[1], (0.0, 0.0, 0.0))))


def target_pos(e, a):
    e.write(a[0], f64(*S.get("tpos", (1.0, 2.0, 3.0))))


F.add(0x4C1780, 6, fmt=lambda e, a: printf(e, a[2:]) + (a[0], a[1]))
F.add(0x4AF810, 2, fmt=printf)
F.add(0x4A53B3, 5, fmt=lambda e, a: printf(e, a[1:]))
F.add(0x401B20, 1)
F.add(0x401CF0, 2, ret=st("isa"), fmt=lambda e, a: (a[0] & 0xFFFF, a[1] & 0xFFFF))
F.add(0x4028B0, 0, ret=st("ticks"))
F.add(0x413C80, 1, ret=st("wtype"))
F.add(0x416D20, 0, ret=0x1234)
F.add(0x4B4770, 1, ret=st("rand", 0.5), st0=True)
F.add(0x4F1030, 1, ret=lambda e, a: S.get("tasks", {}).get(a[0], 0))
F.add(0x4F1050, 1, ret=77)
F.add(0x4F0EE0, 0, ret=0x5555)
F.add(0x4F23D0, 2, ret=write_ammo, fmt=lambda e, a: (a[0],))
F.add(0x4F2350, 2, fmt=lambda e, a: (a[0], a[1] & 0xFF))
F.add(0x4E6B00, 2, ret=st("sound"), fmt=lambda e, a: (a[0], e.read_cstr(a[1])))
F.add(0x4E6C30, 2)
F.add(0x4F9720, 2, ret=lambda e, a: S.get("nodes", {}).get(a[1], 0))
F.add(0x4F9770, 2, ret=node_pos, fmt=lambda e, a: (a[1],))
F.add(0x5096F0, 3, ret=nearest, fmt=lambda e, a: (a[1], e.read(a[2], 24)))
F.add(0x4C48C0, 0, ret=0x42)
F.add(0x463130, 1, ret=3)
F.add(0x475310, 3)
F.add(0x450D50, 2, fmt=lambda e, a: (a[0], e.read(a[1], 0x1C)))
F.add(0x450FF0, 2)
F.add(0x4511F0, 3)
F.add(0x4512A0, 3, fmt=lambda e, a: (a[0], a[1], a[2] & 0xFF))
F.add(0x451380, 2, ret=target_pos, fmt=lambda e, a: (a[1],))
F.add(0x4513F0, 1)
F.add(0x4522A0, 4, ret=st("run"), fmt=lambda e, a: (a[0], a[1], a[2], a[3] & 0xFF))
F.add(0x452580, 4, ret=st("face"), fmt=lambda e, a: (a[0], e.read(a[1], 24), a[2], a[3] & 0xFF))
F.add(0x4525B0, 4, ret=st("face"), fmt=lambda e, a: (a[0], e.read(a[1], 24), a[2], a[3] & 0xFF))
F.add(0x4525E0, 1)
F.add(0x452660, 1)
F.add(0x4532D0, 1, ret=lambda e, a: a[0] in S.get("claimed", ()))
F.add(0x453300, 2, ret=lambda e, a: 0x9000 + (a[0] & 0xFFF), fmt=lambda e, a: (a[0], a[1] & 0xFF))
F.add(0x453390, 1)
F.add(0x453540, 1, ret=st("cmdtype"))
F.add(0x485320, 1, ret=st("cmdarg"))
F.add(0x453720, 1, ret=st("cmd"))
F.add(0x453760, 1)
F.add(0x4537E0, 3)
F.add(0x48B260, 4, ret=lambda e, a: a[1] in S.get("good", ()), fmt=lambda e, a: (a[0], a[1], a[2] & 0xFF, a[3] & 0xFF))
F.add(0x48B9C0, 2, fmt=lambda e, a: (a[0], a[1] & 0xFF))
SEARCH_FN = F.handler(3, ret=lambda e, a: e.u32(a[1]) == S.get("hit"))


def check(orig, ours, setup=None, state=None, cw=None, **kw):
    """compare() plus equal logs of the Python fakes."""
    logs = []
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(E, F, orig, cw), with_cw(E, F, ours, cw)

    def s(em):
        S.clear()
        S.update(state or {})
        r = setup(em) if setup else None
        F.log = []
        logs.append(F.log)
        return r
    res = compare(E, orig, ours, setup=s, normalized=True, **kw)
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return res


def pose_state(t, pose):
    return struct.unpack("<i", E.read(E.u32(POSE_MAP + 4 * t) + 4 * pose, 4))[0]


def new_ai(e, t=7, state=999):
    ai = e.alloc(0x3ED0)
    h = e.alloc(0x1000)
    e.write(ai + 0x14, p32(h))
    e.write(ai + 0x20, f64(100.0, 200.0, 300.0))
    e.write(ai + 0x3D15, b"AITYPE_TEST\0")
    e.write(ai + 0x3D38, p32(t))
    e.write(h + 0x2FC, p32(state))
    e.write(h + 0x4C4, p32(e.alloc(0x40)))
    graph = e.alloc(0x100)
    e.write(ai + 0x3B6C, p32(graph))
    return ai


def new_action(e, param=b""):
    a = e.alloc(0x30)
    p = e.alloc(0x30)
    e.write(p + 8, param)
    e.write(a + 0x28, p32(p))
    return a


def run(addr, name, param=b"", state=None, t=7, pose=None, extra=None, cw=None, **kw):
    """Calls handler(ai, action) on a fresh AI; pose: the pose the human is in."""
    def setup(e):
        ai = new_ai(e, t, 999 if pose is None else pose_state(t, pose))
        a = new_action(e, param)
        if extra:
            extra(e, ai, a)
        return (ai, a)
    return check(addr, name, setup=setup, state=state, cw=cw, **kw)


# --- channels and poses ----------------------------------------------------------

@pytest.mark.parametrize("t", [0, 1, 3, 6, 7])
@pytest.mark.parametrize("channel", range(10))
def test_set_channel(t, channel):
    def setup(e):
        return (new_ai(e, t), channel, 0x3FC00000)
    check(0x48EF70, "HumanAI_SetChannel", setup=setup)


@pytest.mark.parametrize("t", [0, 1, 3, 7])
@pytest.mark.parametrize("pose", range(5))
@pytest.mark.parametrize("match", [0, 1])
def test_in_pose(t, pose, match):
    def setup(e):
        s = pose_state(t, pose)
        return (new_ai(e, t, s if match else 999), pose)
    check(0x48EFE0, "HumanAI_InPose", setup=setup, check_eax=True, eax_mask=0xFF)


# --- combat moves ----------------------------------------------------------------

@pytest.mark.parametrize("target", [0, 1])
@pytest.mark.parametrize("pose", [None, 4])
def test_combat_end(target, pose):
    def extra(e, ai, a):
        e.write(ai + 0x3AA8, p32(0x7700 if target else 0))
        e.write(ai + 0x3ABA, b"\x01")
    run(0x48D7D0, "AICombat_End", t=1, pose=pose, extra=extra)


MOVES = [
    (0x48D830, "AICombat_StandRollLeft"),
    (0x48D870, "AICombat_StandRollRight"),
    (0x48DBB0, "AICombat_StandCloseCombat"),
    (0x48DBF0, "AICombat_StandThrowGrenade"),
    (0x48DC30, "AICombat_KneelRollLeft"),
    (0x48DC80, "AICombat_KneelRollRight"),
]


@pytest.mark.parametrize("addr,name", MOVES)
@pytest.mark.parametrize("pose", [None, 0, 1])
@pytest.mark.parametrize("t", [0, 7])
def test_moves(addr, name, pose, t):
    run(addr, name, t=t, pose=pose, check_eax=True, eax_mask=0xFF)


RELOADS = [(0x48DB10, "AICombat_StandReload", 0), (0x48DD10, "AICombat_KneelReload", 1),
           (0x48DE00, "AICombat_LieDownReload", 2)]


@pytest.mark.parametrize("addr,name,p", RELOADS)
@pytest.mark.parametrize("ammo", [0, 5])
@pytest.mark.parametrize("inpose", [0, 1])
@pytest.mark.parametrize("toggle", [0, 1])
def test_reload(addr, name, p, ammo, inpose, toggle):
    def extra(e, ai, a):
        e.write(ai + 0x3DF1, bytes([toggle]))
    run(addr, name, t=7, pose=p if inpose else None, state={"ammo": ammo}, extra=extra,
        check_eax=True, eax_mask=0xFF)


def weapon_type(e, kind, mode):
    w = e.alloc(0x700)
    e.write(w + 0x610, p32(kind))
    e.write(w + 0x64C, p32(mode))
    return w


FIRES = [(0x48D8E0, "AICombat_Fire"), (0x48D8B0, "AICombat_StandFire"),
         (0x48DCD0, "AICombat_KneelFire"), (0x48DDC0, "AICombat_LieDownFire")]


def fire_setup(kind=1, mode=0, shots=3, last=7, delay=2, gun=False, same=False, speed=0.3):
    def extra(e, ai, a):
        S["wtype"] = weapon_type(e, kind, mode)
        if gun:
            sg = e.alloc(0x300)
            e.write(sg + 0x20C, p32(e.alloc(0x40)))
            ref = e.alloc(4)
            e.write(ref, p32(sg))
            e.write(e.u32(ai + 0x14) + 0x7B4, p32(ref))
        e.write(ai + 0x3D80, f32(0.3, 0.7))
        e.write(ai + 0x3DC8, f64(speed))
        e.write(ai + 0x3E50, p32(4, shots, last, 0, delay))
        e.write(ai + 0x3E70, f64(10.1, -20.3, 30.7))
        e.write(ai + 0x3E88, f64(10.1, -20.3, 30.7) if same else f64(1000.3, 2000.7, -3000.1))
    return extra


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("addr,name", FIRES)
@pytest.mark.parametrize("case", ["kind", "noshots", "noammo", "auto", "single", "same", "aim", "fire",
                                  "wait", "gun"])
def test_fire(cw, addr, name, case):
    kw = dict(kind={"kind": 3}, noshots={"shots": 0}, single={"mode": 1}, same={"last": 5},
              aim={"last": 5, "delay": 0}, fire={"last": 5, "delay": 0}, wait={"last": 5},
              gun={"gun": True, "mode": 1}).get(case, {})
    if case == "kind" and addr == 0x48D8E0:
        kw["kind"] = -1
    if case == "same":
        kw["same"] = True
    state = {"ammo": 0 if case == "noammo" else 5, "face": case in ("fire", "wait", "same"), "rand": 0.37}
    pose = 4 if case == "gun" else None
    run(addr, name, t=7 if case != "gun" else 1, pose=pose, state=state, extra=fire_setup(**kw), cw=cw,
        check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("speed", [0.3, 0.123456789, 1.0 / 3])
def test_fire_rounding(cw, speed):
    run(0x48D8E0, "AICombat_Fire", state={"ammo": 5}, extra=fire_setup(last=5, speed=speed), cw=cw,
        check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("r", [0.0, 0.25, 0.5, 0.999])
def test_fire_single_delay(cw, r):
    run(0x48D8E0, "AICombat_Fire", state={"ammo": 5, "rand": r}, extra=fire_setup(mode=1), cw=cw,
        check_eax=True, eax_mask=0xFF)


# --- actions ---------------------------------------------------------------------

STARTS = [
    (0x48DEA0, "AIPlayAnim_Start"), (0x48DF70, "AIPlaySound_Start"), (0x48E080, "AILookAtEvent_Start"),
    (0x48E130, "AIStunned_Start"), (0x48E510, "AIIdle_Start"), (0x48E540, "AISetCombat_Start"),
    (0x48EB40, "AIFallFlat_Start"), (0x48EBE0, "AIWalkToNode_Start"), (0x48EC40, "AIRunToNode_Start"),
    (0x48ECA0, "AIFireAtNode_Start"), (0x48EDE0, "AIFireAtTask_Start"),
]


@pytest.mark.parametrize("addr,name", STARTS)
def test_start(addr, name):
    run(addr, name, param=b"\x11" * 0x28)


@pytest.mark.parametrize("pose", [None, 0, 3])
@pytest.mark.parametrize("started", [0, 1])
def test_play_anim(pose, started):
    run(0x48DEC0, "AIPlayAnim_Update", param=p32(12) + bytes([started]), pose=pose)


@pytest.mark.parametrize("on_self", [0, 1])
@pytest.mark.parametrize("sound", [0, 0x4444])
def test_play_sound(on_self, sound):
    run(0x48DF80, "AIPlaySound_Update", param=b"gunshot\0".ljust(0x20, b"\0") + bytes([on_self]),
        state={"sound": sound})


EVENT = f64(5000.5, -6000.25, 70.125)


def nearest_node(e, ai, a, node_id=31):
    n = e.alloc(0x240)
    e.write(n, p32(node_id))
    S["nearest"] = n
    return n


def test_move_to_event_start():
    run(0x48DFE0, "AIMoveToEvent_Start", param=EVENT, extra=nearest_node)


@pytest.mark.parametrize("run_", [0, 1])
@pytest.mark.parametrize("arrived", [0, 1])
def test_move_to_event_update(run_, arrived):
    run(0x48E020, "AIMoveToEvent_Update", param=EVENT + p32(31) + bytes([run_]), state={"run": arrived})


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("until", [-1, 500, 1500])
@pytest.mark.parametrize("face", [0, 1])
@pytest.mark.parametrize("time", [1.5, 0.7, 2.0 / 3])
def test_look_at_event(cw, until, face, time):
    run(0x48E0A0, "AILookAtEvent_Update", param=EVENT + f32(time) + p32(until),
        state={"ticks": 1000, "face": face}, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("addr,name", [(0x48E150, "AIStunned_Update"), (0x48EB60, "AIFallFlat_Update")])
@pytest.mark.parametrize("until", [-1, 500, 1500])
@pytest.mark.parametrize("pose", [None, 3])
@pytest.mark.parametrize("time", [1.5, 0.7, 2.0 / 3])
def test_timed(cw, addr, name, until, pose, time):
    run(addr, name, param=f32(time) + p32(until), state={"ticks": 1000}, pose=pose, cw=cw)


def test_kick_grenade_start():
    run(0x48E1E0, "AIKickGrenade_Start", param=EVENT + p32(0, 0x01010101, 0x777), extra=nearest_node)


@pytest.mark.parametrize("ref", [0, 0x1230])
def test_kick_grenade_end(ref):
    run(0x48E230, "AIKickGrenade_End", param=EVENT + p32(0, 0, ref))


def test_kick_grenade_update():
    run(0x48E250, "AIKickGrenade_Update", param=EVENT)


def graph(e, ai, links, nodes=None):
    """links: {id: [other ids]}; returns {id: node}"""
    ids = set(links) | {o for v in links.values() for o in v if o is not None}
    nodes = {}
    for i in sorted(ids):
        n = e.alloc(0x240)
        e.write(n, p32(i))
        nodes[i] = n
    for i, others in links.items():
        for k, o in enumerate(others):
            if o is None:
                continue
            l = e.alloc(0x10)
            e.write(l + 8, p32(o, i) if k % 2 else p32(i, o))
            e.write(nodes[i] + 0x13C + 4 * (2 * k + 1), p32(l))
    S["nodes"] = nodes
    S["nodepos"] = {n: (i * 4096.0, 0.0, 0.0) for i, n in nodes.items()}
    return nodes


LINKS = {1: [2, 3, None, 4], 2: [1, 5, 6], 3: [7, 1], 4: [8], 5: [9, 10], 6: [11, 12, 13], 7: [14, 15]}


@pytest.mark.parametrize("hit", [None, 1, 3, 9, 12, 15])
@pytest.mark.parametrize("max_", [0, 3, 10, 100])
def test_graph_search(hit, max_):
    def setup(e):
        ai = new_ai(e)
        nodes = graph(e, ai, LINKS)
        S["hit"] = hit
        return (ai, nodes[1], max_, SEARCH_FN, 0x4321)
    check(0x48E300, "AIGraph_Search", setup=setup, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("start,fn", [(0, 1), (1, 0), (-1, 1)])
def test_graph_search_none(start, fn):
    def setup(e):
        ai = new_ai(e)
        n = e.alloc(0x240)
        e.write(n, p32(start))
        return (ai, n if start else 0, 10, SEARCH_FN if fn else 0, 0)
    check(0x48E300, "AIGraph_Search", setup=setup, check_eax=True, eax_mask=0xFF)


PANIC = f64(4096.0 * 3, 0.0, 0.0)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("radius", [1.0, 2.5, 4.0, 20.0])
@pytest.mark.parametrize("good", [(), (5,), (1, 9), (12,)])
def test_run_panicking_start(cw, radius, good):
    def extra(e, ai, a):
        e.write(ai + 0x20, f64(4096.0 * 4, 0.0, 0.0))
        nodes = graph(e, ai, LINKS)
        S["nearest"] = nodes[1]
        S["good"] = [nodes[i] for i in good]
    run(0x48E270, "AIRunPanicking_Start", param=PANIC + f64(radius) + p32(5), extra=extra, cw=cw)


@pytest.mark.parametrize("case", ["big", "none"])
def test_run_panicking_start_fail(case):
    def extra(e, ai, a):
        e.write(ai + 0x20, f64(0.0, 0.0, 0.0))
        e.write(e.u32(ai + 0x3B6C) + 0x5C, p32(0x400 if case == "big" else 0x3FF))
    run(0x48E270, "AIRunPanicking_Start", param=PANIC + f64(20.0), extra=extra)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("x", [0.0, 4096.0, 4096.0 * 2.5, 4096.0 * 5, 4096.0 * 5.5])
@pytest.mark.parametrize("good", [0, 1])
def test_run_panicking_check(cw, x, good):
    def setup(e):
        ai = new_ai(e)
        n = e.alloc(0x240)
        e.write(n, p32(44))
        S["nodepos"] = {n: (x, 0.0, 0.0)}
        S["good"] = [n] if good else []
        p = e.alloc(0x30)
        e.write(p + 8, f64(0.0, 0.0, 0.0, 2.5) + p32(-1))
        return (ai, n, p + 8)
    check(0x48E430, "AIRunPanicking_Check", setup=setup, check_eax=True, eax_mask=0xFF, cw=cw)


@pytest.mark.parametrize("node", [-1, 9])
@pytest.mark.parametrize("arrived", [0, 1])
def test_run_panicking_update(node, arrived):
    run(0x48E4A0, "AIRunPanicking_Update", param=PANIC + f64(1.0) + p32(node), state={"run": arrived})


def test_idle_update():
    run(0x48E520, "AIIdle_Update")


@pytest.mark.parametrize("frm", [0, 1, 2])
@pytest.mark.parametrize("about", [0, 1, 2])
def test_set_combat_update(frm, about):
    def extra(e, ai, a):
        refs = []
        for v in (frm, about):
            r = 0
            if v:
                r = e.alloc(4)
                e.write(r, p32(0x6000 + v if v == 2 else 0))
            refs.append(r)
        e.write(e.u32(a + 0x28) + 0x20, p32(*refs))
    run(0x48E560, "AISetCombat_Update", param=EVENT, extra=extra)


@pytest.mark.parametrize("frm", [0, 0x100])
@pytest.mark.parametrize("about", [0, 0x200])
def test_set_combat_end(frm, about):
    run(0x48E5F0, "AISetCombat_End", param=EVENT + p32(frm, about))


ACT = {"id": 50, "trigger": 60}


@pytest.mark.parametrize("case", ["notask", "nocontrol", "claimed", "ctlclaimed", "nottype", "nonode",
                                  "found_here", "found_other", "notfound", "alarm"])
def test_activate_start(case):
    def extra(e, ai, a):
        alarm = case in ("nocontrol", "ctlclaimed", "alarm")
        e.write(ai + 0x3AC0, p32(50 if alarm else 49, 61))
        t = e.alloc(0x40)
        e.write(t + 0x1C, struct.pack("<H", 0x42))
        e.write(t + 0x20, f64(1.5, 2.5, 3.5))
        ctl = e.alloc(0x40)
        tasks = {}
        if case != "notask":
            tasks[50] = t
        if case != "nocontrol":
            tasks[61] = ctl
        S["tasks"] = tasks
        S["claimed"] = [t] if case == "claimed" else [ctl] if case == "ctlclaimed" else []
        S["isa"] = case != "nottype"
        nodes = graph(e, ai, LINKS)
        S["nearest"] = 0 if case == "nonode" else nodes[1]
        S["good"] = {"found_here": [nodes[1]], "found_other": [nodes[6]], "alarm": [nodes[3]]}.get(case, [])
    run(0x48E620, "AIActivate_Start", param=p32(50, 7, 8, 1), extra=extra)


@pytest.mark.parametrize("good", [0, 1])
def test_activate_check(good):
    def setup(e):
        ai = new_ai(e)
        n = e.alloc(0x240)
        e.write(n, p32(23))
        S["good"] = [n] if good else []
        out = e.alloc(4)
        e.write(out, p32(-1))
        return (ai, n, out)
    check(0x48E7A0, "AIActivate_Check", setup=setup, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("n1,n2", [(-1, -1), (-1, 8), (7, -1), (7, 8)])
@pytest.mark.parametrize("arrived", [0, 1])
@pytest.mark.parametrize("run_", [0, 1])
def test_activate_update(n1, n2, arrived, run_):
    run(0x48E7D0, "AIActivate_Update", param=p32(50, n1, n2) + bytes([run_]), state={"run": arrived})


@pytest.mark.parametrize("c1,c2", [(0, 0), (0x100, 0), (0, 0x200), (0x100, 0x200)])
def test_activate_end(c1, c2):
    def extra(e, ai, a):
        e.write(ai + 0x3AB0, p32(c1, c2))
    run(0x48E870, "AIActivate_End", extra=extra)


def test_activate_start_handlers():
    def extra(e, ai, a):
        e.write(ai + 0x3AC0, p32(49))
    run(0x48E620, "AIActivate_Start", param=p32(50), extra=extra)


@pytest.mark.parametrize("count", [0, 3])
def test_patrol_start(count):
    def extra(e, ai, a):
        e.write(ai + 0x1514, p32(count))
    run(0x48E8C0, "AIPatrol_Start", param=p32(17, 4), extra=extra)


@pytest.mark.parametrize("count", [1, 2])
def test_patrol_end(count):
    def extra(e, ai, a):
        e.write(ai + 0x1514, p32(count))
    run(0x48EB30, "AIPatrol_End", extra=extra)


@pytest.mark.parametrize("cmd", [None, 0, 1, 2, 3, 4, 5, 6, 7, 8, -1])
@pytest.mark.parametrize("timer", [-1, 0, 1200])
@pytest.mark.parametrize("flags", [(0, 0), (1, 0), (0, 1), (1, 1)])
@pytest.mark.parametrize("ok", [0, 1])
def test_patrol_update(cmd, timer, flags, ok):
    def extra(e, ai, a):
        e.write(ai + 0x1514, p32(2))
        r = ai + 0x1518 + 0x1C
        e.write(r + 0x14, bytes(flags) + b"\0\0" + p32(timer))
        e.write(ai + 0x3E2C, p32(66))
        S["cmd"] = 0 if cmd is None else 0x8800
        S["cmdtype"] = cmd if cmd is not None else 0
        S["cmdarg"] = 25
        n = e.alloc(0x240)
        S["nodes"] = {25: n}
        S["nodepos"] = {n: (1.0, 2.0, 3.0)}
        S["run"] = S["face"] = ok
        S["ticks"] = 1000 + 300 * ok
    pose = None
    if cmd == 0 and ok:
        pose = flags[0] != 0
    run(0x48E920, "AIPatrol_Update", extra=extra, pose=pose)


@pytest.mark.parametrize("addr,name", [(0x48EBF0, "AIWalkToNode_Update"), (0x48EC50, "AIRunToNode_Update")])
@pytest.mark.parametrize("arrived", [0, 1])
def test_to_node(addr, name, arrived):
    run(addr, name, param=p32(19), state={"run": arrived})


@pytest.mark.parametrize("addr,name", [(0x48ECC0, "AIFireAtNode_Update"), (0x48EE00, "AIFireAtTask_Update")])
@pytest.mark.parametrize("case", ["over", "nogun", "noreload", "reload", "aim", "wait", "fire", "busy",
                                  "notask", "nottype"])
def test_fire_at(addr, name, case):
    def extra(e, ai, a):
        S["wtype"] = weapon_type(e, 1, 0)
        if case == "nogun":
            e.write(e.u32(ai + 0x14) + 0x4C4, p32(0))
        n = e.alloc(0x240)
        S["nodes"] = {19: n}
        S["nodepos"] = {n: (1.0, 2.0, 3.0)}
        t = e.alloc(0x40)
        e.write(t + 0x1C, struct.pack("<H", 0x42))
        e.write(t + 0x20, f64(4.0, 5.0, 6.0))
        S["tasks"] = {} if case == "notask" else {19: t}
        S["isa"] = case != "nottype"
        e.write(ai + 0x3E50, p32(4, 3, 7, 0, 2))
    state = {"ammo": 0 if case in ("noreload", "reload") else 5, "face": case == "fire"}
    time = -1 if case == "over" else 40
    wait = 3 if case == "wait" else -1
    aimed = 0 if case in ("aim", "reload") else 1
    run(addr, name, param=p32(19, time, 90, wait) + bytes([case == "noreload", aimed]), state=state,
        extra=extra)
