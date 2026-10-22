import pytest

from emu import compare
from test_props_util import (RET, SCALES, add_child, d64, f32, handler, init_emu, logfake, make_emu, mat, rigid, run,
                             set_handler, setup_init, task, u32, vfake)

FENCE, CHILD, SPARK, HUMAN = 0x55, 0x56, 0x57, 0x58
EV12 = 0x35
NAN = float("nan")
IDENT = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]


def model_point_local(e, a):
    f32(e, a[0], *RET["l%d" % a[2]])


emu = make_emu()
vfake(emu, 0x414ED0, 3, show=lambda e, a: (e.normalize(a[1]), a[2]), effect=model_point_local)
vfake(emu, 0x44C830, 1, effect=lambda e, a: RET["spark"])
vfake(emu, 0x44C860, 5, show=lambda e, a: (e.normalize(a[0]), e.read(a[1], 24), e.normalize(a[2]), e.read(a[3], 24),
                                           e.read_cstr(a[4])))
logfake(emu, 0x416870, 0, ret=EV12)  # Game_GetEvent12
logfake(emu, 0x4871C0, 2)
for t in (FENCE, CHILD):
    set_handler(emu, 1, t, handler(emu, "1_%x" % t, 1))
set_handler(emu, 0x31, SPARK, handler(emu, "31_spark", 2))
set_handler(emu, EV12, HUMAN, handler(emu, "ev12_human", 2))
emu.baseline = emu.snapshot()

init = init_emu()


def fence(e, scale=(1.0, 2.0, 1.5), on=0, spark=0, m=None):
    t = rigid(e, 0x178, FENCE, scale)
    if m:
        mat(e, t + 0x70, m)
    d64(e, t + 0xF0, 11.0, 12.0, 13.0)
    f32(e, t + 0x108, 0.7)
    e.write(t + 0x10C, b"fence_02\0")
    e.write(t + 0x11C, bytes([on]))
    u32(e, t + 0x170, spark)
    return t


def test_open():
    compare(init, 0x43F900, "Fence_Open", setup=setup_init, check_eax=False, normalized=True)


@pytest.mark.parametrize("scale", SCALES[:3])
def test_create(scale):
    def setup(e):
        RET[0x4D0950] = 2.5
        return (fence(e, scale=scale, on=1, spark=0x6000),)
    run(emu, 0x43FAE0, "Fence_Create", setup)


@pytest.mark.parametrize("has,value", [(0, 0.0), (1, 0.0), (1, 1.0), (1, 0.4), (1, 2.6), (1, 256.0)])
@pytest.mark.parametrize("spark", [0, 1])
@pytest.mark.parametrize("handled", [0, 1])
@pytest.mark.parametrize("m", [None, IDENT])
@pytest.mark.parametrize("children", [0, 2])
def test_run(has, value, spark, handled, m, children):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        RET["l0"], RET["l1"] = (-2048.5, 3.0, 4.0), (6100.25, -1.0, 2.0)
        s = task(e, 0x40, SPARK)
        RET["spark"] = s
        if not handled:
            set_handler(e, 0x31, SPARK, 0)
        t = fence(e, spark=s if spark else 0, m=m)
        for i in range(children):
            add_child(e, t, task(e, 0x40, CHILD if i else FENCE))
        return (t,)
    run(emu, 0x43FC30, "Fence_Run", setup)


def test_delete():
    run(emu, 0x43FE50, "Fence_Delete", lambda e: (fence(e),))


def test_get_params():
    def setup(e):
        u32(e, 0x57C150, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x43FE90, "Fence_GetParams", setup)


@pytest.mark.parametrize("model", [0, 0x3300])
@pytest.mark.parametrize("scale", SCALES[:3])
def test_params_changed(model, scale):
    def setup(e):
        RET[0x4CEC10] = model
        RET[0x4D0950] = 1.5
        return (fence(e, scale=scale),)
    run(emu, 0x43FEA0, "Fence_ParamsChanged", setup)


# offsets from the fence position, in fence space for the identity matrix
TOUCH = [(0.0, 0.0, 100.0), (-3300.0, 0.0, 100.0), (-3200.0, 0.0, 100.0), (7400.0, 0.0, 100.0), (7300.0, 0.0, 100.0),
         (0.0, 1300.0, 100.0), (0.0, -1200.0, 100.0), (0.0, -1300.0, 100.0), (0.0, 0.0, 12288.0), (0.0, 0.0, 12000.0),
         (0.0, 0.0, 0.0), (0.0, 0.0, -1.0), (NAN, 0.0, 100.0), (0.0, NAN, 100.0), (0.0, 0.0, NAN)]


@pytest.mark.parametrize("d", TOUCH)
@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("m", [IDENT, None])
def test_touch(d, on, m):
    def setup(e):
        RET["l0"], RET["l1"] = (-2048.5, 3.0, 4.0), (6100.25, -1.0, 2.0)
        human = task(e, 0x40, HUMAN)
        msg = e.alloc(0x20)
        u32(e, msg, human, 0x4444)
        d64(e, msg + 8, 11.0 + d[0], 12.0 + d[1], 13.0 + d[2])
        t = fence(e, on=on, m=m)
        d64(e, t + 0x20, 11.0, 12.0, 13.0)
        return t, msg
    run(emu, 0x43FF70, "Fence_Touch", setup)


USE = [(0.0, -500.0, 0.0), (0.0, 500.0, 0.0), (0.0, 1600.0, 0.0), (-2100.0, 300.0, 0.0), (6200.0, 300.0, 0.0),
       (100.0, -300.0, 5.0), (NAN, 300.0, 0.0), (0.0, NAN, 0.0)]
FACING = [(0.0, 1.0, 0.0), (0.0, -1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.7, 0.7), (0.0, 0.4, 0.9)]


@pytest.mark.parametrize("d", USE)
@pytest.mark.parametrize("face", FACING)
@pytest.mark.parametrize("on", [0, 1])
def test_use(d, face, on):
    def setup(e):
        RET["l0"], RET["l1"] = (-2048.5, 3.0, 4.0), (6100.25, -1.0, 2.0)
        q = e.alloc(0x88)
        u32(e, q, 0x7100)
        d64(e, q + 8, 11.0 + d[0], 12.0 + d[1], 13.0 + d[2])
        f32(e, q + 0x20, 1.0, face[0], 0.0, 0.0, face[1], 0.0, 0.0, face[2], 1.0)
        f32(e, q + 0x70, 9.0, 9.0)
        d64(e, q + 0x78, 1500.0)
        u32(e, q + 0x80, 0x8080)
        t = fence(e, on=on, m=IDENT)
        d64(e, t + 0x20, 11.0, 12.0, 13.0)
        return t, q
    run(emu, 0x4400C0, "Fence_Use", setup)


def test_draw_icon():
    def setup(e):
        p = e.alloc(4)
        u32(e, p, 0x7100)
        return fence(e), p
    run(emu, 0x440230, "Fence_DrawIcon", setup)


def test_close():
    def setup(e):
        u32(e, 0x57C150, 0x5151)
        u32(e, 0x57C154, 0x12340044)
    run(emu, 0x440250, "Fence_Close", setup)
