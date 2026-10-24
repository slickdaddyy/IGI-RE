import random
import struct

import pytest

from emu import Emu
from test_haze_util import (CWS, Fakes, check, check_error, cstr, f32, f64, new_task, rmatrix, set_handler,
                            task_system, with_children)

# real task system
e = Emu()
ef = Fakes(e)

# callees replaced by logging fakes
fe = Emu()
fk = Fakes(fe)
DYNCUBE_ADD = fk.add(0x4C7140, 5, deref={2: 24})
DYNCUBE_MOVE = fk.add(0x4C6940, 4, deref={2: 24})
fk.add(0x4C7760, 1)
fk.add(0x4D2D60, 2)
fk.add(0x4D2E40, 1)
RADIUS = fk.add(0x4D0950, 1, ret=2.75, st0="f")
VIRMODEL_FIND = fk.add(0x4CEC10, 1, fmt=lambda em, a: [em.read_cstr(a[0])])
fk.add(0x4C5710, 3)
fk.add(0x4C4950, 3)
LOG2 = fk.handler(2, deref={1: 4})
LOG2RAW = fk.handler(2)
LOG2MOVE = fk.handler(2, deref={1: 8})
LOG1 = fk.handler(1)

WORLD_EVENT = 0x548176
BONE_TYPE, BONE_EVENT = 0x548618, 0x54861A
QOBJ_TYPE, QOBJ_EVENT = 0x548638, 0x54863A
CAM_EVENT, CAM_LIST = 0x548644, 0xA54690
MAGIC_TYPE, MAGIC_MOVE = 0x548648, 0x54864A
RIGID_TYPE = 0x547F42


def getter(orig, ours, addr, data, mask):
    def setup(em):
        em.write(addr, data)
    check(fe, fk, orig, ours, setup=setup, eax_mask=mask)


@pytest.mark.parametrize("orig,ours,addr,data,mask", [
    (0x4D9610, "BoneDynCubeObj_GetType", BONE_TYPE, b"\x34\x12", 0xFFFF),
    (0x4D97E0, "QObj_GetEvent", QOBJ_EVENT, b"\x55", 0xFF),
    (0x4D9850, "Camera_GetChangedEvent", CAM_EVENT, b"\x66", 0xFF),
    (0x4D9860, "Camera_GetListeners", CAM_LIST, b"\x78\x56\x34\x12", 0xFFFFFFFF),
    (0x4D9FA0, "MagicObj_GetType", MAGIC_TYPE, b"\x21\x01", 0xFFFF),
    (0x4D9FB0, "MagicObj_GetMoveEvent", MAGIC_MOVE, b"\x77", 0xFF),
])
def test_getters(orig, ours, addr, data, mask):
    getter(orig, ours, addr, data, mask)


# --- BoneDynCubeObj ----------------------------------------------------------

def test_bone_open():
    check(e, ef, 0x4D7550, "BoneDynCubeObj_Open", setup=task_system, check_eax=False)


def test_bone_close():
    def setup(em):
        task_system(em)
        em.call(0x4D7550)
    check(e, ef, 0x4D95E0, "BoneDynCubeObj_Close", setup=setup, check_eax=False)


@pytest.mark.parametrize("name", [True, False])
def test_bone_get_info(name):
    def setup(em):
        o = new_task(em, 0x40, 0xD8)
        info = em.alloc(0x58)
        em.write(info + 0x50, struct.pack("<I", em.alloc(16) if name else 0))
        return o, info
    check(fe, fk, 0x4D9480, "BoneDynCubeObj_GetInfo", setup=setup, check_eax=False)


