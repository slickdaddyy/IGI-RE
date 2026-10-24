"""Explosion_Burst (0x47F200) with a blast and no tasks to send it to."""
import pytest

from test_explosion_burst import CWS, e, explosion
from test_explosion_util import RET, STATE, randoms, run, seeded, u32


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("fireballs,fragments", [(1, 0), (2, 2)])
def test_burst_no_targets(fireballs, fragments, cw):
    def setup(e):
        rng = seeded(40 + fireballs)
        randoms(rng, 200)
        arr = e.alloc(8)
        u32(e, arr, 0, 0)
        u32(e, 0x57BAA8, arr)
        STATE["scale"] = (1.0, 2.0, 0.5)
        RET[0x4E6B00] = 0
        return (explosion(e, rng, fireballs, fragments, 8192.0, 300.0),)
    run(e, 0x47F200, "Explosion_Burst", setup, cw=cw)
