import struct

import pytest

from emu import Emu, compare
from test_game_stubs import PyFake, d64, f32, fakes, u32


@pytest.fixture(autouse=True, params=[0x37F, 0x7F, 0xC7F, 0x77F], ids=hex)
def cw(request):
    """Every test at the emulator default and at the control words the game uses."""
    Emu.cw = request.param
    yield
    Emu.cw = None


emu = Emu()

G_GAME = 0x57BABC
EVENTS = 0x539540
KEYS, KEYS_PREV = 0xC28C62, 0xC28D62
CHEATS = 0x57B194

WTYPES, WCOUNT, WEVENTS = 0x56E258, 0x57B18C, 0x5391AC

IS_A, CUBE_TYPE, VIR_TYPE = 0x401CF0, 0x4C48C0, 0x4D9610
COUNT, GET930, GET970, GETA50, VIR_FIND = 0x4D0D20, 0x4D0930, 0x4D0970, 0x4D0A50, 0x4CEC10

F = fakes(emu, {0x4F1D80: 1, 0x4B0D10: 1, COUNT: (1, 3), GET930: 4, GET970: 3, GETA50: 3, VIR_FIND: (1, 0)})
PyFake(emu, 0x4018C0, log=(("b", 0),))
PyFake(emu, CUBE_TYPE, ret=0x11)
PyFake(emu, VIR_TYPE, ret=0x22)
PyFake(emu, IS_A, log=(("w", 0), ("w", 1)), ret=lambda e: int(e.arg(0) & 0xFFFF == e.arg(1) & 0xFFFF))
PROFILE = emu.alloc(0x600)
GET_PROFILE = PyFake(emu, 0x406220, ret=PROFILE)

# Coll_DropPoints: logs the query, then reports the hits listed in DROP
DROP = []


def drop(e):
    res = e.arg(1)
    found, z = DROP.pop(0) if DROP else (0, 0.0)
    e.write(res + 0x58, u32(found))
    e.write(res + 0x18, struct.pack("<d", z))


PyFake(emu, 0x4CABF0, log=(0, (2, 6), 3, (4, 2), ("b", 5), lambda e: e.u32(e.arg(6) + 4), 7), fn=drop)
emu.baseline = emu.snapshot()


def test_weapon_type_close():
    def setup(e):
        e.write(WCOUNT, u32(3))
        for i, (a, b) in enumerate([(0x1000, 0x2000), (0, 0x3000), (0x4000, 0)]):
            e.write(WTYPES + i * 0x6E8 + 0x6E0, u32(a) + u32(b))
        e.write(WTYPES + 3 * 0x6E8 + 0x6E0, u32(0x5000) + u32(0x6000))
        e.write(WEVENTS, bytes([3, 4, 5, 6, 7, 8, 9]))
    compare(emu, 0x414810, "WeaponType_Close", setup=setup, check_eax=False, normalized=True)


def vec(x, y, z):
    return d64(x) + d64(y) + d64(z)


VECS = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1), (0, 0, -1), (3, 4, 5), (-3, 4, -5), (0.5, -2, 0.25),
        (-1e-3, 7, 1e-3), (2, -0.0, -0.0), (1e30, 1e-30, -3)]


@pytest.mark.parametrize("v", VECS)
@pytest.mark.parametrize("addr,name", [(0x4148F0, "Vec3d_PitchAngle"), (0x414950, "Vec3d_YawAngle")])
def test_angles(addr, name, v):
    compare(emu, addr, name, args=vec(*v), check_eax=False, check_st0=True)


def set_game(e):
    g = e.alloc(0x158)
    e.write(g + 0x20, u32(0x7700))
    e.write(G_GAME, u32(g))
    e.write(EVENTS, bytes(range(0x40, 0x4F)))
    return g


@pytest.mark.parametrize("hits", [[(1, 12.5)], [(0, 0), (1, -7.25)], [(0, 0), (0, 0)]])
def test_drop_to_ground(hits):
    def setup(e):
        set_game(e)
        DROP[:] = hits
        pos = e.alloc(24)
        e.write(pos, struct.pack("<3d", 100.0, -50.0, 3.0))
        return 0x1234, pos
    compare(emu, 0x4149B0, "Game_DropToGround", setup=setup, check_eax=False, normalized=True)


