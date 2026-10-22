import random
import struct

import pytest
from unicorn import UC_HOOK_CODE

from emu import CALL_LOG, FAKE_BASE, Emu, compare

emu = Emu()
R = {}  # address -> return value for the current test, read by the fakes


def log(e, words):
    n = e.u32(CALL_LOG)
    e.write(CALL_LOG + 4 + 4 * n, struct.pack("<%dI" % len(words), *[w & 0xFFFFFFFF for w in words]))
    e.write(CALL_LOG, struct.pack("<I", n + len(words)))


class PyFakes:
    """Callees emulated in Python. Each fake is a 'ret' slot in the fake area; the
    original function gets a jump to its slot."""

    def __init__(self, e, base=FAKE_BASE + 0xC0000):
        self.e, self.next, self.fns = e, base, {}
        e.uc.hook_add(UC_HOOK_CODE, self._hook, begin=base, end=base + 0x10000)

    def _hook(self, uc, addr, size, user):
        fn = self.fns.get(addr)
        if fn:
            fn(self.e)

    def slot(self, fn):
        t = self.next
        self.next += 16
        self.e.write(t, b"\xc3")
        self.fns[t] = fn
        return t

    def fake(self, addr, nargs=0, ret=0, masks=(), fn=None, logged=True):
        def run(e):
            args = [e.arg(i) for i in range(nargs)]
            args = [a & m for a, m in zip(args, masks)] + args[len(masks):]
            if logged:
                log(e, [addr] + args)
            r = fn(e, args) if fn else R.get(addr, ret)
            e.ret(r or 0)
        t = self.slot(run)
        if addr is not None:
            self.e.write(addr, b"\xe9" + struct.pack("<i", t - addr - 5))
            self.e.uc.ctl_remove_cache(addr, addr + 5)
        return t


fakes = PyFakes(emu)
f32 = lambda e, p, v: e.write(p, struct.pack("<f", v))
i32 = lambda e, p, v: e.write(p, struct.pack("<i", v))
u8 = lambda e, p, v: e.write(p, bytes([v]))
rf32 = lambda e, p: struct.unpack("<f", e.read(p, 4))[0]
WORLD, RNG, HUMAN_TYPE = 0x5A5A0000, 0x5B5B0000, 0x77


def words(e, p, n):
    return list(struct.unpack("<%dI" % n, e.read(p, 4 * n)))


# --- callee fakes --------------------------------------------------------------

INFO, AMMO, FIND, HASAMMO, EVAL, AMMOLIST = {}, {}, {}, {}, {}, {}


def get_ammo(e, a):
    log(e, [0x4F23D0, a[0]])
    clip, reserve = AMMO.get(a[0], (0, 0))
    e.write(a[1], struct.pack("<ii", clip, reserve))


def get_matrix(e, a):
    log(e, [0x465880, a[1]])
    e.write(a[0], struct.pack("<9f", *R["matrix"]))


def place_world(e, a):
    log(e, [a[0], a[1], a[3]] + words(e, a[2], 6) + a[4:])


def to_angles(e, a):
    log(e, [0x4B2D00] + words(e, a[0], 3))
    f32(e, a[1], R.get("pitch", 0.25))
    f32(e, a[2], R.get("yaw", -7.0))


def printf_args(e, first, fmt):
    out, i = [], first
    text = e.read_cstr(fmt)
    for conv in text.split(b"%")[1:]:
        if conv[:1] in (b".", b"f"):
            out += [e.arg(i), e.arg(i + 1)]
            i += 2
        else:
            out.append(e.arg(i))
            i += 1
    return out


def human_printf(e, a):
    log(e, [0x460C80, a[0], a[1]] + printf_args(e, 2, a[1]))


def debug_printf(e, a):
    log(e, [0x4E7BD0, a[0]] + printf_args(e, 1, a[0]))


