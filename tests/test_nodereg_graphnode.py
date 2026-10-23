"""NodeCriteria and the GraphNode task (0x5186C0 - 0x518900)."""
import pytest

from emu import Emu, ErrorShown
from test_nodereg_util import RET, cstr, logfake, run, task, u8, u32

CRITERIA = 0xA800F8

emu = Emu()
logfake(emu, 0x4B8810, 2, strings=(0,))  # Script_RegisterConstant
logfake(emu, 0x4B8920, 1, strings=(0,))  # Script_UnregisterSymbol
logfake(emu, 0x4F9720, 2, ret=0x9720)  # Graph_GetNode
logfake(emu, 0x4D3710, 2)  # Task_Nothing
logfake(emu, 0x4015F0, 1)  # Task_Delete
logfake(emu, 0x4B6F30, 1)  # Text_Free
emu.baseline = emu.snapshot()


def criteria(e, used):
    for i in range(32):
        u8(e, CRITERIA + 8 * i, 1 if i in used else 0)
        u32(e, CRITERIA + 8 * i + 4, cstr(e, b"CRIT_%d" % i) if i in used else 0)


@pytest.mark.parametrize("used", [(), (0, 1, 2), (0, 1, 3), tuple(range(31)), tuple(range(1, 32))])
def test_register(used):
    def setup(e):
        criteria(e, used)
        return (cstr(e, b"NODECRITERIA_NEW"),)
    run(emu, 0x5186C0, "NodeCriteria_Register", setup, check_eax=True)


def test_register_full():
    msgs = []
    for target in (0x5186C0, "NodeCriteria_Register"):
        emu.reset()
        criteria(emu, tuple(range(32)))
        with pytest.raises(ErrorShown) as ex:
            emu.call(target, cstr(emu, b"NODECRITERIA_FULL"))
        msgs.append(str(ex.value))
    assert msgs[0] == msgs[1]


@pytest.mark.parametrize("mask", [0, 1, 0x80000001, 0x15, 0xFFFFFFFF])
def test_unregister(mask):
    def setup(e):
        criteria(e, tuple(range(0, 32, 2)) + (5, 31))
        return (mask,)
    run(emu, 0x518720, "NodeCriteria_Unregister", setup)


def node(e, loaded=1, texts=(0x111, 0, 0x333)):
    n = task(e, 0x118, 0x60, task(e, 0x40, 0x61))
    u32(e, n + 0x38, 77)
    u8(e, n + 0x3C, loaded)
    u32(e, n + 0x4C, *texts)
    return n


def test_create():
    def setup(e):
        u32(e, 0xA44344, 0x4344)
        idp = e.alloc(4)
        u32(e, idp, 1234)
        return node(e), idp
    run(emu, 0x518870, "GraphNode_Create", setup)


def test_params_changed():
    run(emu, 0x518890, "GraphNode_ParamsChanged", lambda e: (node(e),))


def test_get_param_table():
    def setup(e):
        u32(e, 0xA801FC, 0x1FC1FC)
        return 0, e.alloc(4)
    run(emu, 0x5188B0, "GraphNode_GetParamTable", setup)


def test_run():
    run(emu, 0x5188C0, "GraphNode_Run", lambda e: (node(e),))


@pytest.mark.parametrize("loaded,texts", [(0, (1, 2, 3)), (1, (1, 0, 3)), (1, (0, 0, 0)), (2, (5, 6, 7))])
def test_delete(loaded, texts):
    run(emu, 0x5188D0, "GraphNode_Delete", lambda e: (node(e, loaded, texts),))
