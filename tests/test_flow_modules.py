import re

from emu import Emu, compare

emu = Emu()
for addr in sorted({int(a, 16) for a in re.findall(r"ORIG\((0x[0-9A-F]+)", open("../src/flow/modules.c").read())}):
    emu.fake(addr)
for addr in (0x48F100, 0x48F330, 0x48F220, 0x402800, 0x4028A0, 0x401A60, 0x402880, 0x4015F0):
    if addr not in emu.fake_ret_at:
        emu.fake(addr, 0, 1)


def test_modules_open():
    compare(emu, 0x402930, "Modules_Open", check_eax=False, normalized=True)
    assert len(emu.call_log()) == 156


def test_modules_close():
    compare(emu, 0x402C40, "Modules_Close", check_eax=False, normalized=True)
    assert len(emu.call_log()) == 155


def test_game_start():
    for v in (0, 1):
        emu.set_fake_ret(0x48F220, v)
        compare(emu, 0x401000, "Game_Start", eax_mask=0xFF, normalized=True)
    emu.set_fake_ret(0x48F220, 0)


def test_game_shutdown():
    for count in (0, 3):
        emu.set_fake_ret(0x401A60, count)
        compare(emu, 0x4010B0, "Game_Shutdown", check_eax=False, normalized=True)
    emu.set_fake_ret(0x401A60, 0)