FAKES = {  # address: (nargs, return, masks, fn, logged)
    0x401400: (3, 0, (0xFFFF, 0xFF)), 0x401530: (3, 0, (0xFFFF,)), 0x401900: (4, 0x1234, (~0, ~0, 0xFFFF)),
    0x401B20: (1,), 0x401FA0: (2, 0, (0xFFFF,)), 0x408420: (1,), 0x408460: (1,),
    0x408720: (1, 0, (), lambda e, a: HASAMMO.get(a[0], 1)), 0x40C140: (1,), 0x40C2D0: (1,), 0x40CD30: (2,),
    0x411000: (1,), 0x411240: (1,), 0x411300: (1,), 0x411350: (1,), 0x4113C0: (1,), 0x4113E0: (1,),
    0x411770: (1,), 0x411AE0: (1,), 0x411F20: (1,), 0x4137E0: (2,), 0x413A60: (2,),
    0x413C80: (1, 0, (), lambda e, a: INFO.get(a[0], 0)),
    0x413CC0: (2, 0, (), lambda e, a: FIND.get(a[1], 0)), 0x415020: (1,), 0x4167E0: (0, 0x1203),
    0x416810: (0, 0x3404), 0x4168C0: (0,), 0x416920: (0, WORLD), 0x416D20: (0, RNG), 0x416D40: (0,),
    0x460BE0: (0, 0xABC0000 | HUMAN_TYPE), 0x460C10: (0, 0x5606), 0x460C80: (2, 0, (), human_printf, False),
    0x460CF0: (1,), 0x460EE0: (1,), 0x4610C0: (1,), 0x4613D0: (1,), 0x4614B0: (1,), 0x462CF0: (1,),
    0x462E50: (1,), 0x463080: (1,), 0x463170: (1,), 0x4632D0: (1,), 0x4635F0: (1,), 0x463600: (1,),
    0x463610: (1,), 0x463620: (1,), 0x463680: (1, 0x6680), 0x463740: (1,), 0x463760: (2,), 0x4638A0: (3,),
    0x463920: (1,), 0x464660: (1,), 0x464700: (1,), 0x464710: (1,), 0x464770: (5,), 0x4647A0: (3,),
    0x4647D0: (1,), 0x464800: (2,), 0x464BD0: (2,), 0x465880: (2, 0, (), get_matrix, False), 0x4659E0: (1,),
    0x46C020: (1,), 0x46C050: (0,), 0x46C060: (0,),
    0x47CDA0: (1, 0, (), lambda e, a: EVAL[e.read_cstr(a[0])]), 0x47CE70: (3,),
    0x47CEE0: (2, 0, (), lambda e, a: AMMOLIST.get(a[1], 0)), 0x489C30: (2,), 0x489E10: (2,),
    0x489E70: (3,), 0x489EA0: (4,), 0x48A330: (1,), 0x48F360: (2,), 0x4B2D00: (3, 0, (), to_angles, False),
    0x4B47C0: (2,), 0x4B8890: (4,), 0x4B8A20: (2, 0, (), lambda e, a: R["ints"][a[1]]), 0x4C1790: (2,),
    0x4C17C0: (2,), 0x4C1800: (1, 0x4C18), 0x4C6940: (4, 0, (), place_world, False), 0x4C7140: (5, 0, (), place_world, False),
    0x4C7560: (2,), 0x4CEA20: (0, 0x7709), 0x4D2FB0: (3,), 0x4D38C0: (2,), 0x4D61D0: (5,), 0x4D9610: (0, 0x880011),
    0x4D9620: (4,), 0x4D96F0: (1,), 0x4E6B00: (2, 0x6B00), 0x4E6C00: (1,), 0x4E7BD0: (1, 0, (), debug_printf, False),
    0x4EC190: (6,), 0x4EC1E0: (2,), 0x4EE140: (1,), 0x4F1980: (2, 0x1980), 0x4F1A60: (0, 0x11A60A),
    0x4F1A70: (0, 0x11A70B), 0x4F2030: (0, 7), 0x4F2060: (2, 0, (), lambda e, a: e.u32(a[0] + 4 * a[1])),
    0x4F2070: (3,), 0x4F2290: (1,), 0x4F2350: (2, 0, (~0, 0xFF)), 0x4F2390: (1,),
    0x4F23D0: (2, 0, (), get_ammo, False), 0x4F40D0: (0, 0x40D00C), 0x4F40E0: (0, 0x40E00D), 0x4F4140: (0, 0x41400E),
    0x416840: (0, 0x16840F), 0x416860: (0, 0x168610), 0x416870: (0, 0x168711), 0x460C40: (0, 0x60C412),
    0x460C50: (0, 0x60C50), 0x467D90: (0, 0x67D913), 0x47E990: (0, 0x7E9914), 0x487190: (0, 0x871915),
}
for addr, spec in FAKES.items():
    nargs, ret, masks, fn, logged = (spec + (0, (), None, True)[len(spec) - 1:])[:5]
    fakes.fake(addr, nargs, ret, masks, fn, logged)

