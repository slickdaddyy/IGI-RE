"""EditCamera (0x507F10 - 0x508D60)."""
import struct

import pytest

from test_editcam_util import CWS, LOG, RET, add_child, d64, handler, logfake, make, pyfake, run, set_handler, task, u32
from test_menu_util import f32, trap

CAM, TARGET, OTHER = 0x61, 0x62, 0x63
CHANGED, DAC = 0x34, 0x32
TYPE, PARAMS = 0xA7B260, 0xA7B278


def find_by_id(e):
    i = e.arg(0)
    if LOG:
        LOG[-1].append(("find", i))
    e.ret(RET.get(("id", i), 0))


def obj_info(e):
    """Slot 7 handler: fills the info from RET['info'] = (pos, matrix, unk24)."""
    t, info = e.arg(0), e.arg(1)
    if LOG:
        LOG[-1].append(("info", e.normalize(t), e.read(info + 0x50, 4)))
    pos, m, n = RET.get("info", ((1.0, 2.0, 3.0), (1, 0, 0, 0, 1, 0, 0, 0, 1), 0))
    d64(e, info, *pos)
    u32(e, info + 0x18, 0x18181818, 0x1C1C1C1C, 0x20202020)
    f32(e, info + 0x24, *m)
    u32(e, info + 0x48, n)
    f32(e, info + 0x4C, 77.0)
    e.ret(0)


emu = make()
pyfake(emu, 0x4F1030, find_by_id)
logfake(emu, 0x4B8770, 1, strings=(0,), effect=lambda e, a: RET.get("eval", 5))  # Script_EvalExpr
INFO = trap(emu, "obj_info", obj_info)
set_handler(emu, 7, TARGET, INFO)
set_handler(emu, 7, CAM, INFO)
set_handler(emu, CHANGED, TARGET, handler(emu, "target_changed", 2, strings=(1,)))
set_handler(emu, CHANGED, OTHER, handler(emu, "other_changed", 2))
set_handler(emu, CHANGED, CAM, handler(emu, "cam_changed", 2))
set_handler(emu, DAC, CAM, handler(emu, "dac", 1))
emu.baseline = emu.snapshot()


def camera(e, fov=1.0, pos_attach=-1, rot_attach=-1, name=b"CAMERAFILTER_TYPE_LINE"):
    t = task(e, 0xE8, CAM)
    e.write(t + 0x20, bytes(range(0x30, 0x30 + 0xC8)))
    f32(e, t + 0x88, fov)
    u32(e, t + 0x8C, pos_attach, rot_attach)
    e.write(t + 0xA9, name + b"\0")
    return t


def test_init():
    run(emu, 0x507F10, "EditCamera_Init", lambda e: u32(e, 0xA7B280, 9, 9, 9, 9))


def test_create():
    def setup(e):
        t = task(e, 0xE8, CAM)
        e.write(t + 0x20, bytes(i & 0xFF for i in range(0x40, 0x40 + 0xC8)))
        return (t,)
    run(emu, 0x508290, "EditCamera_Create", setup)


@pytest.mark.parametrize("id_", [-1, 0, 5])
@pytest.mark.parametrize("refs", [(0, 0, 0), (1, 0, 2), (2, 2, 2), (0, 1, 0)])
def test_delete(id_, refs):
    def setup(e):
        RET[0x4F1050] = id_
        t = camera(e)
        for i, r in enumerate(refs):
            p = 0
            if r:
                p = e.alloc(4)
                u32(e, p, 0x7000 + i if r == 2 else 0)
            u32(e, t + 0x9C + 4 * i, p)
        return (t,)
    run(emu, 0x508350, "EditCamera_Delete", setup)


def test_get_params_shutdown_type():
    def setup(e):
        u32(e, PARAMS, 0x1234)
        e.write(TYPE, struct.pack("<H", CAM))
        return 0, e.alloc(4)
    run(emu, 0x508400, "EditCamera_GetParams", setup)
    run(emu, 0x5085E0, "EditCamera_Shutdown", setup)
    run(emu, 0x508610, "EditCamera_GetType", setup, check_eax=True, eax_mask=0xFFFF)
    run(emu, 0x508D60, "EditCamera_GetFilterEnum", setup, check_eax=True)


CHANGED_CASES = {
    "none": dict(),
    "fov_low": dict(fov=0.0001),
    "fov_neg": dict(fov=-2.0),
    "fov_high": dict(fov=7.5),
    "fov_max": dict(fov=3.0),
    "fov_nan": dict(fov=float("nan")),
    "self_rot": dict(rot=3, ids={3: "self"}),
    "other_rot": dict(rot=3, ids={3: "target"}),
    "pos_target": dict(pos=4, ids={4: "target"}),
    "pos_nohandler": dict(pos=4, ids={4: "other"}),
    "pos_missing": dict(pos=4),
    "both_self": dict(pos=4, rot=4, ids={4: "self"}),
    "arg": dict(pos=4, ids={4: "target"}, arg=b"x"),
    "arg_empty": dict(pos=4, ids={4: "target"}, arg=b""),
}


