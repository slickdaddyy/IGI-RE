import random
import struct

import pytest

from emu import Emu
from test_fxobj_util import init_emu, setup_init
from test_menu_util import LOG, handler, pyfake, set_handler, task, trap
from test_patrol_util import RET, d64, f32, fake, logfake, run, u32
from test_vehicle_util import floatfake

HT = 0x60  # Human type
TT = 0x61  # type of a used task
EV3, EV4, EV_CHANGED = 0x33, 0x34, 0x35

e = Emu()
CFG = {}

for a, n in ((0x489E70, 3), (0x489EA0, 4), (0x4D61D0, 5), (0x4D65B0, 3), (0x413C80, 1), (0x4F2350, 2),
             (0x463770, 1), (0x4E6C30, 2), (0x48A330, 1), (0x48A320, 1), (0x4F2390, 1), (0x4F2070, 3),
             (0x4F2290, 1), (0x45D5B0, 4), (0x463760, 2), (0x464700, 1), (0x4635F0, 1), (0x4E6C60, 3),
             (0x4E6CA0, 2), (0x4EC190, 6), (0x489C30, 2), (0x4636A0, 2), (0x444BC0, 1), (0x447DA0, 1),
             (0x4D96F0, 1), (0x4C7140, 5), (0x4D38C0, 2)):
    logfake(e, a, n)
logfake(e, 0x4D9620, 4, masks={2: 0xFFFF})


def warning(e):
    fmt = e.read_cstr(e.arg(0))
    if LOG:
        LOG[-1].append(("warning", fmt) + tuple(e.arg(i + 1) for i in range(fmt.count(b"%"))))
    e.ret(0)


pyfake(e, 0x4AF810, warning)
logfake(e, 0x4638A0, 3, strings=(1,))
fake(e, 0x4F23D0, 2, skip=(1,))  # Weapon_GetAmmo fills a stack buffer
for a, n in ((0x4D74F0, 3), (0x4F2060, 2), (0x4F0EE0, 0), (0x489E10, 2), (0x4D64C0, 2), (0x416D20, 0),
             (0x463180, 1), (0x416FB0, 0), (0x413CC0, 2), (0x4F1050, 1), (0x4F1030, 1), (0x416920, 0)):
    fake(e, a, n, effect=lambda e, args, a=a: CFG.get(a, 0))
for a, n in ((0x4D6520, 2), (0x4D64E0, 2), (0x4B4770, 1)):
    floatfake(e, a, n, effect=lambda e, args, a=a: CFG.get(a, 0.0))


def sound_name(e, args):
    if LOG:
        LOG[-1].append(("sound", e.read_cstr(args[1])))
    return CFG.get(0x4E6B00, 0)


fake(e, 0x4E6B00, 2, skip=(1,), effect=sound_name)

STATES = [handler(e, "state%d" % i, 2) for i in range(13)]


def use_result(e):
    if LOG:
        LOG[-1].append(("ev3", e.normalize(e.arg(0))))
    u32(e, e.arg(1), CFG.get("result", 0))
    e.ret(0)


def use_msg(e):
    m = e.read(e.arg(1), 0x48)
    if LOG:
        LOG[-1].append(("ev4", e.normalize(e.arg(0)), m[:4], m[8:0x3C], e.normalize(struct.unpack("<I", m[0x3C:0x40])[0]),
                        struct.unpack("<2I", m[0x40:0x48])))
    e.ret(0)


u32(e, 0x5BDAF8, HT)
set_handler(e, 0, HT, handler(e, "create", 2))
set_handler(e, 1, HT, handler(e, "run", 1))
set_handler(e, EV_CHANGED, HT, handler(e, "changed", 2))
set_handler(e, EV3, TT, trap(e, "use_result", use_result))
set_handler(e, EV4, TT, trap(e, "use_msg", use_msg))
set_handler(e, EV4, TT + 1, trap(e, "use_msg2", use_msg))
e.write(0x54E0E5, bytes([EV_CHANGED]))
e.baseline = e.snapshot()


