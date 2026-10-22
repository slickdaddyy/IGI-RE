import pytest

from test_elevator_util import RET, add_child, make_emu, obj, run, set_handler, strz, task, u32

AL, CHILD = 0x60, 0x61

emu = make_emu((AL, CHILD))


def light(e, on=0, dead=0, registered=0, sound=0, attached=None, children=0, scale=(1.0, 2.0, 1.5)):
    t = obj(e, 0x1F8, AL, scale=scale)
    strz(e, t + 0x130, b"lamp_off")
    strz(e, t + 0x140, b"lamp_on")
    strz(e, t + 0x150, b"lamp_dead")
    strz(e, t + 0x160, b"alarm.wav")
    strz(e, t + 0x180, b"glass.wav")
    e.write(t + 0x1A0, bytes([on, dead, registered]))
    u32(e, t + 0x1F4, sound)
    if attached:
        u32(e, t + 0xE8, attached)
    for i in range(children):
        add_child(e, t, task(e, 0x40, CHILD if i else AL))
    return t


def test_close():
    def setup(e):
        u32(e, 0x57BE9C, 0x9C9C)
        u32(e, 0x57BEA0, 0x00440033)
    run(emu, 0x43A810, "AlarmLight_Close", setup)


@pytest.mark.parametrize("scale", [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 2.0, 2.5), (2.0, 2.0, 2.0)])
def test_create(scale):
    def setup(e):
        RET[0x4D0950] = 12.3456789
        return (light(e, on=1, dead=1, registered=1, sound=5, scale=scale),)
    run(emu, 0x43A0E0, "AlarmLight_Create", setup)


@pytest.mark.parametrize("on", [0, 1])
@pytest.mark.parametrize("dead", [0, 1])
@pytest.mark.parametrize("sound", [0, 0x6000])
@pytest.mark.parametrize("has,value", [(0, 0.0), (1, 0.0), (1, 1.0), (1, 2.6)])
@pytest.mark.parametrize("children", [0, 2])
def test_run(on, dead, sound, has, value, children):
    def setup(e):
        RET[0x4F16C0] = has
        RET[0x4F16E0] = value
        RET[0x4E6B00] = 0x6100
        att = task(e, 0x40, CHILD) if children else None
        return (light(e, on=on, dead=dead, sound=sound, attached=att, children=children),)
    run(emu, 0x43A270, "AlarmLight_Run", setup)


@pytest.mark.parametrize("id_", [-1, 3])
@pytest.mark.parametrize("handled", [0, 1])
@pytest.mark.parametrize("attached", [0, 1])
def test_delete(id_, handled, attached):
    def setup(e):
        RET[0x4F1050] = id_
        if not handled:
            set_handler(e, 0x25, AL, 0)
        return (light(e, attached=task(e, 0x40, CHILD) if attached else None),)
    run(emu, 0x43A3C0, "AlarmLight_Delete", setup)


def test_get_params():
    def setup(e):
        u32(e, 0x57BE9C, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x43A440, "AlarmLight_GetParams", setup)


@pytest.mark.parametrize("arg", [None, b"", b"x"])
@pytest.mark.parametrize("attached", [0, 1])
@pytest.mark.parametrize("children", [0, 1, 3])
def test_changed(arg, attached, children):
    def setup(e):
        new = task(e, 0x40, AL)
        for i in range(children):
            add_child(e, new, task(e, 0x40, CHILD if i % 2 else AL))
        RET[0x4E0930] = new
        RET[0x4D0950] = 3.25
        p = 0 if arg is None else e.alloc(4)
        if arg is not None:
            strz(e, p, arg)
        return light(e, attached=task(e, 0x40, CHILD) if attached else None), p
    run(emu, 0x43A450, "AlarmLight_Changed", setup)


@pytest.mark.parametrize("orig,ours", [(0x43A5E0, "AlarmLight_Hit"), (0x43A5F0, "AlarmLight_Destroy")])
@pytest.mark.parametrize("dead", [0, 1])
def test_destroy(orig, ours, dead):
    run(emu, orig, ours, lambda e: (light(e, dead=dead),))


@pytest.mark.parametrize("f", [0.0, -0.0, 0.5, -1.0, 1e-30, float("nan")])
@pytest.mark.parametrize("dead", [0, 1])
def test_blast(f, dead):
    def setup(e):
        RET[0x416C90] = f
        RET[0x4D0950] = 7.5
        return light(e, dead=dead), 0x4242
    run(emu, 0x43A660, "AlarmLight_Blast", setup)


@pytest.mark.parametrize("orig,ours", [(0x43A710, "AlarmLight_Activate"), (0x43A790, "AlarmLight_Deactivate")])
@pytest.mark.parametrize("registered", [0, 1])
def test_activate(orig, ours, registered):
    run(emu, orig, ours, lambda e: (light(e, registered=registered),))
