"""SplineObjSegment_Setup (0x4E4530): axis aligned ends, flat slopes and empty models."""
import struct

import pytest

from test_shadow_fakes import f32, f64, i32
from test_sound_segsetup import CWS, MODEL, PARTS, run, setup_for

ROT_Z90 = (0.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)   # first column (0, 1, 0)
ROT_Y90 = (0.0, 0.0, -1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0)   # first column (0, 0, 1)


def lod_info(em, models, k):
    return em.u32(em.u32(em.u32(models + 4 * k) + 0xC) + 0xC)


def tweak(base, ends=None, rots=None, lods=None, no_verts=False, no_mesh_verts=False):
    def setup(em):
        args = list(base(em))
        if ends:
            em.write(args[1], f64(*ends[0]))
            em.write(args[2], f64(*ends[1]))
        if rots:
            for p, m in zip(args[3:5], rots):
                em.write(p, f32(*m))
        if no_verts:
            i32(em, em.u32(lod_info(em, MODEL[0], 0) + 0x24) + 0xC, 0)
            i32(em, lod_info(em, MODEL[0], 0) + 0x20, 0)
        if lods is not None:
            em.write(MODEL[0] + 0x3C, struct.pack("<h", lods))
        if no_mesh_verts:
            for part in range(2):
                PARTS[part] = (PARTS[part][0], 0)
        return tuple(args)
    return setup


Y_ONLY = ((5.0, -100.0, 7.0), (5.0, 300.0, 7.0))
Z_ONLY = ((5.0, 6.0, -100.0), (5.0, 6.0, 300.0))
CASES = [
    tweak(setup_for(1, 1, 0, 3), ends=Y_ONLY),                           # rigid, along y
    tweak(setup_for(2, 1, 1, 3), ends=Z_ONLY),                           # rigid, along z
    tweak(setup_for(3, 0, 0, 6), ends=Y_ONLY, rots=(ROT_Z90, ROT_Z90)),  # no slope in x
    tweak(setup_for(4, 0, 1, 6), ends=Z_ONLY, rots=(ROT_Y90, ROT_Y90)),  # no slope in x or y
    tweak(setup_for(5, 0, 0, 4), lods=0),                                # a model without LODs
    tweak(setup_for(6, 1, 0, 4), lods=0),
    tweak(setup_for(7, 0, 0, 4), no_verts=True),                         # a LOD without vertices
    tweak(setup_for(8, 0, 0, 4), no_mesh_verts=True),                    # no bent vertices
    setup_for(9, 1, 0, 4, old="same"),                                   # rigid, same model as before
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(CASES)))
def test_setup(case, cw):
    run(CASES[case], cw)
