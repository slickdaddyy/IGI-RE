"""Helpers for the dxinit tests: x87 control word trampolines, Direct3D device fakes, scenes."""
import math
import random
import struct

from test_d3d_util import LOG, check, com, out, u32

# argument counts (with this) of the device and vertex buffer methods used here
DEV = {0: 3, 1: 1, 2: 1, 11: 3, 16: 2, 18: 3, 20: 3, 31: 6, 35: 3, 37: 4, 44: 3}
VB = {0: 3, 1: 1, 2: 1, 3: 4, 4: 1}

G_D3D, G_DEVICE = 0x5CA110, 0x5CA114
VIEW = 0xBCAAE0
SCALE = 0xBCABEC
LISTS = (0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4)


def with_cw(e, target, cw):
    """An entry point that loads x87 control word cw and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    e.cw_tramps = getattr(e, "cw_tramps", {})
    if (addr, cw) not in e.cw_tramps:
        p = e.fake_top
        e.fake_top += 32
        code = b"\xd9\x2d" + struct.pack("<I", p + 16)
        code += b"\xe9" + struct.pack("<i", addr - (p + len(code) + 5))
        e.write(p, code.ljust(16, b"\x90") + struct.pack("<H", cw))
        e.cw_tramps[(addr, cw)] = p
    return e.cw_tramps[(addr, cw)]


def check_cw(e, orig, ours, cw, setup=None, **kw):
    return check(e, with_cw(e, orig, cw), with_cw(e, ours, cw), setup=setup, **kw)


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def noise(n, seed=0):
    return bytes((i * 151 + seed * 13 + 7) & 0xFF for i in range(n))


def rotation(rng):
    a, b, c = (rng.uniform(-math.pi, math.pi) for _ in range(3))
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    rz = [[ca, -sa, 0], [sa, ca, 0], [0, 0, 1]]
    rx = [[1, 0, 0], [0, cb, -sb], [0, sb, cb]]
    ry = [[cc, 0, sc], [0, 1, 0], [-sc, 0, cc]]

    def mul(p, q):
        return [[sum(p[i][k] * q[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    return [x for row in mul(mul(rz, rx), ry) for x in row]


def set_view(e, rng, pos=None, scale=1 / 4096.0):
    e.write(VIEW, f32(*rotation(rng)))
    e.write(VIEW + 0x24, struct.pack("<i", rng.randrange(1000)))
    e.write(VIEW + 0x28, f64(*(pos or [rng.uniform(-3e5, 3e5), rng.uniform(-3e5, 3e5), rng.uniform(-2e4, 9e4)])))
    e.write(VIEW + 0x48, f32(300.0, 300.0, 320.0, 240.0))
    e.write(SCALE, f32(scale))
    e.write(0xC28B44, struct.pack("<ii", 640, 480))


def device(e, methods=None):
    m = {11: (0, lambda e: (None, e.read(e.arg(2), 64)))}
    m.update(methods or {})
    dev = com(e, "dev", DEV, m)
    u32(e, G_DEVICE, dev)
    for p in LISTS:
        lst = e.alloc(0x40)
        e.write(lst, struct.pack("<5I", 0xABCDABCD, 3, 4, 4, 0xDCBADCBA))
        u32(e, p, lst)
    return dev


def vertex_buffer(e, size=0x780 * 24, lock_ret=0):
    buf = e.alloc(size)
    e.write(buf, noise(size, 5))
    vb = com(e, "vb", VB, {3: (lock_ret, out(2, buf))})
    return vb, buf


def d3d(e, vb):
    obj = com(e, "d3d", {0: 3, 1: 1, 2: 1, 5: 4},
              {5: (0, lambda e: (u32(e, e.arg(2), vb), (None, e.read(e.arg(1), 16)))[1])})
    u32(e, G_D3D, obj)
    return obj


__all__ = ["LOG"]
