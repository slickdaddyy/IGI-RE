import random
import struct

import pytest

from emu import Emu, ErrorShown, compare
from test_missile_stubs import (R, PyFakes, add_child, f32, f64, i32, log, model, set_handler, task, u8, u16,
                                words)

emu = Emu()
fakes = PyFakes(emu)

MT, VT = 0x55, 0x77  # Missile and Vehicle type ids
WORLD, PARENT, SOUND, SMOKE, PHYSF = 0x5A5A0000, 0x5B5B0000, 0x5C5C0000, 0x5D5D0000, 0x5E5E0000
EV = dict(weaponconfig=0x41, wsize=0x42, winit=0x43, fire=0x44, ev7=0x45, ev4fc840=0x46, table=0x47, changed=0x48,
          args=0x49, size=0x4A, move=0x4B)
REALS = emu.alloc(8 * 64)


def deref_log(*spec):
    """fn for PyFakes.fake: spec items are 'a' (log arg) or ('d', n) (log n words at the arg) or
    ('s', n) (log the arg's first n bytes as one masked word list) or None (skip)."""
    def run(e, args):
        out = []
        for a, s in zip(args, spec):
            if s == 'a':
                out.append(a)
            elif s and s[0] == 'd':
                out += words(e, a, s[1])
        log(e, out)
        return 0
    return run


def ret_after(addr, spec, value):
    def run(e, args):
        log(e, [addr])
        deref_log(*spec)(e, args)
        return R.get(addr, value)
    return run


def drop_points(e, a):
    log(e, [0x4CABF0, a[0]] + words(e, a[2], 6) + [a[3]] + words(e, a[4], 2) + [a[5], e.u32(a[6] + 4), a[7]])
    i32(e, a[1] + 0x58, R.get("found", 0))


def out_vec3f(e, a):
    log(e, [0x414ED0, a[1], a[2]])
    e.write(a[0], struct.pack("<3f", *R["v3f"][a[2]]))


def out_vec3d(e, a):
    log(e, [0x414E20, a[1], a[2]])
    e.write(a[0], struct.pack("<3d", *R["v3d"]))


def get_string(e, a):
    log(e, [0x4B8A80, a[0], a[1], a[2], a[3]])
    s = R["strs"][a[1]].encode()[: a[3] - 1] + b"\0"
    e.write(a[2], s)


def find_record(addr):
    def run(e, a):
        log(e, [addr, a[0], a[1]] + (words(e, a[2], 2) + [e.u32(a[2] + 8) & 0xFF] if len(a) > 2 else []))
        return R.get(addr, 0)
    return run


L = lambda addr, *spec: (len(spec), 0, (), lambda e, a: (log(e, [addr]), deref_log(*spec)(e, a))[1], False)

