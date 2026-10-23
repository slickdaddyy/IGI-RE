import pytest

from test_define_util import RET, f32, make_emu, run, task, u16, u32, with_cw  # noqa: F401

e = make_emu()
FADE, START_TYPE, END_TYPE, GLOW_TYPE = 0xA5EC74, 0xA5EC70, 0xA5EC78, 0x54DA50
RECTS = [(0.5, 10.7, 639.5, 479.49), (-3.5, 2.5, 40000.4, -1.5)]


def fade(e, rgba=(0, 0, 0, 255), rect=RECTS[0]):
    f = task(e, 0x7C, 0x70)
    u32(e, f + 0x20, *rgba)
    e.write(f + 0x48, f32(*rect))
    return f


@pytest.mark.parametrize("cw", [0x37F, 0x7F, 0xC7F, 0x77F])
@pytest.mark.parametrize("rect", RECTS)
@pytest.mark.parametrize("rgba", [(0, 0, 0, 255), (0, 0, 0, 254), (0, 0, 0, 0), (0, 0, 0, -1), (0, 0, 0, 300),
                                  (300, 0, 0, 255), (0, -2, 0, 255), (0, 0, 7, 100), (255, 128, 64, 128)])
def test_draw(rgba, rect, cw):
    run(e, 0x4E9110, "FadeOut_Draw", lambda e: (fade(e, rect=rect),) + rgba, cw=cw)


@pytest.mark.parametrize("rgba", [(0, 0, 0, 255), (255, 256, 1000, 0), (254, -5, 12, 64)])
def test_end_draw(rgba):
    def setup(e):
        u32(e, FADE, fade(e, rgba))
        return (0x1234,)
    run(e, 0x4E93B0, "FadeOutEnd_Draw", setup)


def test_start():
    def setup(e):
        e.write(0xBCABB8, f32(1.0, 2.0, 640.0, 480.0))
        return (fade(e, (5, 6, 7, 8)),)
    run(e, 0x4E9360, "FadeOutStart_Draw", setup)
    run(e, 0x4E90C0, "FadeOut_SetRect", setup)
    run(e, 0x4E9090, "FadeOut_InitTask", setup)
    run(e, 0x4E92B0, "FadeOut_FreeScene", setup)
    run(e, 0x4E93A0, "FadeOutStart_Delete", setup)


def test_types():
    def setup(e):
        u16(e, START_TYPE, 0x41)
        u16(e, END_TYPE, 0x42)
        u16(e, GLOW_TYPE, 0x43)
        u16(e, 0x54D9A0, 0x44)
        return (0x1000,)
    run(e, 0x4E92C0, "FadeOut_Init", setup)
    run(e, 0x4E9410, "FadeOut_Shutdown", setup)
    run(e, 0x4E9430, "FadeOutStart_New", setup, check_eax=True)
    run(e, 0x4E9460, "FadeOutEnd_New", setup, check_eax=True)
    run(e, 0x4E9480, "GlowTask_Init", setup)
    run(e, 0x4E94D0, "GlowTask_Draw", setup)
    run(e, 0x4E94E0, "GlowTask_Shutdown", setup)
    run(e, 0x4E9500, "GlowTask_New", setup, check_eax=True)
    run(e, 0x4E8370, "TerraDrawTask_New", lambda e: (setup(e)[0], 0x5555), check_eax=True)
