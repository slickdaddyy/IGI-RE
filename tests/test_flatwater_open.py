"""Type setup and teardown of SoundManager, FlatWater, FlatSky, MipMapControl,
SmoothQTask and MoveRigidObj (0x51C090 - 0x51FD60)."""
import pytest

from test_flatwater_util import fake_emu, open_emu, run, setup_open, u16, u32

init = open_emu()

emu = fake_emu({
    0x401A20: (1, {"masks": {0: 0xFFFF}}),  # TaskType_Free
    0x4C1830: 1,  # QTaskList_Free
})


@pytest.mark.parametrize("orig,ours", [
    (0x51C2B0, "FlatWater_Open"),
    (0x51DDF0, "FlatSky_Open"),
    (0x51ECB0, "MipMapControl_Open"),
    (0x51EEE0, "SmoothQTask_Open"),
    (0x51F580, "MoveRigidObj_Open"),
])
def test_open(orig, ours):
    run(init, orig, ours, setup_open)


@pytest.mark.parametrize("orig,ours", [
    (0x51C290, "SoundManager_Close"),
    (0x51DDB0, "FlatWater_Close"),
    (0x51EC70, "FlatSky_Close"),
    (0x51EEC0, "MipMapControl_Close"),
    (0x51F560, "SmoothQTask_Close"),
])
def test_close(orig, ours):
    def setup(e):
        u16(e, 0x54EE3C, 0x41)
        u32(e, 0xA83780, 0x3780)
        u32(e, 0xBA2018, 0x42, 0x201C, 0x43, 0x2024)
        u32(e, 0xBA1FDC, 0x00450044, 0x1FE0, 0x1FE4)
        u16(e, 0x54EEE0, 0x46)
        u32(e, 0xA83790, 0x3790)
        u32(e, 0xA837A0, 0xABCD0047, 0x37A4)
    run(emu, orig, ours, setup)


@pytest.mark.parametrize("orig,ours,addr", [
    (0x51C280, "SoundManager_GetParams", 0xA83780),
    (0x51D8E0, "FlatWater_GetParams", 0xBA201C),
    (0x51DDA0, "FlatWaterLayer_GetParams", 0xBA2024),
    (0x51EB30, "FlatSky_GetParams", 0xBA1FE4),
    (0x51EC60, "FlatSkyLayer_GetParams", 0xBA1FE0),
    (0x51EEB0, "MipMapControl_GetParams", 0xA83790),
    (0x51F330, "SmoothQTask_GetParams", 0xA837A4),
    (0x51FA90, "MoveRigidObj_GetParams", 0xA837D0),
])
def test_get_params(orig, ours, addr):
    def setup(e):
        out = e.alloc(4)
        u32(e, addr, 0x12345678)
        return 0, out
    run(emu, orig, ours, setup)