FAKES = {  # address: (nargs, return, masks, fn, logged)
    0x401A20: (1, 0, (0xFFFF,)), 0x401900: (4, 0xABC0000 | MT, (~0, ~0, 0xFFFF)), 0x401400: (3, 0, (0xFFFF, 0xFF)),
    0x401530: (3, 0, (0xFFFF,)), 0x4015F0: (1,), 0x401AE0: (1, 0, (), lambda e, a: a[0] + 3), 0x401B20: (1,),
    0x4012A0: (3, 0, (~0, 0xFFFF), lambda e, a: R["new"]),
    0x4071D0: (0, EV["weaponconfig"]), 0x413B00: (0, EV["wsize"]), 0x413B10: (0, EV["winit"]),
    0x413B20: (0, EV["fire"]), 0x416830: (0, EV["ev7"]), 0x4FC840: (0, EV["ev4fc840"]), 0x4F1A60: (0, EV["table"]),
    0x4F1A70: (0, EV["changed"]), 0x4EE0E0: (0, EV["args"]), 0x4EE0F0: (0, EV["size"]), 0x4D9FB0: (0, EV["move"]),
    0x4F45F0: (0, 0xABC0000 | VT), 0x416920: (0, WORLD), 0x4F0EE0: (0, PARENT),
    0x414E20: (3, 0, (), out_vec3d, False), 0x414ED0: (3, 0, (), out_vec3f, False),
    0x4169D0: (1, 0x5150), 0x480FE0: L(0x480FE0, ('d', 6), *['a'] * 8, None, *['a'] * 6), 0x4B8810: (2,),
    0x4B8A20: (2, 0, (), lambda e, a: R["ints"][a[1]]), 0x4B8A80: (4, 0, (), get_string, False),
    0x4C1830: (1,), 0x4C5710: (3,), 0x4C6940: L(0x4C6940, 'a', 'a', ('d', 6), 'a'),
    0x4C7140: L(0x4C7140, 'a', 'a', ('d', 6), 'a', 'a'), 0x4C7560: (2,), 0x4CABF0: (8, 0, (), drop_points, False),
    0x4CEC10: (1, 0, (), lambda e, a: R.get(0x4CEC10, 0)), 0x4E6B00: (2, SOUND, (), lambda e, a: R.get(0x4E6B00, 0)),
    0x4E6C30: (2,), 0x4EC2C0: L(0x4EC2C0, 'a', 'a', 'a', 'a', 'a', ('d', 3)), 0x4ECDB0: (1,),
    0x4ECF50: L(0x4ECF50, 'a', ('d', 6)), 0x4EE140: (1, 0, (), lambda e, a: R.get(0x4EE140, 0)),
    0x4F4640: (1,), 0x4F4660: (1,), 0x4F4680: (1,), 0x4F4800: (1,), 0x4F4820: (1,), 0x4FB700: (2,),
    0x4FC0E0: (7, 0, (), lambda e, a: (log(e, [0x4FC0E0] + a[:6] + words(e, a[6] + 4, 4)), 0)[1], False),
    0x4FC7B0: (1, 0, (), lambda e, a: R.get(0x4FC7B0, 0)),
    0x4FDF00: (13, 0, (), ret_after(0x4FDF00, ['a', 'a', 'a', ('d', 3), ('d', 3), ('d', 2)] + ['a'] * 7, SMOKE),
               False),
    0x4FE120: L(0x4FE120, 'a', ('d', 6), ('d', 3), 'a', 'a', 'a', 'a', 'a', 'a'), 0x4FE5D0: (2,),
    0x4FE6B0: (3, 0, (), find_record(0x4FE6B0), False), 0x4FE730: (2, 0, (), find_record(0x4FE730), False),
}
for addr, spec in FAKES.items():
    nargs, ret, masks, fn, logged = (spec + (0, (), None, True)[len(spec) - 1:])[:5]
    fakes.fake(addr, nargs, ret, masks, fn, logged)
# Script_GetArgReal(args, i) returns REALS[i]
stub = fakes.slot(lambda e: log(e, [0x4B8A50, e.arg(0), e.arg(1)]),
                  b"\x8b\x44\x24\x08\xdd\x04\xc5" + struct.pack("<I", REALS) + b"\xc3")
fakes.patch(0x4B8A50, stub)
RUN = [fakes.logger(1) for _ in range(3)]
ARG = [fakes.logger(2, deref=(1, 3)) for _ in range(3)]
ARG2 = [fakes.logger(2) for _ in range(3)]
emu.baseline = emu.snapshot()


def check(orig, ours, setup, **kw):
    kw.setdefault("check_eax", False)
    return compare(emu, orig, ours, setup=setup, normalized=True, **kw)


def base(e):
    R.clear()
    e.call(0x4B0BA0)  # Mem_Init
    u16(e, 0x53BB8C, MT)


class Obj:
    pass


