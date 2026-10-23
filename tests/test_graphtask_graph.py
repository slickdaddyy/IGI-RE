import random
import struct

import pytest

from emu import Emu
from test_graphtask_util import Fakes, check, f32, f64, i32

emu = Emu()
fk = Fakes(emu)
R = {}


def read_value(e, a):
    """GraphStream_ReadValue(v, out, size): writes R["value"] to out."""
    e.write(a[1], R["value"])


GETTERS = {0x5034D0: 0x41, 0x4F1A60: 0x47, 0x4F1A70: 0x48, 0x4F1B60: 3, 0x4F1020: 0x1020, 0x4F1010: 0x1010,
           0x4F0EF0: 0x0EF0, 0x4F1000: 0x1000}
for _a, _r in GETTERS.items():
    fk.add(_a, 0, _r)
fk.add(0x401900, 5, 0x77, fmt=lambda e, a: (e.read_cstr(a[0]), a[1], a[2] & 0xFFFF, a[3]))
fk.add(0x4017C0, 1, lambda e, a: R.setdefault("ev", iter(range(0x30, 0x40))).__next__())
fk.add(0x4018C0, 1, fmt=lambda e, a: (a[0] & 0xFF,))
fk.add(0x401A20, 1, fmt=lambda e, a: (a[0] & 0xFFFF,))
fk.add(0x401400, 3, fmt=lambda e, a: (a[0] & 0xFFFF, a[1] & 0xFF, e.normalize(a[2])))
fk.add(0x401530, 3, fmt=lambda e, a: (a[0] & 0xFFFF, a[1], e.normalize(a[2])))
fk.add(0x401B20, 1)
fk.add(0x4B0D10, 1)
fk.add(0x4B0C60, 2, lambda e, a: 0x5A000000 + a[0])
fk.add(0x4C1830, 1)
fk.add(0x4F1950, 1, 0x1950)
fk.add(0x4F1A80, 6, fmt=lambda e, a: (a[0], (a[2] - a[1]) & 0xFFFFFFFF, a[3], a[4], a[5]))
fk.add(0x4F1BC0, 3)
fk.add(0x4F9460, 2, fmt=lambda e, a: (a[0] & 0xFFFF, a[1]))
fk.add(0x4F9DF0, 3)
fk.add(0x4B8A20, 2, lambda e, a: 1000 + a[1])
fk.add(0x4F1050, 1, 77)
fk.add(0x4F9FF0, 2, fmt=lambda e, a: (a[0], e.read_cstr(a[1])))
fk.add(0x4F9CD0, 3, read_value, fmt=lambda e, a: (a[0], "stack" if 0x100000 <= a[1] < 0x200000 else a[1], a[2]))
fk.add(0x4F94D0, 1, 0x240)
fk.add(0x4F96E0, 2)
fk.add(0x4F9720, 2, lambda e, a: R["nodes"].get(a[1], 0))
fk.add(0x4F9C50, 1)
fk.add(0x4CE400, 2, lambda e, a: 0x4E000000 + a[1])
fk.add(0x4F9510, 8, 0x9510, fmt=lambda e, a: (a[0], a[1], e.read(a[2], 24), a[3], a[4], a[5], e.normalize(a[6]), a[7]))
fk.add(0x4F9B80, 4, 0x9B80)
fk.add(0x4B17C0, 1)
fk.add(0x4B16D0, 3, lambda e, a: (e.write(a[1], bytes([R["byte"]])), 1)[1] if R["byte"] is not None else 0,
       fmt=lambda e, a: (a[0], a[2]))
fk.add(0x4B1730, 3)
emu.baseline = emu.snapshot()


@pytest.mark.parametrize("byte", [0x5A, 0, None])
def test_peek_byte(byte):
    def setup(e):
        R.clear()
        R["byte"] = byte
        s = e.alloc(8)
        e.write(s, i32(0, 0xF11E))
        return (s,)
    check(emu, fk, 0x4FAA10, "GraphStream_PeekByte", setup)


def test_open():
    def setup(e):
        R.clear()
        e.call(0x4B0BA0)  # Mem_Init
    check(emu, fk, 0x4FAA60, "Graph_Open", setup, check_eax=False)


