"""FlatWater, FlatSky, their layers and MipMapControl (0x51D840 - 0x51EE80)."""
import struct

import pytest

from emu import ErrorShown
from test_computer_util import com
from test_flatwater_util import (CWS, LOG, RET, cstr, d64, f32, fake_emu, run, task, u16, u32, with_cw)

emu = fake_emu({
    0x4C6890: 3,  # DynCube_ListAdd
    0x4C6840: 3,  # DynCube_ListRemove
    0x4B5B60: (1, {"strings": (0,)}),  # Resource_Exists
    0x4B22F0: 1,  # Texture_Free
    0x4B24C0: (3, {"strings": (2,)}),  # Texture_LoadFile
    0x52CB30: 0,  # SkyDome_New
    0x52D030: 1,  # SkyDome_Free
    0x52D350: 2,  # SkyDome_SetAngle
    0x52D2B0: 6,  # SkyDome_SetColors
    0x491E70: (3, {"masks": {0: 0xFF, 1: 0xFF, 2: 0xFF}}),  # Display_SetClearColor
    0x492BE0: 1,  # DDraw_GetErrorString
})
DEVICE = com(emu, "dev", {37: 4}, effects={37: lambda e, a: RET.get("hr", 0)})
ERRTEXT = cstr(emu, b"bad state")
emu.baseline = emu.snapshot()

WATER_LAYER = 0x30
NAN = float("nan")


def world(e):
    w = e.alloc(0xE00)
    u32(e, 0xA44344, w)
    return w


def test_water_create():
    def setup(e):
        world(e)
        t = task(e, 0x90)
        e.write(t + 0x20, bytes(range(0x70)))
        return (t,)
    run(emu, 0x51D840, "FlatWater_Create", setup)


def test_water_delete():
    def setup(e):
        w = world(e)
        u32(e, w + 0xDFC, 0xBA2000)
        t = task(e, 0x90)
        u32(e, t + 0x68, w)
        return (t,)
    run(emu, 0x51D8B0, "FlatWater_Delete", setup)


def layer(e, type_=WATER_LAYER, color=(0.5, 0.25, 1.0), name=b"", last=b"", loaded=0):
    t = task(e, 0x26C, type_)
    u32(e, t + 0x44, loaded)
    e.write(t + 0x48, last + b"\0")
    e.write(t + 0x148, name + b"\0")
    f32(e, t + 0x260, *color)
    return t


