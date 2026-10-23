"""ConditionalContainer (0x4872C0 - 0x487920)."""
import pytest

from test_mission_util import RET, add_child, handler, make_emu, run, set_handler, task, u16, u32

TYPE, PARAMS = 0x5C8A80, 0x5C8A7C
CC, CHILD = 0x60, 0x61
EV_REMOVED, EV_ADDED, EV_COMPUTER, EV_DAC = 0x2F, 0x2E, 0x2B, 0x32

emu = make_emu()
set_handler(emu, 1, CHILD, handler(emu, "run", 1))
set_handler(emu, EV_REMOVED, CHILD, handler(emu, "removed", 2, derefs={1: 4}))
set_handler(emu, EV_ADDED, CHILD, handler(emu, "added", 2, derefs={1: 12}))
set_handler(emu, EV_COMPUTER, CHILD, handler(emu, "computer", 2))
set_handler(emu, EV_DAC, CC, handler(emu, "dac", 1))
emu.baseline = emu.snapshot()


def container(e, active=0, shown=0, children=2):
    c = task(e, 0x114, CC)
    e.write(c + 0x110, bytes([active, 0x11, shown]))
    for i in range(children):
        add_child(e, c, task(e, 0x40, CHILD if i != 1 else 0x62))
    return c


def test_init():
    run(emu, 0x4872C0, "ConditionalContainer_Init", lambda e: u16(e, TYPE, 0x3F))


def test_create():
    run(emu, 0x4874A0, "ConditionalContainer_Create", lambda e: (container(e, 1, 0),))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("active", [0, 1])
@pytest.mark.parametrize("shown", [0, 1])
@pytest.mark.parametrize("cond", [None, 0.0, 1.0, 0.4, 0.6, -1.0, 256.0])
@pytest.mark.parametrize("handlers", [(0, 0), (1, 1)])
@pytest.mark.parametrize("children", [0, 3])
def test_run(active, shown, cond, handlers, children, cw):
    def setup(e):
        c = container(e, active, shown, children)
        if cond is not None:
            RET[("has", c + 0x20)] = 1
            RET[("val", c + 0x20)] = cond
        RET[("has", c + 0x70)], RET[("has", c + 0xC0)] = handlers
        return (c,)
    run(emu, 0x4874E0, "ConditionalContainer_Run", setup, cw=cw)


@pytest.mark.parametrize("cid", [-1, 0, 5])
def test_delete(cid):
    def setup(e):
        RET[0x4F1050] = cid
        return (container(e),)
    run(emu, 0x487730, "ConditionalContainer_Delete", setup)


def test_get_params():
    def setup(e):
        u32(e, PARAMS, 0x9876)
        return 0, e.alloc(4)
    run(emu, 0x487790, "ConditionalContainer_GetParams", setup)


def test_changed():
    run(emu, 0x4877A0, "ConditionalContainer_Changed", lambda e: (container(e),))


@pytest.mark.parametrize("orig,ours", [(0x4877D0, "ConditionalContainer_ComputerEvent"),
                                       (0x487860, "ConditionalContainer_Show")])
@pytest.mark.parametrize("shown", [0, 1])
@pytest.mark.parametrize("children", [0, 3])
def test_events(orig, ours, shown, children):
    run(emu, orig, ours, lambda e: (container(e, 0, shown, children), 0xA4A4))
