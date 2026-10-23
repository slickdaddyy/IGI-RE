import math
import random
import struct

import pytest

from emu import Emu
from test_physobj_util import CWS, Fakes, check, d64, f32, fake_sprintf, u32

e = Emu()
fk = Fakes(e)
ARGS = {}
NAME = e.alloc(80)
e.write(NAME, b"guard_17_hut\0")


def cursor_fmt(em, a):
    return (em.read(a[0], 16), a[1])


fk.add(0x4018F0, 0, ret=0x10)                                   # Task_GetRootType
fk.add(0x401900, 5, ret=lambda em, a: 0xABCD0000 + len(em.read_cstr(a[0])),
       fmt=lambda em, a: (em.read_cstr(a[0]), a[1], a[2] & 0xFFFF, a[3], a[4]))  # TaskType_New
fk.add(0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})                 # TaskType_SetHandler
fk.add(0x401530, 3, masks={0: 0xFFFF})                          # TaskType_SetHandler2
fk.add(0x4017C0, 1, ret=0x1234561A, masks={0: 0xFFFF})          # TaskEvent_Alloc
fk.add(0x401810, 1, ret=0x7654321B, masks={0: 0xFFFF})          # TaskEvent_Alloc2
fk.add(0x4CEA10, 0, ret=0x21)                                   # World_GetEvent0
fk.add(0x4CEA20, 0, ret=0x22)                                   # World_GetEvent1
fk.add(0x4F19C0, 2, ret=lambda em, a: sum(em.read_cstr(a[0])) % 97,
       fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))            # ParamType_New
for i, addr in enumerate(range(0x4F1B60, 0x4F1BC0, 0x10)):
    fk.add(addr, 0, ret=0xF0 + i)                               # ParamOp_*
fk.add(0x4F1BC0, 3, masks={1: 0xFF})                            # ParamType_SetOp
fk.add(0x4B8890, 4)                                             # Script_RegisterFunction
fk.add(0x4F1180, 1, ret=NAME)                                   # TaskRef_GetName
fk.add(0x4C1780, 2)                                             # Debug_Nothing
fk.add(0x4F1C20, 2, fmt=cursor_fmt)                             # ParamCursor_Need
fk.add(0x4B8A20, 2, ret=lambda em, a: ARGS.get(a[1], 0))        # Script_GetArgInt
fk.add(0x4B8A50, 2, st0="d", ret=lambda em, a: ARGS.get(a[1], 0.0))  # Script_GetArgReal
fk.add(0x4B38E0, 4)                                             # Matrix_SetEuler
fk.add(0x4B45A0, 4)                                             # Quat_SetEuler
fake_sprintf(fk)
e.baseline = e.snapshot()


def test_level_open():
    def setup(em):
        em.write(0xA71890, b"\x55" * 0x4000)
        em.write(0xA70CA8, b"\x66" * (0xA7186C - 0xA70CA8))
        em.write(0xA758BD, b"\x77\x77\x77")
        u32(em, 0xA758C0, 1, 2)
    check(e, fk, 0x4EE970, "Level_Open", setup, check_eax=False)


def blob(n, seed=1):
    r = random.Random(seed)
    return bytes(r.randrange(256) for _ in range(n))


COPIES = [(0x4EF990, "ParamGraphEnvelope_Copy", 44), (0x4EFA90, "Param32_Copy", 4),
          (0x4EFB60, "ParamReal32x3_Copy", 12), (0x4EFC90, "ParamReal32x9_Copy", 40),
          (0x4EFEE0, "ParamQuat32_Copy", 20), (0x4EFF90, "ParamReal64_Copy", 8),
          (0x4F0060, "ParamReal64x3_Copy", 24)]
PASTES = [(0x4EF9B0, "ParamGraphEnvelope_Paste", 44), (0x4EFAA0, "Param32_Paste", 4),
          (0x4EFB80, "ParamReal32x3_Paste", 12), (0x4EFCB0, "ParamReal32x9_Paste", 40),
          (0x4EFF00, "ParamQuat32_Paste", 20), (0x4EFFB0, "ParamReal64_Paste", 8),
          (0x4F0080, "ParamReal64x3_Paste", 24)]


@pytest.mark.parametrize("addr,name,n", COPIES + PASTES)
def test_copy_paste(addr, name, n):
    def setup(em):
        field = em.alloc(64)
        em.write(field, blob(64, 1))
        buf = em.alloc(64)
        em.write(buf, blob(64, 2))
        ref = em.alloc(4)
        u32(em, ref, buf)
        return field, ref
    check(e, fk, addr, name, setup, check_eax=False)


@pytest.mark.parametrize("addr,name", [(0x4EF850, "ParamQTaskNote_Size"), (0x4EF8B0, "ParamGraphEnvelope_ArgCount"),
                                       (0x4EF9D0, "ParamGraphEnvelope_Size"), (0x4EFA10, "Param_One"),
                                       (0x4EFB00, "Param_Three"), (0x4EFCD0, "ParamReal32x9_Size"),
                                       (0x4EFF20, "ParamQuat32_Size"), (0x4F00A0, "ParamReal64x3_Size")])
def test_size(addr, name):
    def setup(em):
        out = em.alloc(4)
        u32(em, out, 0xDEADBEEF)
        return 0x1234, out
    check(e, fk, addr, name, setup, check_eax=False)


def test_qtasknote_copy():
    def setup(em):
        buf = em.alloc(0x41)
        em.write(buf, b"\xEE" * 0x41)
        ref = em.alloc(4)
        u32(em, ref, buf)
        return 0x20007770, ref
    check(e, fk, 0x4EF7F0, "ParamQTaskNote_Copy", setup, check_eax=False)


