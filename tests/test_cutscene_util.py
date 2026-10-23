"""Helpers shared by the test_cutscene tests (CutScene and RainDynCubeObj, 0x4F4860 - 0x4F80B0)."""
import struct

from test_camera_util import (LOG, RET, add_child, cstr, d64, f32, floatfake, handler, logfake, make_emu as  # noqa: F401
                              _camera_emu, pyfake, run, set_handler, task, u16, u32)

CUT, KEY, OTHER = 0x60, 0x50, 0x61
CWS = [0x37F, 0x7F]
FISTP_CWS = [0x37F, 0x7F, 0xC7F, 0x77F]


def rel(a):
    """An argument pointer as an offset into the task under test (RET['base'])."""
    return a - RET.get("base", 0)


def make_emu():
    e = _camera_emu()
    logfake(e, 0x4F16C0, 1, effect=lambda e, a: RET.get(("has", rel(a[0])), 0))  # VarString_HasExpr
    floatfake(e, 0x4F16E0, 2, effect=lambda e, a: RET.get(("val", rel(a[0])), 0.0))  # VarString_EvalReal
    logfake(e, 0x4CEBA0, 1, strings=(0,), ret=0xB0DE)  # VirModel_Add
    logfake(e, 0x5034D0, 0, ret=0x13)  # Position_GetType
    logfake(e, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
    logfake(e, 0x4B7E80, 2, strings=(0,))  # Script_RegisterInt
    logfake(e, 0x491CF0, 0, effect=lambda e, a: RET.get("mode", 0))  # Display_GetMode
    logfake(e, 0x4A23E0, 6, ret=0x1111)  # LineFilter_New
    logfake(e, 0x4A2430, 10, ret=0x2222)  # WideScreenFilter_New
    logfake(e, 0x4A2490, 8, ret=0x3333)  # BandFilter_New
    logfake(e, 0x4A2800, 4)  # BandFilter_SetColor
    logfake(e, 0x4A28F0, 0)  # BandFilter_NextCamera
    logfake(e, 0x4A2910, 5, ret=0x4444)  # HandiCamFilter_New
    logfake(e, 0x4A4FF0, 2)  # LineFilter_SetLevel
    logfake(e, 0x4D9870, 10, derefs={1: 24, 2: 40})  # Camera_Set
    logfake(e, 0x508940, 5, derefs={0: 24, 1: 40, 2: 0x18, 3: 0x28},
            effect=lambda e, a: d64(e, a[0], 1.5, -2.5, 3.25))  # EditCamera_AttachPosition
    logfake(e, 0x508620, 3, derefs={0: 24, 1: 40},
            effect=lambda e, a: f32(e, a[1], 0.5, 0.25, -0.75, 1, 0, 0, 0, 1, 0))  # EditCamera_AttachRotation
    e.baseline = e.snapshot()
    return e


def drop_points(e):
    """Coll_DropPointsMat: logs the points and fills the hits from RET['hits'] (z or None per point)."""
    world, hits, pts, count, dz, unk, unk2, flags = (e.arg(i) for i in range(8))
    if LOG:
        LOG[-1].append(("drop", world, rel(hits), e.read(pts, 24 * count), count, e.read(dz, 8 * count), unk, unk2,
                        flags))
    for i in range(count):
        z = RET["hits"].pop(0) if RET.get("hits") else None
        h = bytearray(0x70)
        if z is not None:
            struct.pack_into("<d", h, 0x18, z)
            struct.pack_into("<I", h, 0x58, 1)
        e.write(hits + 0x70 * i, bytes(h))
    e.ret(0)


def angle(e, a):
    seq = RET.get("angle")
    return seq.pop(0) if seq else 0.5


def make_rain_emu():
    e = make_emu()
    logfake(e, 0x497450, 1, strings=(0,), ret=0x7450)  # Sound_HashBucket
    logfake(e, 0x497410, 1, strings=(0,), ret=0x7410)  # Sound_HashName
    logfake(e, 0x497800, 1, effect=lambda e, a: e.alloc(a[0]))  # Frame_Alloc
    logfake(e, 0x4978B0, 2, derefs={1: 16})  # RenderQueue_Add
    logfake(e, 0x4978D0, 1, strings=(0,), ret=0x77)  # Primitive_Register
    logfake(e, 0x4978F0, 2)  # Primitive_SetHandler
    logfake(e, 0x490370, 0, ret=0x12345)  # Timer_GetMs
    logfake(e, 0x4B0C60, 2, effect=lambda e, a: e.alloc(a[0]))  # Mem_Alloc
    logfake(e, 0x4B0D10, 1)  # Mem_Free
    logfake(e, 0x4B0D40, 0)  # Mem_SetDefaultMode
    logfake(e, 0x4B0D50, 0)  # Mem_ClearMode
    logfake(e, 0x4B0DB0, 0)  # Mem_GetMode
    floatfake(e, 0x4B2B30, 2, derefs={0: 12, 1: 12}, effect=angle)  # Vec3_Angle
    logfake(e, 0x4C6840, 3)  # DynCube_ListRemove
    logfake(e, 0x4C6890, 3)  # DynCube_ListAdd
    logfake(e, 0x4C7760, 1)  # DynCubeObj_Init
    pyfake(e, 0x4CB9F0, drop_points)
    logfake(e, 0x4E1130, 6, derefs={0: 24, 1: 24, 2: 12, 4: 12})  # Line3D_Draw2
    logfake(e, 0x4E7BD0, 2, strings=(0,))  # Debug_printf
    logfake(e, 0x4F1160, 0)  # Level_IsRunning
    e.baseline = e.snapshot()
    return e
