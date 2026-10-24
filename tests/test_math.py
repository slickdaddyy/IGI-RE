import math
import random
import struct

import pytest

from unicorn.x86_const import UC_X86_REG_FP0, UC_X86_REG_FPSW

from emu import TRAP_BASE, Emu, compare


@pytest.fixture(autouse=True, params=[0x37F, 0x7F, 0xC7F, 0x77F], ids=hex)
def cw(request):
    """Every test at the emulator default and at the control words the game uses."""
    Emu.cw = request.param
    yield
    Emu.cw = None


class FpuEmu(Emu):
    """Emu.call reports physical register R0 as st0; report (TOP, st(0)) instead."""

    def call(self, *args, **kw):
        eax, edx, _ = super().call(*args, **kw)
        top = (self.uc.reg_read(UC_X86_REG_FPSW) >> 11) & 7
        return eax, edx, (top, self.uc.reg_read(UC_X86_REG_FP0 + top))


emu = FpuEmu()
rng = random.Random(7)

# fninit; ret -- every test starts from an empty x87 stack
FNINIT = emu.alloc(4)
emu.write(FNINIT, b"\xdb\xe3\xc3")

# the draw functions hand a stack buffer to a driver callback; record what it receives
DRAW_OUT = emu.alloc(0x100)


def on_draw(e):
    cmd = e.arg(0)
    count = e.u32(cmd + 12)
    e.write(DRAW_OUT, e.read(cmd, 16) + e.read(e.u32(cmd + 16), count * 0x24))
    e.ret(0)


DRAW_CALLBACK = TRAP_BASE + 4 * len(emu.imports)
emu.imports.append(("test", "DrawCallback"))
emu.write(DRAW_CALLBACK, b"\xc3")
emu.stub("DrawCallback", on_draw)
emu.baseline = emu.snapshot()


def f32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def d64(x):
    return struct.unpack("<II", struct.pack("<d", x))


def put(e, fmt, vals):
    data = struct.pack("<" + fmt, *vals)
    p = e.alloc(len(data))
    e.write(p, data)
    return p


def rnd(n, lo=-3.0, hi=3.0):
    return [rng.uniform(lo, hi) for _ in range(n)]


def run(orig, ours, make_args, **kw):
    def setup(e):
        e.call(FNINIT)
        return make_args(e)
    return compare(emu, orig, ours, setup=setup, **kw)


# --- Vec3d ------------------------------------------------------------------

VECS = [rnd(3) for _ in range(6)] + [[1e-3, 2e5, -7.5], [0.0, 0.0, 4.0]]


@pytest.mark.parametrize("v", VECS)
def test_vec3d_set_length(v):
    n = rng.uniform(-5, 5)
    run(0x4B30C0, "Vec3d_SetLength", lambda e: (put(e, "3d", v),) + d64(n), check_eax=False)


@pytest.mark.parametrize("v", VECS)
def test_vec3d_normalize(v):
    run(0x4B3100, "Vec3d_Normalize", lambda e: (put(e, "3d", v),), check_eax=False)


@pytest.mark.parametrize("orig,ours", [(0x4B3130, "Vec3d_Length"), (0x4B3160, "Vec3d_LengthSq")])
@pytest.mark.parametrize("v", VECS)
def test_vec3d_length(orig, ours, v):
    run(orig, ours, lambda e: (put(e, "3d", v),), check_eax=False, check_st0=True)


@pytest.mark.parametrize("alias", [False, True])
def test_vec3d_cross(alias):
    for _ in range(5):
        a, b = rnd(3), rnd(3)

        def args(e):
            pa, pb = put(e, "3d", a), put(e, "3d", b)
            return (pa if alias else e.alloc(24)), pa, pb
        run(0x4B3190, "Vec3d_Cross", args, check_eax=False)


ANGLE_CASES = [
    ([0, 0, 0], [1, 2, 3]),
    ([1, 2, 3], [0, 0, 0]),
    ([1, 2, 3], [2, 4, 6]),
    ([1, 2, 3], [-2, -4, -6]),
    ([1, 0, 0], [0, 1, 0]),
] + [(rnd(3), rnd(3)) for _ in range(6)]