def missile(e, rng, mtype=0, kind=0):
    """A Missile with its physics type and config, random state."""
    o = Obj()
    o.m = m = task(e, 0x898, MT)
    o.phys = e.alloc(0x2D8)
    o.cfg = cfg = e.alloc(0x250)
    i32(e, o.phys + 0x2D4, cfg)
    i32(e, m + 0x1B4, o.phys)
    i32(e, m + 0x6C, model(e, rng.uniform(100, 5000), kind))
    e.write(m + 0x98, struct.pack("<3f", *[rng.choice([0.5, 1.0, 2.0, 1.5]) for _ in range(3)]))
    e.write(m + 0x20, struct.pack("<3d", *[rng.uniform(-1e6, 1e6) for _ in range(3)]))
    a, b, c = rng.uniform(-3, 3), rng.uniform(-3, 3), rng.uniform(-3, 3)
    e.write(m + 0x70, struct.pack("<9fi", *rotation(a, b, c), rng.randint(0, 50)))
    e.write(m + 0x828, struct.pack("<3d", *[rng.uniform(-1e6, 1e6) for _ in range(3)]))
    e.write(m + 0x840, struct.pack("<3d", *[rng.uniform(-1, 1) for _ in range(3)]))
    e.write(m + 0xF0, struct.pack("<12f", *[rng.uniform(-500, 500) for _ in range(12)]))
    e.write(m + 0x720, struct.pack("<8f", *[rng.choice([0, rng.uniform(0, 1.5)]) for _ in range(8)]))
    e.write(m + 0x734, struct.pack("<3f", *[rng.choice([0, -1, 1, rng.uniform(-1, 1)]) for _ in range(3)]))
    f32(e, m + 0x864, rng.choice([0, rng.uniform(0, 100)]))
    i32(e, m + 0x868, SMOKE)
    f32(e, m + 0x880, rng.uniform(0, 4))
    f32(e, m + 0x884, rng.uniform(100, 3000))
    f32(e, m + 0x88C, rng.uniform(0.5, 3))
    i32(e, m + 0x894, 0x89400)
    e.write(cfg, struct.pack("<i9f", 0, *[rng.uniform(0, 1) if i % 2 else rng.uniform(-90, 90) for i in range(6)],
                             rng.uniform(0, 3), rng.uniform(0, 0.1), rng.uniform(0, 0.1)))
    i32(e, cfg + 0x28, rng.randint(0, 4))
    e.write(cfg + 0x2C, struct.pack("<56f", *[rng.uniform(-10, 10) for _ in range(56)]))
    i32(e, cfg + 0x118, rng.randint(0, 1))
    i32(e, cfg + 0x11C, mtype)
    e.write(cfg + 0x120, struct.pack("<7f", *[rng.uniform(0.1, 50) for _ in range(7)]))
    e.write(cfg + 0x23C, b"missile_snd\0")
    i32(e, cfg + 0x24C, 0x24C00)
    return o


def rotation(a, b, c):
    import math
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    return [cb * cc, -cb * sc, sb, sa * sb * cc + ca * sc, -sa * sb * sc + ca * cc, -sa * cb,
            -ca * sb * cc + sa * sc, ca * sb * sc + sa * cc, ca * cb]


# --- small functions ------------------------------------------------------------

def test_hitzone_close():
    def setup(e):
        base(e)
        u16(e, 0x53BB6C, 0x1234)
    check(0x427FB0, "HitZone_Close", setup)


@pytest.mark.parametrize("case", range(6))
def test_coll_filter(case):
    def setup(e):
        base(e)
        t = task(e, 0x20, MT if case == 4 else 3)
        msg = e.alloc(16)
        e.write(msg, struct.pack("<4I", 0x77777777, t if case == 1 else 0x111, t if case == 2 else 0x222,
                                 t if case == 3 else 0x333))
        return t, msg
    check(0x427FC0, "Missile_CollFilter", setup)


def test_open():
    def setup(e):
        base(e)
    check(0x427FF0, "Missile_Open", setup)


def test_close_and_get_type():
    def setup(e):
        base(e)
        i32(e, 0x57BC94, 0x9494)
    check(0x429A00, "Missile_Close", setup)
    check(0x429A20, "Missile_GetType", base, check_eax=True, eax_mask=0xFFFF)


@pytest.mark.parametrize("fn", [(0x429720, "Missile_GetSize"), (0x429890, "Missile_GetParams"),
                                (0x429910, "Missile_GetModelSize")])
def test_getters(fn):
    def setup(e):
        base(e)
        i32(e, 0x57BC94, 0x9494)
        return 0x1234, e.alloc(4)
    check(fn[0], fn[1], setup)


