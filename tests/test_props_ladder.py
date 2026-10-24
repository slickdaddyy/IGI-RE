import pytest

from emu import ErrorShown, compare
from test_props_util import (RET, SCALES, d64, f32, init_emu, logfake, make_emu, mat, rigid, run, setup_init, task,
                             u32, vfake)

LADDER = 0x59
NAN = float("nan")
IDENT = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]


def model_point(e, a):
    d64(e, a[0], *RET["p%d" % a[2]])


def model_point_local(e, a):
    f32(e, a[0], *RET["l%d" % a[2]])


emu = make_emu()
vfake(emu, 0x414E20, 3, show=lambda e, a: (e.normalize(a[1]), a[2]), effect=model_point)
vfake(emu, 0x414ED0, 3, show=lambda e, a: (e.normalize(a[1]), a[2]), effect=model_point_local)
vfake(emu, 0x416AD0, 3, show=lambda e, a: (e.normalize(a[0]), e.normalize(a[1]), e.read(a[2], 24)),
      effect=lambda e, a: RET.get("use", 0))
logfake(emu, 0x4C4950, 3)
logfake(emu, 0x4871C0, 2)
logfake(emu, 0x491CF0, 0, ret=0x9100)  # Display_GetMode
emu.baseline = emu.snapshot()

init = init_emu()


def ladder(e, scale=(1.0, 2.0, 1.5), m=None):
    t = rigid(e, 0x140, LADDER, scale)
    mat(e, t + 0x70, m)
    d64(e, t + 0x20, 100.0, 200.0, 300.0)
    u32(e, t + 0x138, 0x1388)
    return t


def test_climb_time():
    run(emu, 0x440270, "Fence_GetClimbTime", lambda e: (), check_st0=True)


def create_arg(e):
    a = e.alloc(0x90)
    name = e.alloc(16)
    e.write(name, b"ladder_03\0")
    u32(e, a, name, 0x6868)
    d64(e, a + 8, 1.0, 2.0, 3.0)
    mat(e, a + 0x38, unk=12)
    u32(e, a + 0x88, 0x8888)
    return a


@pytest.mark.parametrize("ends", [(0.0, 12288.0), (5000.0, 100.0), (614.4, 0.0), (1843.2, 0.0), (NAN, 0.0)])
@pytest.mark.parametrize("scale", SCALES[:3])
def test_create(ends, scale):
    def setup(e):
        RET[0x4CEC10] = 0x3300
        RET[0x4D0950] = 2.0
        RET["l2"], RET["l3"] = (0.0, 0.0, ends[0]), (0.0, 0.0, ends[1])
        return ladder(e, scale=scale), create_arg(e)
    run(emu, 0x440280, "Ladder_Create", setup)


def test_create_no_model():
    def setup(e):
        RET[0x4CEC10] = 0
        return ladder(e), create_arg(e)
    with pytest.raises(ErrorShown):
        run(emu, 0x440280, "Ladder_Create", setup)


def test_delete():
    run(emu, 0x4403C0, "Ladder_Delete", lambda e: (ladder(e),))


def test_open():
    compare(init, 0x4403E0, "Ladder_Open", setup=setup_init, check_eax=False, normalized=True)


# human offsets from the ladder in ladder space (identity matrix)
USE = [(0.0, -500.0, 100.0), (0.0, 500.0, 100.0), (0.0, -1600.0, 100.0), (-2900.0, -500.0, 100.0),
       (2900.0, -500.0, 100.0), (0.0, -500.0, -10.0), (0.0, -500.0, 5000.0), (0.0, -500.0, 3900.0),
       (0.0, 100.0, 4100.0), (NAN, -500.0, 100.0), (0.0, NAN, 100.0)]
FACING = [(0.0, 1.0, 0.0), (0.0, -1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.8, 0.6)]


@pytest.mark.parametrize("d", USE)
@pytest.mark.parametrize("face", FACING)
@pytest.mark.parametrize("use", [0, 1])
def test_use(d, face, use):
    def setup(e):
        RET["p1"] = (10.0, -20.0, 4000.0)
        RET["use"] = use
        human = task(e, 0xE00, 0x60)
        e.write(human + 0xD7B, b"\x07")
        q = e.alloc(0x88)
        u32(e, q, human)
        d64(e, q + 8, 100.0 + d[0], 200.0 + d[1], 300.0 + d[2])
        f32(e, q + 0x20, 1.0, face[0], 0.0, 0.0, face[1], 0.0, 0.0, face[2], 1.0)
        f32(e, q + 0x70, 9.0, 9.0)
        d64(e, q + 0x78, 1500.0)
        u32(e, q + 0x80, 0x8080)
        return ladder(e, m=IDENT), q
    run(emu, 0x4404C0, "Ladder_Use", setup)


@pytest.mark.parametrize("seed", range(6))
def test_use_random(seed):
    import random

    def setup(e):
        rng = random.Random(seed)
        RET["p1"] = (rng.uniform(-50, 50), rng.uniform(-50, 50), 4000.0)
        RET["use"] = 1
        human = task(e, 0xE00, 0x60)
        q = e.alloc(0x88)
        u32(e, q, human)
        d64(e, q + 8, *[c + rng.uniform(-1500, 1500) for c in (100.0, 200.0, 300.0)])
        mat(e, q + 0x20, seed=seed)
        d64(e, q + 0x78, 2500.0)
        return ladder(e), q
    run(emu, 0x4404C0, "Ladder_Use", setup)


def test_get_use_kind():
    run(emu, 0x4406C0, "Ladder_GetUseKind", lambda e: (ladder(e), e.alloc(4)))


def test_draw_icon():
    def setup(e):
        p = e.alloc(4)
        u32(e, p, 0x7100)
        return ladder(e), p
    run(emu, 0x4406D0, "Ladder_DrawIcon", setup)


def test_close():
    def setup(e):
        u32(e, 0x57C15C, 0x12340044)
    run(emu, 0x440700, "Ladder_Close", setup)
