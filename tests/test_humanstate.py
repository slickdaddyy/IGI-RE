import math
import struct

import pytest
from unicorn import UC_HOOK_CODE

import emu as emu_mod
from emu import Emu, compare

e = Emu()

CALL_LOG = emu_mod.CALL_LOG
STUBS = emu_mod.FAKE_BASE + 0x40000   # one "ret" per state table entry
FLOAT_TAIL = emu_mod.FAKE_BASE + 0x48000
FLOAT_VALUE = FLOAT_TAIL + 0x10

OBJ = 0x5000_0000  # placeholders, set in make_human
cfg = {}


def f32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def log_words(words):
    n = e.u32(CALL_LOG)
    e.write(CALL_LOG + 4 + 4 * n, struct.pack("<%dI" % len(words), *[w & 0xFFFFFFFF for w in words]))
    e.write(CALL_LOG, struct.pack("<I", n + len(words)))


def string_words(p):
    s = e.read_cstr(p) + b"\0"
    s += b"\0" * (-len(s) % 4)
    return list(struct.unpack("<%dI" % (len(s) // 4), s))


e.write(FLOAT_TAIL, b"\xdd\x05" + struct.pack("<I", FLOAT_VALUE) + b"\xc3")


def fake(addr, spec="", ret=0, fret=None, fill=None):
    """Python stub for the original function at addr. spec has one token per argument:
    v value, x ignored, s C string, pN N words at the pointer, w low 16 bits, b low 8 bits.
    ret / fret may be a cfg key; fill maps an argument to bytes written to where it points."""
    tokens = spec.split()

    def hit(uc, address, size, user):
        args = [e.arg(i) for i in range(len(tokens))]
        words = [address]
        for t, a in zip(tokens, args):
            if t == "v":
                words.append(a)
            elif t == "s":
                words += string_words(a)
            elif t[0] == "p":
                words += list(struct.unpack("<%dI" % int(t[1:]), e.read(a, 4 * int(t[1:]))))
            elif t == "w":
                words.append(a & 0xFFFF)
            elif t == "b":
                words.append(a & 0xFF)
        log_words(words)
        for i, data in (fill or {}).items():
            e.write(args[i], cfg[data] if isinstance(data, str) else data)
        if fret is not None:
            v = cfg[fret] if isinstance(fret, str) else fret
            e.write(FLOAT_VALUE, struct.pack("<d", v))
            e.uc.reg_write(emu_mod.UC_X86_REG_EIP, FLOAT_TAIL)
        else:
            e.ret(cfg[ret] if isinstance(ret, str) else ret)

    e.uc.hook_add(UC_HOOK_CODE, hit, begin=addr, end=addr)


def vec3d(*v):
    return struct.pack("<3d", *v)


def matrix(ax, ay, az):
    """Row major RotZ(az) * RotY(ay) * RotX(ax), then the counter word."""
    cx, sx, cy, sy, cz, sz = math.cos(ax), math.sin(ax), math.cos(ay), math.sin(ay), math.cos(az), math.sin(az)
    m = [cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx,
         sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx,
         -sy, cy * sx, cy * cx]
    return struct.pack("<9fI", *m, 0)


for addr, spec in {
    0x408460: "v", 0x4137E0: "v v", 0x413A60: "v v", 0x443EA0: "v", 0x444B90: "v", 0x444BA0: "v",
    0x444BC0: "v", 0x447D60: "v v", 0x447DA0: "v", 0x450CE0: "p7", 0x460CE0: "v", 0x4631A0: "v p6",
    0x463220: "v v", 0x463260: "v", 0x4632D0: "v", 0x463310: "v v", 0x463550: "v", 0x463570: "v",
    0x463580: "v", 0x463590: "v", 0x4635A0: "v", 0x4635F0: "v", 0x463600: "v", 0x463610: "v",
    0x463620: "v", 0x4636A0: "v v", 0x463700: "v v", 0x463760: "v v", 0x464700: "v", 0x464710: "v",
    0x464720: "v", 0x475340: "v", 0x489E70: "v v v", 0x489EA0: "v v v v", 0x4D2FB0: "v v v",
    0x4D6580: "v v v", 0x4D65B0: "v v v", 0x4E6C00: "v", 0x4E6C30: "v v",
}.items():
    fake(addr, spec)

fake(0x42FDD0, "v", ret=0x4242)
fake(0x43E4E0, "", ret="type")
fake(0x43F8A0, "v", ret="b43F8A0")
fake(0x447D40, "v", ret="b447D40")
fake(0x447D50, "v", ret="b447D50")
fake(0x4485D0, "v v v", ret=0x1234ABCD)
fake(0x4636E0, "v", ret="obj")
fake(0x463740, "v", ret=0x77)
fake(0x4C3440, "v", ret=0x31337)
fake(0x4D7040, "v v", ret="frames53")
fake(0x4E6B00, "v s", ret="sound")
fake(0x4F0EE0, "s", ret=0x5150)
fake(0x4F1030, "v", ret="o2")
fake(0x401CF0, "w w", ret="isa")
fake(0x416850, "", ret=0x12345601)
fake(0x416920, "", ret=0x9999)
for addr in (0x42FDF0, 0x444B30, 0x448570):
    fake(addr, "v x x", fill={1: "pos", 2: "mat"})
fake(0x4CC2A0, "v x p6 p6 v b p2 v", fill={1: "trace"})
fake(0x440270, "", fret=1.37)
fake(0x444B80, "v", fret=2.1)
fake(0x447D30, "v", fret=3.3)
fake(0x4485C0, "v", fret=1.9)
fake(0x4D64E0, "v v", fret=0.7512345)
fake(0x4D6520, "v v", fret=0.3000001)

for n in range(48):
    e.write(STUBS + 16 * n, b"\xc3")
    fake(STUBS + 16 * n, "v v")

DEFAULTS = dict(type=7, b43F8A0=0, b447D40=1, b447D50=1, sound=0x50D0, isa=1, frames53=37,
                pos=vec3d(100.5, -20.25, 3.125), mat=matrix(0.1, -0.2, 1.1), trace=bytes(0x70))


def make_human(e, state=0, prev=0, inp=0, fields=None):
    """A Human with its state machine at +0x2EC whose enter table points at logging stubs."""
    cfg.clear()
    cfg.update(DEFAULTS)
    h = e.alloc(0x1000)
    obj = e.alloc(0x300)
    cfg["obj"] = obj
    cfg["o2"] = e.alloc(0x200)
    e.write(obj + 0x2AC, struct.pack("<i", 5))
    table = e.alloc(48 * 4)
    e.write(table, struct.pack("<48I", *[STUBS + 16 * n for n in range(48)]))
    view = e.alloc(0x100)
    e.write(view + 0xE0, vec3d(90.0, -25.5, 60.75))
    ground = e.alloc(0x200)
    e.write(ground + 0x114, b"step_concrete\0")
    e.write(h + 0x20, vec3d(130.0, 10.0, 5.0))
    e.write(h + 0x78, matrix(0.0, 0.0, 0.7)[:36])
    e.write(h + 0x520, struct.pack("<3f", 3.3, -7.1, 0.4))
    e.write(h + 0x728, struct.pack("<3f", 0.25, 0.5, -0.75))
    e.write(h + 0x664, struct.pack("<6f", 11.0, -3.5, 7.25, 1.0, 2.0, 3.0))
    e.write(h + 0xDA4, struct.pack("<4f", 0.6, 0.0, -0.8, 123.4))
    e.write(h + 0x4EC, struct.pack("<I", view))
    e.write(h + 0x6EC, struct.pack("<I", ground))
    e.write(h + 0xFC, struct.pack("<I", 0xABC))
    e.write(h + 0x2EC, struct.pack("<6I", 0, table, inp, prev, state, 0))
    e.write(0x56E1EC, struct.pack("<2f", 245.7, 312.9))
    for off, v in (fields or {}).items():
        if isinstance(off, str):
            cfg[off] = v
        else:
            e.write(h + off, struct.pack(v[0], *(v[1] if isinstance(v[1], tuple) else (v[1],))))
    return h


def run(addr, name, state=0, prev=0, inp=0, fields=None, conf=None, args=None):
    def setup(e):
        h = make_human(e, state, prev, inp, fields)
        cfg.update(conf or {})
        return args(h) if args else (h + 0x2EC, h)
    compare(e, addr, name, setup=setup, normalized=True, check_eax=False)


ON, RUN, CROUCH = 0x4, 0x10, 0x40000
F, B, L, R = 0x80, 0x100, 0x40, 0x20


# --- running ----------------------------------------------------------------

RUN_UPDATES = [
    (0x40AEF0, "HumanPlayer_RunForwardLeftUpdate", F | L),
    (0x40B0A0, "HumanPlayer_RunForwardRightUpdate", F | R),
    (0x40B240, "HumanPlayer_RunBackUpdate", B),
    (0x40B410, "HumanPlayer_RunBackLeftUpdate", B | L),
    (0x40B5C0, "HumanPlayer_RunBackRightUpdate", B | R),
    (0x40B760, "HumanPlayer_RunLeftUpdate", L),
    (0x40B920, "HumanPlayer_RunRightUpdate", R),
]
INPUTS = [0, ON, ON | 0x10000, ON | 0x20000, ON | RUN, ON | 0x2000, ON | CROUCH, ON | RUN | CROUCH]
INPUTS += [ON | RUN | d for d in (F, B, L, R, F | L, F | R, B | L, B | R)]
INPUTS += [ON | d for d in (F, B, L, R, F | L, F | R, B | L, B | R)]
INPUTS += [ON | CROUCH | d for d in (F | L, B | R, L, R)]


@pytest.mark.parametrize("addr,name,keys", RUN_UPDATES)
@pytest.mark.parametrize("inp", INPUTS)
def test_run_update(addr, name, keys, inp):
    run(addr, name, state=21, inp=inp)


RUN_ENTERS = [
    (0x40B020, "HumanPlayer_RunForwardRightEnter", 22, (3, 0x14, 0x1B)),
    (0x40B1D0, "HumanPlayer_RunBackEnter", 23, (0xC,)),
    (0x40B390, "HumanPlayer_RunBackLeftEnter", 24, (5, 0xD, 0x17)),
    (0x40B540, "HumanPlayer_RunBackRightEnter", 25, (6, 0xE, 0x17)),
    (0x40B6F0, "HumanPlayer_RunLeftEnter", 26, (7,)),
    (0x40B8B0, "HumanPlayer_RunRightEnter", 27, (8,)),
]


@pytest.mark.parametrize("addr,name,state,keep", RUN_ENTERS)
def test_run_enter(addr, name, state, keep):
    for prev in keep + (0, 0x13):
        run(addr, name, state=state, prev=prev)


# --- jump and fall ------------------------------------------------------------

@pytest.mark.parametrize("move", [(3.3, -7.1), (0.0, 0.0), (0.0, 2.5), (-1.75, 0.0), (1e-3, 1234.5)])
def test_jump_enter(move):
    run(0x40BA70, "HumanPlayer_JumpEnter", state=28, fields={0x520: ("<2f", move)})


@pytest.mark.parametrize("case", ["none", "nullobj", "same", "other"])
def test_jump_enter_platform(case):
    def args(h):
        if case != "none":
            pp = e.alloc(8)
            obj = e.alloc(0x40)
            e.write(obj + 0x1C, struct.pack("<H", 7 if case == "same" else 8))
            e.write(pp, struct.pack("<I", 0 if case == "nullobj" else obj))
            e.write(h + 0x768, struct.pack("<I", pp))
        return h + 0x2EC, h
    run(0x40BA70, "HumanPlayer_JumpEnter", state=28, args=args)


@pytest.mark.parametrize("inp", [0, ON, ON | F, ON | B, ON | L, ON | R, ON | CROUCH, ON | CROUCH | F,
                                 ON | CROUCH | B, ON | CROUCH | L, ON | CROUCH | R, ON | F | B | L])
@pytest.mark.parametrize("ground", [True, False])
@pytest.mark.parametrize("sound", [0, 0x50D0])
def test_jump_update(inp, ground, sound):
    fields = {} if ground else {0x6EC: ("<I", 0)}
    run(0x40BBD0, "HumanPlayer_JumpUpdate", state=28, inp=inp, fields=fields, conf={"sound": sound})


def test_fall():
    run(0x40BDB0, "HumanPlayer_FallEnter", state=29)
    for inp in (0, ON, ON | F):
        for ground in (True, False):
            run(0x40BE40, "HumanPlayer_FallUpdate", state=29, inp=inp,
                fields={} if ground else {0x6EC: ("<I", 0)})


# --- actions ------------------------------------------------------------------

def test_flags():
    for addr, name in ((0x40C140, "HumanPlayer_SetFlagD44"), (0x40C150, "HumanPlayer_SetFlagD68"),
                       (0x40C2D0, "HumanPlayer_ClearFlagD44"), (0x40C2E0, "HumanPlayer_ClearFlagD68")):
        run(addr, name, fields={0xD44: ("<I", 0x55555555), 0xD68: ("<I", 0x55555555)},
            args=lambda h: (h,))


def test_picklock_noise():
    run(0x40C270, "HumanPlayer_PicklockNoise", args=lambda h: (cfg["obj"], h))


# human positions in front of and behind the object
SIDES = [(130.0, 10.0, 5.0), (60.0, -80.0, -2.0), (100.5, -20.25, 3.125)]
MATS = [matrix(0.1, -0.2, 1.1), matrix(0.0, 0.0, 3.0), matrix(0.4, 0.3, -2.2)]


@pytest.mark.parametrize("addr,name,state", [
    (0x40BED0, "HumanPlayer_PicklockEnter", 30),
    (0x40C2F0, "HumanPlayer_Use31Enter", 31),
    (0x40C670, "HumanPlayer_TypeComputerEnter", 32),
    (0x40C970, "HumanPlayer_Use33Enter", 33),
    (0x40CB90, "HumanPlayer_Use34Enter", 34),
    (0x40CF70, "HumanPlayer_Use35Enter", 35),
])
@pytest.mark.parametrize("side", SIDES)
@pytest.mark.parametrize("mat", MATS)
def test_action_enter(addr, name, state, side, mat):
    run(addr, name, state=state, fields={0x20: ("<3d", side), "mat": mat})


# (unk1D4, input, frame, frames)
TICKS = [(1.0, 0, 3, 10), (1.0, 0, 9, 10), (1.0, 0, 10, 10), (0.0, 0, 3, 10), (0.0, 0, 9, 10),
         (1.0, 0x20000, 3, 10), (1.0, 0x20000, 9, 10), (-2.0, 1, 12, 10), (0.0, 0, 12, 10)]


@pytest.mark.parametrize("addr,name", [
    (0x40C160, "HumanPlayer_PicklockUpdate"),
    (0x40C870, "HumanPlayer_TypeComputerUpdate"),
    (0x40D580, "HumanPlayer_Use35Update"),
])
@pytest.mark.parametrize("tick", TICKS)
@pytest.mark.parametrize("sound", [0, 0x50D0])
def test_action_update(addr, name, tick, sound):
    v, inp, frame, frames = tick
    run(addr, name, inp=inp, conf={"sound": sound},
        fields={0x1D4: ("<f", v), 0xD60: ("<2i", (frames, frame)), 0xD70: ("<I", sound)})


@pytest.mark.parametrize("tick", TICKS)
@pytest.mark.parametrize("stage", [0, 1, 2, 5])
@pytest.mark.parametrize("frames53", [37, 0, 2000])
def test_use31_update(tick, stage, frames53):
    v, inp, frame, frames = tick
    run(0x40C510, "HumanPlayer_Use31Update", inp=inp, conf={"frames53": frames53},
        fields={0x1D4: ("<f", v), 0xD60: ("<2i", (frames, frame)), 0xD74: ("<i", stage)})


@pytest.mark.parametrize("v1d4,v134", [(0.0, 0.0), (1.0, 0.0), (1.0, 2.0), (0.0, 2.0)])
@pytest.mark.parametrize("inp", [0, 0x20000])
def test_use33_update(v1d4, v134, inp):
    run(0x40CAE0, "HumanPlayer_Use33Update", inp=inp, fields={0x1D4: ("<f", v1d4), 0x134: ("<f", v134)})


@pytest.mark.parametrize("inp", [0, 1, 0x101])
def test_use34_update(inp):
    run(0x40CCC0, "HumanPlayer_Use34Update", inp=inp)


@pytest.mark.parametrize("action", list(range(15)))
@pytest.mark.parametrize("flags", [(0, 1, 1), (1, 0, 1), (1, 1, 0), (1, 1, 1)])
def test_start_action(action, flags):
    a, b, c = flags
    run(0x40CD30, "HumanPlayer_StartAction", conf={"b447D40": a, "b447D50": b, "b43F8A0": c},
        args=lambda h: (h, action))


def trace(hit, point=(70.0, -40.0, 12.5), normal=(0.0, -3.0, 0.5)):
    t = bytearray(b"\xcd" * 0x70)
    t[8:32] = vec3d(*point)
    t[0x38:0x44] = struct.pack("<3f", *normal)
    t[0x4C:0x50] = struct.pack("<i", hit & 1)
    t[0x58:0x5C] = struct.pack("<i", hit >> 1)
    return bytes(t)


@pytest.mark.parametrize("hit", [0, 1, 2, 3])
@pytest.mark.parametrize("isa", [0, 1])
@pytest.mark.parametrize("id_", [5, -1])
@pytest.mark.parametrize("o2", [True, False])
def test_use36_enter(hit, isa, id_, o2):
    def args(h):
        e.write(cfg["obj"] + 0x2AC, struct.pack("<i", id_))
        if not o2:
            cfg["o2"] = 0
        else:
            e.write(cfg["o2"] + 0x1C, struct.pack("<H", 0x33))
        return h + 0x2EC, h
    run(0x40D180, "HumanPlayer_Use36Enter", state=36, args=args,
        conf={"trace": trace(hit), "isa": isa})


@pytest.mark.parametrize("normal", [(0.0, 0.0, 0.0), (0.0, 0.0, 2.0), (1.5, 0.0, 0.0)])
def test_use36_enter_normal(normal):
    run(0x40D180, "HumanPlayer_Use36Enter", state=36, conf={"trace": trace(3, normal=normal)})


# --- ladders and wires --------------------------------------------------------

def test_simple_enters():
    for addr, name, state in ((0x40D660, "HumanPlayer_LadderEnter", 37),
                              (0x40D8E0, "HumanPlayer_LadderOnEnter", 38),
                              (0x40DA40, "HumanPlayer_LadderOffTopEnter", 39),
                              (0x40DBA0, "HumanPlayer_LadderSlideEnter", 40),
                              (0x40DCA0, "HumanPlayer_State41Enter", 41),
                              (0x40DD60, "HumanPlayer_WireDragEnter", 42),
                              (0x40DE90, "HumanPlayer_ZiplineEnter", 43)):
        run(addr, name, state=state)


STEPS = [(8, 2.0, 1.0), (8, 2.0, 2.0), (7, 2.0, 1.0)]  # unk538, unk540, last step


@pytest.mark.parametrize("step", STEPS)
@pytest.mark.parametrize("inp", [0, F, B, F | B, 0x20000, 1, 0x20000 | 1])
@pytest.mark.parametrize("dir_,pos", [(0, 0), (0, 4), (0, 14), (1, 4), (-1, 1)])
@pytest.mark.parametrize("keys", [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0)])
@pytest.mark.parametrize("counter", [0, 11])
def test_ladder_update(step, inp, dir_, pos, keys, counter):
    s538, s540, last = step
    fields = {0x538: ("<i", s538), 0x540: ("<f", s540), 0xD88: ("<f", last),
              0xD7C: ("<4i", (20, pos, dir_, f32(last))), 0xD8C: ("<i", counter),
              0x1D4: ("<f", keys[0]), 0x134: ("<f", keys[1])}
    run(0x40D6B0, "HumanPlayer_LadderUpdate", inp=inp, fields=fields)
    if dir_ == 0 and keys == (0.0, 0.0):
        for addr, name in ((0x40D920, "HumanPlayer_LadderOnUpdate"),
                           (0x40DA80, "HumanPlayer_LadderOffTopUpdate")):
            run(addr, name, inp=inp, fields=fields)


def test_ladder_slide_update():
    for inp in (0, ON):
        for sound in (0, 0x50D0):
            run(0x40DBF0, "HumanPlayer_LadderSlideUpdate", inp=inp, fields={0xDB4: ("<I", sound)})


@pytest.mark.parametrize("inp", [0, 1, 0x20000, 0x20001])
@pytest.mark.parametrize("frame", [(3, 10), (10, 10)])
def test_state41_update(inp, frame):
    run(0x40DD00, "HumanPlayer_State41Update", inp=inp, fields={0xD60: ("<2i", frame)})


@pytest.mark.parametrize("step", STEPS)
@pytest.mark.parametrize("inp", [0, F, B, F | B])
@pytest.mark.parametrize("dir_", [0, 1, -1])
@pytest.mark.parametrize("counter", [0, 1])
def test_wire_drag_update(step, inp, dir_, counter):
    s538, s540, last = step
    run(0x40DDA0, "HumanPlayer_WireDragUpdate", inp=inp,
        fields={0x538: ("<i", s538), 0x540: ("<f", s540), 0xD98: ("<f", last), 0xD84: ("<i", dir_),
                0xD9C: ("<i", counter)})


@pytest.mark.parametrize("wire", [(0.6, 0.0, -0.8, 123.4), (0.0, 1.0, 0.0, -7.7), (0.3, 0.3, 0.9, 0.0)])
def test_zipline_update(wire):
    run(0x40DEF0, "HumanPlayer_ZiplineUpdate", fields={0xDA4: ("<4f", wire)})
