import pytest

from emu import Emu
from test_plane_util import (CHILD, EV_MOVE, LOG, PLANE, RET, add_child, cstr, d64, f32, handler, init_emu,  # noqa: F401
                             matrix, msg_handler, plane_emu, run, setup_init, task, u16, u32)


@pytest.fixture(autouse=True, params=[0x37F, 0x7F, 0xC7F, 0x77F], ids=hex)
def cw(request):
    """Every test at the emulator default and at the control words the game uses."""
    Emu.cw = request.param
    yield
    Emu.cw = None


PART = 0x74
emu = plane_emu()
emu.baseline = emu.snapshot()

ROT = (0.6123457, -0.7905694, 0.0123457, 0.7904321, 0.6111111, -0.0371234, 0.0217391, 0.0333333, 0.9993827)
ROT2 = (0.3617284, 0.4812345, -0.7984567, -0.8012346, 0.5987654, 0.0123457, 0.4790123, 0.6308642, 0.6012345)
ROT3 = (0.1111111, 0.2345679, 0.3141593, -0.4142136, 0.5772157, 0.6180340, 0.7071068, -0.8660254, 0.9876543)


def owner(e, cockpit=0, gear=0, gear_state=0, hatch_state=0, turn=(0.3, -0.45, 0.6)):
    p = task(e, 0x888, PLANE)
    d64(e, p + 0x20, 1000.5, -2000.25, 300.0)
    matrix(e, p + 0x70, ROT2, 7)
    ph = e.alloc(0x2D8)
    cfg = e.alloc(0x118)
    f32(e, cfg + 4, 0.35)
    u32(e, ph + 0x2D4, cfg)
    u32(e, p + 0x1B4, ph)
    f32(e, p + 0x724, *turn)
    e.write(p + 0x82C, bytes([cockpit, gear, 0, 0, 0, 0, gear_state & 0xFF, hatch_state & 0xFF]))
    return p


def part(e, p, d, angle=0.25):
    o = task(e, 0x140, PART)
    u32(e, o + 0x6C, 0x6600)
    matrix(e, o + 0x70, ROT, 1)
    f32(e, o + 0x98, 1.5, 2.5, 0.5)
    d64(e, o + 0xF0, 120.5, -33.25, 7.125)
    matrix(e, o + 0x108, ROT3, 2)
    u32(e, o + 0x130, p, 0, d)
    f32(e, o + 0x13C, angle)
    return o


def gear_def(e, angle=95.0, roll=1):
    d = e.alloc(0x14)
    f32(e, d, angle, 0.0, 1.0, 2.0, 3.0)
    e.write(d + 4, bytes([roll]))
    return d


def rudder_def(e, kind, angle=40.0):
    d = e.alloc(8)
    u32(e, d, kind)
    f32(e, d + 4, angle)
    return d


def angle_def(e, angle=70.0):
    d = e.alloc(4)
    f32(e, d, angle)
    return d


def create_arg(e, p, d):
    a = e.alloc(0x90)
    u32(e, a, cstr(e, b"canopy"), 0x5000)
    d64(e, a + 8, 1.0, 2.0, 3.0, 4.5, -5.5, 6.5)
    matrix(e, a + 0x38, ROT, 4)
    matrix(e, a + 0x60, ROT3, 6)
    u32(e, a + 0x88, p, d)
    return a


def move_arg(e, p):
    a = e.alloc(12)
    pos = e.alloc(24)
    d64(e, pos, 1e5 + 0.1, -2e4 - 0.3, 512.75)
    m = e.alloc(0x28)
    matrix(e, m, ROT2, 9)
    u32(e, a, pos, m, p)
    return a


def init_setup(e):
    setup_init(e)


@pytest.mark.parametrize("addr,name", [(0x4351E0, "CockpitShield_Init"), (0x435870, "Gear_Init"),
                                       (0x435D90, "Hatch_Init"), (0x4361D0, "Rudder_Init")])
def test_init(addr, name):
    run(init_emu(), addr, name, init_setup)


@pytest.mark.parametrize("addr,name,type_", [(0x435450, "CockpitShield_Close", 0x57BD5C),
                                             (0x435C30, "Gear_Close", 0x57BD64),
                                             (0x4360B0, "Hatch_Close", 0x57BD6C)])
def test_close(addr, name, type_):
    run(emu, addr, name, lambda e: u32(e, type_, 0x12340055))


def test_gear_get_type():
    def setup(e):
        u32(e, 0x57BD64, 0x56780042)
    run(emu, 0x435C40, "Gear_GetType", setup, check_eax=True, eax_mask=0xFFFF)


CREATE = [(0x434E10, "CockpitShield_Create", angle_def), (0x435460, "Gear_Create", gear_def),
          (0x435C50, "Hatch_Create", angle_def), (0x4360C0, "Rudder_Create", lambda e: rudder_def(e, 1))]


@pytest.mark.parametrize("addr,name,mkdef", CREATE)
@pytest.mark.parametrize("flags", [0, 1])
def test_create(addr, name, mkdef, flags):
    def setup(e):
        RET[0x4CEC10] = 0x6600
        RET[0x4D0950] = 50.5
        p = owner(e, cockpit=flags, gear=flags)
        o = part(e, p, 0)
        return o, create_arg(e, p, mkdef(e))
    run(emu, addr, name, setup)


@pytest.mark.parametrize("addr,name,mkdef", CREATE)
def test_create_no_model(addr, name, mkdef):
    from emu import ErrorShown

    def setup(e):
        RET[0x4CEC10] = 0
        p = owner(e)
        return part(e, p, 0), create_arg(e, p, mkdef(e))
    with pytest.raises(ErrorShown):
        run(emu, addr, name, setup)


