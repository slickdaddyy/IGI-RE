"""CutScene_Interpolate (0x4F55D0): band filter timing and fades out of range."""
import pytest

from test_cutscene import CWS, RET, cutscene, dbits, emu, f32, key, run, u32


def interp_case(times, t, end, fade_in=0.0, fade_out=0.0, band=True):
    def setup(e):
        filters = (0, 0, 0x264 if band else 0, 0)
        c = cutscene(e, seed=3, filters=filters, fadeIn=fade_in, fadeOut=fade_out, screen=0.75, unk210=1.0)
        u32(e, 0xA76C80, 0xF117)
        u32(e, 0xA76C7C, 0xCA3E)
        mode = e.alloc(12)
        u32(e, mode, 0, 640, 480)
        RET["mode"] = mode
        RET["rf"] = [0.25, 0.5, 0.75, 0.125, 0.375, 0.625]
        entries = e.alloc(32)
        for i in range(4):
            k = key(e, c, seed=30 + i, filter=2, level=0.0, shake=0.02, unk98=0.5)
            u32(e, entries + 8 * i, k)
            f32(e, entries + 8 * i + 4, times[i])
        ptrs = e.alloc(16)
        u32(e, ptrs, *[entries + 8 * i for i in range(4)])
        return [c, ptrs, *dbits(t), *dbits(end), 0x5A5A00]
    return setup


CASES = [
    interp_case((0.0, 1.0, 1.25, 2.0), 1.1, 3.0),                   # band filter keys closer than half a second
    interp_case((0.0, 1.0, 3.0, 4.0), 0.5, 5.0),                    # before the first key: negative blend
    interp_case((0.0, 1.0, 3.0, 4.0), 2.0, 1.0, fade_out=0.5),      # past the end: negative fade
    interp_case((0.0, 1.0, 3.0, 4.0), -2.0, -10.0, fade_in=-1.0, fade_out=-1.0),  # both fades above one
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(CASES)))
def test_interpolate(case, cw):
    run(emu, 0x4F55D0, "CutScene_Interpolate", CASES[case], cw=cw)
