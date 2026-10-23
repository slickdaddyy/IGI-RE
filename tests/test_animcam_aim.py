"""AnimCam_Aim (0x529010)."""
import random

import pytest

from emu import Emu
from test_animcam_util import CWS, NAN, d64, f32, find_by_id_fake, info_handler, run, set_handler, task, u32

CAM, OBJ, PLAIN = 0x71, 0x72, 0x73
TASKS = {}
INFOS = {}

e = Emu()
find_by_id_fake(e, TASKS)
set_handler(e, 7, OBJ, info_handler(e, INFOS))
e.baseline = e.snapshot()


def camera(e, rng):
    c = task(e, 0x220, CAM)
    e.write(c + 0x20, bytes(rng.randrange(256) for _ in range(0x200)))
    d64(e, c + 0x110, *[rng.uniform(-1e4, 1e4) for _ in range(6)])
    d64(e, c + 0x148, *[rng.uniform(-500, 500) for _ in range(6)])
    return c


# objA, objB (None: id -1, "missing": no task, "plain": no GetInfo handler, or (x, y, z))
CASES = [
    ((0.0, 0.0, 0.0), (1000.0, 2000.0, 300.0)),
    ((10.5, -20.25, 3.0), (-500.0, 7000.0, -250.0)),
    ((1.0, 2.0, 3.0), (1.0, 2.0, 3.0)),
    ((1.0, 2.0, 3.0), (1.0, 2.0, 900.0)),
    ((1.0, 2.0, 3.0), (1.0, 2.0, -900.0)),
    ((1.0, 2.0, 3.0), (51.0, 2.0, 3.0)),
    ((1.0, 2.0, 3.0), (1.0, 52.0, 3.0)),
    ((1.0, 2.0, 3.0), (1.0, 2.0 + 1e-6, 3.0)),
    ((123456.789, -98765.4321, 55.5), (123456.788, -98765.4320, 55.6)),
    (None, (100.0, 200.0, 30.0)),
    ("missing", (100.0, 200.0, 30.0)),
    ("plain", (100.0, 200.0, 30.0)),
    ((5.0, 6.0, 7.0), None),
    ((5.0, 6.0, 7.0), "missing"),
    (None, None),
    ((NAN, 0.0, 0.0), (1.0, 2.0, 3.0)),
    ((0.0, NAN, 0.0), (1.0, 2.0, 3.0)),
    ((1e30, 0.0, 0.0), (-1e30, 2.0, 3.0)),
]


def place(e, rng, spec, id_):
    if spec is None:
        return -1
    if spec == "missing":
        return id_
    t = task(e, 0x40, PLAIN if spec == "plain" else OBJ)
    TASKS[id_] = t
    if spec != "plain":
        INFOS[t] = (list(spec), [rng.uniform(-1, 1) for _ in range(9)], 5)
    return id_


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", range(len(CASES)))
@pytest.mark.parametrize("locks", [(0, 0, 0, 0), (1, 1, 1, 1), (1, 0, 0, 1), (0, 1, 1, 0)])
def test_aim(case, locks, cw):
    def setup(e):
        TASKS.clear()
        INFOS.clear()
        rng = random.Random(case)
        c = camera(e, rng)
        spec_a, spec_b = CASES[case]
        u32(e, c + 0x108, place(e, rng, spec_a, 11), place(e, rng, spec_b, 12))
        e.write(c + 0x140, bytes(locks[:2]))
        e.write(c + 0x19C, bytes(locks[2:]))
        f32(e, c + 0xD0, 0.5)
        return (c,)
    run(e, 0x529010, "AnimCam_Aim", setup, cw=cw)
