"""Task type registration of the widget types, with the original task system."""
import pytest

from emu import Emu, compare
from test_menu_util import task_system, u32

emu = Emu()
PARAM_TYPES = (0xA7186C, 0xA75898, 0xA70C7C, 0xA70C90, 0xA70C9C, 0xA71888)


def setup_init(e):
    task_system(e)
    for i, a in enumerate(PARAM_TYPES):
        u32(e, a, 0x100 + i)
    u32(e, 0x5488C8, 0x60)  # draw event
    u32(e, 0x54DDA8, 0x61, 0x62)  # activate, deactivate events
    e.call(0x423FC0)  # MenuWindow


INITS = [(0x41B310, "PictureBox_Init"), (0x41B8E0, "ScrollListBox_Init"), (0x41C700, "AsciiBox_Init"),
         (0x41D080, "InputBox_Init"), (0x41DC50, "ToggleBox_Init")]


@pytest.mark.parametrize("orig,ours", INITS)
def test_init(orig, ours):
    compare(emu, orig, ours, setup=setup_init, check_eax=False, normalized=True)
