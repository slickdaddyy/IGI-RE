import struct

import pytest

from test_props_util import (EV_MOVE, LOG, RET, SCALES, add_child, d64, f32, floatfake, handler, logfake, make_emu,
                             mat, rigid, run, set_handler, task, tricky, u32, vfake)

ELEV, CHILD = 0x50, 0x51
NAN = float("nan")


def path_node(e, a):
    out, path, i = a
    d64(e, out, *([10.0, -20.0, 5.5] if i == 0 else [110.25, 40.0, -7.75]))
    return 0


def path_point(e, a):
    path, m, pos, found = a[:4]
    f32(e, m, *RET["matrix"])
    u32(e, m + 0x24, 9)
    d64(e, pos, 1.5, 2.5, 3.5)
    e.write(found, bytes([RET["found"]]))
    return 0


emu = make_emu()
floatfake(emu, 0x5020C0, 1)  # Path_GetLength
logfake(emu, 0x502130, 1, ret=5)  # Path_GetNodeCount
vfake(emu, 0x502200, 3, show=lambda e, a: (a[1], a[2]), effect=path_node)  # Path_GetNodePos
vfake(emu, 0x4FEC10, 8, show=lambda e, a: (a[0], e.normalize(a[2]), a[4], a[5], a[6], e.read(a[7], 12)),
      effect=path_point)  # Path_GetPoint
logfake(emu, 0x4E6C00, 1)  # Sound_Stop
for t in (ELEV, CHILD):
    for slot, n in ((1, 1), (0x25, 1), (EV_MOVE, 2)):
        set_handler(emu, slot, t, handler(emu, "%x_%x" % (slot, t), n, derefs={1: 8}))
emu.baseline = emu.snapshot()


def elevator(e, path=7, dist=5.0, start=3.0, linear=0, keep=0, scale=(1.0, 2.0, 1.5), base=None, last=None,
             sound=0, registered=0):
    t = rigid(e, 0x648, ELEV, scale)
    d64(e, t + 0xF0, 1.0, 2.0, 3.0)
    d64(e, t + 0x108, 900.0, -1999.5, 30.0)
    mat(e, t + 0x120, base, unk=4, seed=1)
    mat(e, t + 0x148, last, unk=6, seed=2)
    e.write(t + 0x170, b"lift_01\0")
    u32(e, t + 0x180, path)
    f32(e, t + 0x184, dist, start)
    e.write(t + 0x18C, bytes([linear, keep]))
    u32(e, t + 0x190, 0x190)
    u32(e, t + 0x1A0, *struct.unpack("<4I", struct.pack("<fIfI", 1.5, 0x1A4, 2.5, 0x1AC)))
    e.write(t + 0x1C0, bytes([registered]))
    e.write(t + 0x5D6, b"\1\1\1")
    u32(e, t + 0x63C, sound)
    return t


PATH = [dict(), dict(path=-1), dict(dist=12.0), dict(dist=10.0), dict(dist=-3.0), dict(dist=NAN),
        dict(linear=1), dict(linear=1, dist=11.0), dict(keep=1), dict(found=0)]


@pytest.mark.parametrize("kw", PATH)
@pytest.mark.parametrize("scale", SCALES[:4])
def test_move(kw, scale):
    def setup(e):
        RET[0x5020C0] = 10.0
        RET[0x4D0950] = 3.25
        RET["matrix"] = [0.5, -0.25, 0.75, 1.0, 0.125, -0.5, 0.3, 0.6, -0.9]
        RET["found"] = kw.get("found", 1)
        return (elevator(e, scale=scale, **{k: v for k, v in kw.items() if k != "found"}),)
    run(emu, 0x43D960, "Elevator_UpdatePlace", setup)


@pytest.mark.parametrize("m", tricky())
@pytest.mark.parametrize("side", ["base", "path"])
def test_move_order(m, side):
    ones = [1.0] * 9

    def setup(e):
        RET[0x5020C0] = 10.0
        RET["matrix"] = m[1] if side == "path" else ones
        RET["found"] = 1
        return (elevator(e, base=ones if side == "path" else m[1]),)
    run(emu, 0x43D960, "Elevator_UpdatePlace", setup)


@pytest.mark.parametrize("sound", [0, 0x6000])
@pytest.mark.parametrize("id_", [-1, 0, 3])
@pytest.mark.parametrize("handled", [0, 1])
def test_delete(sound, id_, handled):
    def setup(e):
        RET[0x4F1050] = id_
        if not handled:
            set_handler(e, 0x25, ELEV, 0)
        return (elevator(e, sound=sound),)
    run(emu, 0x43DCF0, "Elevator_DeleteHandler", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57C0E0, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x43DDA0, "Elevator_GetParams", setup)


@pytest.mark.parametrize("start", [3.0, -1.0, 10.0, 12.0, NAN])
@pytest.mark.parametrize("arg", [None, b"", b"x"])
def test_params_changed(start, arg):
    def setup(e):
        RET[0x5020C0] = 10.0
        RET[0x4D0950] = 2.0
        RET["matrix"] = [0.5, -0.25, 0.75, 1.0, 0.125, -0.5, 0.3, 0.6, -0.9]
        RET["found"] = 1
        a = 0
        if arg is not None:
            a = e.alloc(4)
            e.write(a, arg + b"\0")
        return elevator(e, start=start), a
    run(emu, 0x43DDB0, "Elevator_ParamsChanged", setup)


@pytest.mark.parametrize("registered", [0, 1])
def test_activate(registered):
    run(emu, 0x43DF20, "Elevator_Activate", lambda e: (elevator(e, registered=registered),))


@pytest.mark.parametrize("registered", [0, 1])
def test_deactivate(registered):
    run(emu, 0x43E0A0, "Elevator_Deactivate", lambda e: (elevator(e, registered=registered),))


@pytest.mark.parametrize("children", [0, 1, 3])
def test_move_handler(children):
    def setup(e):
        RET[0x5020C0] = 10.0
        RET[0x4D0950] = 2.0
        RET["matrix"] = [0.5, -0.25, 0.75, 1.0, 0.125, -0.5, 0.3, 0.6, -0.9]
        RET["found"] = 1
        t = elevator(e)
        for i in range(children):
            add_child(e, t, task(e, 0x40, CHILD if i % 2 else ELEV))
        return (t,)
    run(emu, 0x43E1F0, "Elevator_MoveHandler", setup)


@pytest.mark.parametrize("m", [None] + tricky())
@pytest.mark.parametrize("side", ["obj", "last"])
def test_get_motion(m, side):
    ones = [1.0] * 9

    def setup(e):
        t = elevator(e, last=(m[1] if side == "last" else ones) if m else None)
        if m and side == "obj":
            mat(e, t + 0x70, m[1])
        elif m:
            mat(e, t + 0x70, ones)
        return t, e.alloc(12)
    run(emu, 0x43E290, "Elevator_GetMotion", setup)


def test_close():
    def setup(e):
        u32(e, 0x57C0E0, 0x5151)
        u32(e, 0x57C108, 0x12340044)
    run(emu, 0x43E4C0, "Elevator_Close", setup)


def test_get_type():
    def setup(e):
        u32(e, 0x57C108, 0x12340044)
    run(emu, 0x43E4E0, "Elevator_GetType", setup, check_eax=True, eax_mask=0xFFFF)
