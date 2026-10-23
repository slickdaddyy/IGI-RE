import random
import struct

import pytest

from test_vehbase_util import (ANIM, ROUND_CWS, RIGID, add_child, check, child_list, ev, f32, make, new_task,
                               set_handler)

e, fk = make()
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)
CONTROL = fk.handler(2, fmt=lambda em, a: [em.normalize(a[0]), em.u32(a[1]), em.read(em.u32(a[1] + 4), 0x88)])
SAVE = fk.handler(2, fmt=lambda em, a: [em.normalize(a[0])] + [em.u32(a[1] + 4 * i) for i in range(5)])
LOAD = fk.handler(2, fmt=lambda em, a: [em.normalize(a[0]), em.u32(a[1]), em.u32(a[1] + 4)])
SIZE = fk.handler(2, fmt=lambda em, a: (em.write(a[1], struct.pack("<I", 0x78)), [em.normalize(a[0])])[1])

VEH = 0x66
SIZE_ANIM = 0x20160


def vehicle(em):
    v = new_task(em, VEH, 0x40)
    fk.rets[0x4F1030] = v
    return v


def anim(em, keys=(), channels=None, wait=0.0, pos=0.0, tick=0, count=None, target=True, fill=False):
    t = new_task(em, ANIM, SIZE_ANIM, fill=fill)
    ch = list(channels or [i * 0.5 for i in range(32)]) + [wait, pos]
    em.write(t + 0x6C, f32(*ch))
    for i, (c, v) in enumerate(keys):
        em.write(t + 0xF4 + 8 * i, struct.pack("<b3xf", c, v))
    em.write(t + 0x2014C, struct.pack("<iii", 0x99, len(keys) if count is None else count, tick))
    fk.rets[0x4F1030] = vehicle(em) if target else 0
    return t


def list_head(em):
    em.write(0xA76C28, struct.pack("<III", 0xA76C2C, 0, 0xA76C28))


def test_open():
    check(e, fk, 0x4F3440, "AnimTask_Open", check_eax=False)


def test_init_type():
    check(e, fk, 0x4F36C0, "AnimTask_InitType", check_eax=False)


def test_close():
    check(e, fk, 0x4F3E40, "AnimTask_Close", check_eax=False)


@pytest.mark.parametrize("orig,ours,mask", [
    (0x4F40D0, "Vehicle_GetControlEvent", 0xFF), (0x4F40E0, "Vehicle_GetInputInfoEvent", 0xFF),
    (0x4F40F0, "AnimData_GetType", 0xFFFFFFFF), (0x4F4100, "AnimTask_GetType", 0xFFFF),
    (0x4F4110, "Vehicle_GetStateSizeEvent", 0xFF), (0x4F4120, "Vehicle_GetSaveEvent", 0xFF),
    (0x4F4130, "Vehicle_GetLoadEvent", 0xFF), (0x4F4140, "Vehicle_GetKeysEvent", 0xFF),
    (0x4F4150, "AnimTask_GetList", 0xFFFFFFFF)])
def test_getters(orig, ours, mask):
    def setup(em):
        em.write(0x54E1E4, struct.pack("<I", 0x12345678))
    check(e, fk, orig, ours, setup=setup, eax_mask=mask)


def test_get_params():
    check(e, fk, 0x4F39F0, "AnimTask_GetParams", setup=lambda em: (0, em.alloc(4)), check_eax=False)


# --- AnimData parameter -------------------------------------------------------

ARGS = [
    [0],
    [1, 0x20, 2.5],
    [3, 0, 0.25, 0x20, 3.5, 5, -1.75],
    [4, 0x20, -2.5, 0x20, 1.49, 0x21, 7.5, 0x120, 9.0],
    [2, 0x1F, 1e9, 0x20, 2147483648.0],
]


def arg_value(em, a, args):
    return args[a[1] - 3]


@pytest.mark.parametrize("cw", ROUND_CWS)
@pytest.mark.parametrize("args", ARGS)
def test_parse(args, cw):
    def setup(em):
        fk.rets[0x4B8A20] = lambda em, a: int(args[a[1] - 3])
        fk.rets[0x4B8A50] = lambda em, a: args[a[1] - 3]
        t = new_task(em, ANIM, SIZE_ANIM, fill=False)
        msg = em.alloc(8)
        em.write(msg, struct.pack("<II", 3, 0xA0A0))
        return t, msg
    check(e, fk, 0x4F3570, "AnimData_Parse", setup=setup, check_eax=False, cw=cw)


