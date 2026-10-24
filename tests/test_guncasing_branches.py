"""Gun_FireHandler (0x478BA0) when the aim matrix's change count is above 1000.

Matrix_SetYAxis never writes that word, so in the original it is whatever is
on the stack; a stack filled with 0x11 makes it large and the original runs its
(unused) re-orthonormalization of the spread matrix."""
import pytest

from test_guncasing_fire import e, fk, setup_fire
from test_guncasing_util import CWS, check, f32, u32

# random state whose first and fourth draws are 0: no spread angle and no turn
ZERO_DRAWS = (0x1A9C0F1F, 0x338FF01B, 0xC32338E4)


def aimed(seed, mat=None, dirv=None, state=None, **kw):
    base = setup_fire(seed, aim=True, **kw)

    def setup(em):
        g, msg = base(em)
        if state:
            em.write(em.u32(0x57BABC) + 0x2C, u32(*state))
        if dirv:
            mat2 = (1, dirv[0], 0, 0, dirv[1], 0, 0, dirv[2], 0)
        else:
            mat2 = mat
        if mat2:
            em.write(em.u32(msg + 8), f32(*mat2) + b"\0" * 4)
        return g, msg
    return setup


CASES = [
    aimed(1, pellets=2, spread=(2.0, 2.0)),
    aimed(2, mat=(0,) * 9, pellets=2, spread=(2.0, 2.0)),
    aimed(49, mat=(1.0, -1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0), pellets=2, spread=(0.5, 9.0)),
] + [aimed(3, dirv=d, state=ZERO_DRAWS, pellets=1, spread=(2.0, 2.0), burst=3)
     for d in [(0, 0, 1), (1, 0, 0), (0, 1, 0), (1, 0, 1)]]


@CWS
@pytest.mark.parametrize("case", range(len(CASES)))
def test_fire_big_count(case, cw):
    check(e, fk, 0x478BA0, "Gun_FireHandler", CASES[case], check_eax=False, cw=cw, stack_fill=0x11)
