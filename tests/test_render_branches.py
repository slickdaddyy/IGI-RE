"""Direct3DRender_DrawRigidMesh (0x49DF60) with the render state lists already selected,
and lightmapped meshes inside the view."""
import random

import pytest

from test_render import INSIDE, LOD, base_setup, rig, rigid_model, setup_view
from test_render_util import u32

LIST_A, LIST_B, LIST_C, STATES = 0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4
TEX_STAGE, LM_STAGE, OPAQUE, LIT, UNLIT = 0x7205B0, 0x723504, 0x7AC09C, 0x7234B8, 0x7ADDD4
CLIP_OFF, CLIP_ON = 0xBCADE0, 0xBCACEC


def preselect(e, lists):
    """each state list already has the index the draw would select"""
    for lst, idx in lists:
        u32(e, e.u32(lst) + 4, e.u32(idx) if idx else 0x7777)


def draw_case(seed, kind, lightmaps, inside, lists=()):
    def setup(e):
        rng = random.Random("branch%d" % seed)
        base_setup(e, rng)
        view = setup_view(e, rng)
        o, h, d, lods = rigid_model(e, rng, view, kind=kind, nverts=rng.randrange(3, 20),
                                    ngroups=rng.randrange(1, 5), lightmaps=lightmaps)
        INSIDE["ret"] = 0
        INSIDE["inside"] = inside
        LOD["lod"] = rng.randrange(2)
        preselect(e, lists)
        p = e.alloc(4)
        u32(e, p, o)
        return (p,)
    return setup


CASES = [
    draw_case(1, 0, "set", 0, [(LIST_A, TEX_STAGE), (LIST_B, OPAQUE), (LIST_C, LIT), (STATES, CLIP_ON)]),
    draw_case(2, 0, "set", 3, [(LIST_B, OPAQUE), (STATES, CLIP_OFF)]),
    draw_case(3, 3, "set", 0),
    draw_case(4, 3, "set", 0, [(LIST_A, LM_STAGE), (LIST_B, OPAQUE), (LIST_C, UNLIT), (STATES, CLIP_ON)]),
    draw_case(5, 3, None, 0),
    draw_case(6, 3, None, 0, [(LIST_A, TEX_STAGE), (LIST_B, OPAQUE), (LIST_C, LIT), (STATES, CLIP_ON)]),
    draw_case(7, 3, None, 2, [(LIST_A, TEX_STAGE), (LIST_C, LIT), (STATES, CLIP_OFF)]),
    draw_case(8, 3, None, 0, [(STATES, None)]),          # a state index no draw uses
    draw_case(9, 3, "set", 0, [(STATES, None)]),
]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", range(len(CASES)))
def test_draw_rigid_mesh_states(case, cw):
    rig.check(0x49DF60, "Direct3DRender_DrawRigidMesh", CASES[case], cw=cw)
    INSIDE["ret"], LOD["lod"] = 0, 0