def test_arg_count():
    def setup(em):
        fk.rets[0x4B8A20] = 6
        msg = em.alloc(12)
        em.write(msg, struct.pack("<III", 0, 4, 0xA0A0))
        return 0, msg
    check(e, fk, 0x4F3610, "AnimData_ArgCount", setup=setup, check_eax=False)


@pytest.mark.parametrize("addr,name", [(0x4F3630, "AnimData_Copy"), (0x4F3670, "AnimData_Paste")])
def test_copy(addr, name):
    def setup(em):
        rng = random.Random(9)
        t = new_task(em, ANIM, SIZE_ANIM, fill=False)
        buf = em.alloc(0x2008C)
        em.write(t + 0x6C, bytes(rng.randrange(256) for _ in range(0x88)))
        em.write(t + 0xF4, bytes(rng.randrange(256) for _ in range(0x400)) * 0x80)
        em.write(t + 0x20150, struct.pack("<I", 0x1234))
        em.write(buf, struct.pack("<I", 0x4321) + bytes(rng.randrange(256) for _ in range(0x88)))
        em.write(buf + 0x8C, bytes(rng.randrange(256) for _ in range(0x400)) * 0x7F)
        out = em.alloc(4)
        em.write(out, struct.pack("<I", buf))
        return t, out
    check(e, fk, addr, name, setup=setup, check_eax=False)


def test_get_size():
    check(e, fk, 0x4F36B0, "AnimData_GetSize", setup=lambda em: (0, em.alloc(4)), check_eax=False)


# --- AnimTask -----------------------------------------------------------------

@pytest.mark.parametrize("arg", [None, b"\0", b"\1"])
@pytest.mark.parametrize("kids", [[], [0x61, 0x62]])
def test_params_changed(arg, kids):
    def setup(em):
        t = anim(em, [(0, 1.0), (0x20, 2.0), (1, 3.0)])
        child_list(em, t, ev(0x54E0E5), LOG2, kids)
        set_handler(em, ev(0x54E1DA), VEH, CONTROL)
        p = 0
        if arg is not None:
            p = em.alloc(4)
            em.write(p, arg)
        return t, p
    check(e, fk, 0x4F3940, "AnimTask_ParamsChanged", setup=setup, check_eax=False)


def test_create():
    def setup(em):
        t = new_task(em, ANIM, SIZE_ANIM, fill=False)
        em.write(t, bytes(random.Random(1).randrange(256) for _ in range(0x200)))
        em.write(t + 8, struct.pack("<III", t + 0xC, 0, t + 8))
        em.write(t + 0x1C, struct.pack("<H", ANIM))
        em.write(t + 0x200F0, b"\xAB" * 0x70)
        list_head(em)
        return (t,)
    check(e, fk, 0x4F3A00, "AnimTask_Create", setup=setup, check_eax=False)


