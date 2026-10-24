"""QVertex_ClipTriangle (0x50BF20): chosen triangles, inverted scene boxes and debug keys."""
import random
import struct

import pytest

from test_dxinit_util import check_cw, f32
from test_qvertex_clip import CWS, DEPTH, KEYS, SCENE_MIN, e, emitted, outcode
from test_d3d_util import logged, trap, u32


def tri_args(em, pts, box, seed=0):
    """clip_args with the corners given: pts are (x, y), box is (x0, y0, x1, y1)."""
    rng = random.Random(seed)
    em.write(SCENE_MIN, f32(*box))
    em.write(0xBCADA0, f32(rng.uniform(0.5, 4)))
    em.write(0xBCADC8, f32(rng.uniform(500, 9000)))
    em.write(0xBCAD8C, f32(0.95))
    em.write(0x6E5BB0, struct.pack("<I", 1))
    args = []
    for x, y in pts:
        attrs = [rng.uniform(0.0005, 2)] + [rng.uniform(0, 1.5) for _ in range(6)]
        args += list(struct.unpack("<9I", f32(x, y, *attrs))) + [outcode(x, y, box)]
    s = em.alloc(0x14)
    em.write(s, struct.pack("<iIiiI", 0, 0x20000, -1, 3, em.alloc(0x6C)))
    cb = trap(em, "emit", logged("emit", 2, pop=0, before=emitted))
    return args + [struct.unpack("<I", f32(1.5))[0], cb, s]


def clip_case(pts, box, keys=()):
    def setup(em):
        u32(em, DEPTH, 5)
        for k in keys:
            em.write(KEYS + k, b"\x80")
        return tri_args(em, pts, box)
    return setup


def run(setup, cw=0x37F):
    check_cw(e, 0x50BF20, "QVertex_ClipTriangle", cw, setup=setup, check_eax=False)


SCREEN = (0.0, 0.0, 640.0, 480.0)
CLIPS = [
    # the right edge leaves a corner below the bottom, or all three below it
    ([(762.0499877929688, 471.3972473144531), (24.71591567993164, 363.85479736328125),
      (835.4705810546875, 700.056640625)], SCREEN),
    ([(-151.90423583984375, 359.79913330078125), (805.4124145507812, 637.9706420898438),
      (715.9365844726562, 661.3685913085938)], SCREEN),
    ([(544.2371215820312, 689.0114135742188), (905.0624389648438, 47.0047607421875),
      (835.3504638671875, 493.24322509765625)], SCREEN),
    # scene boxes with min above max: a corner moved onto the left or top edge is
    # still out on the other side
    ([(55.20009994506836, -222.29339599609375), (314.3133850097656, 898.2886352539062),
      (544.2716064453125, -163.60345458984375)],
     (228.0089569091797, 356.7157897949219, 58.976863861083984, 347.6060791015625)),
    ([(-82.73686218261719, 738.1385498046875), (-251.22763061523438, 279.07147216796875),
      (795.5202026367188, -81.71842956542969)],
     (544.5921630859375, 76.56109619140625, 609.2819213867188, -6.822627067565918)),
    ([(-48.90693283081055, 368.6085510253906), (473.8296813964844, 640.2697143554688),
      (593.297607421875, -66.40792846679688)],
     (304.95550537109375, 393.3864440917969, 603.5867919921875, 258.8666076660156)),
    ([(-377.9891052246094, 186.85049438476562), (174.94412231445312, 231.621826171875),
      (-108.454345703125, 365.36859130859375)],
     (11.433062553405762, 274.124267578125, -81.73180389404297, 492.2672119140625)),
]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(CLIPS)))
def test_clip_cases(case, cw):
    run(clip_case(*CLIPS[case]), cw)


@pytest.mark.parametrize("keys", [(0x1D,), (0x1D, 0x2A), (0x1D, 0x07)])
def test_clip_debug_keys(keys):
    """the outline needs control, shift and 6 held"""
    run(clip_case([(10.0, 20.0), (300.0, 40.0), (100.0, 400.0)], SCREEN, keys))
