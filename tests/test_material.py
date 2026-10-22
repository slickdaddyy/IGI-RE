import itertools
import random
import struct

import pytest

from emu import CALL_LOG, FAKE_BASE, Emu, ErrorShown, compare

emu = Emu()

# inputs for the stubs below; this part of the fake area is never compared
P = FAKE_BASE + 0x40000
INTS, REALS, STRS = P, P + 0x100, P + 0x200
PHASE, LENGTH, SIZE, USED, PAIR = P + 0x800, P + 0x804, P + 0x810, P + 0x814, P + 0x820
FAST_SM, FAST_P = P + 0x1000, P + 0x2000

SIZE_EV, PARSE_EV, IMPACT_EV = 0x20, 0x21, 0x22
HANDLERS = 0xA96AE0
ACTIVE, MATERIALS, MSIZE = 0x53851C, 0x569210, 0x268


def log_code(tag, items):
    """x86 that appends tag and the chosen values to the fake call log.
    items: ("arg", i) | ("byte", i) | ("deref", i, offset)"""
    log = CALL_LOG + 4
    code = b"\x8b\x0d" + struct.pack("<I", CALL_LOG)
    code += b"\xc7\x04\x8d" + struct.pack("<II", log, tag) + b"\x41"
    for it in items:
        d = 4 + 4 * it[1]
        if it[0] == "byte":
            code += b"\x0f\xb6\x44\x24" + bytes([d])
        else:
            code += b"\x8b\x44\x24" + bytes([d])
            if it[0] == "deref":
                code += b"\x8b\x80" + struct.pack("<i", it[2])
        code += b"\x89\x04\x8d" + struct.pack("<I", log) + b"\x41"
    return code + b"\x89\x0d" + struct.pack("<I", CALL_LOG)


def make_stub(e, tag, items, body=b"\xc3"):
    code = log_code(tag, items) + body
    p = e.fake_top
    e.fake_top += (len(code) + 15) & ~15
    e.write(p, code)
    return p


def stub(e, addr, items, body=b"\xc3"):
    p = make_stub(e, addr, items, body)
    e.write(addr, b"\xe9" + struct.pack("<i", p - addr - 5))
    e.uc.ctl_remove_cache(addr, addr + 5)


def imm(v):
    return struct.pack("<I", v)


# script argument readers return INTS[i], REALS[i], STRS[i]
stub(emu, 0x4B8A20, [("arg", 1)], b"\x8b\x44\x24\x08\x8b\x04\x85" + imm(INTS) + b"\xc3")
stub(emu, 0x4B8A50, [("arg", 1)], b"\x8b\x44\x24\x08\xdd\x04\xc5" + imm(REALS) + b"\xc3")
stub(emu, 0x4B8A80, [("arg", 1), ("arg", 3)], b"\x56\x57\x8b\x44\x24\x10\xc1\xe0\x06\x8d\xb0" + imm(STRS)
     + b"\x8b\x7c\x24\x14\xac\xaa\x84\xc0\x75\xfa\x8b\x44\x24\x14\x5f\x5e\xc3")
stub(emu, 0x4018C0, [("byte", 0)])  # TaskEvent_Free takes a char, the rest of the word is junk
SIZE_FN = make_stub(emu, 0x51, [("arg", 0)], b"\x8b\x44\x24\x08\x8b\x15" + imm(SIZE) + b"\x89\x10\xc3")
PARSE_FN = make_stub(emu, 0x52, [("arg", 0), ("deref", 1, 0), ("deref", 1, 4), ("deref", 1, 8)],
                     b"\x8b\x44\x24\x08\x8b\x15" + imm(USED) + b"\x89\x50\x0c\xc3")
IMPACT_FN = make_stub(emu, 0x53, [("arg", 0)] + [("deref", 1, o) for o in range(0, 24, 4)])
for addr, n in ((0x4B80B0, 1), (0x4B85B0, 1), (0x4B83D0, 1)):
    emu.fake(addr, 0, n)

