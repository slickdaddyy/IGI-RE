"""RigidDynCubeObj_LineHit (0x4C1840) with only the z scale away from 1."""
import random

import pytest

from test_qasm_big import Scene, config, run


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("scale", [[1.0, 1.0, 2.5], [1.0, 1.0, 0.5]])
def test_line_hit_z_scale(scale, cw):
    rng = random.Random(77)
    scene = Scene(rng, ntri=20)
    run(scene, config(77, scale=scale, prior=0.5), cw)