@pytest.mark.parametrize("seed", range(4))
def test_init_and_reset(seed):
    def setup(e):
        base(e)
        o = missile(e, random.Random(seed))
        return (o.m,)
    check(0x428270, "Missile_Init", setup)
    check(0x428280, "Missile_Reset", setup)


@pytest.mark.parametrize("sound", [0, SOUND])
def test_update_and_start_sound(sound):
    def setup(e):
        base(e)
        o = missile(e, random.Random(1))
        i32(e, o.m + 0x890, sound)
        R[0x4E6B00] = sound
        return (o.m,)
    check(0x428610, "Missile_UpdateSound", setup)
    check(0x429D80, "Missile_StartSound", setup)


@pytest.mark.parametrize("refs", [0, 1, 2, 3, 7])
def test_delete_handler(refs):
    def setup(e):
        base(e)
        o = missile(e, random.Random(2))
        for i, off in enumerate((0x858, 0x85C, 0x860)):
            i32(e, o.m + off, 0x8580 + i if refs & (1 << i) else 0)
        return (o.m,)
    check(0x428CA0, "Missile_DeleteHandler", setup)


# --- steering -------------------------------------------------------------------

def test_steer_none():
    def setup(e):
        base(e)
        return (missile(e, random.Random(3)).m,)
    check(0x428910, "Missile_SteerNone", setup)


@pytest.mark.parametrize("keys", range(16))
def test_steer_keyboard(keys):
    def setup(e):
        base(e)
        for i, k in enumerate((0xCB, 0xCD, 0xC8, 0xD0)):
            u8(e, 0xC28C62 + k, 0x80 | i if keys & (1 << i) else 0x7F)
        return (missile(e, random.Random(4)).m,)
    check(0x428A00, "Missile_SteerKeyboard", setup)


def near(rng, o_pos, spread):
    return [p + rng.uniform(-spread, spread) for p in o_pos]


@pytest.mark.parametrize("seed", range(30))
def test_steer_to(seed):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        o = missile(e, rng)
        pos = struct.unpack("<3d", e.read(o.m + 0x20, 24))
        t = e.alloc(24)
        tgt = pos if seed == 0 else near(rng, pos, rng.choice([1.0, 1000.0, 1e6]))
        e.write(t, struct.pack("<3d", *tgt))
        return o.m, t
    check(0x428B60, "Missile_SteerTo", setup)
    check(0x428A60, "Missile_SteerBallistic", setup)
    check(0x428AD0, "Missile_SteerAntitank", setup)


@pytest.mark.parametrize("seed", range(20))
def test_steer_hind(seed):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        o = missile(e, rng)
        pos = struct.unpack("<3d", e.read(o.m + 0x20, 24))
        e.write(o.m + 0x828, struct.pack("<3d", *near(rng, pos, 1e5)))
        return (o.m,)
    check(0x428930, "Missile_SteerHind", setup)


# --- smoke ----------------------------------------------------------------------

@pytest.mark.parametrize("seed", range(24))
def test_emit_smoke(seed):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        o = missile(e, rng, kind=seed % 3)
        i32(e, o.m + 0x878, rng.randint(-1, 4))
        f32(e, o.m + 0xF8, rng.uniform(0, 3) * e_f32(e, o.m + 0x884))
        R["v3f"] = [[rng.uniform(-3000, 3000) for _ in range(3)] for _ in range(2)]
        R["v3d"] = [rng.uniform(-3000, 3000) for _ in range(3)]
        return (o.m,)
    check(0x428630, "Missile_EmitSmoke", setup)


def e_f32(e, p):
    return struct.unpack("<f", e.read(p, 4))[0]


@pytest.mark.parametrize("seed", range(4))
def test_init_smoke(seed):
    def setup(e):
        base(e)
        return (missile(e, random.Random(seed)).m,)
    check(0x429DC0, "Missile_InitSmoke", setup)


@pytest.mark.parametrize("count", range(5))
def test_copy_parts(count):
    def setup(e):
        base(e)
        o = missile(e, random.Random(count))
        i32(e, o.cfg + 0x28, count)
        return (o.m,)
    check(0x429EF0, "Missile_CopyParts", setup)


# --- movement -------------------------------------------------------------------

