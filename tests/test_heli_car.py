import pytest

from emu import compare
from test_heli_util import RET, coll_task, d64, f32, handler, new_emu, run, set_handler, task, u16, u32, vfake

CAR, VEHICLE, CHILD = 0x60, 0x61, 0x70


def write_name(e, a):
    e.write(a[0], b"car_7\0")


emu = new_emu(
    fakes=[(0x4CEC10, 1, dict(strings=(0,))), (0x4C48D0, 2), (0x4EE140, 1, dict(strings=(0,))), (0x4FB700, 2),
           (0x4F4600, 1), (0x42EC30, 1), (0x464250, 2), (0x4896C0, 6), (0x416920, 0), (0x4C6940, 4),
           (0x4F4820, 1), (0x4F4800, 1), (0x4F1400, 2), (0x4F4780, 2), (0x4F46E0, 2), (0x4015F0, 1),
           (0x4B7EE0, 2, dict(strings=(0,))), (0x4B80A0, 1, dict(strings=(0,))), (0x4C1830, 1),
           (0x401A20, 1, dict(masks={0: 0xFFFF})), (0x401BE0, 2, dict(masks={1: 0xFFFF}))],
    floats=[(0x4D0950, 1), (0x416C90, 5)])
vfake(emu, 0x4F1270, 2, show=lambda e, a: (e.normalize(a[1]),), effect=write_name)
vfake(emu, 0x414E20, 3, effect=lambda e, a: d64(e, a[0], 1.5, -2.25, 1000.125))
e = emu
e.write(0x54E0E5, bytes([0x30]))  # Param_GetChangedEvent
e.write(0x548177, bytes([0x31]))  # World_GetEvent1
u16(e, 0x54E200, VEHICLE)
u16(e, 0x53BDA8, CAR)
u32(e, 0x57BD28, 0x12345678)
u16(e, 0x5BE388, 0x77)
set_handler(e, 0x30, CHILD, handler(e, "changed", 2))
set_handler(e, 0x31, VEHICLE, handler(e, "world1", 2))
set_handler(e, 1, CAR, handler(e, "run", 1))
e.baseline = e.snapshot()


def car(e, name=b"car_7", scale=(1.0, 2.0, 1.5), wheels=(), objects=0, children=2):
    c = task(e, 0x2ED8, CAR)
    e.write(c + 0x154, name + b"\0")
    f32(e, c + 0x98, *scale)
    u32(e, c + 0x6C, 0x4444)
    d64(e, c + 0x20, 100.0, 200.0, -50.5)
    d64(e, c + 0x168, 1.0, 2.0, 3.0)
    f32(e, c + 0x18C, *range(1, 10))
    u32(e, c + 0x18C + 36, 7)
    f32(e, c + 0x730, 0.4)
    f32(e, c + 0x740, 0.75)
    f32(e, c + 0x2EC0, 30.0, 45.5, 12.25)
    e.write(c + 0x91C, b"\1\1")
    e.write(c + 0x2ED5, b"\1")
    u32(e, c + 0xE8, coll_task(e, children, CHILD))
    data = e.alloc(0x2C0)
    u32(e, data + 0x118, len(wheels))
    for i, (steered, driven) in enumerate(wheels):
        e.write(data + 0x11C + 0x14 * i, bytes([steered, driven]))
    e.write(data + 0x260, bytes([objects]))
    phys = e.alloc(0x2D8)
    u32(e, phys + 0x2D4, data)
    u32(e, c + 0x1B4, phys)
    return c


@pytest.mark.parametrize("model,phys", [(1, 1), (1, 0), (0, 0)])
@pytest.mark.parametrize("name", [b"car_7", b"", b"x*", b"a"])
def test_params_changed(model, phys, name):
    def setup(e):
        RET.clear()
        RET[0x4CEC10] = 0x5000 if model else 0
        RET[0x4EE140] = 0x6000 if phys else 0
        RET[0x464250] = 0x99
        RET[0x416920] = 0x7777
        RET[0x4D0950] = 3.25
        return (car(e, name, wheels=((1, 0), (0, 1), (1, 1))),)
    run(e, 0x42F530, "Car_ParamsChanged", setup)


@pytest.mark.parametrize("objects", [0, 1])
@pytest.mark.parametrize("wheels", [(), ((1, 0),), ((1, 0), (0, 1), (1, 1), (0, 0))])
def test_init_wheels(objects, wheels):
    def setup(e):
        c = car(e, wheels=wheels, objects=objects)
        u32(e, c + 0x750, 9)
        for i in range(16):
            e.write(c + 0x75C + 0x1C * i, b"\x05")
        return (c,)
    run(e, 0x42F7C0, "Car_InitWheels", setup)


def test_get_sound_params():
    def setup(e):
        c = car(e)
        u32(e, c + 0x74C, 0x1234)
        out = e.alloc(12)
        u32(e, out, 7, 7, 7)
        return c, out
    run(e, 0x42F850, "Car_GetSoundParams", setup)


