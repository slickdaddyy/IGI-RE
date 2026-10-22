"""Task type registration of the front end widget types, with the original task system."""
import pytest

from emu import Emu, compare
from test_menu_util import check, logfake, task_system, u32

emu = Emu()
emu2 = Emu()
logfake(emu2, 0x4B8920, 1, strings=(0,))  # Script_UnregisterSymbol needs more script state
PARAM_TYPES = (0xA7186C, 0xA75898, 0xA70C7C, 0xA70C90, 0xA70C9C, 0xA71888)


def setup_init(e):
    task_system(e)
    for i, a in enumerate(PARAM_TYPES):
        u32(e, a, 0x100 + i)
    u32(e, 0x5488C8, 0x60)  # draw event
    u32(e, 0x54DDA8, 0x61, 0x62)  # activate, deactivate events
    e.call(0x423FC0)  # MenuWindow


INITS = [(0x41E690, "ListBox_Init"), (0x41F340, "SlideBar_Init"), (0x420300, "MenuTextConditional_Init"),
         (0x420CC0, "MenuText_Init"), (0x421430, "MenuButton_Init"), (0x421750, "MenuScreen_Init")]


@pytest.mark.parametrize("orig,ours", INITS)
def test_init(orig, ours):
    compare(emu, orig, ours, setup=setup_init, check_eax=False, normalized=True)


@pytest.mark.parametrize("init,orig,ours", [
    (0x41DC50, 0x41E660, "ToggleBox_Shutdown"), (0x41E690, 0x41F2A0, "ListBox_Shutdown"),
    (0x41F340, 0x4201B0, "SlideBar_Shutdown"), (0x420300, 0x420C90, "MenuTextConditional_Shutdown"),
    (0x421430, 0x421700, "MenuButton_Shutdown")])
def test_shutdown(init, orig, ours):
    def setup(e):
        setup_init(e)
        e.call(init)
    compare(emu, orig, ours, setup=setup, check_eax=False, normalized=True)


def test_menutext_shutdown():
    def setup(e):
        setup_init(e)
        e.call(0x420CC0)
    check(emu2, 0x4213E0, "MenuText_Shutdown", setup=setup, check_eax=False, normalized=True)
