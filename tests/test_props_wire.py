import pytest

from emu import compare
from test_props_util import (RET, SCALES, d64, f32, handler, init_emu, logfake, make_emu, mat, rigid, run,
                             set_handler, setup_init, tricky, u32, vfake)

WIRE = 0x54
NAN = float("nan")


def model_point(e, a):
    d64(e, a[0], *RET["p%d" % a[2]])


def model_point_local(e, a):
    f32(e, a[0], *RET["l%d" % a[2]])


emu = make_emu()
vfake(emu, 0x414E20, 3, show=lambda e, a: (e.normalize(a[1]), a[2]), effect=model_point)
vfake(emu, 0x414ED0, 3, show=lambda e, a: (e.normalize(a[1]), a[2]), effect=model_point_local)
vfake(emu, 0x416AD0, 3, show=lambda e, a: (e.normalize(a[0]), e.normalize(a[1]), e.read(a[2], 24)))
logfake(emu, 0x4871C0, 2)
set_handler(emu, 0x25, WIRE, handler(emu, "25_wire", 1))
emu.baseline = emu.snapshot()

init = init_emu()


def wire(e, scale=(1.0, 2.0, 1.5), registered=0, in_use=1, last=0, m=None):
    t = rigid(e, 0x138, WIRE, scale)
    if m:
        mat(e, t + 0x70, m)
    d64(e, t + 0xF0, 100.0, 200.0, 300.0)
    d64(e, t + 0x108, 4196.0, -1848.0, 1324.0)
    e.write(t + 0x120, b"wire_01\0")
    e.write(t + 0x130, bytes([in_use, last, registered]))
    return t


def test_open():
    compare(init, 0x43EFF0, "Wire_Open", setup=setup_init, check_eax=False, normalized=True)


@pytest.mark.parametrize("scale", SCALES)
def test_create(scale):
    def setup(e):
        RET[0x4D0950] = 2.5
        u32(e, 0xA44344, 0x4434)
        return (wire(e, scale=scale, registered=1),)
    run(emu, 0x43F1F0, "Wire_Create", setup)


def test_run():
    run(emu, 0x43F340, "Wire_Run", lambda e: (wire(e),))


@pytest.mark.parametrize("id_", [-1, 0, 4])
@pytest.mark.parametrize("handled", [0, 1])
def test_delete(id_, handled):
    def setup(e):
        RET[0x4F1050] = id_
        if not handled:
            set_handler(e, 0x25, WIRE, 0)
        return (wire(e),)
    run(emu, 0x43F360, "Wire_Delete", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57C148, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x43F3C0, "Wire_GetParams", setup)


ENDS = [((100.0, 200.0, 300.0), (4196.0, -1848.0, 1324.0)), ((0.0, 0.0, 0.0), (0.0, 4096.0, 0.0)),
        ((1e15, 3.0, -2.0), (1e15 + 1.0, 2 ** -30, 7.0)), ((5.0, 5.0, 5.0), (5.0, 5.0, 5.0))]


@pytest.mark.parametrize("ends", ENDS)
@pytest.mark.parametrize("scale", SCALES[:3])
def test_params_changed(ends, scale):
    def setup(e):
        RET[0x4D0950] = 1.25
        t = wire(e, scale=scale)
        d64(e, t + 0xF0, *ends[0])
        d64(e, t + 0x108, *ends[1])
        return (t,)
    run(emu, 0x43F3D0, "Wire_ParamsChanged", setup)


POINTS = [((0.0, 0.0, 0.0), (4096.0, 100.0, 50.0)), ((10.0, 10.0, 10.0), (10.0, 10.0, 4000.0)),
          ((-5.0, 2.0, 1.0), (300.0, -2.0, 40.0)), ((0.0, 0.0, 0.0), (0.0, 1000.0, 0.0))]
LOCAL = [((0.0, -500.0, 0.0), (0.0, 500.0, 0.0)), ((0.0, 1e9, 0.0), (0.0, 2e9, 0.0)),
         ((0.0, -2e9, 0.0), (0.0, -1e9, 0.0)), ((0.0, NAN, 0.0), (0.0, NAN, 0.0))]
HUMANS = [(1100.0, -1900.0, 40.0), (1000.0, -2000.0, 30.5), (-3000.0, 50.0, 1e6)]


@pytest.mark.parametrize("pts", POINTS)
@pytest.mark.parametrize("local", LOCAL)
@pytest.mark.parametrize("human", HUMANS)
def test_use(pts, local, human):
    def setup(e):
        RET["p0"], RET["p1"] = pts
        RET["l0"], RET["l1"] = local
        q = e.alloc(0x88)
        u32(e, q, 0x7100)
        d64(e, q + 8, *human)
        return wire(e), q
    run(emu, 0x43F560, "Wire_Use", setup)


def test_get_use_kind():
    run(emu, 0x43F6C0, "Wire_GetUseKind", lambda e: (wire(e), e.alloc(4)))


def test_set_in_use():
    run(emu, 0x43F6D0, "Wire_SetInUse", lambda e: (wire(e, in_use=0),))


def test_draw_icon():
    def setup(e):
        p = e.alloc(4)
        u32(e, p, 0x7100)
        return wire(e), p
    run(emu, 0x43F6E0, "Wire_DrawIcon", setup)


@pytest.mark.parametrize("registered", [0, 1])
def test_activate(registered):
    run(emu, 0x43F700, "Wire_Activate", lambda e: (wire(e, registered=registered),))


@pytest.mark.parametrize("registered", [0, 1])
def test_deactivate(registered):
    run(emu, 0x43F780, "Wire_Deactivate", lambda e: (wire(e, registered=registered),))


def test_close():
    def setup(e):
        u32(e, 0x57C148, 0x5151)
        u32(e, 0x57C144, 0x12340044)
    run(emu, 0x43F800, "Wire_Close", setup)


@pytest.mark.parametrize("pts", POINTS)
def test_get_matrix(pts):
    def setup(e):
        RET["p0"], RET["p1"] = pts
        return e.alloc(40), wire(e)
    run(emu, 0x43F820, "Wire_GetMatrix", setup)


SLOPES = POINTS + [((0.0, 0.0, 0.0), (0.0, 1000.0, 30.0)), ((0.0, 0.0, 0.0), (0.0, 1000.0, -40.0)),
                   ((0.0, 0.0, 0.0), (0.0, -1000.0, 10.0)), ((0.0, 0.0, 0.0), (0.0, 1000.0, 34.9))]


@pytest.mark.parametrize("pts", SLOPES)
def test_is_sloped(pts):
    def setup(e):
        RET["p0"], RET["p1"] = pts
        return (wire(e),)
    run(emu, 0x43F8A0, "Wire_IsSloped", setup, check_eax=True, eax_mask=0xFF)
