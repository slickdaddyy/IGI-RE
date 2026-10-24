"""DynCube_SendEventLines (0x4C8130): long lines along x that walk both low children of a cell."""
import random

import pytest

from emu import compare
from test_terrain_query import LINES, SLOT_EVENT, SLOTS, WORLDS, near_object, pick_emu, set_cells, vecs, world_terrain


def wide_lines(seed, world, cells):
    def setup(e):
        set_cells(e, cells)
        rng = random.Random(seed * 11 + world * 7 + 1000)
        t = world_terrain(e, rng, WORLDS[world])
        slot = rng.choice(SLOTS)
        e.write(SLOT_EVENT + slot, bytes([0xFF]))
        starts, ends = [], []
        for _ in range(rng.choice([1, 2, 3, 6])):
            p, size = near_object(e, rng, [0.3, 1.5, 300.0, 5000.0, 40000.0, 200000.0])
            d = [rng.uniform(-size, size) * 3 for _ in range(3)]
            d[0] = abs(d[0]) * 4 + size
            starts.append([a - b for a, b in zip(p, d)])
            ends.append([a + b for a, b in zip(p, d)])
        tree = rng.choice([0, 1, 3, 4, 5])
        return t, slot, 0x5555, vecs(e, starts), vecs(e, ends), len(starts), 0xFF, tree
    return setup


@pytest.mark.parametrize("cells", ["real", 0, 1])
def test_send_event_lines_wide(cells):
    compare(pick_emu(cells), LINES, "DynCube_SendEventLines", setup=wide_lines(47, 0, cells), check_eax=False,
            normalized=True)