@pytest.mark.parametrize("i", [-1, 0, 2, 4, 5, 100])
def test_get_control_info(i):
    def setup(e):
        a = e.alloc(8)
        u32(e, a, i, 0x55)
        return 0, a
    run(e, 0x42F870, "Car_GetControlInfo", setup)


KEYS = [(), (0x1D, 0xCD), (0x9D, 0xCB), (0xCD,), (0xCB,), (0xCB, 0xCD), (0x29,), (0x29, 2), (3,), (4, 7), (0xB,),
        (0xA, 0x30), (0x31,), (0x32,), (0x31, 0x32), (0x26,), (5, 6, 8, 9, 0x26, 0x30)]


@pytest.mark.parametrize("keys", KEYS)
@pytest.mark.parametrize("prev", [(), (0x31, 0x32, 0x26)])
@pytest.mark.parametrize("target", [0.5, 0.005, 0.995, -3.0])
def test_read_keys(keys, prev, target):
    def setup(e):
        for k in keys:
            e.write(0xC28C62 + k, b"\x80")
        for k in prev:
            e.write(0xC28D62 + k, b"\x80")
        c = car(e)
        f32(e, c + 0x730, target)
        out = e.alloc(20)
        f32(e, out, 9.0, 9.0, 9.0, 9.0, 9.0)
        a = e.alloc(4)
        u32(e, a, out)
        return c, a
    run(e, 0x42F8A0, "Car_ReadKeys", setup)


def state_arg(e):
    s = e.alloc(0x78)
    e.write(s, bytes(range(0x78)))
    a = e.alloc(12)
    u32(e, a, s, 0, 0)
    return a


def test_load_state():
    def setup(e):
        RET.clear()
        RET[0x4D0950] = 2.0
        RET[0x416920] = 0x7777
        return car(e), state_arg(e)
    run(e, 0x42FA60, "Car_LoadState", setup)


def test_save_state():
    def setup(e):
        c = car(e)
        e.write(c + 0x744, b"\x03")
        return c, state_arg(e)
    run(e, 0x42FB10, "Car_SaveState", setup)


@pytest.mark.parametrize("kind", [None, 12, 11])
@pytest.mark.parametrize("hit", [0.0, 0.5, float("nan")])
def test_blast(kind, hit):
    def setup(e):
        RET.clear()
        RET[0x4D0950] = 1.75
        RET[0x416C90] = hit
        b = e.alloc(0x40)
        if kind is not None:
            k = e.alloc(4)
            u32(e, k, kind)
            u32(e, b + 0x18, k)
        c = car(e)
        e.write(c + 0x91C, b"\0")
        return c, b
    run(e, 0x42FB40, "Car_Blast", setup)


def test_get_frame():
    run(e, 0x42FC00, "Car_GetFrame", lambda e: (car(e), e.alloc(12)))


def test_explode():
    run(e, 0x42FC40, "Car_Explode", lambda e: (e.alloc(0x2ED8),))


@pytest.mark.parametrize("who", [0, 1, 2])
def test_coll_filter(who):
    def setup(e):
        m = e.alloc(16)
        u32(e, m, 0x77777777, 0x1000, 0x2000, 0x3000)
        return [0x1000, 0x2000, 0x3000][who], m
    run(e, 0x42FC50, "Car_CollFilter", setup)


def test_get_light():
    run(e, 0x42FC70, "Car_GetLight", lambda e: (car(e), e.alloc(4)))


@pytest.mark.parametrize("exploded", [0, 1])
@pytest.mark.parametrize("sound", [0, 0x4321])
def test_on_world_event1(exploded, sound):
    def setup(e):
        c = car(e)
        e.write(c + 0x2ED5, bytes([exploded]))
        u32(e, c + 0x748, sound)
        return c, 0x999
    run(e, 0x42FC80, "Car_OnWorldEvent1", setup)


@pytest.mark.parametrize("registered", [0, 1])
def test_register_symbols(registered):
    def setup(e):
        c = car(e)
        e.write(c + 0x2ED4, bytes([registered]))
        return (c,)
    run(e, 0x42FCE0, "Car_RegisterSymbols", setup)
    run(e, 0x42FD40, "Car_RemoveSymbols", setup)


def test_close():
    run(e, 0x42FDA0, "Car_Close", lambda e: None)


def test_get_type():
    compare(e, 0x42FDC0, "Car_GetType", eax_mask=0xFFFF)


def test_find_child():
    def setup(e):
        RET.clear()
        RET[0x401BE0] = 0x8888
        return (car(e),)
    run(e, 0x42FDD0, "sub_42FDD0", setup, check_eax=True)


def test_get_point8():
    def setup(e):
        c = car(e)
        f32(e, c + 0x70, *[0.5 * i for i in range(9)])
        return c, e.alloc(24), e.alloc(40)
    run(e, 0x42FDF0, "sub_42FDF0", setup)