WATER_COLORS = [
    [],
    [(0.1, 0.2, 0.7)],
    [(0.1, 0.2, 0.7), (0.3, 0.6, 0.9), (0.33, 0.01, 1.0)],
    [(0.7, 0.7, 0.7)] * 7,
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("colors", WATER_COLORS)
@pytest.mark.parametrize("own", [(0.5, -0.5, 1.5), (NAN, 1.0, 0.0), (-0.0, 0.999, 2.0)])
def test_water_params_changed(cw, colors, own):
    def setup(e):
        from test_menu_util import add_child
        u16(e, 0xBA2018, WATER_LAYER)
        w = task(e, 0x90)
        d64(e, w + 0x70, 12.75)
        f32(e, w + 0x80, *own)
        add_child(e, w, task(e, 0x40, 0x31))
        for c in colors:
            add_child(e, w, layer(e, color=c))
            add_child(e, w, task(e, 0x40, 0x31))
        return (w,)
    run(emu, with_cw(emu, 0x51D8F0, cw), with_cw(emu, "FlatWater_ParamsChanged", cw), setup)


ONE_UP = 1.0 + 2.0 ** -23
RUN_CASES = [
    (0.1, 0.2, 0.3, 0.4),
    (0.3, 0.3, 0.9, 0.9),
    (-0.3, -0.3, 0.1, 0.1),
    (0.0, 0.0, 5.5, -5.5),
    (2.0 ** -30, 2.0 ** -30, 1.0, 1.0),
    (-(2.0 ** -30), -(2.0 ** -30), 0.0, 0.0),
    (0.0, 0.0, 1.0, 0.0),
    (0.0, 0.0, ONE_UP, -0.0),
    (0.7, -0.9, 0.95, 0.05),
    (0.333, 0.777, 0.999, 0.6667),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("su,sv,ou,ov", RUN_CASES)
def test_layer_run(cw, su, sv, ou, ov):
    def setup(e):
        t = layer(e)
        f32(e, t + 0x24C, su, sv, ou, ov)
        return (t,)
    run(emu, with_cw(emu, 0x51DA90, cw), with_cw(emu, "FlatLayer_Run", cw), setup)


def test_layer_create():
    def setup(e):
        t = task(e, 0x26C)
        e.write(t + 0x20, bytes(range(256)) * 2)
        return (t,)
    run(emu, 0x51DB80, "FlatLayer_Create", setup)


LAYER_CASES = [
    (b"", b"old", 0, 1),
    (b"sky.tga", b"sky.tga", 1, 1),
    (b"sky.tga", b"", 0, 1),
    (b"sky.tga", b"", 1, 1),
    (b"sky.tga", b"", 1, 0),
    (b"sky", b"bigsky", 1, 1),
    (b"sky", b"yks", 1, 1),
    (b"File not found", b"", 0, 1),
    (b"xFile not foundx", b"a", 1, 1),
]


@pytest.mark.parametrize("name,last,loaded,exists", LAYER_CASES)
@pytest.mark.parametrize("color", [(0.5, 1.5, -1.0), (NAN, 1.0, 0.0)])
def test_layer_params_changed(name, last, loaded, exists, color):
    def setup(e):
        return (layer(e, color=color, name=name, last=last, loaded=loaded),)
    run(emu, 0x51DBF0, "FlatLayer_ParamsChanged", setup, rets={0x4B5B60: exists})


@pytest.mark.parametrize("loaded", [0, 1])
def test_layer_delete(loaded):
    def setup(e):
        return (layer(e, loaded=loaded),)
    run(emu, 0x51EC30, "FlatLayer_Delete", setup)


def test_sky_create():
    def setup(e):
        world(e)
        t = task(e, 0xD0)
        e.write(t + 0x20, bytes(range(0xB0)))
        return (t,)
    run(emu, 0x51EA10, "FlatSky_Create", setup, rets={0x52CB30: 0x7777})


def test_sky_delete():
    def setup(e):
        t = task(e, 0xD0)
        u32(e, t + 0x68, 0x6868)
        u32(e, t + 0x84, 0x8484)
        return (t,)
    run(emu, 0x51EB00, "FlatSky_Delete", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("mirror", [0, 1])
@pytest.mark.parametrize("clear,angle", [((0.5, 1.0, 0.25), 15.0), ((-0.1, 2.0, 0.00196), 33.3),
                                         ((0.75, 0.1, 0.9), 89.9)])
def test_sky_params_changed(cw, mirror, clear, angle):
    def setup(e):
        t = task(e, 0xD0)
        f32(e, t + 0x78, *clear)
        u32(e, t + 0x84, 0x8484)
        f32(e, t + 0x88, angle)
        f32(e, t + 0x8C, *[0.01 * i for i in range(15)])
        e.write(t + 0xC8, bytes([mirror]))
        return (t,)
    run(emu, with_cw(emu, 0x51EB40, cw), with_cw(emu, "FlatSky_ParamsChanged", cw), setup)


def test_mipmap_create():
    def setup(e):
        u32(e, 0xBC79E8, 0x7F800001)
        e.write(0xBC79EC, bytes([0x85, 0x33]))
        u32(e, 0xBC79F0, 0x7F800001)
        t = task(e, 0x30)
        e.write(t + 0x20, bytes(range(0x10)))
        return (t,)
    run(emu, 0x51EE80, "MipMapControl_Create", setup)


def test_mipmap_create_values():
    def setup(e):
        f32(e, 0xBC79E8, -2.5)
        e.write(0xBC79EC, bytes([2, 1]))
        f32(e, 0xBC79F0, 1.25)
        return (task(e, 0x30),)
    run(emu, 0x51EE80, "MipMapControl_Create", setup)


def mipmap_setup(same, hr):
    def setup(e):
        u32(e, 0x5CA0E0, 3)
        u32(e, 0xA94A64, 3 if same else 4)
        u32(e, 0x5CA114, DEVICE)
        RET["hr"] = hr
        RET[0x492BE0] = ERRTEXT
        t = task(e, 0x30)
        f32(e, t + 0x20, -1.5)
        return (t,)
    return setup


@pytest.mark.parametrize("same", [False, True])
def test_mipmap_params_changed(same):
    run(emu, 0x51EE10, "MipMapControl_ParamsChanged", mipmap_setup(same, 0))


def test_mipmap_params_changed_error():
    logs = []
    for target in (0x51EE10, "MipMapControl_ParamsChanged"):
        emu.reset()
        RET.clear()
        args = mipmap_setup(True, 0x88760096)(emu)
        LOG.append([])
        with pytest.raises(ErrorShown) as err:
            emu.call(target, *args)
        logs.append((LOG[-1], str(err.value), emu.read_cstr(emu.arg(1)), emu.read_cstr(emu.arg(2))))
    assert logs[0] == logs[1]
    assert logs[0][2] == b"Couldn't set texturestagestate"
