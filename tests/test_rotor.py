import random
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import Emu, ErrorShown, compare

HANDLERS = 0xA96AE0

# getters of event slots and task types; set_events gives each a fixed value
GETTERS = [0x4CEA10, 0x4CEA20, 0x4F45F0, 0x4C56F0, 0x4D9FA0, 0x4D9FB0, 0x4E7530, 0x416840, 0x416860, 0x416890,
           0x47E6F0, 0x4EE0E0, 0x4EE0F0, 0x4EE100, 0x42FDC0, 0x470400, 0x426D70, 0x4F1A60, 0x4F1A70, 0x4E6610,
           0x4F40D0, 0x4F40E0, 0x4F4110, 0x4F4120, 0x4F4130, 0x4F4140, 0x489610, 0x4F4850, 0x46AB20, 0x489630,
           0x4F1210, 0x4F1220, 0x464250, 0x4F0EE0, 0x4F0F30, 0x4F0F50, 0x4F0F60, 0x4F0F70, 0x4F0FC0, 0x4F0FD0,
           0x4F1000, 0x4F1010]


class Fakes:
    """Python-side replacements of original functions that log their arguments.
    Pointer arguments can be logged by content, so stack addresses compare equal."""

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
        """deref: {arg index: byte count} logs the bytes instead of the pointer.
        st0: 'f' or 'd' to return a float in st(0); the value comes from ret."""
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
                em.ret(r)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def handler(self, nargs, deref=None):
        """A task handler that only logs."""
        return self.add(self.code(b"\xc3"), nargs, deref=deref)