# human player callees
for addr, n in ((0x4C1780, 1), (0x413A60, 2), (0x489E70, 3), (0x4D65B0, 3), (0x416D80, 0), (0x489E10, 2),
                (0x4659B0, 2), (0x413C80, 1)):
    emu.fake(addr, 0, n)
emu.fake(0x4C3440, 0x1234, 1)
stub(emu, 0x4D6520, [("arg", 0), ("arg", 1)], b"\xd9\x05" + imm(PHASE) + b"\xc3")
stub(emu, 0x4D64E0, [("arg", 0), ("arg", 1)], b"\xd9\x05" + imm(LENGTH) + b"\xc3")
stub(emu, 0x4E6B00, [("arg", 0)] + [("deref", 1, o) for o in range(0, 16, 4)])
stub(emu, 0x450CE0, [("deref", 0, o) for o in range(0, 28, 4)])
stub(emu, 0x4F23D0, [("arg", 0)], b"\x8b\x44\x24\x08\x8b\x15" + imm(PAIR) + b"\x89\x10\x8b\x15" + imm(PAIR + 4)
     + b"\x89\x50\x04\xc3")
ENTER = [make_stub(emu, 0xE000 + n, [("arg", 0), ("arg", 1), ("deref", 0, 0xC), ("deref", 0, 0x10)])
         for n in range(48)]
TABLE = P + 0x3000
emu.write(TABLE, b"".join(imm(a) for a in ENTER))
emu.write(0x538518, bytes([SIZE_EV, PARSE_EV, IMPACT_EV]))
emu.baseline = emu.snapshot()


def fake_ret(addr, v):
    emu.set_fake_ret(addr, v)


def handler(slot, type_, fn):
    emu.write(HANDLERS + 4 * (slot * 0x180 + type_), imm(fn))


def set_args(ints=(), reals=(), strs=()):
    emu.write(INTS, struct.pack("<64i", *(list(ints) + [0] * (64 - len(ints)))))
    emu.write(REALS, struct.pack("<32d", *(list(reals) + [0.0] * (32 - len(reals)))))
    for i in range(24):
        s = strs[i] if i < len(strs) else b""
        emu.write(STRS + 64 * i, s + b"\0")


def script_call(orig, ours, setup=None):
    def go(e):
        if setup:
            setup(e)
        return ret, 0, 0x5A5A0
    ret = 0x20F00000
    ign = [(ret + 4, ret + 8), (ret + 20, ret + 24)]
    compare(emu, orig, ours, setup=go, normalized=True, ignore=ign)


# --- GameMaterial ----------------------------------------------------------

@pytest.mark.parametrize("index", [0, 5, 31])
def test_set_active(index):
    set_args(ints=[index])
    script_call(0x407DE0, "GameMaterial_SetActive")
    assert emu.u32(ACTIVE) == index


@pytest.mark.parametrize("active", [0, 7])
def test_define_q(active):
    set_args(ints=[0, 0, 0, 1 << 20, -12345, 0x7FFFFFFF], reals=[0.1, 2.5, -3.75])
    script_call(0x407E30, "GameMaterial_DefineQ", lambda e: e.write(ACTIVE, imm(active)))


