"""Helpers for the AIStationaryGun, Medipack, Binocular and BinocularOverlay tests (no tests in here)."""
import math
import struct

from test_scamera_fakes import (Fakes, add_child, check, check_error, common, cstr, f32, f64, i32, masked,  # noqa: F401
                                new_task, rmatrix, rvec, set_handler, u8)

# getters of event slots and task types; set_events gives each a fixed value
GETTERS = [0x460BF0, 0x460C10, 0x489610, 0x489630, 0x4C56D0, 0x4C56E0, 0x4D9FA0, 0x4D9FB0, 0x477BF0, 0x4F1FE0,
           0x4F2030, 0x487190, 0x4F1A60, 0x4F1A70, 0x413B40, 0x413B30, 0x4071D0, 0x4E7530, 0x4018F0, 0x460BE0,
           0x416830, 0x4F0FC0]


def set_events(e):
    for i, g in enumerate(GETTERS):
        c = e.read(g, 6)
        if c[0] == 0xA0:
            e.write(struct.unpack("<I", c[1:5])[0], bytes([0x20 + i]))
        elif c[:2] == b"\x66\xa1":
            e.write(struct.unpack("<I", c[2:6])[0], struct.pack("<H", 0x20 + i))
        elif c[0] == 0xA1:
            e.write(struct.unpack("<I", c[1:5])[0], struct.pack("<I", 0x20 + i))
        else:
            raise AssertionError(hex(g))


def event(getter):
    return 0x20 + GETTERS.index(getter)


def fake_matrix(em, p, vals, stamp):
    em.write(p, f32(*vals) + struct.pack("<i", stamp))


def rot(axis, a):
    c, s = math.cos(a), math.sin(a)
    if axis == 0:
        return (1, 0, 0, 0, c, -s, 0, s, c)
    if axis == 1:
        return (c, 0, s, 0, 1, 0, -s, 0, c)
    return (c, -s, 0, s, c, 0, 0, 0, 1)


def f(bits):
    return struct.unpack("<f", struct.pack("<I", bits))[0]


def matrix_fakes(fk):
    """Matrix_SetRotY/Z and Matrix_SetEuler that also set the stamp word (the real ones leave it)."""
    fk.add(0x4B3BA0, 2, ret=lambda em, a: fake_matrix(em, a[0], rot(1, f(a[1])), 3), fmt=lambda em, a: (a[1],))
    fk.add(0x4B3BE0, 2, ret=lambda em, a: fake_matrix(em, a[0], rot(2, f(a[1])), 5), fmt=lambda em, a: (a[1],))

    def euler(em, a):
        x, y, z = f(a[1]), f(a[2]), f(a[3])
        fake_matrix(em, a[0], (math.cos(y) * math.cos(z), -math.sin(z) + x, math.sin(y), math.sin(z), math.cos(x),
                               -math.sin(x) * math.cos(y), -math.sin(y), math.sin(x) + z, math.cos(x) * math.cos(y)), 7)
    fk.add(0x4B38E0, 4, ret=euler, fmt=lambda em, a: tuple(a[1:]))


def u32(e, p, *v):
    e.write(p, struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v]))


def task_list(e, items):
    """A Human_GetList style array: count, unused word, then the pointers."""
    p = e.alloc(8 + 4 * len(items))
    u32(e, p, len(items), 0, *items)
    return p
