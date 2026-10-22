import pytest

from test_soldier_util import LOG, NAN, RET, d64, f32, fake, float_fake, new_emu, pyfake, run, soldier, u32

e = new_emu()


def root_pos(em, a):
    f32(em, a[0], *RET["root"][0 if a[4] == 0 else 1])


def coll_lines(em):
    a = [em.arg(i) for i in range(8)]
    LOG[-1].append((0x4CC2A0, a[0], em.read(a[2], 9 * 24), em.read(a[3], 9 * 24), a[4], a[5] & 0xFF,
                    em.u32(a[6] + 4), a[7]))
    for i in range(9):
        p = a[1] + 0x70 * i
        found, x, y, dist = RET["hits"].get(i, (0, 0.0, 0.0, 0.0))
        u32(em, p + 0x58, found)
        d64(em, p + 8, x, y, 5.0)
        d64(em, p + 0x60, dist)
    em.ret(0)


fake(e, 0x4D6FC0, 5, effect=root_pos, masks={0: 0})
fake(e, 0x4D7020, 2, ret=40)
fake(e, 0x416850, 0, ret=0x1234508)
fake(e, 0x416920, 0, ret=0x920)
pyfake(e, 0x4CC2A0, coll_lines)
float_fake(e, 0x4D74D0, 2)
e.baseline = e.snapshot()

ROOTS = [
    ((0.0, 0.0, 5.0), (0.0, 0.0, 9.0)),
    ((10.0, 20.0, 7.0), (-500.25, 1300.5, -3.0)),
    ((0.1, 0.2, 0.0), (0.4, -0.3, 1.0)),
    ((3.3333, 1.1111, 0.0), (-2000.7777, 0.12345, 0.0)),
]
HITS = [
    {},
    {4: (1, 100.0, 200.0, 50.0)},
    {0: (1, 1.0, 2.0, 30.0), 3: (1, -700.0, 900.5, 20.0), 8: (1, 5.0, 5.0, 25.0)},
    {2: (1, 1e5, 1e5, NAN), 5: (1, 33.0, 44.0, 10.0)},
    {1: (1, 1000.5, -2000.25, 3.0)},
    {6: (1, 999.0, -1999.0, 1.0)},
    {7: (1, 9000.0, 9000.0, 1.0)},
]


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("roots", ROOTS)
@pytest.mark.parametrize("hits", HITS)
@pytest.mark.parametrize("yaw,length", [(0.3, 1.5), (-2.2, 0.75)])
def test_death_velocity(roots, hits, yaw, length, cw):
    def setup(em):
        h, sm = soldier(em)
        d64(em, h + 0x20, 1000.5, -2000.25, 300.125)
        f32(em, h + 0x50C, yaw)
        RET["root"] = roots
        RET["hits"] = hits
        RET[0x4D74D0] = length
        return h + 0xD38, h, 7, 0x33
    run(e, 0x45D5B0, "Soldier_GetDeathVelocity", setup, cw=cw)