def with_cw(e, fk, target, cw):
    """Entry point that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    key = (addr, cw)
    if key not in fk.tramps:
        cell = fk.code(struct.pack("<H", cw))
        p = fk.code(b"\0" * 16)
        e.write(p, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        fk.tramps[key] = p
    return fk.tramps[key]


def check(e, fk, orig, ours, setup=None, normalized=True, cw=None, **kw):
    """compare() plus equal logs of the Python fakes. cw: x87 control word to run with
    (0x7F is the single precision the game runs at)."""
    logs = []
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


def set_events(e):
    for i, g in enumerate(GETTERS):
        c = e.read(g, 6)
        if c[0] == 0xA0:
            e.write(struct.unpack("<I", c[1:5])[0], bytes([0x20 + i]))
        elif c[:2] == b"\x66\xa1":
            e.write(struct.unpack("<I", c[2:6])[0], struct.pack("<H", 0x20 + i))
        elif c[0] == 0xA1:
            e.write(struct.unpack("<I", c[1:5])[0], struct.pack("<I", 0x20 + i))
        else:
            raise AssertionError(hex(g))


def event(e, getter):
    return 0x20 + GETTERS.index(getter)


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def new_task(e, type_, size):
    t = e.alloc(size)
    e.write(t + 8, struct.pack("<III", t + 0xC, 0, t + 8))
    e.write(t + 0x1C, struct.pack("<H", type_))
    return t


def add_child(e, parent, child):
    tailpred = e.u32(parent + 0x10)
    e.write(child, struct.pack("<II", parent + 0xC, tailpred))
    e.write(tailpred, struct.pack("<I", child))
    e.write(parent + 0x10, struct.pack("<I", child))
    e.write(child + 0x14, struct.pack("<I", parent))


def rmatrix(rng):
    return f32(*[rng.uniform(-1.5, 1.5) for _ in range(9)]) + struct.pack("<i", rng.randrange(100))


def rvec(rng, scale=5000.0):
    return f64(*[rng.uniform(-scale, scale) for _ in range(3)])


def with_children(e, t, slot, handler, n=2, type0=0x40):
    for i in range(n):
        c = new_task(e, type0 + i, 0x40)
        add_child(e, t, c)
        set_handler(e, slot, type0 + i, handler)


# --- Train -------------------------------------------------------------------

te = Emu()
tf = Fakes(te)
TRAIN_RUN = tf.handler(1)
TRAIN_EV = tf.handler(2)
tf.add(0x4015F0, 1)
tf.add(0x401A20, 1)
tf.add(0x4C1830, 1)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(16))
def test_train_get_frame(seed, cw):
    def setup(e):
        rng = random.Random(seed)
        t = new_task(e, 0x33, 0x8E0)
        set_handler(e, 1, 0x33, TRAIN_RUN)
        e.write(t + 0x20, rvec(rng))
        e.write(t + 0x70, rmatrix(rng))
        e.write(t + 0x694, rmatrix(rng))
        e.write(t + 0x6C0, rvec(rng))
        return t, e.alloc(12)
    check(te, tf, 0x42B930, "Train_GetFrame", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("which", [0, 1])
@pytest.mark.parametrize("sub", [0, 0x20001234])
def test_train_world_events(which, sub):
    def setup(e):
        set_events(e)
        ev = event(e, (0x4CEA10, 0x4CEA20)[which])
        set_handler(e, ev, event(e, 0x4F45F0), TRAIN_EV)
        t = new_task(e, 0x33, 0x8E0)
        e.write(t + 0x88C, struct.pack("<I", sub))
        return t, 0x1234
    names = ("Train_OnWorldEvent0", "Train_OnWorldEvent1")
    check(te, tf, (0x42BB60, 0x42BBA0)[which], names[which], setup=setup, check_eax=False)


def test_train_close_get_type():
    def setup(e):
        e.write(0x57BCB8, struct.pack("<HHI", 0x45, 0x1111, 0x20005000))
    check(te, tf, 0x42BC00, "Train_Close", setup=setup, check_eax=False)
    check(te, tf, 0x42BC20, "Train_GetType", setup=setup, eax_mask=0xFFFF)


# --- Afterburner / Rotor -----------------------------------------------------

pe = Emu()
pf = Fakes(pe)
MODEL = 0x20F00000
pf.add(0x4CEC10, 1, ret=lambda e, a: e.read_cstr(a[0]) != b"none" and MODEL)
pf.add(0x4EE140, 1, ret=0x20F00100)
pf.add(0x4C5710, 3)
pf.add(0x4D0950, 1, ret=2.75, st0="f")
pf.add(0x4C7140, 5)
pf.add(0x4C6940, 4, deref={2: 24})
pf.add(0x416920, 0, ret=0x20F00500)
pf.add(0x4C4950, 3)
pf.add(0x4B4720, 1)
pf.add(0x497800, 1, ret=lambda e, a: e.prim)
pf.add(0x4DAA20, 1, ret=0x777)
pf.add(0x4DA180, 2)
pf.add(0x4978B0, 2)
pf.add(0x497860, 1)
pf.add(0x401900, 4, ret=0x5A)
pf.add(0x4978D0, 1, ret=0x9)
pf.add(0x4978F0, 2)
pf.add(0x4978E0, 1)
pf.add(0x401400, 3)
pf.add(0x401530, 3)
pf.add(0x401A20, 1)
pf.add(0x4B8810, 2)
pf.add(0x4C1780, 8)
pf.add(0x4ECF50, 2, deref={1: 24})
pf.add(0x4B0D10, 1)
pf.add(0x4B8A20, 2, ret=lambda e, a: (a[1] * 37 + 5) & 0xFF)
pf.add(0x4B8A50, 2, ret=lambda e, a: a[1] * 13.7 - 20.1, st0="d")
pf.add(0x4B8A80, 4)
FRAME_LOG = pf.handler(2, deref={1: 0x40})
MOVE_LOG = pf.handler(2, deref={1: 8})
RUN_LOG = pf.handler(1)
EV9_LOG = pf.handler(2)


def create_arg(e, rng, name=b"rotor01"):
    a = e.alloc(0x90)
    e.write(a, struct.pack("<II", cstr(e, name), 0x4321))
    e.write(a + 8, rvec(rng) + rvec(rng, 100) + rmatrix(rng) + rmatrix(rng))
    e.write(a + 0x88, struct.pack("<II", 0x20E00000, 0))
    return a


SCALES = [(1.0, 2.0, 3.0), (3.0, 2.0, 1.0), (1.0, 3.0, 2.0), (2.0, 2.0, 2.0), (5.0, 1.0, 1.0)]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("scale", SCALES)
@pytest.mark.parametrize("addr,name", [(0x42BC30, "Afterburner_Create"), (0x42C500, "Rotor_Create")])
def test_part_create(addr, name, scale, cw):
    def setup(e):
        rng = random.Random(1)
        o = e.alloc(0x140)
        e.write(o + 0x98, f32(*scale))
        return o, create_arg(e, rng)
    check(pe, pf, addr, name, setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("addr,name", [(0x42BC30, "Afterburner_Create"), (0x42C500, "Rotor_Create")])
def test_part_create_missing_model(addr, name):
    check_error(pe, addr, name, lambda e: (e.alloc(0x140), create_arg(e, random.Random(2), b"none")))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("dist", [0.5, 50.0, 3000.0, -20.0])
@pytest.mark.parametrize("kids", [0, 2])
@pytest.mark.parametrize("addr,name", [(0x42BD40, "Afterburner_Draw"), (0x42D1D0, "Rotor_Draw")])
def test_part_draw(addr, name, dist, kids, cw):
    def setup(e):
        rng = random.Random(3)
        set_events(e)
        o = new_task(e, 0x5A, 0x140)
        e.write(o + 0x20, rvec(rng))
        e.write(o + 0x70, rmatrix(rng))
        e.write(o + 0x138, f32(rng.uniform(0, 1)))
        e.prim = e.alloc(0x30)
        e.write(0xBCAAF8, f32(0.6, 0.0, 0.8))
        x = struct.unpack("<3d", e.read(o + 0x20, 24))
        e.write(0xBCAB08, f64(x[0] - dist * 0.6, x[1] + 7.0, x[2] - dist * 0.8))
        with_children(e, o, event(e, 0x4C56F0), FRAME_LOG, kids)
        return (o,)
    check(pe, pf, addr, name, setup=setup, check_eax=False, cw=cw)


def test_part_init_close():
    def setup(e):
        set_events(e)
        e.write(0x57BCF8, struct.pack("<H", 0x5A))
        e.write(0x57BD10, struct.pack("<HHI", 0x5B, 0, 0x13))
        e.write(0xBC2364, struct.pack("<I", 0x12))
    for addr, name in ((0x42BEB0, "Afterburner_Init"), (0x42C4E0, "Afterburner_Close"),
                       (0x42D2B0, "Rotor_Init"), (0x42DAA0, "Rotor_Close")):
        check(pe, pf, addr, name, setup=setup, check_eax=False)


def owner(e, throttle, power=1.5, b8=0, matrix=None):
    c = e.alloc(0x2ED8)
    d = e.alloc(0x2D8)
    e.write(d + 8, f32(power))
    e.write(c + 0x1B4, struct.pack("<II", d, b8))
    e.write(c + 0x720, f32(throttle))
    if matrix:
        e.write(c + 0x70, matrix)
    return c, d


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("throttle", [0.0, 0.5, 0.81, 0.9, 1.0, 1.3])
@pytest.mark.parametrize("value", [0.0, 0.1, 0.2, 0.6, 0.9])
def test_afterburner_run(throttle, value, cw):
    def setup(e):
        o = new_task(e, 0x5A, 0x140)
        c, _ = owner(e, throttle)
        e.write(o + 0x130, struct.pack("<I", c))
        e.write(o + 0x138, f32(value))
        e.write(0x57BD00, struct.pack("<III", 0x12345, 0x6789, 0xABCDE))
        with_children(e, o, 1, RUN_LOG, 2)
        return (o,)
    check(pe, pf, 0x42BFA0, "Afterburner_Run", setup=setup, check_eax=False, cw=cw)


def test_afterburner_run_equal():
    def setup(e):
        o = new_task(e, 0x5A, 0x140)
        c, _ = owner(e, 0.81 + 0.44 * 0.5)
        e.write(o + 0x130, struct.pack("<I", c))
        e.write(o + 0x138, struct.pack("<f", 0.5))
        return (o,)
    check(pe, pf, 0x42BFA0, "Afterburner_Run", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(16))
@pytest.mark.parametrize("addr,name", [(0x42C0B0, "Afterburner_Move"), (0x42D680, "Rotor_Move")])
def test_part_move(addr, name, seed, cw):
    def setup(e):
        rng = random.Random(seed)
        set_events(e)
        o = new_task(e, 0x5A, 0x140)
        e.write(o + 0x98, f32(*SCALES[seed % 5]))
        e.write(o + 0xF0, rvec(rng, 300) + rmatrix(rng))
        e.write(0x57BD00, struct.pack("<III", 0x12345 + seed, 0x6789, 0xABCDE))
        a = e.alloc(8)
        pos, m = e.alloc(24), e.alloc(40)
        e.write(pos, rvec(rng))
        e.write(m, rmatrix(rng))
        e.write(a, struct.pack("<II", pos, m))
        with_children(e, o, event(e, 0x4D9FB0), MOVE_LOG, seed % 3)
        return o, a
    check(pe, pf, addr, name, setup=setup, check_eax=False, cw=cw)


def test_afterburner_draw_prim():
    def setup(e):
        o = e.alloc(0x140)
        e.write(o + 0x48, struct.pack("<I", 0x4848))
        p = e.alloc(0x30)
        e.write(p, struct.pack("<9I", 0, 1, 2, o, 0x10, 0x14, 0x18, 0x1C, 0x20))
        return (p,)
    check(pe, pf, 0x42C4B0, "Afterburner_DrawPrim", setup=setup, check_eax=False)


ROTOR_CASES = [
    # lift, tilt, throttle, b8, m1, m4
    (1, 0, 0.7, 0, 0.3, 0.2),
    (1, 0, 0.7, 0, 0.0, 0.0),
    (1, 0, 0.7, 0, 0.0, 0.4),
    (1, 1, 0.5, 0, 0.6, 0.1),
    (0, 1, 0.5, 0, 0.6, 0.1),
    (0, 0, 0.5, 0, 0.6, 0.1),
    (1, 0, 0.0, 0, 0.6, 0.1),
    (1, 1, 0.9, 1, 0.6, 0.1),
]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", ROTOR_CASES)
def test_rotor_run(case, cw):
    lift, tilt, throttle, b8, m1, m4 = case
    def setup(e):
        rng = random.Random(7)
        m = list(struct.unpack("<9f", rmatrix(rng)[:36]))
        m[1], m[4] = m1, m4
        c, _ = owner(e, throttle, 1.25, b8, f32(*m) + b"\0" * 4)
        o = new_task(e, 0x5B, 0x140)
        defn = e.alloc(0x2D8)
        d = e.alloc(0x20)
        e.write(d, bytes([lift, tilt, 0, 0]) + f32(0.3, 2.5) + struct.pack("<i", -3))
        e.write(defn + 0x2D4, struct.pack("<I", d))
        e.write(o + 0x130, struct.pack("<I", c))
        e.write(o + 0x138, f32(1.75))
        e.write(o + 0x13C, struct.pack("<I", defn))
        with_children(e, o, 1, RUN_LOG, 1)
        return (o,)
    check(pe, pf, 0x42D440, "Rotor_Run", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("has", [True, False])
def test_rotor_forward_event9(has):
    def setup(e):
        set_events(e)
        c = new_task(e, 0x61, 0x2ED8)
        o = new_task(e, 0x5B, 0x140)
        e.write(o + 0x130, struct.pack("<I", c))
        set_handler(e, event(e, 0x416840), 0x61, EV9_LOG if has else 0)
        return o, 0x5555
    check(pe, pf, 0x42D9B0, "Rotor_ForwardEvent9", setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("index", [0, 3])
def test_rotor_parse_def(index, cw):
    def setup(e):
        d = e.alloc(0x20)
        a = e.alloc(12)
        e.write(a, struct.pack("<IIi", d, 0x20F00200, index))
        return 0, a
    check(pe, pf, 0x42D9F0, "Rotor_ParseDef", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("data", [0, 0x20F00300])
def test_rotor_def_size_free(data):
    def setup(e):
        defn = e.alloc(0x2D8)
        e.write(defn + 0x2D4, struct.pack("<I", data))
        p = e.alloc(4)
        e.write(p, struct.pack("<I", defn))
        return 0, p
    check(pe, pf, 0x42DA70, "Rotor_FreeDef", setup=setup, check_eax=False)
    check(pe, pf, 0x42DA60, "Rotor_GetDefSize", setup=setup, check_eax=False)


DRAW_LOG = pf.add(pf.code(b"\xc3"), 1, fmt=lambda e, a: (e.read(e.u32(a[0]), 0xF0), e.read(a[0] + 4, 16)))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(8))
def test_rotor_draw_prim(seed, cw):
    def setup(e):
        rng = random.Random(60 + seed)
        c, _ = owner(e, rng.choice([0.0, 0.5, 1.0, 1.05, 1.3, -0.2]), 1.0, 0, rmatrix(rng))
        cd = e.alloc(0x2C0)
        e.write(e.u32(c + 0x1B4) + 0x2D4, struct.pack("<I", cd))
        e.write(cd, f32(rng.choice([1.0, 0.05, 30.0])))
        e.write(c + 0x108, f32(*[rng.uniform(-3, 3) for _ in range(3)]))
        o = new_task(e, 0x5B, 0x140)
        e.write(o + 0x20, rvec(rng))
        e.write(o + 0x6C, struct.pack("<I", 0 if seed == 7 else 0x20F00000))
        e.write(o + 0x70, rmatrix(rng)[:36] + struct.pack("<i", rng.choice([0, 500, 998, 999, 1200])))
        e.write(o + 0x98, f32(1.0, 1.5, 2.0))
        e.write(o + 0x138, f32(rng.uniform(-50, 50)))
        defn = e.alloc(0x2D8)
        d = e.alloc(0x20)
        e.write(d, bytes([1, 0, 0, 0]) + f32(rng.uniform(0.05, 0.6), rng.uniform(0.5, 3)) +
                struct.pack("<i", rng.choice([0, 1, 2, 4])))
        e.write(defn + 0x2D4, struct.pack("<I", d))
        e.write(o + 0x130, struct.pack("<I", c))
        e.write(o + 0x13C, struct.pack("<I", defn))
        p = e.alloc(0x24)
        e.write(p, struct.pack("<9I", 0, 1, 2, o, 0x10, 0x14, 0x18, 0x1C, 0x20))
        e.write(0xB46D14, struct.pack("<I", 0x31))
        e.write(0xA94E84 + 4 * 0x31, struct.pack("<I", DRAW_LOG))
        return (p,)
    check(pe, pf, 0x42C630, "Rotor_DrawPrim", setup=setup, check_eax=False, cw=cw)
