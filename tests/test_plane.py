import struct

import pytest

from emu import compare
from test_plane_util import (CHILD, EV_CHANGED, EV_E4, EV_MOVE, GEAR, LOG, PLANE, RET, VEHICLE, add_child, all_events,
                             cstr, d64, f32, handler, init_emu, matrix, msg_handler, plane_emu, run, set_handler,
                             setup_init, task, u16, u32)

emu = plane_emu()
set_handler(emu, 1, CHILD, handler(emu, "run"))
set_handler(emu, 1, GEAR, handler(emu, "run_gear"))
set_handler(emu, EV_E4, CHILD, msg_handler(emu, "e4", 2))
set_handler(emu, EV_MOVE, CHILD, msg_handler(emu, "move", 3))
set_handler(emu, EV_MOVE, GEAR, msg_handler(emu, "move_gear", 3))
set_handler(emu, EV_CHANGED, CHILD, msg_handler(emu, "changed", 0))
emu.baseline = emu.snapshot()

ROT = (0.6123457, -0.7905694, 0.0123457, 0.7904321, 0.6111111, -0.0371234, 0.0217391, 0.0333333, 0.9993827)
ROT2 = (0.3617284, 0.4812345, -0.7984567, -0.8012346, 0.5987654, 0.0123457, 0.4790123, 0.6308642, 0.6012345)


def config(e, **f):
    c = e.alloc(0x118)
    f32(e, c, f.get("unk0", 1.5), f.get("rate0", 3.0), f.get("smooth0", 0.25), 2.5, f.get("smooth1", 0.5), 1.75, f.get("smooth2", 0.75),
        f.get("thrust", 2.0), f.get("accel", 0.05), f.get("slow", 0.02))
    u32(e, c + 0x28, f.get("parts", 5))
    f32(e, c + 0x2C, *[i * 1.25 - 7.0 for i in range(56)])
    f32(e, c + 0x10C, 1.0, 2.0, 3.0)
    return c


def phys(e, joints=3, **f):
    ph = e.alloc(0x2D8)
    f32(e, ph + 8, f.get("power", 3.5))
    u32(e, ph + 0x148, joints)
    u32(e, ph + 0x2D4, config(e, **f))
    return ph


def plane(e, ph=True, coll=False, children=0, sound=None, **f):
    p = task(e, 0x888, PLANE)
    d64(e, p + 0x20, 1000.5, -2000.25, 300.0)
    u32(e, p + 0x6C, 0x6600)
    matrix(e, p + 0x70, f.get("rot", ROT), 3)
    f32(e, p + 0x98, *f.get("scale", (1.5, 2.0, 1.25)))
    f32(e, p + 0xF8, f.get("body_speed", 20.0))
    f32(e, p + 0x108, 11.0, -12.0, 13.0, 0.5, 0.25, 0.125, 1.5, -2.5, 3.5)
    e.write(p + 0x154, f.get("name", b"mig29") + b"\0")
    d64(e, p + 0x168, -5.5, 6.25, 7.75)
    f32(e, p + 0x180, *f.get("vel2", (0.0, 0.0, 0.0)))
    matrix(e, p + 0x18C, ROT2, 5)
    if ph:
        u32(e, p + 0x1B4, phys(e, **f))
    u32(e, p + 0x1B8, f.get("ground", 0))
    f32(e, p + 0x720, f.get("speed", 0.9), *f.get("turn", (0.1, -0.2, 0.3)))
    f32(e, p + 0x730, f.get("target", 1.1), *f.get("steer", (0.5, -0.5, 1.0)))
    f32(e, p + 0x740, f.get("start", 0.7))
    e.write(p + 0x744, bytes([f.get("brake", 0)]))
    f32(e, p + 0x748, *[i * 0.5 for i in range(56)])
    sp = e.alloc(12)
    u32(e, sp, 2)
    u32(e, p + 0x828, f.get("sp", sp))
    e.write(p + 0x82C, bytes([f.get("cockpit", 0), f.get("gear", 0), f.get("catapult", 0), f.get("force_z", 0),
                              f.get("cockpit_init", 1), f.get("gear_init", 0), 0x22, 0x33]))
    if sound is not None:
        ref = e.alloc(4)
        u32(e, ref, sound)
        u32(e, p + 0x834, ref)
    for i in range(children):
        add_child(e, p, task(e, 0x40, CHILD))
    if coll:
        c = task(e, 0x40, CHILD)
        u32(e, p + 0xE8, c)
        for i in range(coll):
            add_child(e, c, task(e, 0x40, CHILD))
    return p


def test_init():
    def setup(e):
        setup_init(e)
        all_events(e)
        t = e.call(0x401900, cstr(e, b"Vehicle"), 0x748, e.call(0x4018F0)[0] & 0xFFFF, 0)[0]
        u16(e, 0x54E200, t)
    init = init_emu()
    run(init, 0x4330F0, "Plane_Init", setup)