@pytest.mark.parametrize("id_", [-1, 0, 5])
@pytest.mark.parametrize("handler", [0, 1])
@pytest.mark.parametrize("ref", [None, 0, 1])
def test_delete(id_, handler, ref):
    def setup(em):
        list_head(em)
        t = anim(em)
        em.call(0x4AF8F0, 0xA76C28, t + 0x60)  # List_AddTail
        fk.rets[0x4F1050] = id_
        set_handler(em, 0x3D, ANIM, LOG1 if handler else 0)
        p = 0
        if ref is not None:
            p = em.alloc(4)
            em.write(p, struct.pack("<I", 0x4567 if ref else 0))
        em.write(t + 0x20148, struct.pack("<I", p))
        return (t,)
    check(e, fk, 0x4F3AC0, "AnimTask_Delete", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", ROUND_CWS)
@pytest.mark.parametrize("expr,value", [(0, 0.0), (1, 0.0), (1, 1.0), (1, 0.5), (1, 1.5), (1, 255.7), (1, -0.6),
                                        (1, 256.0)])
@pytest.mark.parametrize("is_run", [0, 1])
def test_run(expr, value, is_run, cw):
    def setup(em):
        t = anim(em, [(0, 1.0), (0x20, 2.0), (1, 3.0)], tick=4)
        em.write(t + 0x200F4, bytes([is_run]))
        set_handler(em, ev(0x54E1DA), VEH, CONTROL)
        fk.rets[0x4F16C0] = expr
        fk.rets[0x4F16E0] = value
        return (t,)
    check(e, fk, 0x4F3B40, "AnimTask_Run", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("addr,name", [(0x4F3BA0, "AnimTask_RegisterSymbols"), (0x4F3C20, "AnimTask_RemoveSymbols")])
@pytest.mark.parametrize("registered", [0, 1])
def test_symbols(addr, name, registered):
    def setup(em):
        t = anim(em)
        em.write(t + 0x200F5, bytes([registered]))
        return (t,)
    check(e, fk, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("target", [0, 1, 2])
def test_get_state_size(target):
    def setup(em):
        t = anim(em, target=target)
        set_handler(em, ev(0x54E1DD), VEH, SIZE if target == 2 else 0)
        return t, em.alloc(4)
    check(e, fk, 0x4F3CA0, "AnimTask_GetStateSize", setup=setup, check_eax=False)


@pytest.mark.parametrize("target", [0, 1, 2])
@pytest.mark.parametrize("addr,name,slot,handler", [(0x4F3D30, "AnimTask_SaveState", 0x54E1DE, SAVE),
                                                    (0x4F3DB0, "AnimTask_LoadState", 0x54E1DF, LOAD)])
def test_state(addr, name, slot, handler, target):
    def setup(em):
        rng = random.Random(4)
        t = anim(em, [(i % 40, i * 0.25) for i in range(20)], wait=2.0, pos=3.0, tick=17, target=target)
        set_handler(em, ev(slot), VEH, handler if target == 2 else 0)
        buf = em.alloc(0x200)
        em.write(buf, bytes(rng.randrange(256) for _ in range(0x200)))
        msg = em.alloc(20)
        em.write(msg, struct.pack("<5I", buf, 33, 0x3333, 0x4444, 0x5555))
        return t, msg
    check(e, fk, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("target", [0, 1, 2])
def test_send_control(target):
    def setup(em):
        t = anim(em, target=target, tick=9)
        set_handler(em, ev(0x54E1DA), VEH, CONTROL if target == 2 else 0)
        return (t,)
    check(e, fk, 0x4F4060, "AnimTask_SendControl", setup=setup, check_eax=False)


KEYS = [
    [],
    [(0, 1.0)],
    [(0, 1.0), (1, 2.0), (0x20, 3.0), (2, 4.0), (0x20, 1.0), (3, 5.0)],
    [(0x20, 2.5), (5, -1.0), (0x20, 1.5), (6, 0.5), (0x20, 0.49)],
    [(0x20, -2.0), (7, 7.0), (0x20, 0.0), (8, 8.0)],
    [(0x21, 2.0), (9, 9.0), (10, 10.0), (0x21, 4.5)],
    [(31, 1.0), (0x20, float("nan")), (4, 4.0)],
    [(-1, 1.0), (0x20, 1.0), (4, 4.0)],
]
STARTS = [(0.0, 0.0), (1.0, 0.0), (2.0, 1.0), (1.0, 2.5), (0.5, 3.0), (-1.0, 1.0), (float("nan"), 0.0),
          (0.0, 9.0)]


@pytest.mark.parametrize("cw", ROUND_CWS)
@pytest.mark.parametrize("start", STARTS)
@pytest.mark.parametrize("keys", KEYS)
def test_step(keys, start, cw):
    def setup(em):
        t = anim(em, keys, wait=start[0], pos=start[1], tick=3)
        set_handler(em, ev(0x54E1DA), VEH, CONTROL)
        return (t,)
    check(e, fk, 0x4F3F60, "AnimTask_Step", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", ROUND_CWS[:2])
@pytest.mark.parametrize("tick,cur", [(0, 0), (0, 5), (3, 5), (5, 5), (8, 5), (2, 0), (-1, 4), (-3, -5), (1, -1)])
def test_seek(tick, cur, cw):
    def setup(em):
        t = anim(em, KEYS[3], wait=1.0, pos=1.0, tick=cur)
        set_handler(em, ev(0x54E1DA), VEH, CONTROL)
        return t, tick
    check(e, fk, 0x4F3EF0, "AnimTask_Seek", setup=setup, check_eax=False, cw=cw)
