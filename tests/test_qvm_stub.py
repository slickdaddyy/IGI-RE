from emu import Emu, compare

emu = Emu()


def test_get_arg_line():
    compare(emu, 0x4BD070, "Qvm_GetArgLine", args=(0x5000, 3))
