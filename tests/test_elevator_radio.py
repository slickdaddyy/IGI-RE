import pytest

from emu import compare
from test_elevator_util import RET, d64, f32, init_emu, make_emu, run, setup_types, strz, task, u16, u32

RADIO, GEN, BASE = 0x66, 0x67, 0x65

emu = make_emu((RADIO, GEN, BASE))
u16(emu, 0x54097C, BASE)
emu.baseline = emu.snapshot()
init = init_emu()


def radio(e, on=0, registered=0, dead=0, timer=0, sound=None):
    t = task(e, 0x2E8, RADIO)
    d64(e, t + 0x20, 1.5, 2.5, -3.5)
    e.write(t + 0x190, bytes([dead]))
    u32(e, t + 0x23C, timer)
    e.write(t + 0x240, bytes([on, registered]))
    for i in range(4):
        strz(e, t + 0x294 + 16 * i, b"radio_%d" % i)
    strz(e, t + 0x2D4, b"radio_off")
    if sound is not None:
        ref = e.alloc(4)
        u32(e, ref, sound)
        u32(e, t + 0x238, ref)
    return t


def generator(e, on=0, registered=0, dead=0, sound=0, backup=1, timer=-1, on_sound=b"hum"):
    t = task(e, 0x2C0, GEN)
    d64(e, t + 0x20, 1.5, 2.5, -3.5)
    e.write(t + 0x190, bytes([dead]))
    u32(e, t + 0x238, sound)
    e.write(t + 0x23C, bytes([on, registered]))
    strz(e, t + 0x290, on_sound)
    strz(e, t + 0x2A0, b"gen_off")
    f32(e, t + 0x2B0, 2.55)
    e.write(t + 0x2B4, bytes([backup]))
    u32(e, t + 0x2B8, timer)
    return t


@pytest.mark.parametrize("orig,ours", [(0x43AD80, "Radio_Init"), (0x43B2F0, "Generator_Init")])
def test_init(orig, ours):
    compare(init, orig, ours, setup=setup_types, check_eax=False, normalized=True)


def test_radio_create():
    run(emu, 0x43AF20, "Radio_Create", lambda e: (radio(e, on=1, registered=1, timer=5, sound=3), 0x1234))


@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("dead", [0, 1])
@pytest.mark.parametrize("has,value", [(0, 0.0), (1, 0.0), (1, 1.0)])
@pytest.mark.parametrize("sound", [None, 0, 0x6000])
@pytest.mark.parametrize("timer", [0, 3])
@pytest.mark.parametrize("new", [0, 0x6100])
def test_radio_run(on, dead, has, value, sound, timer, new):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        RET[0x4E6B00] = new
        RET[(0x4B47C0, 4)] = 2
        RET[(0x4B47C0, 150)] = 77
        return (radio(e, on=on, dead=dead, sound=sound, timer=timer),)
    run(emu, 0x43AF90, "Radio_Run", setup)


@pytest.mark.parametrize("sound", [None, 0x6000])
def test_radio_delete(sound):
    run(emu, 0x43B110, "Radio_Delete", lambda e: (radio(e, sound=sound),))


@pytest.mark.parametrize("orig,ours,glob", [(0x43B160, "Radio_GetParams", 0x57BEF8),
                                            (0x43B680, "Generator_GetParams", 0x57BF3C)])
def test_get_params(orig, ours, glob):
    def setup(e):
        u32(e, glob, 0x9876)
        return 0, e.alloc(4)
    run(emu, orig, ours, setup)


def test_radio_changed():
    run(emu, 0x43B170, "Radio_Changed", lambda e: (radio(e, on=1, timer=9), 0x4321))


@pytest.mark.parametrize("orig,ours", [(0x43B1C0, "Radio_Activate"), (0x43B250, "Radio_Deactivate")])
@pytest.mark.parametrize("registered", [0, 1])
def test_radio_activate(orig, ours, registered):
    run(emu, orig, ours, lambda e: (radio(e, registered=registered),))


@pytest.mark.parametrize("orig,ours,glob,tglob", [(0x43B2D0, "Radio_Close", 0x57BEF8, 0x53C2E0),
                                                  (0x43B830, "Generator_Close", 0x57BF3C, 0x53C2FC)])
def test_close(orig, ours, glob, tglob):
    def setup(e):
        u32(e, glob, 0x9C9C)
        u16(e, tglob, 0x33)
    run(emu, orig, ours, setup)


def test_generator_create():
    run(emu, 0x43B490, "Generator_Create", lambda e: (generator(e, on=1, registered=1, sound=5, backup=0), 0x1234))


@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("dead,has,value", [(1, 1, 1.0), (0, 0, 0.0), (0, 1, 0.0), (0, 1, 1.0)])
@pytest.mark.parametrize("sound", [0, 0x6000])
@pytest.mark.parametrize("backup,timer", [(0, -1), (1, -1), (1, 0), (1, 1), (1, 40), (1, -5)])
@pytest.mark.parametrize("new,on_sound", [(0, b"hum"), (0x6100, b"hum"), (0x6100, b"")])
def test_generator_run(on, dead, has, value, sound, backup, timer, new, on_sound):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        RET[0x4E6B00] = new
        return (generator(e, on=on, dead=dead, sound=sound, backup=backup, timer=timer, on_sound=on_sound),)
    run(emu, 0x43B500, "Generator_Run", setup)


@pytest.mark.parametrize("orig,ours", [(0x43B650, "Generator_Delete")])
def test_generator_delete(orig, ours):
    run(emu, orig, ours, lambda e: (generator(e, sound=7),))


def test_generator_changed():
    run(emu, 0x43B690, "Generator_Changed", lambda e: (generator(e, on=1), 0x4321))


@pytest.mark.parametrize("orig,ours", [(0x43B6E0, "Generator_Activate"), (0x43B790, "Generator_Deactivate")])
@pytest.mark.parametrize("registered", [0, 1])
def test_generator_activate(orig, ours, registered):
    run(emu, orig, ours, lambda e: (generator(e, registered=registered),))