DIRS = [(0, 0, 1), (0, 0, -1), (0.1, 0.2, 0.97), (-0.3, 0.1, -0.96), (0, 0, 0.96), (1, 0, 0), (0, 1, 0),
        (0.6, 0.8, 0), (0.3, -0.4, 0.5), (0, 0, 0), (-2, 3, 0.95), (0, 0, 0.9500001), (5, -1, 30)]


@pytest.mark.parametrize("d", DIRS)
def test_matrix_set_y_axis(d):
    def setup(e):
        out = e.alloc(36)
        e.write(out, b"\xaa" * 36)
        p = e.alloc(12)
        e.write(p, struct.pack("<3f", *d))
        return out, p
    compare(emu, 0x414A60, "Matrix_SetYAxis", setup=setup, check_eax=False)


def model_task(e, type_, model, model2):
    t = e.alloc(0x80)
    e.write(t + 0x1C, struct.pack("<H", type_))
    for off, m in ((0x6C, model), (0x74, model2)):
        if m:
            p = e.alloc(4)
            e.write(p, u32(m))
            e.write(t + off, u32(p))
    return t


CASES = [(0x11, 0x900, 0, 1), (0x11, 0x900, 0, 3), (0x11, 0x900, 0, -1), (0x11, 0, 0x901, 0),
         (0x22, 0x900, 0x901, 2), (0x22, 0x900, 0x901, 5), (0x22, 0x900, 0, 0), (0x33, 0x900, 0x901, 0)]


@pytest.mark.parametrize("type_,model,model2,i", CASES)
@pytest.mark.parametrize("addr,name,size", [(0x414E20, "Model_GetPoint", 24), (0x414ED0, "sub_414ED0", 12)])
def test_model_points(addr, name, size, type_, model, model2, i):
    def setup(e):
        out = e.alloc(24)
        e.write(out, b"\xaa" * 24)
        return out, model_task(e, type_, model, model2), i
    compare(emu, addr, name, setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("type_,i", [(0x11, 0), (0x22, 0), (0x22, 2), (0x22, 3), (0x33, 0)])
@pytest.mark.parametrize("found", [0, 0x902])
def test_model_points_by_name(type_, i, found):
    def setup(e):
        out = e.alloc(24)
        e.write(out, b"\xaa" * 24)
        name = e.alloc(16)
        e.write(name, b"gun_barrel\0")
        if found:
            p = e.alloc(4)
            e.write(p, u32(found))
            found_ptr = p
        else:
            found_ptr = 0
        F[VIR_FIND].ret = found_ptr
        return out, model_task(e, type_, 0x900, 0x901), i, name
    compare(emu, 0x414F40, "sub_414F40", setup=setup, check_eax=False, normalized=True)


@pytest.mark.parametrize("key,down,prev,ctrl,shift,profile,level,cheats", [
    (0x19, 1, 0, 1, 1, 1, 14, 1), (0x19, 1, 1, 1, 1, 1, 14, 1), (0x19, 0, 0, 1, 1, 1, 14, 1),
    (0x13, 1, 0, 0, 1, 1, 14, 1), (0x13, 1, 0, 1, 0, 1, 14, 1), (0x13, 1, 0, 1, 1, 0, 14, 1),
    (0x13, 1, 0, 1, 1, 1, 13, 1), (0x13, 1, 0, 1, 1, 1, 99, 0), (0x1D, 1, 0, 1, 1, 1, 20, 1)])
@pytest.mark.parametrize("addr,name", [(0x414FD0, "Game_CheatKeyDown"), (0x415020, "Game_CheatKeyPressed")])
def test_cheat_keys(addr, name, key, down, prev, ctrl, shift, profile, level, cheats):
    def setup(e):
        GET_PROFILE.ret = PROFILE if profile else 0
        e.write(PROFILE + 0x560, u32(level))
        e.write(CHEATS, u32(cheats))
        e.write(KEYS + 0x1D, b"\x80" if ctrl else b"\x00")
        e.write(KEYS + 0x2A, b"\x80" if shift else b"\x00")
        e.write(KEYS + key, b"\x81" if down else b"\x01")
        e.write(KEYS_PREV + key, b"\x80" if prev else b"\x7f")
        return (key,)
    compare(emu, addr, name, setup=setup, normalized=True)
