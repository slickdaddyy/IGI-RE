"""HumanAI task: create, update, events and the action stack (src/humanai/humanai.c)."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import Emu, ErrorShown, compare

HANDLERS = 0xA96AE0
AI_SIZE = 0x3ED0
AI_TYPE = 0x60
HUMAN_TYPE = 0x61
SCRIPT_GLOBALS = [(0x5BDA5C, 0x5BDA60)]  # gAIActionList holds a stack address


class Fakes:
    """Python-side replacements of original functions that log their arguments."""

    def __init__(self, e):
        self.e = e
        self.log = []
        self.rets = {}
        self.tramps = {}

    def code(self, data):
        p = self.e.fake_top
        self.e.fake_top += (len(data) + 15) & ~15
        self.e.write(p, data)
        return p

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None):
        e = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code(b"\xd9\x05" + struct.pack("<I", cell) + b"\xc3" if st0 == "f" else
                         b"\xdd\x05" + struct.pack("<I", cell) + b"\xc3") if st0 else None

        def cb(uc, a, size, user):
            em = e.e
            args = [em.arg(i) for i in range(nargs)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = []
                for i, v in enumerate(args):
                    if deref and i in deref:
                        rec.append(em.read(v, deref[i]))
                    else:
                        rec.append(em.normalize(v))
            e.log.append((a, tuple(rec)))
            r = e.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<f" if st0 == "f" else "<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r or 0)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def handler(self, nargs, deref=None, ret=0):
        return self.add(self.code(b"\xc3"), nargs, deref=deref, ret=ret)


def with_cw(e, fk, target, cw):
    addr = e.symbols[target] if isinstance(target, str) else target
    key = (addr, cw)
    if key not in fk.tramps:
        cell = fk.code(struct.pack("<H", cw))
        p = fk.code(b"\0" * 16)
        e.write(p, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        fk.tramps[key] = p
    return fk.tramps[key]


def check(e, fk, orig, ours, setup=None, normalized=True, cw=None, **kw):
    """compare() plus equal logs of the Python fakes; cw is the x87 control word to run with."""
    logs = []
    kw.setdefault("check_eax", False)
    if cw is not None:
        orig, ours = with_cw(e, fk, orig, cw), with_cw(e, fk, ours, cw)

    def s(em):
        r = setup(em) if setup else None
        fk.log = []
        logs.append(fk.log)
        return r
    res = compare(e, orig, ours, setup=s, normalized=normalized, **kw)
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return res


def check_error(e, orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        args = setup(e)
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]


def p32(*v):
    return struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v])


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), p32(fn))


def new_list(e, p):
    e.write(p, p32(p + 4, 0, p))


def add_tail(e, lst, node):
    pred = e.u32(lst + 8)
    e.write(node, p32(lst + 4, pred))
    e.write(pred, p32(node))
    e.write(lst + 8, p32(node))


def ref(e, task):
    c = e.alloc(4)
    e.write(c, p32(task))
    return c


def fake_common(fk):
    """Fakes most HumanAI code needs."""
    fk.add(0x401AE0, 1, ret=lambda e, a: ref(e, a[0]))
    fk.add(0x401B20, 1, deref={0: 4})
    fk.add(0x4AF810, 5, fmt=warning)
    fk.add(0x4F1050, 1, ret=lambda e, a: e.u32(a[0] + 0x18))


def warning(e, a):
    text = e.read_cstr(a[0])
    return (text,) + tuple(a[1:1 + text.count(b"%")])


def new_human(e, dead=0, active=1, team=2, type_=HUMAN_TYPE):
    h = e.alloc(0x1000)
    parent = e.alloc(0x40)
    e.write(parent + 0x1C, struct.pack("<H", 0x70))
    e.write(h + 8, p32(h + 0xC, 0, h + 8))
    e.write(h + 0x14, p32(parent))
    e.write(h + 0x1C, struct.pack("<H", type_))
    e.write(h + 0x20, f64(1000.5, -2000.25, 300.75))
    model = e.alloc(0x300)
    e.write(model + 0x2AC, f32(10.0, 20.0, 1500.0) + f32(*range(10)))
    e.write(h + 0x70, p32(model))
    e.write(h + 0x78, f32(0.8, 0.6, 0.0, -0.6, 0.8, 0.0, 0.0, 0.0, 1.0) + p32(5))
    e.write(h + 0xFC, p32(team))
    e.write(h + 0x4EC, p32(e.alloc(0x100)))
    e.write(h + 0xCF5, bytes([dead, active]))
    return h


def new_ai(e, h=None, create=True):
    """A HumanAI made by the original create handler."""
    if h is None:
        h = new_human(e)
    ai = e.alloc(AI_SIZE)
    e.write(ai + 0x14, p32(h))
    e.write(ai + 0x1C, struct.pack("<H", AI_TYPE))
    if create:
        e.call(0x44F060, ai, 0)
    return ai


def ai_list(e, ai, i):
    return ai + 0x6F0 + 12 * i


def add_event(e, ai, t, about=0, frm=0):
    """Takes an event from the free list like the original, queues it under type t."""
    head = e.u32(ai + 0x810)
    e.call(0x4AF960, head)
    e.write(head + 8, p32(t, 3) + f64(1, 2, 3) + p32(ref(e, frm) if frm else 0, ref(e, about) if about else 0))
    add_tail(e, ai_list(e, ai, t), head)
    e.write(ai + 0x8E0, p32(e.u32(ai + 0x8E0) + 1))
    return head


def new_action(e, ai, slot, start=0, update=0, end=0, flags=0, param=0, event=0, delay=0):
    a = ai + 0x908 + 0x30 * slot
    e.write(a, p32(0, 0, event, delay, flags, start, update, 0, end, 0, param) + b"\1")
    return a


def push_action(e, ai, slot, **kw):
    a = new_action(e, ai, slot, **kw)
    add_tail(e, ai + 0x8FC, a)
    return a


# --- create / init ------------------------------------------------------------------

def test_create():
    e = Emu()

    def setup(em):
        return (new_ai(em, create=False), 0)
    compare(e, 0x44F060, "HumanAI_Create", setup=setup, check_eax=False)


def test_init_events():
    e = Emu()

    def setup(em):
        ai = new_ai(em, create=False)
        em.write(ai + 0x8E4, b"\x55" * 24)
        return (ai,)
    compare(e, 0x44F380, "HumanAI_InitEvents", setup=setup, check_eax=False)


# --- small event / action helpers ---------------------------------------------------

E = Emu()
F = Fakes(E)
fake_common(F)
START = F.handler(2)
END = F.handler(2)
UPDATE = F.handler(2)
F.add(0x4B4770, 1, ret=0.375, st0="f")
F.add(0x416D20, 0, ret=0x1234)


@pytest.mark.parametrize("refs", [0, 1, 2, 3])
def test_free_event(refs):
    def setup(e):
        ai = new_ai(e)
        h = e.u32(ai + 0x14)
        ev = add_event(e, ai, 5, about=h if refs & 1 else 0, frm=h if refs & 2 else 0)
        return (ai, ev)
    check(E, F, 0x44FE50, "HumanAI_FreeEvent", setup=setup)


@pytest.mark.parametrize("n", [0, 1, 3])
def test_free_events(n):
    def setup(e):
        ai = new_ai(e)
        for i in range(n):
            add_event(e, ai, (i * 7) % 24, about=e.u32(ai + 0x14))
        return (ai,)
    check(E, F, 0x4500C0, "HumanAI_FreeEvents", setup=setup)


@pytest.mark.parametrize("case", ["free", "full", "nopos", "refs"])
def test_new_event(case):
    def setup(e):
        ai = new_ai(e)
        if case == "full":
            new_list(e, ai + 0x810)
        pos = e.alloc(24)
        e.write(pos, f64(5, 6, 7))
        h = e.u32(ai + 0x14)
        m = e.alloc(0x1C)
        e.write(m, p32(9, 4, 0) + f32(1.5) + p32(0 if case == "nopos" else pos,
                                                   h if case == "refs" else 0, h if case == "refs" else 0))
        return (ai, m)
    check(E, F, 0x450520, "HumanAI_NewEvent", setup=setup, check_eax=True)


def test_get_params():
    def setup(e):
        e.write(0x58A9D4, p32(0x1234567))
        return (0, e.alloc(4))
    check(E, F, 0x4505C0, "HumanAI_GetParams", setup=setup)


@pytest.mark.parametrize("n", [0, 1, 2])
def test_clear_move(n):
    def setup(e):
        ai = new_ai(e)
        new_list(e, 0x58A9C8)
        for i in range(n):
            add_tail(e, ai + 0xE0, e.alloc(0x28))
        e.write(ai + 0x3E4A, b"\1")
        e.write(ai + 0x3E4C, p32(ref(e, ai) if n else 0))
        return (ai,)
    check(E, F, 0x44FF40, "HumanAI_ClearMove", setup=setup)
    check(E, F, 0x44FF80, "HumanAI_ClearMoveQueue", setup=setup)


def test_top_action():
    for n in (0, 1):
        def setup(e):
            ai = new_ai(e)
            if n:
                push_action(e, ai, 3)
            return (ai,)
        check(E, F, 0x450DC0, "HumanAI_GetTopAction", setup=setup, check_eax=True)


@pytest.mark.parametrize("used", [0, 5, 31])
def test_find_free_action(used):
    def setup(e):
        ai = new_ai(e)
        for i in range(used):
            new_action(e, ai, i)
        return (ai,)
    check(E, F, 0x450DE0, "HumanAI_FindFreeAction", setup=setup, check_eax=True)


def test_find_free_action_full():
    def setup(e):
        ai = new_ai(e)
        for i in range(32):
            new_action(e, ai, i)
        return (ai,)
    check_error(E, 0x450DE0, "HumanAI_FindFreeAction", setup)


@pytest.mark.parametrize("end,param", [(0, 0), (1, 0), (1, 1)])
def test_end_action(end, param):
    def setup(e):
        ai = new_ai(e)
        p = e.call(0x451030, ai)[0] if param else 0
        a = push_action(e, ai, 4, end=END if end else 0, param=p)
        return (ai, a)
    check(E, F, 0x450FF0, "HumanAI_EndAction", setup=setup)


@pytest.mark.parametrize("empty", [0, 1])
def test_params(empty):
    def setup(e):
        ai = new_ai(e)
        if empty:
            new_list(e, ai + 0x1508)
        return (ai,)
    check(E, F, 0x451030, "HumanAI_AllocParam", setup=setup, check_eax=True)

    def setup2(e):
        ai = new_ai(e)
        p = e.call(0x451030, ai)[0]
        return (ai, p)
    check(E, F, 0x451050, "HumanAI_FreeParam", setup=setup2)


@pytest.mark.parametrize("flags", [[], [0], [1], [1, 0], [0, 1], [1, 1, 0]])
def test_pop_actions(flags):
    def setup(e):
        ai = new_ai(e)
        for i, f in enumerate(flags):
            push_action(e, ai, i, flags=f, end=END)
        return (ai,)
    check(E, F, 0x450EE0, "HumanAI_PopActions", setup=setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("r", [0.0, 0.375, 0.999])
def test_start_action(cw, r):
    F.rets[0x4B4770] = r

    def setup(e):
        ai = new_ai(e)
        push_action(e, ai, 0, flags=0, end=END)
        push_action(e, ai, 1, flags=1)
        a = new_action(e, ai, 5, start=START, flags=4)
        return (ai, a)
    check(E, F, 0x450E70, "HumanAI_StartAction", setup=setup, cw=cw)


@pytest.mark.parametrize("n", [0, 1, 3])
def test_start_actions(n):
    F.rets[0x4B4770] = 0.6

    def setup(e):
        ai = new_ai(e)
        push_action(e, ai, 0, flags=0, end=END)
        lst = e.alloc(12)
        new_list(e, lst)
        for i in range(n):
            add_tail(e, lst, new_action(e, ai, 10 + i, start=START, flags=i * 2))
        return (ai, lst)
    check(E, F, 0x450F10, "HumanAI_StartActions", setup=setup)


def test_push_action():
    def setup(e):
        ai = new_ai(e)
        new_action(e, ai, 0)
        lst = e.alloc(12)
        new_list(e, lst)
        e.write(0x5BDA58, p32(ai, lst, 0, 6))
        return (START, 0x1234, 3)
    check(E, F, 0x450E10, "HumanAI_PushAction", setup=setup)


@pytest.mark.parametrize("case", [[], [(0, 0)], [(3, 0), (2, 0)], [(3, 0), (0, 1)], [(0, 0), (0, 1), (1, 1)]])
def test_run_action(case):
    def setup(e):
        ai = new_ai(e)
        e.write(ai + 0x81C, p32(*range(24)))
        for i, (delay, upd) in enumerate(case):
            push_action(e, ai, i, delay=delay, update=UPDATE if upd else 0, event=i)
        return (ai,)
    check(E, F, 0x44FEC0, "HumanAI_RunAction", setup=setup)


# --- event slots and sending -----------------------------------------------------------

@pytest.mark.parametrize("addr,name,var", [(0x450C80, "HumanAI_GetEventSlot", 0x53C922),
                                           (0x450CA0, "HumanAI_GetDoorSlot", 0x53C923),
                                           (0x450CC0, "HumanAI_GetEvent3Slot", 0x53C924)])
@pytest.mark.parametrize("val", [0xFF, 0x33])
def test_event_slots(addr, name, var, val):
    def setup(e):
        e.write(var, bytes([val]))
    compare(Emu(), addr, name, setup=setup, eax_mask=0xFF)


SEND = F.handler(2, deref={1: 0x1C})


def test_send():
    def setup(e):
        e.write(0x53C922, b"\x30")
        t = e.alloc(0x40)
        e.write(t + 0x1C, struct.pack("<H", 0x45))
        set_handler(e, 0x30, 0x45, SEND)
        m = e.alloc(0x1C)
        e.write(m, bytes(range(0x1C)))
        return (t, m)
    check(E, F, 0x450D50, "HumanAI_Send", setup=setup)

    def setup2(e):
        t, m = setup(e)
        return (t, 7, 0x46)
    check(E, F, 0x450D80, "HumanAI_SendSimple", setup=setup2, stack_fill=0)


@pytest.mark.parametrize("n", [0, 1, 3])
def test_broadcast(n):
    def setup(e):
        e.write(0x53C922, b"\x30")
        arr = e.alloc(8 + 4 * n)
        e.write(arr, p32(n, 0))
        for i in range(n):
            t = e.alloc(0x40)
            e.write(t + 0x1C, struct.pack("<H", 0x45 + i))
            set_handler(e, 0x30, 0x45 + i, SEND)
            e.write(arr + 8 + 4 * i, p32(t))
        e.write(0x5BDC24, p32(arr))
        m = e.alloc(0x1C)
        e.write(m, bytes(range(0x1C)))
        return (m,)
    check(E, F, 0x450CE0, "HumanAI_Broadcast", setup=setup)


# --- script runs -----------------------------------------------------------------------

S = Emu()
SF = Fakes(S)
fake_common(SF)
SF.add(0x4B4770, 1, ret=0.25, st0="f")
SF.add(0x416D20, 0, ret=0x1234)
S_START = SF.handler(2)
S_END = SF.handler(2)
SCRIPT_ACTIONS = []


def run_script(e, a):
    """Pretends the script pushed SCRIPT_ACTIONS (action pool slots) with AIAction_ functions."""
    ai = e.u32(0x5BDA58)
    lst = e.u32(0x5BDA5C)
    for slot in SCRIPT_ACTIONS:
        add_tail(e, lst, new_action(e, ai, slot, start=S_START, flags=slot & 1, event=e.u32(0x5BDA64)))
    return 0


SF.add(0x4B85B0, 1, ret=run_script)


@pytest.mark.parametrize("actions", [[], [20], [20, 21, 22]])
def test_run_script(actions):
    SCRIPT_ACTIONS[:] = actions

    def setup(e):
        ai = new_ai(e)
        e.write(ai + 0x3C70, p32(0x5555))
        push_action(e, ai, 0, end=S_END)
        return (ai, 3, 2)
    check(S, SF, 0x44FC90, "HumanAI_RunScript", setup=setup, ignore=SCRIPT_GLOBALS, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("case", ["noscript", "empty", "events", "limit", "done"])
def test_process_events(case):
    SCRIPT_ACTIONS[:] = [20] if case == "done" else []

    def setup(e):
        ai = new_ai(e)
        e.write(ai + 0x3C70, p32(0 if case == "noscript" else 0x5555))
        e.write(ai + 0x81C, p32(4, 7, 16, 2))
        e.write(ai + 0x8DC, p32(4))
        if case != "empty":
            add_event(e, ai, 7)
            add_event(e, ai, 16)
            add_event(e, ai, 16, about=e.u32(ai + 0x14))
            add_event(e, ai, 2)
        if case == "limit":
            push_action(e, ai, 0, event=2)
        return (ai,)
    check(S, SF, 0x44FD50, "HumanAI_ProcessEvents", setup=setup, ignore=SCRIPT_GLOBALS)


# --- event filter ----------------------------------------------------------------------

O = Emu()
OF = Fakes(O)
fake_common(OF)


def msg(e, type_, flags, team=2, radius=5000.0, pos=None, frm=0, about=0):
    p = e.alloc(24)
    e.write(p, f64(*(pos or (1100.0, -2050.0, 300.0))))
    m = e.alloc(0x1C)
    e.write(m, p32(type_, team, flags) + f32(radius) + p32(p, frm, about))
    return m


OTHER = [0, "self_about", "self_from", "dead_ai", "inactive", "noscript", "notstarted", "notstarted0",
         "disabled"]


@pytest.mark.parametrize("case", OTHER)
def test_on_event_gates(case):
    def setup(e):
        h = new_human(e, active=0 if case == "inactive" else 1)
        ai = new_ai(e, h)
        e.write(ai + 0x3C70, p32(0 if case == "noscript" else 0x5555))
        e.write(ai + 0x3D14, b"\0" if str(case).startswith("notstarted") else b"\1")
        e.write(ai + 0x3ABA, b"\1" if case == "dead_ai" else b"\0")
        e.write(ai + 0x8E4 + 9, b"\0" if case == "disabled" else b"\1")
        t = 0 if case == "notstarted0" else 9
        m = msg(e, t, 0, about=h if case == "self_about" else 0, frm=h if case == "self_from" else 0)
        return (ai, m)
    check(O, OF, 0x450110, "HumanAI_OnEvent", setup=setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("flags", [0x2, 0x1, 0x3, 0x4, 0xC, 0x14, 0x24, 0x44, 0x46, 0x26, 0x1F, 0x7F])
@pytest.mark.parametrize("variant", range(4))
def test_on_event_flags(cw, flags, variant):
    def setup(e):
        ai = new_ai(e)
        e.write(ai + 0x3C70, p32(0x5555))
        e.write(ai + 0x3D14, b"\1")
        e.write(ai + 0x8E4 + 16, b"\1")
        # a view looking along +y from the human
        e.write(ai + 0x20, f64(1000.5, -2000.25, 300.75) + f64(1000.5, -2000.25, 400.0))
        e.write(ai + 0x78, f32(1, 0, 0, 0, 1, 0, 0, 0, 1) + p32(0))
        e.write(ai + 0xA0, f64(0.785, 0.87, 122880.0, 122880.0, 0.785, 1.39, 40960.0, 40960.0))
        e.write(ai + 0x3ABB, bytes([variant & 1]))
        other = new_human(e, dead=variant >> 1, team=2 + (variant & 1))
        pos = [(1100.0, 3000.0, 400.0), (1100.0, -9000.0, 400.0), (1100.0, 300000.0, 400.0),
               (1.0e5, 1000.0, 400.0)][variant]
        m = msg(e, 16, flags, team=2 + (variant >> 1), radius=[6000.0, 100.0, 20000.0, 1e9][variant],
                pos=pos, about=other if variant != 3 or flags < 0x10 else 0)
        return (ai, m)
    check(O, OF, 0x450110, "HumanAI_OnEvent", setup=setup, cw=cw)
