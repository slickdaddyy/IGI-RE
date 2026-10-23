import pytest

from emu import Emu
from test_menu_util import check, logfake, task_system, u32

e = Emu()
e2 = Emu()
logfake(e2, 0x4B8920, 1, strings=(0,))  # Script_UnregisterSymbol


def init(e, voices, ready):
    task_system(e)
    u32(e, 0x5CA134, voices)
    e.write(0x6840E9, bytes([ready]))


@pytest.mark.parametrize("voices,ready", [(0, 1), (3, 1), (32, 0)])
def test_init(voices, ready):
    check(e, 0x4E6070, "SoundSys_Init", setup=lambda e: init(e, voices, ready), normalized=True, check_eax=False)


@pytest.mark.parametrize("events", [False, True])
def test_shutdown(events):
    def setup(e):
        init(e, 4, 1)
        e.call(0x4E6070)
        if events:
            for g in (0x4E65F0, 0x4E6630, 0x4E6670, 0x4E66B0, 0x4E66D0, 0x4E6710, 0x4E6730, 0x4E6610, 0x4E6690,
                      0x4E66F0, 0x4E6650):
                e.call(g)
    check(e2, 0x4E63E0, "SoundSys_Shutdown", setup=setup, normalized=True, check_eax=False)
