import random
import struct

import pytest

from emu import Emu, compare
from test_missile_stubs import R, PyFakes, f32, f64, i32, log, model, set_handler, task, u8, u16, words

emu = Emu()
fakes = PyFakes(emu)

TT, VT = 0x56, 0x77  # Train and Vehicle type ids
WORLD = 0x5A5A0000
EV = dict(table=0x47, changed=0x48, move=0x4B, ev4=0x4C, sound=0x4D, input=0x4E, channel=0x4F, unk4110=0x50,
          save=0x51, load=0x52, read=0x53, ev14=0x54, world1=0x55, world0=0x56)
VALUE = emu.alloc(8)


def path_points(e, a):
    log(e, [0x4FEC10, a[0], a[1], a[4], a[5]] + words(e, a[6], a[5]) + [a[7]])
    pts = R["pts"][: a[5] + 1]
    e.write(a[2], b"".join(struct.pack("<3d", *p) for p in pts))
    u8(e, a[3], R.get("found", 1))


def deref(addr, *spec):
    def run(e, a):
        out = [addr]
        for v, s in zip(a, spec):
            out += words(e, v, s) if s else [v]
        log(e, out)
        return 0
    return run


FAKES = {  # address: (nargs, return, masks, fn, logged)
    0x401900: (4, 0xABC0000 | TT, (~0, ~0, 0xFFFF)), 0x401400: (3, 0, (0xFFFF, 0xFF)), 0x401530: (3, 0, (0xFFFF,)),
    0x4015F0: (1,), 0x4028B0: (0, 0, (), lambda e, a: R.get(0x4028B0, 0)),
    0x416890: (0, EV["ev14"]), 0x416920: (0, WORLD), 0x4C1830: (1,), 0x4C5710: (3,),
    0x4C5800: (0, EV["ev4"]), 0x4C6940: (4, 0, (), deref(0x4C6940, 0, 0, 6, 0), False),
    0x4C7140: (5, 0, (), deref(0x4C7140, 0, 0, 6, 0, 0), False), 0x4C7560: (2,),
    0x4CEA10: (0, EV["world0"]), 0x4CEA20: (0, EV["world1"]),
    0x4CEC10: (1, 0, (), lambda e, a: R.get(0x4CEC10, 0)), 0x4D9FB0: (0, EV["move"]),
    0x4E6610: (0, EV["sound"]), 0x4E6AB0: (1, 0, (), lambda e, a: R.get(0x4E6AB0, 0)),
    0x4E6B00: (2, 0, (), lambda e, a: R.get(0x4E6B00, 0)), 0x4ECDB0: (1,),
    0x4F1320: (1,), 0x4F1340: (1,), 0x4F1400: (2,), 0x4F16C0: (1, 0, (), lambda e, a: R.get(0x4F16C0, 0)),
    0x4F1A60: (0, EV["table"]), 0x4F1A70: (0, EV["changed"]),
    0x4F40D0: (0, EV["input"]), 0x4F40E0: (0, EV["channel"]), 0x4F4110: (0, EV["unk4110"]),
    0x4F4120: (0, EV["save"]), 0x4F4130: (0, EV["load"]), 0x4F4140: (0, EV["read"]),
    0x4F45F0: (0, 0xABC0000 | VT), 0x4F4600: (1,), 0x4F4640: (1,), 0x4F4660: (1,), 0x4F4680: (1,), 0x4F4800: (1,),
    0x4F4820: (1,), 0x4FB700: (2,), 0x4FEC10: (8, 0, (), path_points, False),
    0x414ED0: (3, 0, (), lambda e, a: (log(e, [0x414ED0, a[1], a[2]]),
                                       e.write(a[0], struct.pack("<3f", *R["v3f"][a[2]])))[0], False),
}
for addr, spec in FAKES.items():
    nargs, ret, masks, fn, logged = (spec + (0, (), None, True)[len(spec) - 1:])[:5]
    fakes.fake(addr, nargs, ret, masks, fn, logged)
fakes.fake_float(0x4F16E0, 2, VALUE)
RUN = [fakes.logger(1) for _ in range(3)]
ARG2 = [fakes.logger(2) for _ in range(3)]
POS = [fakes.logger(2, deref=(1, 2)) for _ in range(3)]
POS3 = [fakes.logger(2, deref=(1, 3)) for _ in range(3)]
emu.baseline = emu.snapshot()


def check(orig, ours, setup, **kw):
    kw.setdefault("check_eax", False)
    return compare(emu, orig, ours, setup=setup, normalized=True, **kw)


def base(e):
    R.clear()
    e.call(0x4B0BA0)  # Mem_Init
    u16(e, 0x57BCB8, TT)


def rotation(rng):
    import math
    a, b, c = rng.uniform(-3, 3), rng.uniform(-3, 3), rng.uniform(-3, 3)
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    return [cb * cc, -cb * sc, sb, sa * sb * cc + ca * sc, -sa * sb * sc + ca * cc, -sa * cb,
            -ca * sb * cc + sa * sc, ca * sb * sc + sa * cc, ca * cb]


