"""TerrainMesh_Build (0x510010) when the first pools are full and big meshes do not fit."""
import pytest

from test_glowmesh import CWS_INT, build_setup, emu2, run, with_cw

FULL = (3, 1)
CASES = [
    dict(pools=[FULL, FULL, (5, 0)], n=30),                       # every pool full for later cells
    dict(pools=[FULL, (5, 0), (5, 0)], n=30),                     # a pool 1 mesh moves to pool 2
    dict(pools=[FULL, FULL, (5, 0)], n=45, key=0x04030201),       # too big even for pool 2
    dict(pools=[FULL, FULL, (5, 0)], n=48, key=0x04030201, level=14),
]


@pytest.mark.parametrize("seed", [1, 3])
@pytest.mark.parametrize("case", range(len(CASES)))
def test_build_pools(case, seed):
    run(emu2, 0x510010, "TerrainMesh_Build", lambda e: build_setup(e, seed, **CASES[case]))


@pytest.mark.parametrize("cw", CWS_INT)
def test_build_pools_cw(cw):
    run(emu2, with_cw(emu2, 0x510010, cw), with_cw(emu2, "TerrainMesh_Build", cw),
        lambda e: build_setup(e, 5, **CASES[2]))