def add_children(e, parent, handlers, slot):
    for i, h in enumerate(handlers):
        c = task(e, 0x20, 0x60 + i, parent)
        set_handler(e, slot, 0x60 + i, h)


@pytest.mark.parametrize("seed", range(40))
def test_move(seed):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        o = missile(e, rng)
        if seed % 4 == 0:
            f32(e, o.m + 0x864, 0)
        if seed % 5 == 0:
            e.write(o.m + 0x734, struct.pack("<3f", 0, 0, 0))
            e.write(o.m + 0x724, struct.pack("<3f", 0, 0, 0))
        R[0x4FC7B0] = seed % 3 == 0
        for off in (0x858, 0x85C):
            if rng.random() < 0.6:
                ref = e.alloc(4)
                i32(e, ref, rng.choice([0, 0x7A5C]))
                i32(e, o.m + off, ref)
        if seed % 2:
            coll = task(e, 0x20, 9)
            i32(e, o.m + 0xE8, coll)
            add_children(e, coll, ARG[: 1 + seed % 3], EV["move"])
        return (o.m,)
    check(0x428D00, "Missile_Move", setup)


@pytest.mark.parametrize("seed", range(36))
def test_run(seed):
    def setup(e):
        base(e)
        rng = random.Random(100 + seed)
        o = missile(e, rng, mtype=seed % 6, kind=seed % 3)
        R["v3f"] = [[rng.uniform(-3000, 3000) for _ in range(3)] for _ in range(2)]
        R["v3d"] = [rng.uniform(-3000, 3000) for _ in range(3)]
        R["found"] = seed % 7 == 0
        R[0x4FC7B0] = 1
        follow = seed % 4
        if follow:
            ref = e.alloc(4)
            i32(e, o.m + 0x860, ref)
            if follow > 1:
                t = e.alloc(0x40)
                e.write(t + 0x20, struct.pack("<3d", *near(rng, struct.unpack("<3d", e.read(o.m + 0x20, 24)), 1e5)))
                i32(e, ref, t)
        frame = e.alloc(0x80)
        i32(e, 0xBC2360, frame)
        i32(e, frame + 0x68, rng.choice([0, 0x20, 5, 0x41]))
        i32(e, o.m + 0x1B8, seed % 5 == 1)
        add_children(e, o.m, RUN[: seed % 3], 1)
        return (o.m,)
    check(0x428390, "Missile_Run", setup)


# --- params and scripts ---------------------------------------------------------

@pytest.mark.parametrize("case", ["found", "empty", "star", "missing"])
def test_params_changed(case):
    def setup(e):
        base(e)
        o = missile(e, random.Random(5))
        name = {"found": b"m_one", "empty": b"", "star": b"x*abc", "missing": b"m_two"}[case]
        e.write(o.m + 0x154, name + b"\0")
        R[0x4CEC10] = model(e, 1234.5) if case == "found" else 0
        R[0x4EE140] = PHYSF
        coll = task(e, 0x20, 9)
        i32(e, o.m + 0xE8, coll)
        add_children(e, coll, ARG2[:2], EV["changed"])
        f32(e, o.m + 0x740, 0.625)
        return (o.m,)
    check(0x429540, "Missile_ParamsChanged", setup)


def test_parse_args():
    def setup(e):
        base(e)
        cfg = e.alloc(0x250)
        msg = e.alloc(12)
        e.write(msg, struct.pack("<3I", cfg, 0xA0A0, 3))
        R["ints"] = {3: 1, 4: 2}
        for i in range(5, 12):
            f64(e, REALS + 8 * i, 1.25 + i * 0.7)
        R["strs"] = {12: "smoke_sprite", 13: "a_sound_name_longer_than_16"}
        set_handler(e, EV["args"], VT, ARG[0])
        return 0x1234, msg
    check(0x429730, "Missile_ParseArgs", setup)


def test_read_model_arg():
    def setup(e):
        base(e)
        out = e.alloc(16)
        msg = e.alloc(12)
        e.write(msg, struct.pack("<3I", out, 0xA0A0, 2))
        R["strs"] = {2: "model_name_too_long_for_it"}
        return 0x1234, msg
    check(0x4298E0, "Missile_ReadModelArg", setup)