@pytest.mark.parametrize("first", [0, 1])
@pytest.mark.parametrize("kids", [[], [0x41, 0x42, 0x43]])
def test_bone_world_event(first, kids):
    def setup(em):
        em.write(WORLD_EVENT, b"\x30")
        o = new_task(em, 0x40, 0xD8)
        em.write(o + 0xA0, f32(3.5))
        with_children(em, o, 0x30, LOG2RAW, kids)
        set_handler(em, 0x30, 0x42, 0)
        arg = em.alloc(12)
        em.write(arg, struct.pack("<III", first, 0x5555, 9))
        return o, arg
    check(fe, fk, 0x4D94F0, "BoneDynCubeObj_ActivateHandler", setup=setup, check_eax=False)


def test_bone_move():
    def setup(em):
        o = new_task(em, 0x40, 0xD8)
        arg = em.alloc(0x40)
        em.write(arg, f64(1.5, 2.5, -3.5) + rmatrix(random.Random(1)))
        em.write(0xA44344, struct.pack("<I", 0x777))
        return o, arg
    check(fe, fk, 0x4D95A0, "BoneDynCubeObj_MoveHandler", setup=setup, check_eax=False)


@pytest.mark.parametrize("skel", [3, -2])
def test_bone_init(skel):
    def setup(em):
        return 0x999, new_task(em, 0x40, 0xD8), skel & 0xFFFF | 0xABCD0000, 0x4444
    check(fe, fk, 0x4D9620, "BoneDynCubeObj_Init", setup=setup, check_eax=False)


def test_bone_deinit():
    check(fe, fk, 0x4D96F0, "BoneDynCubeObj_Deinit", setup=lambda em: (new_task(em, 0x40, 0xD8),),
          check_eax=False)


def test_bone_set_model():
    check(fe, fk, 0x4D9700, "BoneDynCubeObj_SetModel", setup=lambda em: (new_task(em, 0x40, 0xD8), 0x1234),
          check_eax=False)


def test_bone_set_model_null():
    check_error(fe, 0x4D9700, "BoneDynCubeObj_SetModel", lambda em: (new_task(em, 0x40, 0xD8), 0))


# --- QObj and cameras --------------------------------------------------------

def test_qobj_open():
    check(e, ef, 0x4D9730, "QObj_Open", setup=task_system, check_eax=False)


def test_qobj_close():
    def setup(em):
        task_system(em)
        em.call(0x4D9730)
    check(e, ef, 0x4D9790, "QObj_Close", setup=setup, check_eax=False)


def test_qobj_render_data():
    check(fe, fk, 0x4D97F0, "QObj_SetRenderData", setup=lambda em: (new_task(em, 1, 0x50), 0xABC), check_eax=False)
    check(fe, fk, 0x4D9800, "QObj_GetRenderData", setup=lambda em: (new_task(em, 1, 0x50),))


def test_camera_open():
    check(e, ef, 0x4D9810, "Camera_Open", setup=task_system, check_eax=False)


def test_camera_close():
    def setup(em):
        task_system(em)
        em.call(0x4D9810)
    check(e, ef, 0x4D9830, "Camera_Close", setup=setup, check_eax=False)


def test_camera_init():
    check(fe, fk, 0x4D9950, "Camera_Init", setup=lambda em: (new_task(em, 1, 0x60),), check_eax=False)


def fbits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


@pytest.mark.parametrize("frame,prio", [(5, 2.0), (5, 1.0), (5, 0.5), (6, -1.0), (6, -2.0), (4, 9.0),
                                        (5, float("nan"))])
@pytest.mark.parametrize("listeners", [0, 3])
def test_camera_set(frame, prio, listeners):
    def setup(em):
        cam = em.alloc(0x60)
        em.write(cam, bytes(range(0x48)) + f32(1.0) + struct.pack("<II", 5, 10) + bytes(12))
        em.write(CAM_EVENT, b"\x31")
        lst = em.alloc(8 + 4 * 8)
        items = [new_task(em, 0x50 + i, 0x40) for i in range(listeners)]
        em.write(lst, struct.pack("<II", listeners, 8) + b"".join(struct.pack("<I", t) for t in items))
        for i in range(listeners):
            set_handler(em, 0x31, 0x50 + i, LOG2)
        em.write(CAM_LIST, struct.pack("<I", lst))
        pos = em.alloc(24)
        em.write(pos, f64(1, 2, 3))
        axes = em.alloc(40)
        em.write(axes, rmatrix(random.Random(4)))
        return cam, pos, axes, fbits(1.2), fbits(1.33), fbits(prio), frame, 0x8888, 0x99, 0xAA
    check(fe, fk, 0x4D9870, "Camera_Set", setup=setup, check_eax=False)