@pytest.mark.parametrize("ph", [False, True])
def test_create(ph):
    def setup(e):
        RET[0x4D0950] = 512.0
        return (plane(e, ph=ph, scale=(1.0, 3.0, 2.0)),)
    run(emu, 0x4333F0, "Plane_Create", setup)


@pytest.mark.parametrize("scale", [(1.0, 2.0, 3.0), (3.0, 2.0, 1.0), (1.0, 3.0, 2.0), (2.0, 2.0, 2.0)])
def test_reset(scale):
    def setup(e):
        RET[0x4D0950] = 100.25
        return (plane(e, scale=scale),)
    run(emu, 0x433410, "Plane_Reset", setup)


@pytest.mark.parametrize("ph", [False, True])
def test_reset_controls(ph):
    run(emu, 0x433530, "Plane_ResetControls", lambda e: (plane(e, ph=ph, gear_init=1, cockpit_init=0),))


@pytest.mark.parametrize("parts", [0, 1, 3, 8])
def test_copy_parts(parts):
    run(emu, 0x433580, "Plane_CopyParts", lambda e: (plane(e, parts=parts),))


@pytest.mark.parametrize("has,value", [(0, 0.0), (1, 0.0), (1, 0.4), (1, 1.0), (1, -3.0)])
@pytest.mark.parametrize("sound", [None, 0, 0x6000])
@pytest.mark.parametrize("new", [0, 0x6100])
@pytest.mark.parametrize("children", [0, 2])
def test_run(has, value, sound, new, children):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        RET[0x4E6B00] = new
        return (plane(e, sound=sound, children=children),)
    run(emu, 0x433690, "Plane_Run", setup)


@pytest.mark.parametrize("sound", [None, 0, 0x6000])
def test_stop_sound(sound):
    run(emu, 0x433760, "Plane_StopSound", lambda e: (plane(e, sound=sound),))


@pytest.mark.parametrize("sound", [None, 0x6000])
def test_delete(sound):
    run(emu, 0x433790, "Plane_Delete", lambda e: (plane(e, sound=sound),))


def test_get_params():
    def setup(e):
        u32(e, 0x57BD54, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x4337D0, "Plane_GetParams", setup)


def inputs(e, thrust=-1.0, toggles=(-1.0, -1.0, -1.0, -1.0, -1.0)):
    i = e.alloc(40)
    f32(e, i, 0.25, -0.75, 1.0, thrust, 0.0, *toggles)
    return i


@pytest.mark.parametrize("mode", [-1, 0, 1])
@pytest.mark.parametrize("thrust", [-1.0, 0.85])
@pytest.mark.parametrize("toggles", [(-1.0,) * 5, (1.0, 1.0, 1.0, 1.0, 1.0), (1.0, -1.0, 0.0, -1.0, 2.0),
                                     (-1.0, 1.0, -1.0, 0.0, -1.0)])
@pytest.mark.parametrize("vel2,coll", [((0.0, 0.0, 0.0), 2), ((0.0, 0.0, 0.0), False), ((0.0, 5.0, 0.0), 1)])
def test_control(mode, thrust, toggles, vel2, coll):
    if mode == 1 and not coll:
        pytest.skip("flying needs the collision task")

    def setup(e):
        RET[0x4D0950] = 300.0
        ctl = e.alloc(8)
        u32(e, ctl, mode, inputs(e, thrust, toggles))
        return plane(e, vel2=vel2, coll=coll, cockpit=1), ctl
    run(emu, 0x4337E0, "Plane_Control", setup)


FLY = [
    dict(),
    dict(ground=1),
    dict(ground=1, target=0.5, body_speed=5.0),
    dict(ground=1, target=1.5, body_speed=2000.0),
    dict(speed=0.5, target=0.52),
    dict(speed=0.5, target=0.1),
    dict(speed=0.003, target=0.0, accel=0.001),
    dict(speed=1.0, target=1.0),
    dict(speed=0.0, target=0.0, turn=(0.0, 0.0, 0.0), steer=(0.0, 0.0, 0.0)),
    dict(turn=(0.0, 0.0, 0.0), steer=(0.0, 0.0, 0.5)),
    dict(steer=(-1.0, 1.0, -1.0), turn=(-1.0, 1.0, -1.0), smooth0=0.0, smooth1=0.0),
    dict(steer=(1.0, -1.0, 0.0), turn=(1.0, -1.0, 0.0), smooth0=0.0, smooth1=0.0, body_speed=0.0),
    dict(brake=1),
    dict(catapult=1, gear=1),
    dict(body_speed=-5.0),
    dict(body_speed=1000.0, thrust=0.5),
    dict(parts=0),
    dict(sp=0),
    dict(rate0=0.001),
    dict(rate0=0.001, steer=(-1.0, -1.0, 0.0), turn=(-1.0, -1.0, 0.0)),
    dict(rate0=-0.5),
]