@pytest.mark.parametrize("case", range(4))
def test_config_weapon(case):
    def setup(e):
        base(e)
        wt = e.alloc(0x6E8)
        if case & 1:
            i32(e, wt + 0x6E4, e.call(0x4B0C60, 16, 4)[0])
        rec = e.alloc(0x200)
        e.write(rec + 0x124, b"short\0" if case & 2 else b"exactly15chars!\0")
        R[0x4FE730 if case < 2 else 0x4FE6B0] = rec
        msg = e.alloc(8)
        e.write(msg, struct.pack("<2I", 0xC0C0, wt))
        return 0x1234, msg
    # the bytes after a short name come from uninitialised stack
    check(0x429920, "Missile_ConfigWeapon", setup)


# --- creation -------------------------------------------------------------------

@pytest.mark.parametrize("seed", range(24))
def test_new(seed):
    def setup(e):
        base(e)
        rng = random.Random(200 + seed)
        o = missile(e, rng)
        R["new"] = o.m
        R[0x4CEC10] = model(e, 2000.0)
        R[0x4EE140] = o.phys
        i32(e, o.cfg + 0x11C, seed % 3)
        e.write(0x9422FD, bytes([seed % 2]))
        has_aim = seed % 5 or seed % 3 == 1
        pos, mat, aim = e.alloc(24), e.alloc(40), e.alloc(24)
        e.write(pos, struct.pack("<3d", *[rng.uniform(-1e6, 1e6) for _ in range(3)]))
        e.write(mat, struct.pack("<9fi", *rotation(*[rng.uniform(-3, 3) for _ in range(3)]), 7))
        e.write(aim, struct.pack("<3d", *[rng.uniform(-1e6, 1e6) for _ in range(3)]))
        name = e.alloc(16)
        e.write(name, b"missile_x\0")
        owner = 0x0A000 if seed & 4 else 0
        target = 0x0B000 if seed & 8 else 0
        follow = 0x0C000 if seed & 16 else 0
        speed = struct.unpack("<I", struct.pack("<f", rng.uniform(1, 50)))[0]
        return PARENT, owner, target, pos, mat, speed, aim if has_aim else 0, follow, name, 0x99
    ro, rn = check(0x429A30, "Missile_New", setup, check_eax=True)


def test_new_missing_model():
    def setup(e):
        base(e)
        o = missile(e, random.Random(9))
        R["new"] = o.m
        name = e.alloc(16)
        e.write(name, b"nothing\0")
        return PARENT, 0, 0, e.alloc(24), e.alloc(40), 0, 0, 0, name, 0
    for fn in (0x429A30, "Missile_New"):
        emu.reset()
        args = setup(emu)
        with pytest.raises(ErrorShown, match='VirModel'):
            emu.call(fn, *args)


@pytest.mark.parametrize("seed", range(4))
def test_fire(seed):
    def setup(e):
        base(e)
        rng = random.Random(300 + seed)
        o = missile(e, rng)
        R["new"] = o.m
        R[0x4CEC10] = model(e, 2000.0)
        R[0x4EE140] = o.phys
        i32(e, o.cfg + 0x11C, seed % 3)
        w = e.alloc(0x6E8)
        f32(e, w + 0x660, 12.5)
        name = e.alloc(16)
        e.write(name, b"rocket\0")
        i32(e, w + 0x6E0, name)
        pos, mat, aim = e.alloc(24), e.alloc(40), e.alloc(24)
        e.write(pos, struct.pack("<3d", 1.5, 2.5, 3.5))
        e.write(mat, struct.pack("<9fi", *rotation(0.1, 0.2, 0.3), 0))
        e.write(aim, struct.pack("<3d", 100.5, 200.5, 300.5))
        msg = e.alloc(0x30)
        e.write(msg, struct.pack("<9I", 0xA000, 0xB000, pos, 0x3333, 0x4444, mat, aim, 0xC000 * (seed & 1), w))
        return 0x1234, msg
    check(0x4298A0, "Missile_Fire", setup)
