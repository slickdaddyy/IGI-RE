import pytest

from emu import compare
from test_fxobj_util import init_emu, logfake, run, setup_init, u32

INITS = [(0x436780, "RainEffect_Init"), (0x436B10, "EnvironmentSound_Init"), (0x437510, "Smoke_Init"),
         (0x438390, "GunShotEffect_Init"), (0x4388B0, "PlaceExplosiveTBA_Init"), (0x438DF0, "RotatingObject_Init"),
         (0x4397C0, "AlarmLight_Init")]

CLOSES = [(0x436770, "Rudder_Close"), (0x436AF0, "RainEffect_Close"), (0x4374E0, "EnvironmentSound_Close"),
          (0x438370, "Smoke_Close"), (0x438890, "GunShotEffect_Close"), (0x438C10, "PlaceExplosiveTBA_Close"),
          (0x4397A0, "RotatingObject_Close")]

init = init_emu()

close = init_emu()
logfake(close, 0x4C1830, 1)  # QTaskList_Free
logfake(close, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
close.baseline = close.snapshot()


@pytest.mark.parametrize("orig,ours", INITS)
def test_init(orig, ours):
    compare(init, orig, ours, setup=setup_init, check_eax=False, normalized=True)


@pytest.mark.parametrize("orig,ours", CLOSES)
def test_close(orig, ours):
    def setup(e):
        for i, a in enumerate((0x57BD74, 0x57BD84, 0x57BDB0, 0x57BE00, 0x57BE78, 0x53C1DC, 0x57BE98)):
            u32(e, a, 0xAB0000 + 0x10 + i)
        for i, a in enumerate((0x57BD88, 0x57BDA8, 0x57BE10, 0x57BE8C, 0x57BE90)):
            u32(e, a, 0x5000 + i)
    run(close, orig, ours, setup)