@pytest.mark.parametrize("a,b", ANGLE_CASES)
def test_vec3d_angle(a, b):
    run(0x4B31F0, "Vec3d_Angle", lambda e: (put(e, "3d", a), put(e, "3d", b)), check_eax=False, check_st0=True)


@pytest.mark.parametrize("orig,ours", [(0x4B32B0, "Vec3d_Distance"), (0x4B32F0, "Vec3d_DistanceSq")])
def test_vec3d_distance(orig, ours):
    for _ in range(6):
        a, b = rnd(3, -100, 100), rnd(3, -100, 100)
        run(orig, ours, lambda e: (put(e, "3d", a), put(e, "3d", b)), check_eax=False, check_st0=True)


@pytest.mark.parametrize("v", [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1], [-0.0, 0, 0]])
def test_vec3d_is_zero(v):
    run(0x4B3320, "Vec3d_IsZero", lambda e: (put(e, "3d", v),))


@pytest.mark.parametrize("b", [[1, 2, 3], [9, 2, 3], [1, 9, 3], [1, 2, 9]])
def test_vec3d_equal(b):
    run(0x4B3360, "Vec3d_Equal", lambda e: (put(e, "3d", [1, 2, 3]), put(e, "3d", b)))


# --- Matrix -----------------------------------------------------------------

def rnd_matrix():
    return rnd(9, -2, 2) + [rng.randint(0, 100)]


def put_matrix(e, m):
    return put(e, "9fi", m)


GARBAGE = rnd_matrix()


@pytest.mark.parametrize("alias", [False, True])
def test_matrix_rotate_euler(alias):
    for _ in range(4):
        m, ang = rnd_matrix(), rnd(3, -7, 7)

        def args(e):
            pm = put_matrix(e, m)
            return ((pm if alias else e.alloc(40)), pm) + tuple(f32(a) for a in ang)
        run(0x4B33A0, "Matrix_RotateEuler", args, check_eax=False)


@pytest.mark.parametrize("orig,ours", [(0x4B34F0, "Matrix_RotateX"), (0x4B3640, "Matrix_RotateY"),
                                       (0x4B3790, "Matrix_RotateZ")])
@pytest.mark.parametrize("alias", [False, True])
def test_matrix_rotate(orig, ours, alias):
    for _ in range(4):
        m, a = rnd_matrix(), rng.uniform(-7, 7)

        def args(e):
            pm = put_matrix(e, m)
            return (pm if alias else e.alloc(40)), pm, f32(a)
        run(orig, ours, args, check_eax=False)


def test_matrix_set_euler():
    for _ in range(6):
        ang = rnd(3, -7, 7)
        run(0x4B38E0, "Matrix_SetEuler", lambda e: (put_matrix(e, GARBAGE),) + tuple(f32(a) for a in ang),
            check_eax=False)


@pytest.mark.parametrize("orig,ours", [(0x4B3B60, "Matrix_SetRotX"), (0x4B3BA0, "Matrix_SetRotY"),
                                       (0x4B3BE0, "Matrix_SetRotZ")])
@pytest.mark.parametrize("a", [0.0, 0.3, -1.2, 3.14159, 100.0])
def test_matrix_set_rot(orig, ours, a):
    run(orig, ours, lambda e: (put_matrix(e, GARBAGE), f32(a)), check_eax=False)


def test_matrix_set_scale():
    run(0x4B3C20, "Matrix_SetScale", lambda e: (put_matrix(e, GARBAGE), f32(2.5), f32(-1.0), f32(0.125)),
        check_eax=False)


def euler_matrix(e, ang):
    p = e.alloc(40)
    e.call(0x4B38E0, p, *[f32(a) for a in ang])
    return p


