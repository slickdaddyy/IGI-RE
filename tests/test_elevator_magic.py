"""SimpointObjGenerator and ExplodeMagicObj."""
import pytest

from emu import ErrorShown, compare
from test_elevator_util import (RET, add_child, d64, f32, floatfake, init_emu, logfake, make_emu, obj, pyfake, run, setup_types,
                                strz, task, u16, u32)

GEN, EXP, CHILD = 0x68, 0x69, 0x6A

emu = make_emu((GEN, EXP, CHILD))


def get_arg_string(e):
    args, i, out, size = (e.arg(k) for k in range(4))
    e.write(out, (b"arg%d_%x" % (i, args))[:size - 1] + b"\0")
    e.ret(0)


pyfake(emu, 0x4B8A80, get_arg_string)
logfake(emu, 0x4B8A20, 2, effect=lambda e, a: a[1] * 7 - 3)  # Script_GetArgInt
floatfake(emu, 0x4B8A50, 2, effect=lambda e, a: a[1] * 1.25 + 0.1)  # Script_GetArgReal
u16(emu, 0xA7A62A, 0x77)
emu.baseline = emu.snapshot()
init = init_emu()


@pytest.mark.parametrize("orig,ours", [(0x43B850, "SimpointObjGenerator_Init"), (0x43BFD0, "ExplodeMagicObj_Init")])
def test_init(orig, ours):
    compare(init, orig, ours, setup=setup_types, check_eax=False, normalized=True)


def generator(e, timer=0, group=0, model=b"crate"):
    t = obj(e, 0x158, GEN)
    u32(e, t + 0xF0, timer)
    strz(e, t + 0xF4, model)
    strz(e, t + 0x104, b"simp_a")
    strz(e, t + 0x114, b"simp_b")
    u32(e, t + 0x124, 12, 7)
    f32(e, t + 0x12C, 0.75, 0.5, 300.0)
    u32(e, t + 0x138, group)
    f32(e, t + 0x13C, 0.1, -0.3, 0.9, 1.0, 2.0, 3.0)
    return t


@pytest.mark.parametrize("scale", [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 2.0, 2.5)])
def test_generator_create(scale):
    def setup(e):
        RET[0x4D0950] = 2.75
        t = generator(e, timer=4, group=5)
        f32(e, t + 0x98, *scale)
        return (t,)
    run(emu, 0x43BB20, "SimpointObjGenerator_Create", setup)


@pytest.mark.parametrize("timer", [-1, 0, 1, 5])
@pytest.mark.parametrize("group", [0, 0x4545])
def test_generator_run(timer, group):
    def setup(e):
        RET[0x503C00] = 0x7777
        return (generator(e, timer=timer, group=group),)
    run(emu, 0x43BCA0, "SimpointObjGenerator_Run", setup)


@pytest.mark.parametrize("found", [0, 0x3333])
@pytest.mark.parametrize("model", [b"crate", b"x*", b"", b"long_model_name"])
def test_generator_changed(found, model):
    def setup(e):
        RET[0x4CEC10] = found
        return (generator(e, timer=9, model=model),)
    run(emu, 0x43BDA0, "SimpointObjGenerator_Changed", setup)


@pytest.mark.parametrize("orig,ours,glob", [(0x43BE50, "SimpointObjGenerator_GetParams", 0x57BF60)])
def test_get_params(orig, ours, glob):
    def setup(e):
        u32(e, glob, 0x9876)
        return 0, e.alloc(4)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("orig,ours", [(0x43BE60, "SimpointObjGenerator_Close"), (0x43C380, "ExplodeMagicObj_Close")])
def test_close(orig, ours):
    def setup(e):
        u32(e, 0x57BF60, 0x9C9C)
        u16(e, 0x53C31C, 0x33)
        u16(e, 0x57BF7C, 0x44)
    run(emu, orig, ours, setup)


