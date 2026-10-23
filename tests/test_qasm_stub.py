from emu import Emu, compare

emu = Emu()


def test_debug_log():
    compare(emu, 0x4C1780, "Debug_Log", args=(0x5000, 2, 0x6000), check_eax=False)