def train(e, rng, parent=None, type_=TT):
    t = task(e, 0x8E0, type_, parent)
    i32(e, t + 0x6C, model(e, rng.uniform(100, 5000)))
    e.write(t + 0x98, struct.pack("<3f", *[rng.choice([0.5, 1.0, 2.0, 1.5]) for _ in range(3)]))
    e.write(t + 0x20, struct.pack("<3d", *[rng.uniform(-1e6, 1e6) for _ in range(3)]))
    e.write(t + 0x70, struct.pack("<9fi", *rotation(rng), rng.randint(0, 50)))
    e.write(t + 0x720, struct.pack("<5f", *[rng.uniform(0, 2) for _ in range(5)]))
    f32(e, t + 0x730, rng.choice([-1.0, rng.uniform(0, 2), 0.5]))
    e.write(t + 0x748, struct.pack("<8f", rng.uniform(0, 1), rng.uniform(1000, 9000), rng.uniform(0, 0.3),
                                   rng.uniform(50, 200), rng.uniform(0, 1), 0, 0, rng.uniform(-30, 30)))
    e.write(t + 0x768, struct.pack("<4f", rng.uniform(0, 1e5), rng.uniform(0, 1e5), rng.uniform(-5, 5),
                                   rng.uniform(-5, 5)))
    i32(e, t + 0x77C, rng.randint(0, 9))
    e.write(t + 0x780, b"train_model\0")
    u8(e, t + 0x880, rng.randint(0, 1))
    return t


def with_wagons(e, rng, n, equal=()):
    head = train(e, rng, task(e, 0x20, 3))
    wagons = [train(e, rng, head) for _ in range(n)]
    i32(e, head + 0x778, n)
    p = [rng.uniform(-1e5, 1e5) for _ in range(3)]
    pts = []
    for i in range(n + 2):
        if i not in equal:
            p = [c + rng.uniform(-9000, 9000) for c in p]
        pts.append(list(p))
    R["pts"] = pts
    return head, wagons


# --- small handlers -------------------------------------------------------------

def test_open():
    check(0x42ACC0, "Train_Open", base)


def test_get_params():
    def setup(e):
        base(e)
        i32(e, 0x57BCBC, 0xBCBC)
        return 0x1234, e.alloc(4)
    check(0x42B030, "Train_GetParams", setup)


@pytest.mark.parametrize("parent_type", [TT, 3])
def test_create_delete(parent_type):
    def setup(e):
        base(e)
        rng = random.Random(1)
        parent = task(e, 0x8E0, parent_type)
        i32(e, parent + 0x778, 4)
        t = train(e, rng, parent)
        R[0x4E6AB0] = 0x6AB0
        return (t,)
    check(0x429F80, "Train_CreateHandler", setup)

    def setup2(e):
        t = setup(e)[0]
        i32(e, t + 0x884, 0x8840)
        return (t,)
    check(0x42A100, "Train_DeleteHandler", setup2)


def test_get_sound_source_and_state():
    def setup(e):
        base(e)
        t = train(e, random.Random(2))
        i32(e, t + 0x884, 0x8840)
        msg = e.alloc(12)
        pos, state = e.alloc(24), e.alloc(16)
        e.write(msg, struct.pack("<3I", state, 0x5555, pos))
        e.write(state, struct.pack("<4f", 1.5, 2.5, 3.5, 4.5))
        R["pts"] = [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]
        return t, msg
    check(0x42B220, "Train_GetSoundSource", setup)
    check(0x42B240, "Train_SaveState", setup)
    check(0x42B280, "Train_LoadState", setup)


@pytest.mark.parametrize("index", [-1, 0, 1, 2])
def test_input_channel(index):
    def setup(e):
        base(e)
        msg = e.alloc(8)
        e.write(msg, struct.pack("<iI", index, 0x7777))
        return 0x1234, msg
    check(0x42B2C0, "Train_InputChannelHandler", setup)


@pytest.mark.parametrize("seed", range(40))
def test_read_input(seed):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        for k in [0x1D, 0x9D, 0x29] + list(range(2, 12)):
            u8(e, 0xC28C62 + k, rng.choice([0, 0x80, 0x81, 0x7F]) if rng.random() < 0.3 else 0)
            u8(e, 0xC28D62 + k, rng.choice([0, 0x80]))
        if seed % 4 == 0:
            u8(e, 0xC28C62 + 0x1D, 0x80)
        out = e.alloc(8)
        msg = e.alloc(4)
        i32(e, msg, out)
        return 0x1234, msg
    check(0x42B2F0, "Train_ReadInputHandler", setup)


@pytest.mark.parametrize("name", [0, 1])
def test_get_info(name):
    def setup(e):
        base(e)
        t = train(e, random.Random(3 + name))
        out = e.alloc(0x54)
        if name:
            i32(e, out + 0x50, e.alloc(8))
        return t, out
    check(0x42B500, "Train_GetInfo", setup)