@pytest.mark.parametrize("effects", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_define(effects):
    strs = [b"concrete"] + [b"s%d" % i for i in range(1, 22)]
    ints = [0] * 4 + [0x1FF] + [0] * 17 + [5, 0, 0, 0, 9, 0, 0, 0, 0, 0]
    used = 3
    if not effects[0]:
        ints[23] = 9  # second type comes straight after the first
    set_args(ints=ints, reals=[0, 1.5, 2.5, 3.5, 0, 4.5], strs=strs)
    emu.write(SIZE, imm(40))
    emu.write(USED, imm(used))

    def setup(e):
        e.write(ACTIVE, imm(3))
        if effects[0]:
            handler(SIZE_EV, 5, SIZE_FN)
            handler(PARSE_EV, 5, PARSE_FN)
        if effects[1]:
            handler(SIZE_EV, 9, SIZE_FN)
            handler(PARSE_EV, 9, PARSE_FN)
    script_call(0x407F50, "GameMaterial_Define", setup)
    assert emu.read_cstr(MATERIALS + 3 * MSIZE + 0x20) == b"concrete"


def materials_in_use(e):
    e.call(0x4B0BA0)
    for i, (used, a, b) in enumerate([(1, 1, 1), (1, 0, 1), (0, 1, 1), (1, 1, 0), (1, 0, 0)]):
        m = MATERIALS + i * 7 * MSIZE
        e.write(m + 0x264, bytes([used]))
        if a:
            e.write(m + 0x258, imm(e.call(0x4B0C60, 16, 4)[0]))
        if b:
            e.write(m + 0x260, imm(e.call(0x4B0C60, 24, 4)[0]))


def test_close():
    compare(emu, 0x4082B0, "GameMaterial_Close", setup=materials_in_use, check_eax=False, normalized=True)


@pytest.mark.parametrize("orig,ours", [(0x408320, "GameMaterial_GetSizeEvent"),
                                       (0x408330, "GameMaterial_GetParseEvent"),
                                       (0x408340, "GameMaterial_GetImpactEvent")])
def test_events(orig, ours):
    compare(emu, orig, ours, eax_mask=0xFF)


def test_load_parameters():
    fake_ret(0x4B80B0, 0x20F00100)
    try:
        compare(emu, 0x408350, "GameMaterial_LoadParameters", check_eax=False, normalized=True)
    finally:
        fake_ret(0x4B80B0, 0)
    msgs = []
    for target in (0x408350, "GameMaterial_LoadParameters"):
        emu.reset()
        with pytest.raises(ErrorShown) as err:
            emu.call(target)
        msgs.append((str(err.value), emu.read_cstr(emu.arg(1))))
    assert msgs[0] == msgs[1] == ('GameMaterial_LoadParameters: Couldn\'t load script "%s"', b"LOCAL:material/material.qsc")


def test_attach():
    compare(emu, 0x408390, "GameMaterial_Attach", setup=lambda e: (e.alloc(0xE00),), check_eax=False)


def test_impact():
    def setup(e):
        handler(IMPACT_EV, 0x17, IMPACT_FN)
        pos = e.alloc(12)
        e.write(pos, struct.pack("<3f", 1.5, -2.0, 1e9))
        return 0xABCD0017, 0x55, 0x20F00200, 1, pos
    compare(emu, 0x4083C0, "GameMaterial_Impact", setup=setup, check_eax=False, normalized=True)


# --- HumanPlayer -------------------------------------------------------------

def player(e, base=None, **f):
    if base is None:
        p = e.alloc(0xE00)
    else:
        p = base
        e.write(p, b"\0" * 0xE00)
    e.write(p + 0x2EC + 4, imm(TABLE))
    for off, v in f.items():
        off = int(off[1:], 16)
        e.write(p + off, struct.pack("<f", v) if isinstance(v, float) else imm(v))
    return p


@pytest.mark.parametrize("damage,health,xp", [(5.0, 10.0, 0), (10.0, 10.0, 0), (11.0, 10.0, 0), (11.0, 10.0, 1)])
def test_check_death(damage, health, xp):
    fake_ret(0x416D80, xp)
    try:
        compare(emu, 0x408420, "HumanPlayer_CheckDeath", check_eax=False, normalized=True,
                setup=lambda e: (player(e, f254=damage, f258=health, f2F4=0x84),))
    finally:
        fake_ret(0x416D80, 0)


def fall_player(e, seed, vz, moves, flags, health=None, base=None):
    rnd = random.Random(seed)
    health = rnd.choice([100.0, 7.5]) if health is None else health
    vals = dict(f66C=vz, f2F4=flags, f258=health, f254=rnd.uniform(0, 50),
                f2C8=rnd.uniform(-5, 5), f664=rnd.uniform(-100, 100), f668=rnd.uniform(-100, 100), fFC=0x77,
                f4EC=0x20F00300)
    for i, m in enumerate(moves):
        vals["f%X" % (0x1B8 + 4 * i)] = m
    for o in (0x78, 0x7C, 0x84, 0x88, 0x90, 0x94):
        vals["f%X" % o] = rnd.uniform(-1, 1)
    return player(e, base, **vals)


FALL_CASES = [(-500.0, (1.0, 0.0, 0.0, 0.0), 4), (-30000.0, (0.0, 1.0, 1.0, 0.0), 4),
              (-80000.0, (1.0, 1.0, 1.0, 1.0), 4), (-200000.0, (0.0, 0.0, 0.0, 1.0), 4),
              (900000.0, (0.0, 0.0, 0.0, 0.0), 4), (-30000.0, (1.0, 0.0, 1.0, 0.0), 0),
              (-2000.0, (0.0,) * 4, 4, 100.0), (-2100.0, (0.0,) * 4, 4, 100.0), (-2200.0, (0.0,) * 4, 4, 100.0),
              (-30000.0, (0.0,) * 4, 4, 0.0)]


@pytest.mark.parametrize("case", range(len(FALL_CASES)))
@pytest.mark.parametrize("xp", [0, 1])
def test_update_fall(case, xp):
    def setup(e):
        e.write(0x56E1F4, struct.pack("<f", 312.5))
        return (fall_player(e, case, *FALL_CASES[case]),)
    fake_ret(0x416D80, xp)
    try:
        compare(emu, 0x408460, "HumanPlayer_UpdateFall", setup=setup, check_eax=False, normalized=True)
    finally:
        fake_ret(0x416D80, 0)


def run_fast(target, args):
    emu.write(CALL_LOG, imm(0))
    r = emu.call(target, *args)
    return r, emu.call_log()


def test_update_fall_random():
    rnd = random.Random(5)
    emu.reset()
    emu.write(0x56E1F4, struct.pack("<f", 287.25))
    try:
        for i in range(3000):
            vz = rnd.choice([rnd.uniform(-1e6, 1e6), rnd.uniform(-60000, 0), rnd.uniform(-2400, -1500),
                             -84.74169158935547, 0.0])
            moves = [rnd.choice([0.0, 1.0]) for _ in range(4)]
            flags = rnd.choice([4, 4, 0])
            fake_ret(0x416D80, i & 1)
            out = []
            for target in (0x408460, "HumanPlayer_UpdateFall"):
                fall_player(emu, i, vz, moves, flags, base=FAST_P)
                r, log = run_fast(target, (FAST_P,))
                out.append((emu.read(FAST_P, 0xE00), log))
            assert out[0] == out[1], i
    finally:
        fake_ret(0x416D80, 0)


@pytest.mark.parametrize("pair,flag", [((0, 0), 1), ((0, 0), -3), ((0, 0), 0), ((2, -2), 5), ((1, 0), 0)])
def test_sub_408720(pair, flag):
    def setup(e):
        e.write(PAIR, struct.pack("<ii", *pair))
        obj = e.alloc(0x700)
        e.write(obj + 0x6DC, struct.pack("<b", flag))
        fake_ret(0x413C80, obj)
        return (0x20F00400,)
    try:
        compare(emu, 0x408720, "sub_408720", setup=setup, normalized=True)
    finally:
        fake_ret(0x413C80, 0)


ENTERS = [0x408770, 0x408A40, 0x408C30, 0x408DF0, 0x408FE0, 0x409220, 0x409410, 0x409600, 0x4097F0,
          0x4099E0, 0x409C10, 0x409E00, 0x409FF0, 0x40A220, 0x40A410, 0x40A600, 0x40A800, 0x40AA00,
          0x40AA90, 0x40AB20, 0x40ACB0, 0x40AE70]
UPDATES = [0x4087B0, 0x408AA0, 0x408C80, 0x408E70, 0x409090, 0x4092A0, 0x409490, 0x409660, 0x409850,
           0x409A80, 0x409C90, 0x409E80, 0x40A090, 0x40A2A0, 0x40A490, 0x40A670, 0x40A870, 0x40AA30,
           0x40AAC0, 0x40AB60, 0x40AD20]
INPUT_BITS = [1, 4, 0x10, 0x20, 0x40, 0x80, 0x100, 0x2000, 0x10000, 0x20000, 0x40000]
LANDED = [(0.0, 0.0), (1.0, 0.0), (1.0, 2.0), (0.0, 3.0)]


def state_setup(state, prev, flags=0, landed=(0.0, 0.0), dc0=0.0):
    def setup(e):
        p = player(e, f2F4=flags, f2F8=prev, f2FC=state, f1DC=landed[0], f13C=landed[1], fDC0=dc0)
        e.write(PHASE, struct.pack("<ff", 0.375, 1.75))
        return p + 0x2EC, p
    return setup


@pytest.mark.parametrize("n", range(len(ENTERS)))
def test_enter(n):
    for prev in (28, 1):
        compare(emu, ENTERS[n], "HumanState_Enter%02d" % n, setup=state_setup(n, prev), check_eax=False,
                normalized=True)


@pytest.mark.parametrize("n", range(len(ENTERS)))
def test_enter_all_prev(n):
    emu.reset()
    emu.write(PHASE, struct.pack("<ff", 0.3, 1.7))
    p = FAST_P
    for prev in list(range(-1, 48)):
        out = []
        for target in (ENTERS[n], "HumanState_Enter%02d" % n):
            player(emu, p, f2F4=4, f2F8=prev & 0xFFFFFFFF, f2FC=n)
            r, log = run_fast(target, (p + 0x2EC, p))
            upd = emu.u32(p + 0x2EC + 0x14)
            out.append((emu.read(p, 0x2EC + 0x14), emu.read(p + 0x304, 0xB00), emu.normalize(upd), log))
        assert out[0] == out[1], prev


@pytest.mark.parametrize("n", range(len(UPDATES)))
def test_update(n):
    flags = 4 | 0x80 | 0x40000 if n != 17 else 5
    compare(emu, UPDATES[n], "HumanState_Update%02d" % n, setup=state_setup(n, 3, flags, dc0=-0.5),
            check_eax=False, normalized=True)
    compare(emu, UPDATES[n], "HumanState_Update%02d" % n, setup=state_setup(n, 3, 0x2004, (1.0, 0.0), 0.25),
            check_eax=False, normalized=True)


@pytest.mark.parametrize("n", range(len(UPDATES)))
def test_update_all_inputs(n):
    emu.reset()
    p = FAST_P
    seen = set()
    for bits in itertools.product((0, 1), repeat=len(INPUT_BITS)):
        flags = sum(b for b, on in zip(INPUT_BITS, bits) if on)
        for landed in LANDED:
            out = []
            for target in (UPDATES[n], "HumanState_Update%02d" % n):
                player(emu, p, f2F4=flags, f2F8=2, f2FC=n, f1DC=landed[0], f13C=landed[1])
                r, log = run_fast(target, (p + 0x2EC, p))
                out.append((emu.read(p, 0xE00), log))
            assert out[0] == out[1], (hex(flags), landed)
            seen.add(emu.u32(p + 0x2FC))
    assert len(seen) >= 3


@pytest.mark.parametrize("dc0", [-1.0, -0.002, -1e-30, 0.0, 1e-30, 0.003, 0.0033333334, 2.0])
def test_update00_decay(dc0):
    out = []
    for target in (0x4087B0, "HumanState_Update00"):
        emu.reset()
        player(emu, FAST_P, f2F4=4, f2F8=2, fDC0=dc0)
        run_fast(target, (FAST_P + 0x2EC, FAST_P))
        out.append(emu.read(FAST_P + 0xDC0, 4))
    assert out[0] == out[1]
