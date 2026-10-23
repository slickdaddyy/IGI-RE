"""SkyTask, LevelScript, SimBoneDynCubeObj, PushButton (0x52C6B0 - 0x52CB20)."""
import struct

import pytest

from test_terredit_util import LOG, RET, add_child, f32, handler, make, run, set_handler, task, trap, u16, u32, u8, world

SKY_TYPE, SKY_PARAMS = 0x54F6E4, 0xA84558
LS_TYPE, LS_EVENT, LS_PARAMS = 0x54F6F0, 0x54F6F2, 0xA84564
SIMBONE_TYPE, BUTTON_TYPE = 0x54F700, 0x54F718
LS_SLOT, CHANGED = 0x3A, 0x34


def step(e):
    """LevelScript event handler: logs the task and the step; marks it done while
    RET['done'] has entries left."""
    t, s = e.arg(0), e.arg(1)
    value, done = struct.unpack("<IB", e.read(s, 5))
    if LOG:
        LOG[-1].append(("step", e.read(t + 0x20, 4), value, done))
    left = RET.get("done", 0)
    if left:
        RET["done"] = left - 1
        e.write(s + 4, b"\x01")
    e.ret(0)


emu = make()
for typ in (0x70, 0x71):
    set_handler(emu, LS_SLOT, typ, trap(emu, "step", step))
set_handler(emu, CHANGED, 0x70, handler(emu, "changed", 2))
emu.baseline = emu.snapshot()


# ---- SkyTask ----

def test_sky_init():
    run(emu, 0x52C6B0, "SkyTask_Init")


def test_sky_handlers():
    def setup(e):
        world(e)
        u32(e, SKY_PARAMS, 0x1234)
        u16(e, SKY_TYPE, 0x66)
        t = task(e, 0x78, 0x66)
        e.write(t + 0x20, bytes(range(0x58)))
        f32(e, t + 0x6C, 1.25, -7.5)
        return t, e.alloc(4)
    run(emu, 0x52C790, "SkyTask_GetParams", setup)
    run(emu, 0x52C7A0, "SkyTask_Render", setup)
    run(emu, 0x52C7C0, "SkyTask_Create", setup)
    run(emu, 0x52C7F0, "SkyTask_Shutdown", setup)


# ---- LevelScript ----

def test_level_script_init():
    run(emu, 0x52C820, "LevelScript_Init")


def test_level_script_small():
    def setup(e):
        u32(e, LS_PARAMS, 0x1234)
        u16(e, LS_TYPE, 0x67)
        u8(e, LS_EVENT, 0x21)
        t = task(e, 0x28, 0x67)
        u32(e, t + 0x20, 5, 0x9999)
        return t, e.alloc(4)
    run(emu, 0x52C8E0, "LevelScript_GetParams", setup)
    run(emu, 0x52C8F0, "LevelScript_Create", setup)
    run(emu, 0x52CA00, "LevelScript_Shutdown", setup)


@pytest.mark.parametrize("children", [0, 1, 4])
@pytest.mark.parametrize("done", [0, 1, 2, 5])
@pytest.mark.parametrize("started", [0, 1])
def test_level_script_run(started, done, children):
    def setup(e):
        RET["done"] = done
        u8(e, LS_EVENT, LS_SLOT)
        t = task(e, 0x28, 0x67)
        kids = []
        for i in range(children):
            c = task(e, 0x30, 0x70 if i % 2 == 0 else 0x71)
            u32(e, c + 0x20, 0x100 + i)
            add_child(e, t, c)
            kids.append(c)
        u32(e, t + 0x20, 77, kids[1] if started and children > 1 else 0)
        return (t,)
    run(emu, 0x52C900, "LevelScript_Run", setup)


# ---- SimBoneDynCubeObj, PushButton ----

def test_simbone():
    def setup(e):
        u16(e, SIMBONE_TYPE, 0x68)
    run(emu, 0x52CA30, "SimBoneDynCubeObj_Init", setup)
    run(emu, 0x52CA60, "SimBoneDynCubeObj_Shutdown", setup)


def test_push_button_init_shutdown():
    def setup(e):
        u16(e, BUTTON_TYPE, 0x69)
    run(emu, 0x52CA70, "PushButton_Init", setup)
    run(emu, 0x52CB20, "PushButton_Shutdown", setup)


@pytest.mark.parametrize("text", [0, 0x7E70])
def test_push_button(text):
    def setup(e):
        t = task(e, 0x7C, 0x69)
        e.write(t + 0x20, bytes(range(0x58)))
        u32(e, t + 0x78, text)
        return t, 0x1157
    run(emu, 0x52CAD0, "PushButton_Draw", setup)
    run(emu, 0x52CAF0, "PushButton_AddToList", setup)