GET_EULER_CASES = [
    # cy small, x from atan2(-m5, m4), y with cy == 0
    [0, 0.5, 0.7, 0, 0.2, -0.4, -1, 0, 0, 0],
    [0, 0.5, 0.7, 0, 0.0, -0.4, 1, 0, 0, 0],
    [0, 0.5, 0.7, 0, 0.0, 0.4, 0.5, 0, 0, 0],
    # cy small but not zero
    [1e-5, 0.5, 0.7, 1e-5, 0.2, 0.4, -1, 0, 0, 0],
    # atan2 with x == 0 in each slot
    [0.5, 0, 0, 0.5, 0, 0, 0.3, 0.7, 0.0, 0],
    [0.5, 0, 0, 0.5, 0, 0, 0.3, -0.7, 0.0, 0],
    [0.0, 0, 0, 0.5, 0, 0, 0.3, 0.7, 0.2, 0],
    [0.0, 0, 0, -0.5, 0, 0, 0.3, 0.7, 0.2, 0],
]


@pytest.mark.parametrize("m", GET_EULER_CASES)
def test_matrix_get_euler_cases(m):
    run(0x4B3C50, "Matrix_GetEuler", lambda e: (put_matrix(e, m), e.alloc(4), e.alloc(4), e.alloc(4)),
        check_eax=False)


def test_matrix_get_euler_random():
    for _ in range(12):
        ang = rnd(3, -7, 7)
        run(0x4B3C50, "Matrix_GetEuler", lambda e: (euler_matrix(e, ang), e.alloc(4), e.alloc(4), e.alloc(4)),
            check_eax=False)


def test_matrix_set_axis_angle():
    for _ in range(8):
        axis, a = rnd(3, -1, 1), rng.uniform(-7, 7)
        run(0x4B3EF0, "Matrix_SetAxisAngle", lambda e: (put_matrix(e, GARBAGE), put(e, "3f", axis), f32(a)),
            check_eax=False)


@pytest.mark.parametrize("d", [[0, 0, 1], [0, 0, -2], [0, 1, 0.5], [1, 0, 0], [0.3, -0.2, 0.9]] +
                         [rnd(3) for _ in range(6)])
def test_matrix_set_direction(d):
    run(0x4B4010, "Matrix_SetDirection", lambda e: (put_matrix(e, GARBAGE), put(e, "3f", d)),
        check_eax=False)


@pytest.mark.parametrize("alias", [False, True])
def test_matrix_invert(alias):
    for _ in range(6):
        m = rnd_matrix()

        def args(e):
            pm = put_matrix(e, m)
            return (pm if alias else e.alloc(40)), pm
        run(0x4B4390, "Matrix_Invert", args, check_eax=False)


# --- Quat -------------------------------------------------------------------

def unit_quat():
    q = rnd(4, -1, 1)
    n = math.sqrt(sum(x * x for x in q))
    return [x / n for x in q]


SLERP_CASES = [
    ([0, 0, 0, 1], [0, 0, 0, 1]),
    ([0, 0, 0, 1], [0, 0, 0, -1]),
    ([1, 0, 0, 0], [0, 1, 0, 0]),
    ([0.6, 0, 0, 0.8], [0.8, 0, 0, -0.6 + 1e-9]),
    ([1e-5, 0, 0, 0], [-1e-4, 0, 0, 0]),
] + [(unit_quat(), unit_quat()) for _ in range(8)]


@pytest.mark.parametrize("a,b", SLERP_CASES)
def test_quat_slerp_prepare(a, b):
    def args(e):
        out = e.alloc(12)
        e.write(out, b"\x55" * 12)
        return out, out + 4, out + 8, put(e, "4fi", a + [0]), put(e, "4fi", b + [0])
    run(0x4B44D0, "Quat_SlerpPrepare", args, check_eax=False)


def test_quat_dot():
    for _ in range(6):
        a, b = rnd(4), rnd(4)
        run(0x4B4570, "Quat_Dot", lambda e: (put(e, "4f", a), put(e, "4f", b)), check_eax=False, check_st0=True)