def ev_setup(e):
    # Game_GetEvent3 / 4 read these bytes
    for addr, v in ((0x416810, EV3), (0x416800, EV4)):
        code = e.read(addr, 5)
        assert code[0] == 0xA0
        e.write(struct.unpack("<I", code[1:5])[0], bytes([v]))


def soldier(e, state=0, prev=0, bits=0, fields=None, cfg=None):
    CFG.clear()
    CFG.update({0x4F0EE0: 0x5150, 0x4E6B00: 0x50D0, 0x4D74F0: 1, 0x4F2060: 0x7700, 0x4D6520: 0.3000001,
                0x4D64E0: 0.7512345, 0x416920: 0x9999})
    CFG.update(cfg or {})
    ev_setup(e)
    h = task(e, 0xD50, 0x62)
    table = e.alloc(13 * 4)
    u32(e, table, *STATES)
    motion = e.alloc(0x60)
    u32(e, motion + 4, 0)
    u32(e, motion + 0x4C, 1, 2, 3, 4, 5)
    ground = e.alloc(0x200)
    for i in range(6):
        e.write(ground + 0x54 + 0x20 * i, b"step_%d\0" % i)
    e.write(ground + 0x114, b"land_dirt\0")
    hit = e.alloc(16)
    u32(e, hit, 0, 0x4A, 0x5B, 0x6C)
    CFG[0x489E10] = hit
    d64(e, h + 0x20, 100.5, -20.25, 3.125)
    d64(e, h + 0xD38, 1.5, 2.25, -3.75)
    u32(e, h + 0x70, motion)
    u32(e, h + 0x6EC, ground)
    u32(e, h + 0x2EC, 0x454BA0, table, bits, prev, state, 0)
    u32(e, h + 0x2E8, 3)
    u32(e, h + 0x534, *range(0x534, 0x534 + 28, 4))
    u32(e, h + 0x688, 0x11, 0x22, 0x33)
    u32(e, h + 0x664, 0x44, 0x55, 0x66)
    u32(e, h + 0xCF8, 0xFFFF0006, 0xFFFFFFFF)
    u32(e, h + 0xD0C, 0xFFFFFFFF, 0)
    for off, v in (fields or {}).items():
        if isinstance(v, bytes):
            e.write(h + off, v)
        elif isinstance(v, float):
            f32(e, h + off, v)
        else:
            u32(e, h + off, v)
    return h


def call(addr, name, **kw):
    args = kw.pop("args", None)

    def setup(e):
        h = soldier(e, **kw)
        return args(e, h) if args else (h + 0x2EC, h)
    run(e, addr, name, setup)


BITS = [0x4, 0x10000, 0x20000, 0x200000, 0x800, 0x80000, 0x40000, 0x80, 0x1, 0x200, 0x20, 0x1000, 0x100000]


def bit_sets(n=48, seed=1):
    rng = random.Random(seed)
    out = [0, 4, 0x10000, 0x20004]
    for _ in range(n):
        b = 4 if rng.random() < 0.85 else 0
        for bit in BITS[1:]:
            if rng.random() < 0.25:
                b |= bit
        out.append(b)
    return out


UPDATES = [(0x453AE0, "SoldierFemale_StandUpdate"), (0x453CB0, "SoldierFemale_WalkUpdate"),
           (0x453E50, "SoldierFemale_RunUpdate"), (0x454220, "SoldierFemale_AimUpdate"),
           (0x454330, "SoldierFemale_RaiseUpdate"), (0x454410, "SoldierFemale_LowerUpdate"),
           (0x454630, "SoldierFemale_HitUpdate")]


@pytest.mark.parametrize("addr,name", UPDATES)
@pytest.mark.parametrize("bits", bit_sets())
def test_update(addr, name, bits):
    call(addr, name, state=3, prev=1, bits=bits)


