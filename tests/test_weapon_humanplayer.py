import math
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

import emu as emu_mod
from emu import Emu, ErrorShown, compare

e = Emu()

CALL_LOG = emu_mod.CALL_LOG
FLOAT_TAIL = e.fake_top
FLOAT_VALUE = FLOAT_TAIL + 0x10
e.fake_top += 0x20
e.write(FLOAT_TAIL, b"\xdd\x05" + struct.pack("<I", FLOAT_VALUE) + b"\xc3")

HANDLERS = 0xA96AE0
TYPES, WSIZE = 0x56E258, 0x6E8
RENDER_FUNCS, QVERTEX_STRIP = 0xA94E84, 0xA84A54
cfg = {}
counters = {}


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def f32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def log_words(words):
    n = e.u32(CALL_LOG)
    e.write(CALL_LOG + 4 + 4 * n, struct.pack("<%dI" % len(words), *[w & 0xFFFFFFFF for w in words]))
    e.write(CALL_LOG, u32(n + len(words)))


def string_words(p):
    s = e.read_cstr(p) + b"\0"
    s += b"\0" * (-len(s) % 4)
    return list(struct.unpack("<%dI" % (len(s) // 4), s))


def value(v, args):
    if callable(v):
        return v(args)
    if isinstance(v, str):
        v = cfg[v]
        if isinstance(v, list):
            i = counters.get(id(v), 0)
            counters[id(v)] = i + 1
            return v[i % len(v)]
    return v


def fake(addr, spec="", ret=0, fret=None, fill=None, extra=None):
    """Python stub for the code at addr. spec has one token per argument: v value, x ignored,
    s C string, pN N words at the pointer, w low 16 bits, b low 8 bits, @N the word at pointer+N.
    ret / fret may be a cfg key (a list there gives one value per call) or a function of the args;
    fill maps an argument to bytes written where it points."""
    tokens = spec.split()

    def hit(uc, address, size, user):
        args = [e.arg(i) for i in range(max(len(tokens), 8))]
        words = [address]
        for t, a in zip(tokens, args):
            if t == "v":
                words.append(a)
            elif t == "s":
                words += string_words(a)
            elif t[0] == "p":
                words += list(struct.unpack("<%dI" % int(t[1:]), e.read(a, 4 * int(t[1:]))))
            elif t[0] == "@":
                words.append(e.u32(a + int(t[1:])))
            elif t == "w":
                words.append(a & 0xFFFF)
            elif t == "b":
                words.append(a & 0xFF)
        if extra:
            words += extra(args)
        log_words(words)
        for i, data in (fill or {}).items():
            e.write(args[i], value(data, args))
        if fret is not None:
            e.write(FLOAT_VALUE, struct.pack("<d", value(fret, args)))
            e.uc.reg_write(UC_X86_REG_EIP, FLOAT_TAIL)
        else:
            e.ret(value(ret, args))

    e.uc.hook_add(UC_HOOK_CODE, hit, begin=addr, end=addr)


def code_stub(spec="", **kw):
    """A fresh address that runs fake(spec)."""
    p = e.fake_top
    e.fake_top += 16
    e.write(p, b"\xc3")
    fake(p, spec, **kw)
    return p


def vec3d(*v):
    return struct.pack("<3d", *v)


def matrix(ax, ay, az):
    cx, sx, cy, sy, cz, sz = math.cos(ax), math.sin(ax), math.cos(ay), math.sin(ay), math.cos(az), math.sin(az)
    m = [cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx,
         sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx,
         -sy, cy * sx, cy * cx]
    return struct.pack("<9fI", *m, 0)


for addr, spec in {
    0x401A20: "w", 0x40C140: "v", 0x40C2D0: "v", 0x450CE0: "p7", 0x45E060: "v v", 0x4631A0: "v p6",
    0x463220: "v v", 0x4632D0: "v", 0x463570: "v", 0x463580: "v", 0x463590: "v", 0x4635A0: "v",
    0x4635F0: "v", 0x463600: "v", 0x463610: "v", 0x463620: "v", 0x4636A0: "v v", 0x463760: "v v",
    0x4637C0: "v s v", 0x463AD0: "v p3 v", 0x464230: "v", 0x464240: "v", 0x464700: "v", 0x464710: "v",
    0x464720: "v", 0x465970: "v v v v", 0x467F30: "p9", 0x489E10: "v v", 0x489E70: "v v v",
    0x489ED0: "v v v v", 0x489F00: "v v", 0x489F20: "v v v v", 0x4B4A00: "v " * 10,
    0x4B4BE0: "v " * 23, 0x4B83D0: "v", 0x4B85B0: "v", 0x4C1830: "v", 0x4D65B0: "v v v", 0x4E6C00: "v",
    0x4E6C60: "v v v",
}.items():
    fake(addr, spec)

fake(0x401AE0, "v", ret=0xAAAA)
fake(0x4AF810, "s", extra=lambda a: [a[1]] if b"%" in e.read_cstr(a[0]) else [])
fake(0x401CF0, "w w", ret="isa")
fake(0x406220, "", ret="profile")
fake(0x414E20, "x v v", fill={0: lambda a: cfg["ends"][a[2]]})
fake(0x414ED0, "x v v", fill={0: lambda a: cfg["fends"][a[2]]})
fake(0x416850, "", ret=0x12345601)
fake(0x416920, "", ret=0x9999)
fake(0x416C90, "v v v v v", fret="blast")
fake(0x416D20, "", ret=0x7777)
fake(0x416D40, "", ret="game")
fake(0x416D80, "", ret="xp")
fake(0x43F820, "x v", fill={0: "mat"})
fake(0x460BE0, "", ret=0x33)
fake(0x487190, "", ret=0x205)
fake(0x489C60, "v v", ret="part")
fake(0x491CF0, "", ret="display")
fake(0x4636E0, "v", ret="obj")
fake(0x465800, "x v", fill={0: "cam"})
fake(0x4658A0, "x v", fill={0: "dir"})
fake(0x4B4740, "v", ret="next")
fake(0x4B4770, "v", fret="rand")
fake(0x4B80B0, "s", ret="script")
fake(0x4C3440, "v", ret=lambda a: cfg["positions"].get(a[0], 0x31337))
fake(0x4C48C0, "", ret=0x44)
fake(0x4CC2A0, "v x p6 p6 v b @4 v", fill={1: "coll"})
fake(0x4D2D50, "", fret=1.37)
fake(0x4E6B00, "v s", ret=0x5150)
fake(0x4E7C30, "", ret="range")
fake(0x4F2060, "v v", ret="slot")


def sprintf(a):
    fmt = e.read_cstr(a[1])
    if b"%" in fmt:
        d = struct.unpack("<d", struct.pack("<2I", a[2], a[3]))[0]
        e.write(a[0], (fmt.decode() % d).encode() + b"\0")
        return [a[2], a[3]]
    e.write(a[0], fmt + b"\0")
    return []


def printf(a):
    fmt = e.read_cstr(a[1])
    return string_words(a[2]) if b"%s" in fmt else []


fake(0x4A53B3, "x s", ret=1, extra=sprintf)
fake(0x460C80, "v s", extra=printf)

STATE_STUBS = [code_stub("v v") for _ in range(48)]
TASK_STUB = code_stub("v v")
RENDER_STUB = code_stub("p4", extra=lambda a: list(struct.unpack("<18I", e.read(e.u32(a[0] + 16), 72))))
# answers the weapon type query with the WeaponType pointer at task+0x20
GETTYPE = code_stub("v", fill={1: lambda a: e.read(a[0] + 0x20, 4)})
e.baseline = e.snapshot()

DEFAULTS = dict(isa=1, xp=0, part=3, blast=0.75, next=6, script=0x5C0000, range=0, slot=0,
                mat=matrix(0.1, -0.2, 1.1), cam=vec3d(90.0, -25.5, 60.75), dir=struct.pack("<3f", 0.6, -0.48, 0.64),
                rand=[0.6, 0.1, 0.9, 0.3, 0.7], positions={}, coll=bytes(0x68))


def make(e, fields=None):
    """A HumanPlayer with a view, an attached object and logging state stubs."""
    counters.clear()
    cfg.clear()
    cfg.update(DEFAULTS)
    h = e.alloc(0xDF0)
    view = e.alloc(0x200)
    obj = e.alloc(0x200)
    prof = e.alloc(16)
    e.write(prof, u32(1))
    cfg.update(obj=obj, view=view, profile=prof)
    table = e.alloc(48 * 4)
    e.write(table, struct.pack("<48I", *STATE_STUBS))
    e.write(view + 0xF8, matrix(0.3, -0.1, 0.8)[:36])
    e.write(obj + 0x20, vec3d(500.333, -1200.777, 64.1))
    e.write(obj + 0x70, matrix(0.0, 0.0, 0.9)[:36])
    e.write(obj + 0x130, b"\x01")
    e.write(obj + 0x138, u32(17))
    e.write(h + 0x20, vec3d(130.123456789, 10.987654321, 5.5555))
    e.write(h + 0x78, matrix(0.0, 0.0, 0.7)[:36])
    e.write(h + 0xA0, struct.pack("<f", 300.0))
    e.write(h + 0xFC, u32(0xABC))
    e.write(h + 0x254, struct.pack("<3f", 10.0, 100.0, 0.0))
    e.write(h + 0x260, struct.pack("<34f", *[(i * 0.5, 1.0 + 0.1 * i)[j] for i in range(17) for j in (0, 1)]))
    e.write(h + 0x2EC, struct.pack("<6I", 0, table, 0, 3, 5, 0))
    e.write(h + 0x4EC, u32(view))
    e.write(h + 0x664, struct.pack("<6f", 11.0, -3.5, 7.25, 0.0, 0.0, 3.0))
    e.write(h + 0xCF6, b"\x01")
    e.write(h + 0xDC8, struct.pack("<f", 0.125))
    for off, v in (fields or {}).items():
        if isinstance(off, str):
            cfg[off] = v
        else:
            e.write(h + off, struct.pack(v[0], *(v[1] if isinstance(v[1], tuple) else (v[1],))))
    return h


def run(addr, name, fields=None, args=None, **kw):
    def setup(e):
        h = make(e, fields)
        return args(h) if args else (h,)
    compare(e, addr, name, setup=setup, normalized=True, check_eax=False, **kw)


# --- weapon keys --------------------------------------------------------------

def weapon_tasks(e, h, ids):
    """Child tasks of h holding weapons with these ids."""
    e.write(0x5391B1, b"\x08")
    e.write(HANDLERS + 4 * (8 * 0x180 + 0x30), u32(GETTYPE))
    nodes = []
    for i, wid in enumerate(ids):
        e.write(TYPES + i * WSIZE, u32(wid))
        n = e.alloc(0x24)
        e.write(n + 0x1C, struct.pack("<H", 0x30))
        e.write(n + 0x20, u32(TYPES + i * WSIZE))
        nodes.append(n)
    lst = h + 8
    chain = nodes + [lst + 4]
    e.write(lst, u32(chain[0]) + u32(0) + u32(nodes[-1] if nodes else lst))
    for i, n in enumerate(nodes):
        e.write(n, u32(chain[i + 1]) + u32(chain[i - 1] if i else lst))


CATEGORIES = [(2, [10, 11]), (3, [20, 21, 22]), (0, []), (1, [40]), (2, [50, 51]), (0, []), (1, [70]), (0, []),
              (4, [90, 91, 92, 93])]


@pytest.mark.parametrize("keys,prev,cat,cycle,owned", [
    ([], [], 0, 0, [10]),
    ([1], [], 1, 0, [20, 21, 22]),
    ([1], [], 1, 2, [20, 21, 22]),
    ([1], [], 0, 1, [21]),
    ([1], [1], 1, 0, [20]),
    ([0, 4], [], 3, 1, [51, 11]),
    ([8], [], 8, 3, [91]),
    ([8], [], 8, 0, []),
    ([2], [], 0, 0, [10]),
    ([6, 3], [6], 0, 0, [40, 70]),
])
def test_select_weapon_category(keys, prev, cat, cycle, owned):
    def args(h):
        for k in keys:
            e.write(h + 0x1B0 + 4 * (23 + k), struct.pack("<f", 1.0))
        for k in prev:
            e.write(h + 0x110 + 4 * (23 + k), struct.pack("<f", 1.0))
        e.write(h + 0xD50, u32(cat) + u32(cycle))
        for i, (n, ids) in enumerate(CATEGORIES):
            e.write(0x56E0A0 + 36 * i, struct.pack("<9i", n, *(ids + [0] * (8 - len(ids)))))
        weapon_tasks(e, h, owned)
        return (h,)
    run(0x411000, "HumanPlayer_SelectWeaponCategory", args=args)


# --- per frame effects ----------------------------------------------------------

@pytest.mark.parametrize("level,sound", [(0.5, 0), (0.5, 0x77), (0.003, 0x77), (0.003, 0), (1.5, 0),
                                         (0.0, 0), (float("nan"), 0x77), (1.00001, 0x77)])
def test_ear_ringing(level, sound):
    run(0x411240, "HumanPlayer_UpdateEarRinging", fields={0xDC4: ("<If", (sound, level))})


@pytest.mark.parametrize("flash", [(0.0, 0.01, 0.5, -0.1), (0.0222222, 1.0, 0.0, 3.0), (float("nan"), 0, 0, 0)])
def test_fade_hit_flash(flash):
    run(0x411300, "HumanPlayer_FadeHitFlash", fields={0xDCC: ("<4f", flash)})


@pytest.mark.parametrize("flash", [0.0, 0.01, 0.5, 1.0, 1.005, 2.0, -0.5])
def test_fade_flash(flash):
    run(0x411350, "HumanPlayer_FadeFlash", fields={0xDDC: ("<f", flash)})


@pytest.mark.parametrize("t", [0, 1, 45])
def test_fence_timer(t):
    run(0x4113C0, "HumanPlayer_UpdateFenceTimer", fields={0xDE0: ("<i", t)})


# --- ladder, wire, zipline --------------------------------------------------------

ENDS = {"ends": [vec3d(10.5, -20.25, 30.125), vec3d(-40.0, 50.5, 4100.75), vec3d(1.5, 2.5, -3.5),
                 vec3d(7.0, -8.0, 2048.0)]}


@pytest.mark.parametrize("flags", [(0, 0, 0, 0), (1, 0, 0, 0), (1, 0, 0, 1), (1, 1, 1, 1), (0, 1, 1, 0),
                                   (0, 1, 1, 1)])
@pytest.mark.parametrize("dead", [0, 0x20000])
def test_ladder(flags, dead):
    fields = {0xD78: ("<4B", flags), 0x2F4: ("<I", dead), 0xD7C: ("<2i", (3, 9))}
    fields.update(ENDS)
    run(0x4113E0, "HumanPlayer_UpdateLadder", fields=fields)


@pytest.mark.parametrize("flags", [(1, 0, 0), (0, 1, 1), (0, 1, 0), (0, 0, 0)])
@pytest.mark.parametrize("pos", [(130.0, 10.0, 5.0), (600.0, -2000.0, 3.0)])
@pytest.mark.parametrize("dead", [0, 0x20000])
def test_unk41_attach(flags, pos, dead):
    run(0x411770, "sub_411770", fields={0xD90: ("<3B", flags), 0x20: ("<3d", pos), 0x2F4: ("<I", dead)})


WIRE_FENDS = [struct.pack("<3f", 1.0, -9000.0, 2.0), struct.pack("<3f", -1.0, 7000.0, 3.0)]


def wire_pos(side):
    """Player position whose lateral wire coordinate (object matrix column 1) is side."""
    m = struct.unpack("<9f", matrix(0.1, -0.2, 1.1)[:36])
    base = (500.333, -1200.777, 64.1)
    return tuple(base[i] + side * m[3 * i + 1] + 77.0 * m[3 * i] for i in range(3))


@pytest.mark.parametrize("enter", [1, 0])
@pytest.mark.parametrize("side", [-8000.0, -6000.5, 0.0, 3000.0, 3928.0, 3930.0, 6000.0])
@pytest.mark.parametrize("keys,dead", [((0.0, 0.0), 0), ((1.0, 0.0), 0), ((1.0, 1.0), 0), ((0.0, 1.0), 0x20000)])
def test_wire(enter, side, keys, dead):
    fields = {0xD93: ("<B", enter), 0xD94: ("<B", 1 - enter), 0x20: ("<3d", wire_pos(side)),
              0x1D4: ("<f", keys[0]), 0x134: ("<f", keys[1]), 0x2F4: ("<I", dead), "fends": WIRE_FENDS}
    run(0x411AE0, "HumanPlayer_UpdateWire", fields=fields)


@pytest.mark.parametrize("enter", [1, 0])
@pytest.mark.parametrize("side", [-8000.0, -6000.5, 0.0, 3927.5, 3928.0, 3930.0, 6000.0])
@pytest.mark.parametrize("keys,dead,sound", [((0.0, 0.0), 0, 0x99), ((1.0, 0.0), 0, 0x99), ((1.0, 0.0), 0, 0),
                                             ((0.0, 1.0), 0x20000, 0x99)])
def test_zipline(enter, side, keys, dead, sound):
    fields = {0xDA0: ("<B", enter), 0xDA1: ("<B", 1 - enter), 0x20: ("<3d", wire_pos(side)),
              0x1D4: ("<f", keys[0]), 0x134: ("<f", keys[1]), 0x2F4: ("<I", dead), 0xDB4: ("<I", sound),
              "fends": WIRE_FENDS}
    fields.update(ENDS)
    run(0x411F20, "HumanPlayer_UpdateZipline", fields=fields)


def test_zipline_downhill():
    fields = {0xDA0: ("<B", 1), 0x20: ("<3d", wire_pos(100.0)), "fends": WIRE_FENDS,
              "ends": [vec3d(7.0, -8.0, 2048.0), vec3d(10.5, -20.25, 30.125)]}
    run(0x411F20, "HumanPlayer_UpdateZipline", fields=fields)


# --- input --------------------------------------------------------------------

@pytest.mark.parametrize("i", [-1, 0, 7, 39, 40, 1000])
def test_input_channel(i):
    def args(h):
        q = e.alloc(8)
        e.write(q, struct.pack("<iI", i, 0x55555555))
        return 0, q
    run(0x4123F0, "HumanPlayer_InputChannelHandler", args=args)


def test_read_input():
    def args(h):
        q = e.alloc(8)
        e.write(q, u32(0x1234))
        return h, q
    run(0x412420, "HumanPlayer_ReadInputHandler", args=args)


# --- damage -----------------------------------------------------------------------

def hit_args(dirv, attacker=True, unk=0x42):
    def args(h):
        hit = e.alloc(0x18)
        att = 0
        if attacker:
            att = e.alloc(0x40)
            e.write(att + 0x1C, struct.pack("<H", 0x123))
        e.write(hit, struct.pack("<If3fI", unk, 7.5, *dirv, att))
        return h, hit
    return args


DIRS = [(0.31, -0.77, 0.123), (-0.913, 0.0417, 0.55), (0.071, 0.69, -0.333), (0.4142, 0.1732, 0.9),
        (-0.25, -0.61, -0.17), (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (-1.0, 0.1, 0.0), (0.0, 1.0, 0.0), (0.1, -1.0, 0.2), (0.0, 0.0, 1.0),
        (0.2, 0.1, -1.0), (0.5, 0.5, 0.5), (-0.3, 0.7, 0.7), (0.0, 0.0, 1e-30)]


@pytest.mark.parametrize("dirv", DIRS)
@pytest.mark.parametrize("rand", [[0.6, 0.5], [0.1, 0.0], [1.5, 0.99]])
def test_hit_flash(dirv, rand):
    run(0x412440, "HumanPlayer_HitHandler", fields={"rand": rand}, args=hit_args(dirv))


@pytest.mark.parametrize("case", ["normal", "invulnerable", "noprofile", "easy", "hard", "bad", "xp", "dead",
                                  "dead_noisa", "dead_attacked", "dead_noattacker", "quiet", "noclip"])
def test_hit(case):
    f = {}
    att = True
    level = {"easy": 0, "hard": 2, "bad": 7}.get(case, 1)
    if case == "invulnerable":
        f[0xCF6] = ("<B", 0)
    if case == "noprofile":
        f["profile"] = 0
    if case == "xp":
        f["xp"] = 1
    if case.startswith("dead"):
        f[0x254] = ("<f", 99.0)
    if case == "dead_noisa":
        f["isa"] = 0
    if case == "dead_attacked":
        f[0xDE8] = ("<I", 0x4444)
    if case == "dead_noattacker":
        att = False
    if case == "quiet":
        f[0xCF5] = ("<B", 1)
    if case == "noclip":
        f[0x52C] = ("<B", 1)

    def args(h):
        if cfg["profile"]:
            e.write(cfg["profile"], u32(level))
        return hit_args((0.3, -0.4, 0.2), att)(h)
    run(0x412440, "HumanPlayer_HitHandler", fields=f, args=args)


def blast_args(pos, radius=2000.0, unk10=500.0):
    def args(h):
        b = e.alloc(0x18)
        p = e.alloc(0x18)
        e.write(p, vec3d(*pos))
        e.write(b, struct.pack("<IIIffI", 1, p, 2, radius, unk10, 33))
        return h, b
    return args


@pytest.mark.parametrize("pos", [(90.0, -25.5, 60.75), (123.456, 77.7, -10.01), (-1234.5678, 333.3, 50.505), (100.0, -20.0, 70.0), (90.0, 2000.0, 60.0),
                                 (3000.0, 1000.0, 50.0), (9000.0, 0.0, 60.75), (90.0, 2.0e5, 60.75)])
@pytest.mark.parametrize("blast", [0.75, 0.0])
def test_blast(pos, blast):
    run(0x412990, "HumanPlayer_BlastHandler", fields={"blast": blast}, args=blast_args(pos))


@pytest.mark.parametrize("case", ["invulnerable", "noprofile", "noclip", "quiet", "easy", "bad", "xp"])
def test_blast_cases(case):
    f = {0xCF6: ("<B", 0)} if case == "invulnerable" else {}
    if case == "noprofile":
        f["profile"] = 0
    if case == "noclip":
        f[0x52C] = ("<B", 1)
    if case == "quiet":
        f[0xCF5] = ("<B", 1)
    if case == "xp":
        f["xp"] = 1

    def args(h):
        if cfg["profile"]:
            e.write(cfg["profile"], u32({"easy": 0, "bad": 5}.get(case, 1)))
        return blast_args((100.0, -20.0, 70.0))(h)
    run(0x412990, "HumanPlayer_BlastHandler", fields=f, args=args)


@pytest.mark.parametrize("apos", [(140.0, 900.0, 5.0), (140.0, -900.0, 5.0), (-500.0, 10.0, 5.0)])
@pytest.mark.parametrize("case", ["normal", "odd", "quiet", "invulnerable", "noprofile", "hard"])
def test_punch(apos, case):
    f = {"next": 7 if case == "odd" else 6}
    if case == "quiet":
        f[0xCF5] = ("<B", 1)
    if case == "invulnerable":
        f[0xCF6] = ("<B", 0)
    if case == "noprofile":
        f["profile"] = 0

    def args(h):
        att = e.alloc(0x40)
        hp, ap = e.alloc(0x18), e.alloc(0x18)
        e.write(hp, vec3d(130.0, 10.0, 5.0))
        e.write(ap, vec3d(*apos))
        cfg["positions"] = {h: hp, att: ap}
        if cfg["profile"]:
            e.write(cfg["profile"], u32(2 if case == "hard" else 1))
        p = e.alloc(8)
        e.write(p, struct.pack("<fI", 12.5, att))
        return h, p
    run(0x412CE0, "HumanPlayer_PunchHandler", fields=f, args=args)


@pytest.mark.parametrize("xp,timer,total", [(0, 0, 10.0), (1, 0, 10.0), (0, 3, 10.0), (0, 0, 99.9)])
@pytest.mark.parametrize("dmg", [2.7, 3.14159, 0.333, 17.77])
def test_fence(xp, timer, total, dmg):
    def args(h):
        e.write(0x56E208, struct.pack("<f", dmg))
        return (h,)
    run(0x412F80, "HumanPlayer_FenceHandler", fields={"xp": xp, 0xDE0: ("<i", timer), 0x254: ("<f", total)},
        args=args)


@pytest.mark.parametrize("pos,radius", [((90.0, -25.5, 60.75), 10.0), ((5000.0, -25.5, 60.75), 4000.0),
                                        ((5000.0, -25.5, 60.75), 6000.0), ((90.0, 2.0e5, 60.75), 1e6)])
def test_flashbang(pos, radius):
    def args(h):
        f = e.alloc(8)
        p = e.alloc(0x18)
        e.write(p, vec3d(*pos))
        e.write(f, struct.pack("<If", p, radius))
        return h, f
    run(0x413090, "HumanPlayer_FlashbangHandler", args=args)


# --- render -----------------------------------------------------------------------

@pytest.mark.parametrize("case", ["plain", "weapon", "noweapon", "bar", "range", "range_hit", "range_model",
                                  "range_bar", "effects", "flash_low", "flash_high", "flash_nan", "noeffects"])
def test_render(case):
    def args(h):
        game, settings, display = e.alloc(0x60), e.alloc(0x100), e.alloc(0x10)
        e.write(game + 0x5C, u32(settings))
        e.write(settings + 0xE8, u32(0 if case == "noeffects" else 1))
        e.write(display, struct.pack("<3i", 0, 800, 600))
        cfg.update(game=game, display=display)
        e.write(HANDLERS + 4 * (5 * 0x180 + 0x33), u32(TASK_STUB))
        e.write(HANDLERS + 4 * (5 * 0x180 + 0x44), u32(TASK_STUB))
        e.write(QVERTEX_STRIP, u32(3))
        e.write(RENDER_FUNCS + 12, u32(RENDER_STUB))
        if case in ("weapon", "noweapon"):
            e.write(h + 0xDE4, u32(1))
            if case == "weapon":
                w = e.alloc(0x40)
                e.write(w + 0x1C, struct.pack("<H", 0x44))
                cfg["slot"] = w
        if "bar" in case:
            e.write(h + 0xD60, struct.pack("<2iB", 90, 37, 1))
        if case.startswith("range"):
            cfg["range"] = 1
            if case != "range" and case != "range_bar":
                task = e.alloc(0x80)
                model = e.alloc(0x40)
                e.write(model + 0x14, b"tank_body\0")
                e.write(task + 0x1C, struct.pack("<H", 0x44))
                e.write(task + 0x6C, u32(model))
                cfg["isa"] = 1 if case == "range_model" else 0
                cfg["coll"] = u32(task) + bytes(0x54) + u32(1) + u32(0) + struct.pack("<d", 123456.0)
        if case in ("effects", "flash_low", "flash_high", "flash_nan"):
            e.write(h + 0xDCC, struct.pack("<4f", 0.0, 0.125, 0.25, 0.375))
            e.write(h + 0xDDC, struct.pack("<f", {"flash_low": -0.5, "flash_high": 1.5,
                                                   "flash_nan": float("nan")}.get(case, 0.5)))
        return h, 0xA59
    run(0x413120, "HumanPlayer_RenderHandler", args=args)


# --- the rest -----------------------------------------------------------------------

def test_shape_query():
    def args(h):
        q = e.alloc(0x24)
        e.write(q, b"\x11" * 0x24)
        return h, q
    run(0x413730, "sub_413730", args=args)


def test_close():
    def args(h):
        e.write(0x56E210, u32(0x1111))
        e.write(0x56E09C, u32(0x2222))
        e.write(0x5385B0, struct.pack("<H", 0x123))
        return ()
    run(0x413770, "HumanPlayer_Close", args=args)


def test_load_parameters():
    run(0x4137A0, "HumanPlayer_LoadParameters", args=lambda h: ())
    for fn in (0x4137A0, "HumanPlayer_LoadParameters"):
        e.reset()
        make(e, {"script": 0})
        with pytest.raises(ErrorShown, match="LoadParameters: Couldn't load script \"%s\""):
            e.call(fn)
        assert e.read_cstr(e.arg(1)) == b"LOCAL:humanplayer/humanplayer.qsc"


@pytest.mark.parametrize("anim", list(range(12)) + [-1])
@pytest.mark.parametrize("melee", [0, 1, 2, 3])
def test_set_weapon_anim(anim, melee):
    def args(h):
        e.write(0x5391B1, b"\x08")
        e.write(HANDLERS + 4 * (8 * 0x180 + 0x30), u32(GETTYPE))
        w = e.alloc(0x24)
        e.write(w + 0x1C, struct.pack("<H", 0x30))
        e.write(w + 0x20, u32(TYPES))
        e.write(TYPES + 0x67C, struct.pack("<13i", 100, 101, 102, 103, 104, -1, 200, 201, 202, -1, 204, 205, 206))
        cfg["slot"] = w
        e.write(h + 0x4E8, u32(melee))
        return h, anim
    run(0x4137E0, "HumanPlayer_SetWeaponAnim", args=args)


def test_set_weapon_anim_noslot():
    run(0x4137E0, "HumanPlayer_SetWeaponAnim", args=lambda h: (h, 1))


def test_set_weapon_anim_time():
    def args(h):
        e.write(0x5391B1, b"\x08")
        e.write(HANDLERS + 4 * (8 * 0x180 + 0x30), u32(GETTYPE))
        w = e.alloc(0x24)
        e.write(w + 0x1C, struct.pack("<H", 0x30))
        e.write(w + 0x20, u32(TYPES))
        cfg.update(slot=w, rand=[0.0])
        return h, 6
    run(0x4137E0, "HumanPlayer_SetWeaponAnim", args=args)


def test_small():
    run(0x413A60, "HumanPlayer_RequestWeaponAnim", args=lambda h: (h, 77))
    e.write(0x5385B0, struct.pack("<H", 0x1F3))
    compare(e, 0x413A70, "HumanPlayer_GetType", setup=lambda e: e.write(0x5385B0, struct.pack("<H", 0x1F3)),
            eax_mask=0xFFFF)
    compare(e, 0x413A80, "HumanPlayer_GetMoveScale", setup=lambda e: e.write(0x56E1E8, struct.pack("<f", 1.75)),
            check_st0=True)
