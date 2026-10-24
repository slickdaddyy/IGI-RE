"""Helpers for the mesh renderer tests: one log for faked callees and fake COM objects."""
import random
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import STACK_BASE, STACK_SIZE, Emu, compare

DEVICE, D3D = 0x5CA114, 0x5CA110
LIST_A, LIST_B, LIST_C, LIST_STATE = 0xBCADE8, 0xBCAD7C, 0xBCADAC, 0xBCADC4
VB_NORMAL = 0xBCAD90
VIEW, SCENE = 0xBCAAE0, 0xBCABA0
WORLD_SCALE, NEAR_PLANE = 0xBCABEC, 0xBCADC0
STATE_IDX = [0x7205B0, 0x7205B8, 0x7205BC, 0x7234B8, 0x723504, 0x723508, 0x7AC09C, 0x7ADDD4, 0xBCACEC, 0xBCADE0]


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def u32(e, p, *v):
    e.write(p, struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v]))


class Rig:
    def __init__(self):
        self.e = Emu()
        self.log = []
        self.tramps = {}
        self.objs = {}

    def code(self, data):
        p = self.e.fake_top
        self.e.fake_top += (len(data) + 15) & ~15
        self.e.write(p, data)
        return p

    def norm(self, v):
        if STACK_BASE <= v < STACK_BASE + STACK_SIZE:
            return "stack"
        return self.e.normalize(v)

    def hook(self, addr, name, nargs, pop=0, ret=0, fmt=None, st0=False):
        """Log (name, args) when addr is reached and return ret (a value or fn(e, args)).
        fmt(e, args) gives the logged args. st0: return ret as a double in st(0)."""
        rig = self
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code(b"\xdd\x05" + struct.pack("<I", cell) + b"\xc3") if st0 else None

        def cb(uc, a, size, user):
            e = rig.e
            args = [e.arg(i) for i in range(nargs)]
            rec = fmt(e, args) if fmt else tuple(rig.norm(v) for v in args)
            rig.log.append((name, rec))
            r = ret(e, args) if callable(ret) else ret
            if st0:
                e.write(cell, struct.pack("<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                e.ret(r or 0, pop)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def fake(self, addr, name, nargs, **kw):
        return self.hook(addr, name, nargs, **kw)

    def func(self, name, nargs, **kw):
        """A new logged cdecl function."""
        return self.hook(self.code(b"\xc3"), name, nargs, **kw)

    def com(self, name, slots, size=64):
        """Fake COM object in memory that survives reset. slots: {slot: (nargs, ret, fmt)}, nargs with this."""
        vt = self.code(b"\0" * (4 * size))
        obj = self.code(struct.pack("<I", vt))
        for i in range(size):
            nargs, ret, fmt = slots.get(i, (1, 0, None))
            addr = self.hook(self.code(b"\xc3"), "%s.%d" % (name, i), nargs, pop=4 * nargs, ret=ret,
                             fmt=(lambda f: (lambda e, a: (self.norm(a[0]),) + tuple(f(e, a[1:]))))(fmt) if fmt else None)
            self.e.write(vt + 4 * i, struct.pack("<I", addr))
        self.objs[name] = obj
        return obj

    def with_cw(self, target, cw):
        addr = self.e.symbols[target] if isinstance(target, str) else target
        key = (addr, cw)
        if key not in self.tramps:
            cell = self.code(struct.pack("<H", cw))
            p = self.code(b"\0" * 16)
            self.e.write(p, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
            self.tramps[key] = p
        return self.tramps[key]

    def check(self, orig, ours, setup=None, cw=None, **kw):
        logs = []
        if cw is not None:
            orig, ours = self.with_cw(orig, cw), self.with_cw(ours, cw)

        def s(e):
            r = setup(e) if setup else None
            self.log = []
            logs.append(self.log)
            return r
        kw.setdefault("check_eax", False)
        res = compare(self.e, orig, ours, setup=s, normalized=True, **kw)
        assert logs[0] == logs[1], "calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
        return logs[0]


def rd(n):
    """fmt reading n bytes behind the pointer argument (first arg after this)."""
    return lambda e, a: (e.read(a[0], n),) + tuple(a[1:])


def device_slots():
    return {
        11: (3, 0, lambda e, a: (a[0], e.read(a[1], 64))),  # SetTransform
        16: (2, 0, lambda e, a: (e.read(a[0], 0x44),)),  # SetMaterial
        18: (3, 0, lambda e, a: (a[0], e.read(a[1], 0x68))),  # SetLight
        20: (3, 0, None),  # SetRenderState
        32: (8, 0, None),  # DrawIndexedPrimitiveVB
        35: (3, 0, None),  # SetTexture
        37: (4, 0, None),  # SetTextureStageState
        44: (3, 0, None),  # LightEnable
    }


class VB:
    """Fake vertex buffer whose Lock hands out a heap buffer set by the test."""

    def __init__(self, rig, name):
        self.buf = 0
        self.hr = 0
        self.cell = rig.code(b"\0" * 8)  # buffer address and hr, read at call time
        cell = self.cell

        def lock(e, a):
            buf, hr = struct.unpack("<II", e.read(cell, 8))
            if not hr:
                u32(e, a[2], buf)
            return hr
        self.obj = rig.com(name, {3: (4, lock, lambda e, a: (a[0],)), 4: (1, 0, None), 2: (1, 0, None)})

    def set(self, e, buf, hr=0):
        e.write(self.cell, struct.pack("<II", buf, hr))


def state_list(rig, e, name, n, current):
    """PtrList of logged state functions."""
    lst = e.alloc(0x14 + 4 * n)
    u32(e, lst, 0xABCDABCD, current, n, n, 0xDCBADCBA)
    for i in range(n):
        u32(e, lst + 0x14 + 4 * i, rig.state_fns[name][i])
    return lst


def make_state_fns(rig, n=8):
    rig.state_fns = {k: [rig.func("%s%d" % (k, i), 1) for i in range(n)] for k in ("A", "B", "C", "S")}


def setup_states(rig, e, rng, keep=False):
    """State lists A, B, C and the clip list, with random current entries; the index globals point into them."""
    for idx_addr in STATE_IDX:
        u32(e, idx_addr, rng.randrange(8))
    for name, addr in (("A", LIST_A), ("B", LIST_B), ("C", LIST_C), ("S", LIST_STATE)):
        cur = rng.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7])
        u32(e, addr, state_list(rig, e, name, 8, cur))


def rmatrix(rng):
    """A random rotation (as floats) from a random axis and angle."""
    import math
    ax = [rng.uniform(-1, 1) for _ in range(3)]
    n = math.sqrt(sum(a * a for a in ax)) or 1
    x, y, z = (a / n for a in ax)
    t = rng.uniform(-3, 3)
    c, s = math.cos(t), math.sin(t)
    C = 1 - c
    return [x * x * C + c, x * y * C - z * s, x * z * C + y * s,
            y * x * C + z * s, y * y * C + c, y * z * C - x * s,
            z * x * C - y * s, z * y * C + x * s, z * z * C + c]


def setup_view(e, rng):
    """gTransContext (0xBCAAE0), gScene clip rectangle, near plane and world scale."""
    m = rmatrix(rng)
    pos = [rng.uniform(-5000, 5000) for _ in range(3)]
    e.write(VIEW, f32(*m) + struct.pack("<I", rng.randrange(100)) + f64(*pos))
    e.write(VIEW + 0x40, f32(1.0, 0.75, 320.0, -240.0, 320.0, 240.0))
    e.write(SCENE + 0x20, f32(640.0, 480.0, 320.0, 240.0, 0.0, 0.0, 640.0, 480.0))
    e.write(WORLD_SCALE, f32(0.001))
    e.write(NEAR_PLANE, f32(rng.choice([409.6, 50.0, 2000.0])))
    e.write(0xB81700, f32(rng.uniform(0.5, 1.5)))
    return m, pos
