"""WaterMesh_DrawNode (0x52E660): waves running along y, so the normals have no x part."""
import random

import pytest

from emu import ErrorShown
from test_terredit_util import CWS, d64, run, with_cw
from test_terredit_water import ne, node_scene


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", ["same", "deeper", "nolevel"])
@pytest.mark.parametrize("seed", range(3))
def test_draw_node_along_y(seed, case, cw):
    def setup(em):
        w, node, lvl = node_scene(em, random.Random(seed * 13 + len(case)), case)
        d64(em, w + 0x4888, 0.0, 1.0)
        return w, node, lvl, 1, 2
    orig, ours = with_cw(ne, 0x52E660, cw), with_cw(ne, "WaterMesh_DrawNode", cw)
    ne.reset()
    try:
        ne.call(orig, *setup(ne))
    except ErrorShown as err:
        ne.reset()
        with pytest.raises(ErrorShown) as mine:
            ne.call(ours, *setup(ne))
        assert str(mine.value) == str(err)
        return
    run(ne, 0x52E660, "WaterMesh_DrawNode", setup, cw=cw)