def test_close():
    def setup(e):
        R.clear()
        e.write(0xA76D10, i32(0x1950))
        e.write(0x54E364, struct.pack("<H", 0x77))
        e.write(0x54E368, bytes(range(0x30, 0x3A)))
    check(emu, fk, 0x4FB110, "Graph_Close", setup, check_eax=False)


@pytest.mark.parametrize("which", [0, 1, 2, None, "null"])
def test_pre_delete(which):
    def setup(e):
        R.clear()
        g = e.alloc(0x80)
        e.write(g + 0x58, i32(6))
        lst = 0xA77248
        e.write(lst, i32(lst + 4, 0, lst))
        refs = []
        for i in range(3):
            n = e.alloc(12)
            ref = e.alloc(4)
            e.write(ref, i32(g if i == which else e.alloc(4)))
            e.write(n + 8, i32(0 if which == "null" and i == 1 else ref))
            e.call(0x4AF8F0, lst, n)
            refs.append(n)
        nodes = {}
        rng = random.Random(3)
        for i in (0, 2, 5):
            n = e.alloc(0x240)
            for j in range(64):
                if rng.random() < 0.2:
                    e.write(n + 0x13C + 4 * j, i32(0x1100 + 64 * i + j))
            nodes[i] = n
        R["nodes"] = nodes
        return (g,)
    check(emu, fk, 0x4FADB0, "Graph_PreDelete", setup, check_eax=False)


def test_get_params():
    def setup(e):
        e.write(0xA76D10, i32(0x1950))
        return 0x1234, e.alloc(4)
    check(emu, fk, 0x4FAE40, "Graph_GetParams", setup, check_eax=False)


def test_parse_param():
    def setup(e):
        R.clear()
        g = e.alloc(0x80)
        e.write(g + 0x58, b"\xAA" * 0x18)
        cur = e.alloc(16)
        e.write(cur, i32(4, 0xA4A4))
        return g, cur
    check(emu, fk, 0x4FAE50, "Graph_ParseParam", setup, check_eax=False)


def value(e, nodes=0):
    v = e.alloc(0x14)
    g = e.alloc(0x80)
    e.write(g + 0x58, i32(17))
    if nodes:
        e.write(g + 0x6C, i32(nodes))
    e.write(v, i32(0x5555, g, 0x8888, 0x9999, 5))
    return v


@pytest.mark.parametrize("nodes", [0, 0x12340])
def test_read_max_nodes(nodes):
    def setup(e):
        R.clear()
        R["value"] = i32(40)
        return (value(e, nodes),)
    check(emu, fk, 0x4FAEE0, "Graph_ReadMaxNodes", setup, check_eax=False)


FIELDS = [(0x4FAF50, "Graph_ReadNodeID", i32(42)), (0x4FAF80, "Graph_ReadNodePosition", f64(1.5, -2.5, 3e6)),
          (0x4FAFA0, "Graph_ReadNodeGamma", f32(0.75)), (0x4FAFC0, "Graph_ReadNodeRadius", f32(812.5)),
          (0x4FAFE0, "Graph_ReadNodeMaterial", i32(7)), (0x4FAFE0, "Graph_ReadNodeMaterial", i32(-1)),
          (0x4FB080, "Graph_ReadLinkID1", i32(3)), (0x4FB0B0, "Graph_ReadLinkID2", i32(9)),
          (0x4FB0D0, "Graph_ReadLinkType", i32(2)), (0x4FB030, "Graph_ReadNodeCriteria", b"NODE_A | NODE_B\0")]


@pytest.mark.parametrize("addr,name,data", FIELDS)
def test_read_field(addr, name, data):
    def setup(e):
        R.clear()
        R["value"] = data
        rng = random.Random(addr)
        e.write(0xA77260, bytes(rng.randrange(256) for _ in range(0x240)))
        e.write(0xA77280, f32(rng.uniform(-4, 4), rng.uniform(0, 5000)))
        e.write(0xA77254, i32(11, 22))
        e.write(0xA76D24, i32(33))
        e.write(0xA44344, i32(0x7E770000))
        return (value(e),)
    check(emu, fk, addr, name, setup, check_eax=False)