def test_quat_set_euler():
    for _ in range(8):
        ang = rnd(3, -7, 7)
        run(0x4B45A0, "Quat_SetEuler", lambda e: (put(e, "5I", [0x55555555] * 5),) + tuple(f32(a) for a in ang),
            check_eax=False)


def test_quat_set_axis_angle():
    for _ in range(8):
        axis, a = rnd(3, -1, 1), rng.uniform(-7, 7)
        run(0x4B4660, "Quat_SetAxisAngle", lambda e: (put(e, "5I", [0x55555555] * 5), put(e, "3f", axis), f32(a)),
            check_eax=False)


# --- Tables and random --------------------------------------------------------

def test_math_init_tables():
    run(0x4B46A0, "Math_InitTables", lambda e: (), check_eax=False)


STATE = [0x12345678, 0x9ABCDEF0, 0x0F1E2D3C]


def test_random_init():
    run(0x4B4720, "Random_Init", lambda e: (put(e, "3I", STATE),), check_eax=False)


@pytest.mark.parametrize("state", [STATE, [0xC8FB1A07, 0x39EA7756, 0x7FF13CCD], [0, 0, 0xFFFFFFFF]])
def test_random_next(state):
    run(0x4B4740, "Random_Next", lambda e: (put(e, "3I", state),))
    run(0x4B4770, "Random_Float", lambda e: (put(e, "3I", state),), check_eax=False, check_st0=True)


@pytest.mark.parametrize("n", [1, 7, 1000, 0x80000001])
def test_random_int(n):
    run(0x4B47C0, "Random_Int", lambda e: (put(e, "3I", STATE), n))


@pytest.mark.parametrize("seed", [0, 1, 12345, 0xFFFFFFFF])
def test_random_seed(seed):
    run(0x4B47E0, "Random_Seed", lambda e: (put(e, "3I", STATE), seed), check_eax=False)


# --- Draw -------------------------------------------------------------------

def draw_setup(e):
    e.write(0xA84A54, struct.pack("<I", 3))
    e.write(0xA94E84 + 12, struct.pack("<I", DRAW_CALLBACK))
    e.write(0xA84A50, struct.pack("<I", 0x11111111))
    e.write(0xA84A58, struct.pack("<I", 0x33333333))


RECT = [f32(v) for v in (10.5, 20.25, 100.0, 50.75, 255.0, 128.0, 3.0)]


@pytest.mark.parametrize("orig,ours,extra", [
    (0x4B4820, "Draw_Rect", (5, 6)),
    (0x4B4910, "Draw_RectZ", (5, 6, f32(0.25))),
    (0x4B4A00, "Draw_RectA", (f32(0.5), 5, 6)),
    (0x4B4AF0, "Draw_RectAZ", (f32(0.5), f32(0.25), 5, 6)),
    (0x4B4BE0, "Draw_RectGradient", tuple(f32(v) for v in rnd(9, 0, 255)) +
     tuple(f32(v) for v in (0.1, 0.2, 0.3, 0.4, 0.9)) + (5, 6)),
])
def test_draw_rect(orig, ours, extra):
    def args(e):
        draw_setup(e)
        return tuple(RECT) + extra
    run(orig, ours, args, check_eax=False)


def sprite_sheet(e):
    frames = b"".join(struct.pack("<ii4hi4f", 0x100 + i, 0, -3 + i, 7 * i, 16 + i, 9 - i, 0, 0.1 * i, 0.2, 0.3, 0.4 + i)
                      for i in range(4))
    pf = put(e, "%ds" % len(frames), [frames])
    return put(e, "iiI", [0x4444, 0, pf])


@pytest.mark.parametrize("frame", [0, 3])
@pytest.mark.parametrize("orig,ours,extra", [(0x4B4E00, "Draw_Sprite", ()), (0x4B4F40, "Draw_SpriteZ", (f32(0.75),))])
def test_draw_sprite(orig, ours, extra, frame):
    def args(e):
        draw_setup(e)
        return (sprite_sheet(e), f32(12.5), f32(-4.25), f32(255), f32(64), f32(0), frame, 5, 6) + extra
    run(orig, ours, args, check_eax=False)