@pytest.mark.parametrize("f", FLY)
@pytest.mark.parametrize("collide", [0, 1])
def test_fly(f, collide):
    def setup(e):
        RET[0x4FC7B0] = collide
        return (plane(e, coll=2, children=2, **f),)
    run(emu, 0x433B50, "Plane_Fly", setup)


def gear(e, p, i):
    g = task(e, 0x140, GEAR)
    d64(e, g + 0x20, 10.0 * i, -20.0 * i, 30.5 + i)
    matrix(e, g + 0x70, ROT2 if i % 2 else ROT)
    d64(e, g + 0xF0, 1.5 * i, -2.25, 3.0 + i)
    return g


@pytest.mark.parametrize("gears", [0, 1, 3, 4, 5])
@pytest.mark.parametrize("hits", [[], [(1, 8000.0)], [(1, 7000.0), (1, 9000.0), (0, 1.0), (1, 100.0), (1, 7577.6)],
                                  [(1, 8192.0), (1, 8192.5), (1, 7600.0), (1, 7577.5)]])
def test_check_wheels(gears, hits):
    def setup(e):
        RET["hits"] = hits
        RET[0x4D0950] = 64.0
        p = plane(e, coll=1)
        c = e.u32(p + 0xE8)
        for i in range(gears):
            add_child(e, c, gear(e, p, i))
            add_child(e, c, task(e, 0x40, CHILD))
        return (p,)
    run(emu, 0x434310, "Plane_CheckWheels", setup)


@pytest.mark.parametrize("model", [0, 0x6700])
@pytest.mark.parametrize("name", [b"", b"x", b"x*abc", b"su27"])
@pytest.mark.parametrize("vel2,ph", [((0.0, 0.0, 0.0), True), ((0.0, 0.0, 0.0), False), ((1.0, 0.0, -2.0), False)])
def test_params_changed(model, name, vel2, ph):
    def setup(e):
        RET[0x4CEC10] = model
        RET[0x4D0950] = 77.0
        return (plane(e, name=name, vel2=vel2, ph=ph, coll=2),)
    run(emu, 0x434770, "Plane_ParamsChanged", setup)


def test_sound_info():
    run(emu, 0x434A30, "Plane_GetSoundInfo", lambda e: (plane(e), e.alloc(12)))


@pytest.mark.parametrize("i", [-1, 0, 5, 9, 10])
def test_input_info(i):
    def setup(e):
        m = e.alloc(8)
        u32(e, m, i, 0x1234)
        return 0, m
    run(emu, 0x434A50, "Plane_GetInputInfo", setup)


KEYS = [
    (), (0x9D, 0xCD), (0x1D, 0xC8), (0xCB, 0xD0, 0x2C), (0xCD, 0xC8, 0x2D, 0x29), (0x29, 0x02), (0x03, 0x07, 0x0B),
    (0x0A,), (0x31,), (0x32,), (0x31, 0x32), (0x2E, 0x22, 0x30, 0x25, 0x10), (0x02, 0x31),
]


@pytest.mark.parametrize("keys", KEYS)
@pytest.mark.parametrize("prev", [False, True])
@pytest.mark.parametrize("target", [1.1, 0.805, 1.245, 0.5, 2.0])
def test_read_keys(keys, prev, target):
    def setup(e):
        for k in keys:
            e.write(0xC28C62 + k, b"\x80")
            if prev:
                e.write(0xC28D62 + k, b"\x80")
        m = e.alloc(4)
        i = e.alloc(40)
        f32(e, i, *[9.0] * 10)
        u32(e, m, i)
        return plane(e, target=target), m
    run(emu, 0x434A80, "Plane_ReadKeys", setup)


def state(e):
    s = e.alloc(0x80)
    e.write(s, bytes(range(0x80)))
    m = e.alloc(12)
    u32(e, m, s, e.alloc(24), 0)
    return m


def test_load_state():
    def setup(e):
        RET[0x4D0950] = 8.0
        return plane(e), state(e)
    run(emu, 0x434CD0, "Plane_LoadState", setup)


def test_save_state():
    run(emu, 0x434DA0, "Plane_SaveState", lambda e: (plane(e, cockpit=1, gear=2, brake=3, catapult=4, force_z=5),
                                                      state(e)))


def test_close():
    def setup(e):
        u32(e, 0x57BD54, 0x4455)
        u32(e, 0x53BEE8, 0x12345678)
    run(emu, 0x434DF0, "Plane_Close", setup)