@pytest.mark.parametrize("bits", bit_sets(24, 2))
@pytest.mark.parametrize("fire", [0.0, 1.0, float("nan")])
def test_fire_update(bits, fire):
    call(0x453FB0, "SoldierFemale_FireUpdate", state=3, bits=bits, fields={0x1C8: fire})


@pytest.mark.parametrize("bits", bit_sets(24, 3))
def test_reload_update(bits):
    call(0x454100, "SoldierFemale_ReloadUpdate", state=4, bits=bits)


@pytest.mark.parametrize("bits", bit_sets(16, 4))
@pytest.mark.parametrize("ground,sound", [(True, 0x50D0), (True, 0), (False, 0)])
def test_fall_update(bits, ground, sound):
    call(0x454510, "SoldierFemale_FallUpdate", state=8, bits=bits,
         fields=None if ground else {0x6EC: 0}, cfg={0x4E6B00: sound})


@pytest.mark.parametrize("bits", bit_sets(24, 5))
@pytest.mark.parametrize("anim", [0, -1])
def test_anim_update(bits, anim):
    call(0x454880, "SoldierFemale_AnimUpdate", state=11, bits=bits, fields={0xD0C: anim & 0xFFFFFFFF})


@pytest.mark.parametrize("bits", [0, 1, 4, 5])
def test_die_update(bits):
    call(0x4547B0, "SoldierFemale_DieUpdate", state=10, bits=bits)


ENTERS = [(0x453F80, "SoldierFemale_FireEnter"), (0x4540D0, "SoldierFemale_ReloadEnter"),
          (0x4541F0, "SoldierFemale_AimEnter"), (0x454300, "SoldierFemale_RaiseEnter"),
          (0x4543E0, "SoldierFemale_LowerEnter"), (0x4544C0, "SoldierFemale_FallEnter"),
          (0x4545E0, "SoldierFemale_HitEnter"), (0x454940, "SoldierFemale_UseEnter")]


@pytest.mark.parametrize("addr,name", ENTERS)
def test_enter(addr, name):
    call(addr, name, state=5, prev=2)


@pytest.mark.parametrize("idle,has", [(-1, 1), (12, 1), (12, 0)])
def test_stand_enter(idle, has):
    call(0x453A30, "SoldierFemale_StandEnter", fields={0xCFC: idle & 0xFFFFFFFF}, cfg={0x4D74F0: has})
    call(0x4547F0, "SoldierFemale_AnimEnter", state=11, fields={0xD0C: idle & 0xFFFFFFFF}, cfg={0x4D74F0: has})


@pytest.mark.parametrize("prev", [0, 1, 2, 3])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_move_enter(prev, cw):
    for addr, name, state in ((0x453C40, "SoldierFemale_WalkEnter", 1), (0x453DE0, "SoldierFemale_RunEnter", 2)):
        run(e, addr, name, lambda e: (lambda h: (h + 0x2EC, h))(soldier(e, state=state, prev=prev)), cw=cw)


@pytest.mark.parametrize("weapon,ai,anim", [(0, 0, -1), (0x7700, 0, -1), (0x7700, 1, -1), (0x7700, 1, 0x33)])
def test_die_enter(weapon, ai, anim):
    def args(e, h):
        if ai:
            a = e.alloc(0x3C00)
            u32(e, a + 0x3AD0, anim & 0xFFFFFFFF)
            u32(e, h + 0x800, a)
        return (h + 0x2EC, h)
    call(0x4546C0, "SoldierFemale_DieEnter", state=10, cfg={0x4F2060: weapon}, args=args)