# Script_GetArgReal returns REALS[i] in st0
REALS = emu.alloc(8 * 64)
stub = emu.fake_top
emu.fake_top += 32
emu.write(stub, b"\x8b\x44\x24\x08\xdd\x04\xc5" + struct.pack("<I", REALS) + b"\xc3")
emu.write(0x4B8A50, b"\xe9" + struct.pack("<i", stub - 0x4B8A50 - 5))
emu.uc.ctl_remove_cache(0x4B8A50, 0x4B8A55)


def logger(nargs, deref=None, effect=None):
    """A handler slot that logs its address and args; deref=(arg index, words) logs memory too."""
    def run(e):
        args = [e.arg(i) for i in range(nargs)]
        shown = list(args)
        if deref:
            shown[deref[0]:deref[0] + 1] = words(e, args[deref[0]], deref[1])
        log(e, [t] + shown)
        if effect:
            effect(e, args)
        e.ret(0)
    t = fakes.slot(run)
    return t


BODY_STATES = [logger(2) for _ in range(0x31)]
WEAPON_STATES = [logger(2) for _ in range(13)]
emu.baseline = emu.snapshot()


def set_handler(e, slot, type_, fn):
    e.write(0xA96AE0 + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


class Player:
    pass


def player(e, inputs=None, prev=None, body_state=0, body_in=0, weapon_state=1, weapon_in=0, weapon=True,
           kind=5, wid=40, clip=5, reserve=10, clip_size=30, **fields):
    p = Player()
    p.h = h = e.alloc(0xDF0)
    p.v = v = e.alloc(0x300)
    m, vm = e.alloc(0x60), e.alloc(0x60)
    i32(e, h + 0x4EC, v)
    i32(e, h + 0x70, m)
    i32(e, v + 0x70, vm)
    p.body, p.weapon = h + 0x2EC, h + 0xCF8
    bt, wt = e.alloc(4 * 0x31), e.alloc(4 * 13)
    e.write(bt, struct.pack("<49I", *BODY_STATES))
    e.write(wt, struct.pack("<13I", *WEAPON_STATES))
    e.write(p.body + 4, struct.pack("<IIii", bt, body_in, -1, body_state))
    e.write(p.weapon + 4, struct.pack("<IIii", wt, weapon_in, -1, weapon_state))
    for i, x in (inputs or {}).items():
        f32(e, h + 0x1B0 + 4 * i, x)
    for i, x in (prev or {}).items():
        f32(e, h + 0x110 + 4 * i, x)
    p.w = 0
    if weapon:
        p.w = w = e.alloc(0x20)
        e.write(w + 0x1C, struct.pack("<H", 5))
        p.info = info = e.alloc(0x700)
        i32(e, info, wid)
        i32(e, info + 0x610, kind)
        i32(e, info + 0x644, 4)
        i32(e, info + 0x648, clip_size)
        for off in (0x67C, 0x684, 0x688, 0x68C):
            i32(e, info + off, off)
        heal = e.alloc(4)
        f32(e, heal, 0.25)
        i32(e, info + 0x6E4, heal)
        INFO[w] = info
        AMMO[w] = (clip, reserve)
        i32(e, h + 0x4C4, w)
    i32(e, h + 0xD48, -1)
    i32(e, h + 0xD4C, -1)
    for off, (fmt, x) in fields.items():
        e.write(h + int(off[1:], 16), struct.pack(fmt, x))
    return p


def check(orig, ours, setup, **kw):
    compare(emu, orig, ours, setup=setup, check_eax=False, normalized=True, **kw)


INPUT = dict(left=2, right=3, fwd=4, back=5, fire=6, alt=7, reload=8, use=9, lean=12, weapon=13, d45=20,
             next=21, prev=22, map=32, fly=33, binoc=34)


# --- body states ---------------------------------------------------------------

@pytest.mark.parametrize("crouch", [0, 1])
def test_body_lean_enter(crouch):
    def setup(e):
        p = player(e, body_state=0x2C, x33D=("<B", crouch))
        return p.body, p.h
    check(0x40DF70, "HumanPlayer_BodyLeanEnter", setup)


LEAN_CASES = []
_rng = random.Random(7)
for _ in range(60):
    LEAN_CASES.append(dict(
        left=_rng.choice([0, 1.0]), right=_rng.choice([0, 1.0]), pleft=_rng.choice([0, 1.0]),
        pright=_rng.choice([0, 1.0]), fwd=_rng.choice([0, 1.0]), pfwd=_rng.choice([0, 1.0]),
        crouch=_rng.choice([0, 1]), x=_rng.choice([-3000.0, -2048.0, -100.0, 0.0, 50.0, 2048.0, 2900.0, -1999.5]),
        z=_rng.choice([-50.0, 0.0, 30.0, 1000.0, 1200.0, 999.0]), sx=_rng.choice([100.0, 3.7, 6000.0]),
        sz=_rng.choice([2.5, 300.0])))


@pytest.mark.parametrize("c", LEAN_CASES)
def test_body_lean_run(c):
    def setup(e):
        p = player(e, inputs={2: c["left"], 3: c["right"], 4: c["fwd"]},
                   prev={2: c["pleft"], 3: c["pright"], 4: c["pfwd"]}, x33D=("<B", c["crouch"]))
        e.write(0x56E1F8, struct.pack("<ffi", 2048.0, 1000.0, 7))
        e.write(p.v + 0x19C, struct.pack("<6f", c["x"], 0, c["z"], c["sx"], 0, c["sz"]))
        return p.body, p.h
    check(0x40DFC0, "HumanPlayer_BodyLeanRun", setup)


def test_body_computer_enter():
    def setup(e):
        p = player(e, body_state=0x2D)
        return p.body, p.h
    check(0x40E370, "HumanPlayer_BodyComputerEnter", setup)


@pytest.mark.parametrize("cur,prev,flags", [(1, 0, 0), (1, 1, 0), (0, 0, 0), (0, 0, 0x20000), (1, 0, 0x20000)])
def test_body_computer_run(cur, prev, flags):
    def setup(e):
        p = player(e, inputs={32: cur}, prev={32: prev}, body_in=flags, body_state=0x2D)
        return p.body, p.h
    check(0x40E3C0, "HumanPlayer_BodyComputerRun", setup)


def hit_anims(e, p):
    a = e.alloc(16)
    e.write(a, struct.pack("<4I", 1, 0x111, 0x222, 0x333))
    R[0x489E10] = a


def test_body_hit_enter():
    def setup(e):
        p = player(e, body_state=0x2E)
        hit_anims(e, p)
        return p.body, p.h
    check(0x40E470, "HumanPlayer_BodyHitEnter", setup)


@pytest.mark.parametrize("flags", [0, 1, 0x10000, 0x10001, 0x20000, 0x30001])
def test_body_hit_run(flags):
    def setup(e):
        p = player(e, body_in=flags, body_state=0x2E)
        return p.body, p.h
    check(0x40E4D0, "HumanPlayer_BodyHitRun", setup)


@pytest.mark.parametrize("r", [0, 1])
def test_body_death_enter(r):
    def setup(e):
        p = player(e, body_state=0x2F)
        hit_anims(e, p)
        R[0x4B47C0] = r
        e.write(p.h + 0x688, struct.pack("<3f", 1.5, -2.0, 3.25))
        return p.body, p.h
    check(0x40E560, "HumanPlayer_BodyDeathEnter", setup)


@pytest.mark.parametrize("flags,weapon", [(0, True), (4, True), (5, True), (5, False), (1, True)])
def test_body_death_run(flags, weapon):
    def setup(e):
        p = player(e, body_in=flags, body_state=0x2F, weapon=weapon)
        e.write(p.h + 0x664, struct.pack("<6f", 1, 2, 3, 4, 5, 6))
        return p.body, p.h
    check(0x40E620, "HumanPlayer_BodyDeathRun", setup)


def test_body_fly_enter():
    def setup(e):
        p = player(e, body_state=0x30)
        e.write(p.h + 0x664, struct.pack("<6f", 1, 2, 3, 4, 5, 6))
        return p.body, p.h
    check(0x40E6E0, "HumanPlayer_BodyFlyEnter", setup)


FLY_FLAGS = [0, 0x80, 0x100, 0x40, 0x20, 0x180, 0xA0, 0x1080, 0x2140, 0x4020, 0x1E0, 0x60, 0x7000]


@pytest.mark.parametrize("flags", FLY_FLAGS)
@pytest.mark.parametrize("seed", [1, 2])
def test_body_fly_run(flags, seed):
    rnd = random.Random(seed)
    R["matrix"] = [rnd.uniform(-1, 1) for _ in range(9)]

    def setup(e):
        p = player(e, body_in=flags, body_state=0x30, xA0=("<i", 0x55))
        e.write(p.h + 0x20, struct.pack("<3d", 1000.5, -20333.25, 77.125))
        return p.body, p.h
    check(0x40E740, "HumanPlayer_BodyFlyRun", setup)


# --- weapon states -------------------------------------------------------------

def wsetup(addr_fields=None, **kw):
    def setup(e):
        p = player(e, **kw)
        return p.weapon, p.h
    return setup


@pytest.mark.parametrize("lock", [0, 1])
def test_weapon_disabled(lock):
    check(0x40E960, "HumanPlayer_WeaponDisabledEnter", wsetup(weapon_state=0))
    check(0x40E980, "HumanPlayer_WeaponDisabledRun", wsetup(weapon_state=0, xD44=("<B", lock)))


def test_weapon_simple_enters():
    for orig, ours in [(0x40E9B0, "HumanPlayer_WeaponIdleEnter"), (0x40EF60, "HumanPlayer_WeaponFireEnter"),
                       (0x40F170, "HumanPlayer_WeaponReloadEnter")]:
        check(orig, ours, lambda e: (player(e).weapon,))
    for orig, ours in [(0x40F0D0, "HumanPlayer_WeaponDryFireEnter"), (0x40F230, "HumanPlayer_WeaponSwitchEnter"),
                       (0x40F370, "HumanPlayer_WeaponLeanEnter"), (0x40F4A0, "HumanPlayer_WeaponAltFireEnter"),
                       (0x40F930, "HumanPlayer_WeaponPlaceEnter"), (0x40FA60, "HumanPlayer_WeaponMeleeEnter"),
                       (0x40FB30, "HumanPlayer_WeaponMedipackEnter"), (0x40FDA0, "HumanPlayer_WeaponComputerEnter")]:
        check(orig, ours, wsetup())


EVAL.update({b"WEAPON_ID_BINOCULARS": 900, b"AMMO_ID_MEDIPACK": 901, b"WEAPON_ID_M16A2": 902,
             b"AMMO_ID_M203": 903, b"WEAPON_ID_PROXIMITYMINE": 904})

IDLE_CASES = []
_rng = random.Random(3)
for _ in range(150):
    IDLE_CASES.append(dict(
        lock=_rng.random() < 0.08, anim=_rng.choice([(0, 0), (1, 4)]), binoc=_rng.random() < 0.15,
        binoc_have=_rng.choice([0, 1, 2]), saved=_rng.random() < 0.1, weapon=_rng.random() < 0.9,
        kind=_rng.choice([3, 4, 5, 6, 7, 9]), wid=_rng.choice([40, 902, 904]),
        flags=_rng.choice([0, 0x200, 0x400, 0x800, 0x200 | 0x800, 0x400 | 0x800]),
        clip=_rng.choice([0, 3, 30]), reserve=_rng.choice([0, 7]), unlimited=_rng.choice([0, 1]),
        ammo=_rng.choice([None, 0, 2]), damage=_rng.choice([0.0, 0.5]), lean=_rng.choice([0, 1.0]),
        ground=_rng.choice([0, 4]), next=_rng.choice([-1, -1, 41]), b7=_rng.choice([0, 1]),
        use=_rng.choice([0, 1.0]), attach=_rng.random() < 0.5, map=_rng.choice([(0, 0), (1, 0), (1, 1)])))


@pytest.mark.parametrize("c", IDLE_CASES)
def test_weapon_idle_run(c):
    out = {}

    def att_handler(e, a):
        i32(e, a[1], 0x1234)

    att_fn = logger(2, (1, 1), att_handler) if "att" not in out else None

    def setup(e):
        p = player(e, weapon=c["weapon"], kind=c["kind"], wid=c["wid"], weapon_in=c["flags"], clip=c["clip"],
                   reserve=c["reserve"], inputs={34: float(c["binoc"]), 12: c["lean"], 9: c["use"], 32: c["map"][0]},
                   prev={32: c["map"][1]}, body_in=c["ground"], xD44=("<B", c["lock"]),
                   xD58=("<i", c["anim"][0]), xD5C=("<i", c["anim"][1]), xD4C=("<i", c["next"]),
                   x7B9=("<B", c["b7"]), x254=("<f", c["damage"]))
        u8(e, 0x56E214, c["unlimited"])
        AMMOLIST.clear()
        if c["ammo"] is not None:
            a = e.alloc(12)
            e.write(a, struct.pack("<ii", 0, c["ammo"]))
            AMMOLIST[901] = AMMOLIST[903] = a
        b = e.alloc(0x20)
        e.write(b + 0x1C, struct.pack("<H", 9))
        binfo = e.alloc(0x700)
        i32(e, binfo, 900)
        INFO[b] = binfo
        FIND.clear()
        FIND[900] = {0: 0, 1: b, 2: p.w}[c["binoc_have"]] if c["binoc_have"] != 2 or p.w else b
        if c["saved"]:
            i32(e, p.h + 0xDE4, b)
        if FIND[900] and FIND[900] == p.w:
            i32(e, p.h + 0xDE4, b)
        set_handler(e, 7, 9, logger(1) if "b1" not in out else out["b1"])
        set_handler(e, 7, 5, out.setdefault("b5", logger(1)))
        R[0x463740] = 0
        if c["attach"]:
            t = e.alloc(0x20)
            e.write(t + 0x1C, struct.pack("<H", 11))
            R[0x463740] = t
            set_handler(e, 4, 11, att_fn)
        return p.weapon, p.h
    out["b1"] = logger(1)
    check(0x40E9C0, "HumanPlayer_WeaponIdleRun", setup)