def test_move_handler():
    def setup(e):
        base(e)
        t = train(e, random.Random(4))
        for i, h in enumerate(POS[:2]):
            task(e, 0x20, 0x60 + i, t)
            set_handler(e, EV["ev4"], 0x60 + i, h)
        return (t,)
    check(0x42AC20, "Train_MoveHandler", setup)


# --- placing on the path --------------------------------------------------------

def children(e, t, n):
    for i in range(n):
        c = task(e, 0x20, 0x60 + i, t)
        set_handler(e, EV["ev4"], 0x60 + i, POS[i])


def colls(e, t, n):
    coll = task(e, 0x20, 9)
    i32(e, t + 0xE8, coll)
    for i in range(n):
        task(e, 0x20, 0x68 + i, coll)
        set_handler(e, EV["move"], 0x68 + i, POS3[i])


@pytest.mark.parametrize("seed", range(30))
def test_place(seed):
    def setup(e):
        base(e)
        rng = random.Random(seed)
        n = seed % 4
        equal = {rng.randint(0, n + 1)} if seed % 3 == 0 else ()
        head, wagons = with_wagons(e, rng, n, equal)
        R["found"] = seed % 7 != 0
        for t in [head] + wagons:
            if rng.random() < 0.5:
                children(e, t, rng.randint(1, 2))
            if rng.random() < 0.5:
                colls(e, t, rng.randint(1, 2))
        return head, seed % 2
    check(0x42A2C0, "Train_Place", setup)


@pytest.mark.parametrize("seed", range(10))
def test_place_wagons(seed):
    def setup(e):
        base(e)
        rng = random.Random(50 + seed)
        n = 1 + seed % 3
        head, wagons = with_wagons(e, rng, n, {rng.randint(0, n + 1)} if seed % 2 else ())
        for t in wagons:
            if rng.random() < 0.5:
                children(e, t, 1)
            if rng.random() < 0.5:
                colls(e, t, 1)
        pts = e.alloc(24 * (n + 2))
        e.write(pts, b"".join(struct.pack("<3d", *p) for p in R["pts"]))
        return head, pts + 24, seed % 2
    check(0x42A7C0, "Train_PlaceWagons", setup)


@pytest.mark.parametrize("seed", range(16))
def test_params_changed(seed):
    def setup(e):
        base(e)
        rng = random.Random(70 + seed)
        head, wagons = with_wagons(e, rng, seed % 3)
        t = wagons[-1] if wagons and seed % 2 else head
        R[0x4CEC10] = model(e, 2500.0)
        R["v3f"] = [[rng.uniform(-9000, 9000) for _ in range(3)] for _ in range(2)]
        coll = task(e, 0x20, 9)
        i32(e, t + 0xE8, coll)
        task(e, 0x20, 0x6C, coll)
        set_handler(e, EV["changed"], 0x6C, ARG2[0])
        arg = [0, e.alloc(4), e.alloc(4)][seed % 3]
        if seed % 3 == 2:
            e.write(arg, b"x\0")
        return t, arg
    check(0x42A150, "Train_ParamsChanged", setup)


@pytest.mark.parametrize("seed", range(24))
def test_input(seed):
    def setup(e):
        base(e)
        rng = random.Random(90 + seed)
        head, wagons = with_wagons(e, rng, seed % 2)
        inp = e.alloc(8)
        e.write(inp, struct.pack("<2f", rng.choice([-1.0, 0.0, 0.5, 1.0, rng.uniform(0, 2)]), 7.0))
        msg = e.alloc(8)
        e.write(msg, struct.pack("<2I", seed % 4 != 0, inp))
        if seed % 3 == 0:
            f32(e, head + 0x720, struct.unpack("<f", e.read(head + 0x730, 4))[0])
        return head, msg
    check(0x42B040, "Train_InputHandler", setup)


@pytest.mark.parametrize("seed", range(24))
def test_run(seed):
    def setup(e):
        base(e)
        rng = random.Random(110 + seed)
        head, wagons = with_wagons(e, rng, seed % 2 + 1)
        t = wagons[0] if seed % 2 else head
        R[0x4028B0] = 77 if seed % 8 == 0 else 78
        i32(e, t + 0x888, 77)
        R[0x4F16C0] = seed % 3 != 0
        f64(e, VALUE, [0.0, 0.4, 0.6, 3.0, -0.2][seed % 5])
        i32(e, t + 0x88C, 0x88C0 if seed % 4 < 2 else 0)
        R[0x4E6B00] = 0x6B00 if seed % 5 else 0
        if seed % 6:
            lst = e.alloc(12)
            i32(e, t + 0x884, lst)
        f32(e, head + 0x720, [0.0, 0.0005, 0.1, 0.17, 0.3, 2.0][seed % 6])
        for i, h in enumerate(RUN[: seed % 3]):
            task(e, 0x20, 0x60 + i, t)
            set_handler(e, 1, 0x60 + i, h)
        if seed % 4 != 3:
            coll = task(e, 0x20, 9)
            i32(e, t + 0xE8, coll)
            task(e, 0x20, 0x6A, coll)
            set_handler(e, 1, 0x6A, RUN[2])
        return (t,)
    check(0x42B700, "Train_Run", setup)