@pytest.mark.parametrize("seed", range(6))
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_footstep(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        fields = {0x538: rng.choice((0x3F2, 0x3F2, 0x3F1)), 0x53C: 7, 0x84C: rng.randrange(12)}
        if rng.random() < 0.2:
            fields[0x6EC] = 0
        cfg = {0x4D64C0: rng.choice((7, 7, 8)), 0x4B4770: rng.random(), 0x463180: rng.choice((0, 1, 0x100)),
               0x416FB0: rng.choice((0, 1)), 0x4E6B00: rng.choice((0, 0x50D0))}
        return (soldier(e, fields=fields, cfg=cfg),)
    run(e, 0x4538F0, "SoldierFemale_Footstep", setup, cw=cw)


def test_create():
    call(0x454B10, "SoldierFemale_Create", args=lambda e, h: (h, 0x1234))


def test_run_get_params():
    call(0x454EE0, "Human_RunHandler", args=lambda e, h: (h,))

    def args(e, h):
        u32(e, 0x5BDA84, 0x4455)
        return (h, e.alloc(4))
    call(0x454F00, "SoldierFemale_GetParams", args=args)


@pytest.mark.parametrize("spawned", [0, 1])
@pytest.mark.parametrize("idle,has", [(-1, 1), (12, 1), (12, 0)])
def test_changed(spawned, idle, has):
    call(0x454F10, "SoldierFemale_Changed", fields={0xD01: bytes([spawned]), 0xCFC: idle & 0xFFFFFFFF,
                                                    0xA0: 2.5}, cfg={0x4D74F0: has}, args=lambda e, h: (h, 0x99))


def used_task(e, type_=TT):
    return task(e, 0x40, type_)


@pytest.mark.parametrize("found,result,type_", [(0, 0, TT), (1, 0, TT), (1, 2, TT), (1, 3, TT), (1, 3, TT + 1)])
def test_use_target(found, result, type_):
    def args(e, h):
        CFG[0x4F1030] = used_task(e, type_) if found else 0
        CFG["result"] = result
        u32(e, h + 0xD10, 0x31)
        return (h,)
    call(0x454DA0, "SoldierFemale_UseTarget", state=2, args=args)


INPUT_CASES = [
    {}, {2: 1.0, 5: -1.0, 9: float("nan")}, {3: 0.5, 4: 2.0, 6: 1.0, 7: 1.0, 8: 1.0},
    {21: 4.6}, {21: -2.5}, {22: 5.0}, {23: 1.0}, {24: 3.0}, {24: 3.0, "weapon": 0x8800},
    {21: 1.0, 22: 2.0, 23: 1.0, 24: 7.4, "weapon": 0x8800, "run": 0},
]


@pytest.mark.parametrize("case", range(len(INPUT_CASES)))
@pytest.mark.parametrize("flags", [0, 0x100, 0x10000, 0x10100])
@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_update_input(case, flags, cw):
    c = INPUT_CASES[case]

    def setup(e):
        h = soldier(e, bits=0xFFFFFFFF, state=1, cfg={0x413CC0: c.get("weapon", 0), 0x4F1030: used_task(e),
                                                       "result": 3, 0x4F1050: 0x21})
        e.write(h + 0xD00, bytes([c.get("run", 1)]))
        u32(e, e.u32(h + 0x70) + 4, flags)
        for i, v in c.items():
            if isinstance(i, int):
                f32(e, h + 0x1B0 + 4 * i, v)
        return (h + 0x2EC, h)
    run(e, 0x454BA0, "SoldierFemale_UpdateInput", setup, cw=cw)


# --- registration ---------------------------------------------------------------

init = init_emu()


def setup_open(e):
    setup_init(e)
    root = e.call(0x4018F0)[0] & 0xFFFF
    t = e.call(0x401900, e.alloc(8), 0xD04, root, 0)[0] & 0xFFFF
    u32(e, 0x5BDAF8, t)
    u32(e, 0x5BDC18, e.call(0x4F1950, 3)[0])
    for i, a in enumerate((0x54E1DA, 0x53F317, 0x540914)):
        e.write(a, bytes([0x40 + i]))


def test_open():
    run(init, 0x454970, "SoldierFemale_Open", setup_open)
