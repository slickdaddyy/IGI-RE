from emu import Emu, compare

emu = Emu()


def test_task_nothing():
    compare(emu, 0x4D3710, "Task_Nothing", args=(0x5000,), check_eax=False)