def test_qtasknote_paste():
    def setup(em):
        ref = em.alloc(4)
        u32(em, ref, 0x20005550)
        return 0x20007770, ref
    check(e, fk, 0x4EF830, "ParamQTaskNote_Paste", setup, check_eax=False)


REALS = [1.0, -2.5, 0.1, 1e-40, 3.4e38, 1e39, -0.0, 1.0 / 3.0, 123456.789, 2.0 ** -149]


def cursor(em, index):
    c = em.alloc(16)
    u32(em, c, index, 0x2000C000, 0x2000D000, 77)
    return c


PARSES = [(0x4EF860, "ParamGraphEnvelope_Parse", 44), (0x4EF9E0, "ParamReal32_Parse", 4),
          (0x4EFAB0, "ParamReal32x3_Parse", 12), (0x4EFBA0, "ParamReal32x9_Parse", 40),
          (0x4EFCE0, "ParamQuat32_Parse", 20), (0x4EFF30, "ParamReal64_Parse", 8),
          (0x4EFFD0, "ParamReal64x3_Parse", 24), (0x4F00B0, "ParamInt32_Parse", 4),
          (0x4F0110, "ParamInt16_Parse", 4)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("addr,name,n", PARSES)
@pytest.mark.parametrize("seed", [0, 1])
def test_parse(cw, addr, name, n, seed):
    def setup(em):
        ARGS.clear()
        r = random.Random(seed)
        for i in range(5, 30):
            ARGS[i] = r.choice(REALS) * r.choice([1, -1])
        if "Int" in name:
            ARGS.update({5: 0x12345678, 6: -3, 7: 0x8001})
        field = em.alloc(64)
        em.write(field, blob(64, 3))
        return field, cursor(em, 5 + seed * 2)
    check(e, fk, addr, name, setup, check_eax=False, cw=cw)


def text(em, prefix=b"x = "):
    buf = em.alloc(512)
    em.write(buf, prefix + b"\0" + b"\xAA" * 100)
    t = em.alloc(8)
    u32(em, t, 0x99, buf)
    return t


FLOATS = [1.0, -2.5, 0.1, 1e-40, 3.4e38, -0.0, 1.0 / 3.0, 123456.789, float("inf"), float("nan"), 0.0]


@pytest.mark.parametrize("prefix", [b"", b"x = "])
@pytest.mark.parametrize("v", FLOATS)
def test_real32_format(prefix, v):
    def setup(em):
        f = em.alloc(4)
        f32(em, f, v)
        return f, text(em, prefix)
    check(e, fk, 0x4EFA20, "ParamReal32_Format", setup, check_eax=False, stack_fill=0)


SMALL = [1.0, -2.5, 0.1, 1e-40, -0.0, 1.0 / 3.0, 123456.789, -999999.0, float("inf"), float("nan")]
FORMATS = [(0x4EF8C0, "ParamGraphEnvelope_Format", "f", 11), (0x4EFB10, "ParamReal32x3_Format", "f", 3),
           (0x4EFC10, "ParamReal32x9_Format", "f", 10), (0x4EFF60, "ParamReal64_Format", "d", 1),
           (0x4F0020, "ParamReal64x3_Format", "d", 3), (0x4F00E0, "ParamInt32_Format", "i", 1),
           (0x4F0140, "ParamInt16_Format", "h", 2)]


@pytest.mark.parametrize("addr,name,kind,n", FORMATS)
@pytest.mark.parametrize("seed", [0, 1, 2])
def test_format(addr, name, kind, n, seed):
    def setup(em):
        r = random.Random(seed)
        f = em.alloc(96)
        if kind == "f":
            # the envelope prints each number into a 20 byte buffer
            vals = SMALL if n == 11 else FLOATS
            f32(em, f, *[r.choice(vals) for _ in range(n)])
        elif kind == "d":
            d64(em, f, *[r.choice(FLOATS) * 1e3 for _ in range(n)])
        elif kind == "i":
            u32(em, f, r.choice([0, -1, 0x7FFFFFFF, -0x80000000, 42]))
        else:
            em.write(f, struct.pack("<hh", r.choice([0, -1, 0x7FFF, -0x8000, 42]), 0x5555))
        return f, text(em, b"v: " if seed else b"")
    check(e, fk, addr, name, setup, check_eax=False)


def quat_from_euler(x, y, z):
    cx, sx, cy, sy, cz, sz = (math.cos(x / 2), math.sin(x / 2), math.cos(y / 2), math.sin(y / 2), math.cos(z / 2),
                              math.sin(z / 2))
    return (sx * cy * cz - cx * sy * sz, cx * sy * cz + sx * cy * sz, cx * cy * sz - sx * sy * cz,
            cx * cy * cz + sx * sy * sz)


QUATS = [(0.0, 0.0, 0.0, 1.0), (0.0, math.sqrt(0.5), 0.0, math.sqrt(0.5)), (0.0, -math.sqrt(0.5), 0.0, math.sqrt(0.5)),
         (0.5, 0.5, 0.5, 0.5), (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 0.0), (0.1, 0.70710, 0.0, 0.70710),
         (2.0, -3.0, 0.5, 1.0)]
r_ = random.Random(5)
QUATS += [quat_from_euler(r_.uniform(-7, 7), r_.uniform(-7, 7), r_.uniform(-7, 7)) for _ in range(12)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("q", QUATS)
def test_quat32_format(cw, q):
    def setup(em):
        f = em.alloc(20)
        f32(em, f, *q)
        u32(em, f + 16, 0x77)
        return f, text(em)
    check(e, fk, 0x4EFD50, "ParamQuat32_Format", setup, check_eax=False, cw=cw)