# --- MagicObj ----------------------------------------------------------------

@pytest.mark.parametrize("kids", [[], [0x41, 0x42, 0x43]])
def test_magic_run(kids):
    def setup(em):
        o = new_task(em, 0x40, 0x138)
        with_children(em, o, 1, LOG1, kids)
        set_handler(em, 1, 0x42, 0)
        return (o,)
    check(fe, fk, 0x4D9970, "MagicObj_RunHandler", setup=setup, check_eax=False)


def test_magic_rigid_enter():
    def setup(em):
        em.write(RIGID_TYPE, struct.pack("<H", 0x44))
        set_handler(em, 4, 0x44, LOG2RAW)
        return (new_task(em, 0x40, 0x138),)
    check(fe, fk, 0x4D99E0, "MagicObj_EnterHandler", setup=setup, check_eax=False)


def magic_types(em):
    task_system(em)
    em.call(0x4C3450)


def test_magic_open():
    check(e, ef, 0x4D9A00, "MagicObj_Open", setup=magic_types, check_eax=False)


def test_magic_close():
    def setup(em):
        magic_types(em)
        em.call(0x4D9A00)
    check(e, ef, 0x4D9F60, "MagicObj_Close", setup=setup, check_eax=False)


SCALES = [(1, 2, 3), (3, 2, 1), (2, 3, 1), (2, 2, 2), (5, 1, 5), (1, 5, 5), (0.3, 0.7, 0.5)]


def magic_obj(em, rng, scale):
    o = new_task(em, 0x40, 0x138)
    em.write(o + 0x98, f32(*scale))
    em.write(o + 0x6C, struct.pack("<I", 0x3333))
    em.write(o + 0x68, struct.pack("<I", 0x2222))
    em.write(o + 0xF0, f64(*[rng.uniform(-20, 20) for _ in range(3)]))
    em.write(o + 0x108, rmatrix(rng))
    return o


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("model", [0x5000, 0])
@pytest.mark.parametrize("scale", SCALES)
def test_magic_create(scale, model, cw):
    def setup(em):
        rng = random.Random(7)
        fk.rets[VIRMODEL_FIND] = model
        o = magic_obj(em, rng, scale)
        arg = em.alloc(0x90)
        em.write(arg, struct.pack("<II", cstr(em, b"barrel"), 0x1111) + f64(1, 2, 3) + f64(4, 5, 6)
                 + rmatrix(rng) + rmatrix(rng) + struct.pack("<I", 0x42))
        return o, arg
    check(fe, fk, 0x4D9B10, "MagicObj_CreateHandler", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("kids", [[], [0x41, 0x42]])
@pytest.mark.parametrize("seed", range(len(SCALES)))
def test_magic_move(seed, kids, cw):
    def setup(em):
        rng = random.Random(seed)
        em.write(MAGIC_MOVE, b"\x32")
        o = magic_obj(em, rng, SCALES[seed])
        with_children(em, o, 0x32, LOG2MOVE, kids)
        pos = em.alloc(24)
        em.write(pos, f64(*[rng.uniform(-5000, 5000) for _ in range(3)]))
        m = em.alloc(40)
        em.write(m, rmatrix(rng))
        arg = em.alloc(8)
        em.write(arg, struct.pack("<II", pos, m))
        return o, arg
    check(fe, fk, 0x4D9C30, "MagicObj_MoveHandler", setup=setup, check_eax=False, cw=cw)
