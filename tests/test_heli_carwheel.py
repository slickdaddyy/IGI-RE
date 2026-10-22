import math
import random
import struct

import pytest

from test_heli_util import RET, f32, new_emu, run, task, u32, vfake

CAR = 0x60

emu = new_emu(fakes=[(0x4ECF50, 2, dict(derefs={1: 24}))])
vfake(emu, 0x4ECF80, 3, show=lambda e, a: (e.normalize(a[1]), e.read(a[2], 12)),
      effect=lambda e, a: f32(e, a[0], *RET.get("vel", (300.0, -40.0, 12.0))))
emu.baseline = emu.snapshot()
e = emu


def bits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def rot(yaw, pitch=0.0):
    c, s, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    return (c, -s * cp, s * sp, s, c * cp, -c * sp, 0.0, sp, cp)


def car(e, i=1, joint=3, steered=1, driven=1, steer=25.0, turn=0.4, speed=0.6, hit=1, airborne=0, brake=0,
        tank=0, side=0, normal=(0.0, 0.1, 0.995), mat=rot(0.3, 0.1), mass=1500.0, drag=0.25, thrust=80.0,
        driven_count=2, grip=(0.8, 0.3)):
    c = task(e, 0x2ED8, CAR)
    f32(e, c + 0xF0, mass, 0.0, drag)
    f32(e, c + 0x70, *mat)
    u32(e, c + 0x94, 2)
    f32(e, c + 0x720, speed)
    f32(e, c + 0x72C, turn)
    e.write(c + 0x744, bytes([brake]))
    e.write(c + 0x6D9, bytes([airborne]))
    u32(e, c + 0x758, driven_count)
    data = e.alloc(0x2C0)
    f32(e, data + 0x1C, thrust)
    p = data + 0x11C + 0x14 * i
    e.write(p, bytes([steered, driven]))
    f32(e, p + 4, steer)
    e.write(data + 0x261, bytes([tank]))
    phys = e.alloc(0x2D8)
    u32(e, phys + 0x2D4, data)
    u32(e, c + 0x1B4, phys)
    for j in range(22):
        q = c + 0x1BC + 0x38 * j
        u32(e, q, side)
        f32(e, q + 0x1C, *grip)
    ct = c + 0x75C + 0x1C * i
    e.write(ct, bytes([1, hit]))
    f32(e, ct + 4, *normal)
    f32(e, ct + 0x10, 120.0, -340.0, -55.0)
    return c


FORCES = [
    dict(), dict(vel=(0.0, 0.0, 5.0)), dict(vel=(0.0, 0.0, 0.0)), dict(vel=(1.0, 0.5, 0.0)),
    dict(vel=(5000.0, 20000.0, -300.0)), dict(grip=(0.0, 0.0)), dict(mass=10.0), dict(mass=1e6),
]


@pytest.mark.parametrize("c0", FORCES)
@pytest.mark.parametrize("angle", [0.0, 0.35, -1.2])
def test_wheel_force(c0, angle):
    def setup(e):
        c = dict(c0)
        RET.clear()
        if "vel" in c:
            RET["vel"] = c.pop("vel")
        o = car(e, **c)
        ct = o + 0x75C + 0x1C
        return o + 0xF0, o + 0x70, ct + 0x10, ct + 4, bits(angle), bits(0.8), bits(0.3), 0x42A3D70A
    run(e, 0x430960, "Car_WheelForce", setup)


UPDATES = [
    dict(), dict(driven=0), dict(hit=0), dict(hit=0, airborne=1), dict(speed=0.0), dict(speed=0.0, airborne=1),
    dict(steered=0), dict(brake=1), dict(joint=-1), dict(tank=1, turn=0.6, side=1), dict(tank=1, turn=-0.6),
    dict(normal=(0.3, -0.2, 0.93)), dict(speed=-0.5, mat=rot(2.5, -0.3)), dict(driven_count=3, drag=1000.0),
]


@pytest.mark.parametrize("c0", UPDATES)
def test_update_wheel(c0):
    def setup(e):
        c = dict(c0)
        RET.clear()
        joint = c.pop("joint", 3)
        return car(e, joint=3, **c), 1, joint
    run(e, 0x430D10, "Car_UpdateWheel", setup)


def random_case(seed):
    r = random.Random(seed)
    u = lambda a: r.uniform(-a, a)  # noqa: E731
    return dict(mat=tuple(u(1.0) for _ in range(9)), normal=(u(1.0), u(1.0), u(1.0)), turn=u(1.0),
                speed=u(1.0), steer=u(40.0), mass=r.uniform(1.0, 5000.0), drag=u(500.0), thrust=u(300.0),
                grip=(u(1.0), u(1.0)), vel=(u(30000.0), u(30000.0), u(3000.0)), tank=r.randint(0, 1),
                side=r.randint(0, 1), brake=r.randint(0, 1))


@pytest.mark.parametrize("seed", range(40))
def test_wheel_random(seed):
    def setup(e):
        c = random_case(seed)
        RET.clear()
        RET["vel"] = c.pop("vel")
        return car(e, **c), 1, 3
    run(e, 0x430D10, "Car_UpdateWheel", setup)
