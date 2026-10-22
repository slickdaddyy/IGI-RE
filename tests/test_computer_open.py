"""Type setup and teardown of the map computer types (0x4663D0 - 0x4688B0)."""
import pytest

from emu import compare
from test_computer_util import logfake, make_emu, run, setup_open, u16, u32

init = make_emu()

emu = make_emu()
logfake(emu, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
logfake(emu, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
logfake(emu, 0x4C1830, 1)  # QTaskList_Free
logfake(emu, 0x4978E0, 1)  # RenderTask_Unregister
logfake(emu, 0x4B0EB0, 1)  # RenderMode_DeleteMethod
emu.baseline = emu.snapshot()


@pytest.mark.parametrize("orig,ours", [
    (0x4663D0, "ComputerStartup_Open"),
    (0x466E40, "ComputerLimit_Open"),
    (0x466F70, "ComputerObjective_Open"),
    (0x467700, "ComputerObject_Open"),
    (0x4682F0, "ComputerHilight_Open"),
    (0x4688B0, "ComputerLog_Open"),
])
def test_open(orig, ours):
    compare(init, orig, ours, setup=setup_open, check_eax=False, normalized=True)


@pytest.mark.parametrize("orig,ours", [
    (0x466E10, "ComputerStartup_Close"),
    (0x466F40, "ComputerLimit_Close"),
    (0x467570, "ComputerObjective_Close"),
    (0x467D30, "ComputerObject_Close"),
    (0x468870, "ComputerHilight_Close"),
])
def test_close(orig, ours):
    def setup(e):
        u16(e, 0x53F520, 0x41)
        u32(e, 0x5BDC50, 0x50, 0x54)
        u16(e, 0x5BDC60, 0x42)
        u32(e, 0x5BDC5C, 0x5C5C)
        u16(e, 0x5BDC70, 0x43)
        u32(e, 0x5BDC6C, 0x6C6C)
        u16(e, 0x53F580, 0x44)
        e.write(0x53F582, bytes([0x21, 0x22, 0x23]))
        u16(e, 0x53F6D0, 0x45)
        u32(e, 0x5BDC80, 0x8080, 0x8484)
    run(emu, orig, ours, setup)