@pytest.mark.parametrize("children", [0, 1, 3])
@pytest.mark.parametrize("case", sorted(CHANGED_CASES))
def test_changed(case, children):
    c = CHANGED_CASES[case]

    def setup(e):
        RET["eval"] = 0x33
        RET["info"] = ((10.5, -20.25, 30.125), (0.5, 0.25, 0.125, 1, 2, 3, 4, 5, 6), 0x99)
        t = camera(e, c.get("fov", 1.0), c.get("pos", -1), c.get("rot", -1))
        tasks = {"self": t, "target": task(e, 0x40, TARGET), "other": task(e, 0x40, OTHER)}
        for i, name in c.get("ids", {}).items():
            RET[("id", i)] = tasks[name]
        for i in range(children):
            add_child(e, t, task(e, 0x40, [OTHER, CAM, TARGET][i]))
        arg = 0
        if "arg" in c:
            arg = e.alloc(4)
            e.write(arg, c["arg"] + b"\0")
        return t, arg
    run(emu, 0x508410, "EditCamera_Changed", setup)


ROT_CASES = [
    (1.0, 2.0, 3.0),
    (1000.5, 0.0, 0.0),
    (0.0, 0.0, 0.0),
    (0.0, 0.0, 5.0),
    (0.0, -7.0, 0.0),
    (0.0, 0.0, -1e-30),
    (-12345.678, 9876.54321, -0.001),
    (3.3e-5, 1.7e7, 2.25),
    (1e300, 1e300, 0.0),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("d", ROT_CASES)
def test_attach_rotation(d, cw):
    def setup(e):
        pos = e.alloc(24)
        d64(e, pos, 100.25, -200.5, 300.75)
        RET["info"] = ((100.25 + d[0], -200.5 + d[1], 300.75 + d[2]), (0,) * 9, 0)
        RET[("id", 9)] = task(e, 0x40, TARGET)
        rot = e.alloc(40)
        u32(e, rot, *range(0x100, 0x10A))
        return pos, rot, 9
    run(emu, 0x508620, "EditCamera_AttachRotation", setup, cw=cw)


@pytest.mark.parametrize("which", ["missing", "nohandler"])
def test_attach_none(which):
    def setup(e):
        if which == "nohandler":
            RET[("id", 9)] = task(e, 0x40, OTHER)
        pos, rot = e.alloc(24), e.alloc(40)
        return pos, rot, 9
    run(emu, 0x508620, "EditCamera_AttachRotation", setup)
    run(emu, 0x508940, "EditCamera_AttachPosition", lambda e: setup(e)[:2] + (e.alloc(24), e.alloc(40), 9))


POS_CASES = [
    ((1.0, 2.0, 3.0), (0.5, 0.25, 0.125), (0.36, 0.48, -0.8, -0.8, 0.6, 0.0, 0.48, 0.64, 0.6),
     (0.0, 1.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0)),
    ((-1234.5678, 98765.4321, -0.001), (77.7, -88.8, 99.9), (0.1, 0.7, -0.3, 0.9, 0.2, 0.4, -0.2, 0.5, 0.8),
     (0.6, -0.3, 0.7, 0.33, 0.91, -0.12, -0.75, 0.25, 0.61)),
    ((1e7, -3e6, 2.5e5), (1.0 / 3, 2.0 / 3, 1.0 / 7), (1 / 3, 1 / 7, 1 / 9, 1 / 11, 1 / 13, 1 / 17, 1 / 19, 1 / 23, 1 / 29),
     (1 / 31, 1 / 37, 1 / 41, 1 / 43, 1 / 47, 1 / 53, 1 / 59, 1 / 61, 1 / 67)),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(POS_CASES)))
def test_attach_position(case, cw):
    p, off, a, m = POS_CASES[case]

    def setup(e):
        RET["info"] = ((5000.125, -6000.25, 70.5), m, 0x40)
        RET[("id", 9)] = task(e, 0x40, TARGET)
        pos, rot, offp, offr = e.alloc(24), e.alloc(40), e.alloc(24), e.alloc(40)
        d64(e, pos, *p)
        d64(e, offp, *off)
        f32(e, rot, *[x * 0.9 + 0.01 * i for i, x in enumerate(a[::-1])])
        u32(e, rot + 36, 7)
        f32(e, offr, *a)
        u32(e, offr + 36, 0x10)
        return pos, rot, offp, offr, 9
    run(emu, 0x508940, "EditCamera_AttachPosition", setup, cw=cw)
