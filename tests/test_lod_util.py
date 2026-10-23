"""Helpers for the lod tests: Python fakes that log by value, x87 control word, model builders."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import ErrorShown, compare

RENDER_FUNCS = 0xA94E84
TASK_HANDLERS = 0xA96AE0
CTX = 0xBCAAE0  # gTransContext
SCENE = 0xBCABA0


class Fakes:
    """Python-side replacements of original functions that log their arguments."""

    def __init__(self, e):
        self.e = e
        self.log = []
        self.rets = {}
        self.tramps = {}

    def code(self, data):
        p = self.e.fake_top
        self.e.fake_top += (len(data) + 15) & ~15
        self.e.write(p, data)
        return p

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None, name=None):
        """deref: {arg index: byte count or 's'} logs bytes / a C string instead of the pointer.
        st0: 'f' or 'd' returns ret as a float in st(0). ret may be callable(emu, args)."""
        fk = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = None
        if st0:
            stub = self.code((b"\xd9\x05" if st0 == "f" else b"\xdd\x05") + struct.pack("<I", cell) + b"\xc3")

        def cb(uc, a, size, user):
            em = fk.e
            args = [em.arg(i) for i in range(nargs)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = []
                for i, v in enumerate(args):
                    d = (deref or {}).get(i)
                    if d == "s":
                        rec.append(em.read_cstr(v))
                    elif d and v:
                        rec.append(em.read(v, d))
                    else:
                        rec.append(em.normalize(v))
            fk.log.append((name or a, tuple(rec)))
            r = fk.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<f" if st0 == "f" else "<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r or 0)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def func(self, nargs=0, ret=0, deref=None, st0=None, fmt=None, name=None):
        """A new function address that only logs."""
        return self.add(self.code(b"\xc3"), nargs, ret, deref, st0, fmt, name)


def with_cw(e, fk, target, cw):
    """Entry point that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    key = (addr, cw)
    if key not in fk.tramps:
        cell = fk.code(struct.pack("<H", cw))
        p = fk.code(b"\0" * 16)
        e.write(p, b"\xd9\x2d" + struct.pack("<I", cell) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        fk.tramps[key] = p
    return fk.tramps[key]


def check(e, fk, orig, ours, setup=None, normalized=True, cw=None, **kw):
    """compare() plus equal logs of the Python fakes."""
    logs = []
    if cw is not None:
        orig, ours = with_cw(e, fk, orig, cw), with_cw(e, fk, ours, cw)

    def s(em):
        r = setup(em) if setup else None
        fk.log = []
        logs.append(fk.log)
        return r
    res = compare(e, orig, ours, setup=s, normalized=normalized, **kw)
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return res


def check_error(e, fk, orig, ours, setup):
    msgs, logs = [], []
    for target in (orig, ours):
        e.reset()
        args = setup(e) or ()
        fk.log = []
        logs.append(fk.log)
        with pytest.raises(ErrorShown) as err:
            e.call(target, *args)
        msgs.append(str(err.value))
    assert msgs[0] == msgs[1]
    assert logs[0] == logs[1]


def u32(e, p, v):
    e.write(p, struct.pack("<I", v & 0xFFFFFFFF))


def f32(e, p, *v):
    e.write(p, struct.pack("<%df" % len(v), *v))


def f64(e, p, *v):
    e.write(p, struct.pack("<%dd" % len(v), *v))


def put(e, data):
    p = e.alloc(len(data))
    e.write(p, data)
    return p


def cstr(e, s):
    return put(e, s + b"\0")


def set_func(e, table, index, fn):
    u32(e, table + 4 * index, fn)


# --- models -----------------------------------------------------------------

def mesh_header(e, center=(0, 0, 0), sphere=1.0, complexity=100, radius=1.0, verts=(), atta=0, anims=None):
    h = e.alloc(0x90)
    e.write(h, struct.pack("<f", 0.1))
    f32(e, h + 0x30, *center)
    f32(e, h + 0x3C, sphere)
    u32(e, h + 0x60, complexity)
    f32(e, h + 0x78, radius)
    e.write(h + 0x7C, struct.pack("<hh", len(verts), atta))
    if verts:
        u32(e, h + 0x88, put(e, b"".join(struct.pack("<4f", *v, 0.0) for v in verts)))
    if atta:
        u32(e, h + 0x8C, put(e, bytes(range(256)) * ((atta * 0x44) // 256 + 1)))
    return h


def model_lod(e, name=b"lod", header=None, textures=0, anims=()):
    """ModelLod -> Mesh -> MESH header. anims: (frames, frame, slot, saved) tuples."""
    lod = e.alloc(0x1C)
    mesh = e.alloc(0x18)
    u32(e, lod + 8, cstr(e, name))
    u32(e, lod + 0xC, mesh)
    u32(e, mesh + 0xC, header or mesh_header(e))
    if textures:
        u32(e, lod + 0x10, put(e, b"".join(struct.pack("<I", 0x1000 + i) for i in range(textures))))
    if anims:
        u32(e, mesh + 0x10, len(anims))
        u32(e, mesh + 0x14, put(e, b"".join(struct.pack("<5i", *a, 0) for a in anims)))
    return lod


def vir_model(e, lods, table=None, count=None):
    vm = e.alloc(0x48)
    for i, l in enumerate(lods):
        u32(e, vm + 4 * i, l)
    if table is not None:
        u32(e, vm + 0x24, 1)
        f32(e, vm + 0x28, *table)
    e.write(vm + 0x3C, struct.pack("<h", len(lods) if count is None else count))
    return vm


def set_view(e, rng, fov=None):
    """Random camera: matrix, position, fov."""
    f32(e, CTX, *[rng.uniform(-1, 1) for _ in range(9)])
    f64(e, CTX + 0x28, *[rng.uniform(-3000, 3000) for _ in range(3)])
    f32(e, CTX + 0x40, fov if fov is not None else rng.uniform(0.3, 1.5), rng.uniform(0.3, 1.2))
