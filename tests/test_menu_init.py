"""Task type registration of the menu types, with the original task system."""
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


INITS = [(0x4190E0, "MenuLoader_Init"), (0x419760, "BackgroundFX_Init"), (0x4199F0, "DialogWindow_Init"),
         (0x41A1E0, "TypeWriterBox_Init"), (0x41AAF0, "TextBox_Init")]


@pytest.mark.parametrize("orig,ours", INITS)
def test_init(orig, ours):
    compare(emu, orig, ours, setup=setup_init, check_eax=False, normalized=True)


@pytest.mark.parametrize("init,orig,ours", [
    (0x4170A0, 0x418AC0, "MenuManager_Shutdown"), (0x4190E0, 0x4192B0, "MenuLoader_Shutdown"),
    (0x419760, 0x4199B0, "BackgroundFX_Shutdown"), (0x4199F0, 0x41A1A0, "DialogWindow_Shutdown"),
    (0x41A1E0, 0x41AAC0, "TypeWriterBox_Shutdown")])
def test_shutdown(init, orig, ours):
    def setup(e):
        setup_init(e)
        e.call(init)
    compare(emu, orig, ours, setup=setup, check_eax=False, normalized=True)