def params(e):
    p = e.alloc(0x3C)
    strz(e, p, b"wreck")
    f32(e, p + 0x10, 50.0, 3.0, 1.5, 2.25, 9.0)
    u32(e, p + 0x24, 4, 6)
    strz(e, p + 0x2C, b"boom")
    return p


def explode(e, damage=0.0, health=50.0, exploded=0, children=0):
    t = obj(e, 0x150, EXP)
    u32(e, t + 0x138, params(e))
    f32(e, t + 0x13C, damage, health, 2.25)
    e.write(t + 0x148, bytes([exploded]))
    for i in range(children):
        add_child(e, t, task(e, 0x40, CHILD if i else EXP))
    return t


def create_arg(e):
    a = e.alloc(0x90)
    u32(e, a, 0, 0x44)
    d64(e, a + 8, 5.0, 6.0, 7.0, 15.0, 25.0, -35.0)
    f32(e, a + 0x38, *[0.1 * i for i in range(9)])
    u32(e, a + 0x5C, 3)
    f32(e, a + 0x60, *[-0.2 * i for i in range(9)])
    u32(e, a + 0x84, 4)
    u32(e, a + 0x88, 0x8888, params(e))
    return a


@pytest.mark.parametrize("found", [0, 0x3333])
def test_explode_create(found):
    def setup(e):
        RET[0x4CEC10] = found
        RET[0x4D0950] = 4.5
        a = create_arg(e)
        name = e.alloc(16)
        strz(e, name, b"barrel")
        u32(e, a, name)
        return explode(e, damage=3.0, exploded=1), a
    if found:
        run(emu, 0x43BE80, "ExplodeMagicObj_Create", setup)
    else:
        with pytest.raises(ErrorShown):
            run(emu, 0x43BE80, "ExplodeMagicObj_Create", setup)


@pytest.mark.parametrize("damage,health", [(0.0, 50.0), (49.9, 50.0), (50.0, 50.0), (51.0, 50.0),
                                           (float("nan"), 50.0), (60.0, float("nan"))])
@pytest.mark.parametrize("exploded", [0, 1])
@pytest.mark.parametrize("children", [0, 2])
def test_explode_run(damage, health, exploded, children):
    def setup(e):
        RET[0x4CEC10] = 0x3333
        return (explode(e, damage=damage, health=health, exploded=exploded, children=children),)
    run(emu, 0x43C0B0, "ExplodeMagicObj_Run", setup)


def test_explode_run_no_model():
    def setup(e):
        RET[0x4CEC10] = 0
        return (explode(e, damage=60.0),)
    with pytest.raises(ErrorShown):
        run(emu, 0x43C0B0, "ExplodeMagicObj_Run", setup)


@pytest.mark.parametrize("index", [0, 3])
def test_parse_args(index):
    def setup(e):
        a = e.alloc(12)
        u32(e, a, e.alloc(0x3C), 0x5151, index)
        return 0, a
    run(emu, 0x43C1F0, "ExplodeMagicObj_ParseArgs", setup)


def test_get_arg_size():
    run(emu, 0x43C2A0, "ExplodeMagicObj_GetArgSize", lambda e: (0, e.alloc(4)))


@pytest.mark.parametrize("add", [0.0, 1.5, 1e-9, -2.0])
def test_hit(add):
    def setup(e):
        a = e.alloc(8)
        f32(e, a + 4, add)
        return explode(e, damage=3.1), a
    run(emu, 0x43C2B0, "ExplodeMagicObj_Hit", setup)


@pytest.mark.parametrize("f", [0.0, 0.5, 1e-9, 12345.678])
@pytest.mark.parametrize("exploded", [0, 1])
def test_blast(f, exploded):
    def setup(e):
        RET[0x416C90] = f
        RET[0x4D0950] = 7.5
        return explode(e, damage=3.1, exploded=exploded), 0x4242
    run(emu, 0x43C2D0, "ExplodeMagicObj_Blast", setup)