@pytest.mark.parametrize("angle", [0.0, 0.25, -1.5, 3.0])
@pytest.mark.parametrize("roll,gear", [(0, 0), (1, 0), (1, 1), (0, 1)])
@pytest.mark.parametrize("addr,name", [(0x434F50, "CockpitShield_SetMatrix"), (0x4355A0, "Gear_SetMatrix")])
def test_set_matrix(angle, roll, gear, addr, name):
    def setup(e):
        p = owner(e, gear=gear)
        o = part(e, p, gear_def(e, roll=roll), angle=angle)
        m = e.alloc(0x28)
        matrix(e, m, ROT2, 11)
        return o, m
    run(emu, addr, name, setup)


@pytest.mark.parametrize("cockpit", [0, 1])
@pytest.mark.parametrize("angle", [0.0, 1.2217305, 0.5, -0.1])
def test_cockpit_run(cockpit, angle):
    run(emu, 0x4352A0, "CockpitShield_Run", lambda e: (part(e, owner(e, cockpit=cockpit), angle_def(e), angle),))


STATES = [0, 1, 2, -1]


@pytest.mark.parametrize("gear", [0, 1])
@pytest.mark.parametrize("hatch_state", [0, 1, -1])
@pytest.mark.parametrize("angle", [0.0, 0.05, -0.05, 1.658, 1.6, 1.5, 0.8, 1.66, 1.7, -1.7, 1.65806])
def test_gear_run(gear, hatch_state, angle):
    def setup(e):
        p = owner(e, gear=gear, hatch_state=hatch_state, gear_state=7)
        return (part(e, p, gear_def(e, 95.0), angle),)
    run(emu, 0x435930, "Gear_Run", setup)


@pytest.mark.parametrize("gear", [0, 1])
@pytest.mark.parametrize("gear_state", [0, 1, -1])
@pytest.mark.parametrize("angle", [0.0, 0.05, 0.11, 1.2, 1.22, 1.3, -0.3, 1.32])
def test_hatch_run(gear, gear_state, angle):
    def setup(e):
        p = owner(e, gear=gear, gear_state=gear_state, hatch_state=7)
        return (part(e, p, angle_def(e, 70.0), angle),)
    run(emu, 0x435E50, "Hatch_Run", setup)


@pytest.mark.parametrize("addr,name,mkdef", [(0x435300, "CockpitShield_Move", angle_def),
                                             (0x435A80, "Gear_Move", gear_def),
                                             (0x435F60, "Hatch_Move", angle_def)])
@pytest.mark.parametrize("scale", [(1.5, 2.5, 0.5), (3.0, 2.0, 1.0)])
def test_move(addr, name, mkdef, scale):
    def setup(e):
        RET[0x4D0950] = 33.25
        p = owner(e, gear=1)
        o = part(e, p, mkdef(e), 0.75)
        f32(e, o + 0x98, *scale)
        return o, move_arg(e, p)
    run(emu, addr, name, setup)


@pytest.mark.parametrize("kind", [0, 1, 2, 3, 4, -1])
@pytest.mark.parametrize("turn", [(0.3, -0.45, 0.6), (-0.9, 0.95, -1.0), (0.0, 0.0, 0.0), (0.5, 0.5, 0.5)])
@pytest.mark.parametrize("angle", [40.0, 10.0])
def test_rudder_move(kind, turn, angle):
    def setup(e):
        RET[0x4D0950] = 12.5
        p = owner(e, turn=turn)
        return part(e, p, rudder_def(e, kind, angle)), move_arg(e, p)
    run(emu, 0x4362C0, "Rudder_Move", setup)


def parse_arg(e):
    a = e.alloc(12)
    out = e.alloc(0x20)
    e.write(out, b"\xee" * 0x20)
    u32(e, a, out, 0x7777, 5)
    return a


def arg_real(e, a):
    return a[1] * 1.5 - 2.0


@pytest.mark.parametrize("addr,name", [(0x435BD0, "Gear_ParseDef"), (0x436090, "Hatch_ParseDef")])
def test_parse_def(addr, name):
    from test_plane_util import floatfake, logfake
    e = plane_emu()
    floatfake(e, 0x4B8A50, 2, effect=arg_real)
    logfake(e, 0x4B8A20, 2, effect=lambda e, a: 0x100 + a[1])
    run(e, addr, name, lambda e: (0, parse_arg(e)))


def rand_matrix(rng):
    return tuple(rng.uniform(-3.0, 3.0) * 10 ** rng.randint(-3, 3) for _ in range(9))


@pytest.mark.parametrize("seed", range(40))
@pytest.mark.parametrize("addr,name", [(0x434F50, "CockpitShield_SetMatrix"), (0x4355A0, "Gear_SetMatrix"),
                                       (0x4362C0, "Rudder_Move")])
def test_matrix_random(seed, addr, name):
    import random
    rng = random.Random(seed)
    local, parent = rand_matrix(rng), rand_matrix(rng)
    angle, turn = rng.uniform(-2, 2), (rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))
    kind = rng.randint(0, 3)

    def setup(e):
        RET[0x4D0950] = 12.5
        p = owner(e, gear=1, turn=turn)
        d = rudder_def(e, kind, 33.0) if name == "Rudder_Move" else gear_def(e, roll=1)
        o = part(e, p, d, angle)
        matrix(e, o + 0x108, local, 2)
        a = move_arg(e, p)
        matrix(e, e.u32(a + 4), parent, 9)
        return (o, a) if name == "Rudder_Move" else (o, e.u32(a + 4))
    run(emu, addr, name, setup)
