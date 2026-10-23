"""Helpers shared by the test_flatwater tests."""
import math
import struct

from emu import Emu
from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, pyfake,  # noqa: F401
                            set_handler, task, task_system, trap, u16, u32)
from test_vehicle_util import d64, floatfake, init_emu, set_events, setup_init  # noqa: F401

# globals read by the param type getters 0x4F0F20 - 0x4F1010
PARAM_GLOBALS = (0xA70C84, 0xA70CA4, 0xA71880, 0xA71870, 0xA70C6C, 0xA71874, 0xA70C7C, 0xA70C9C, 0xA71884,
                 0xA71888, 0xA7186C, 0xA70C90, 0xA70C88)
# event getters and the slots they return
EVENTS = {0x4F1A60: 0x38, 0x4F1A70: 0x39, 0x4F1210: 0x3A, 0x4F1220: 0x3B, 0x4E6610: 0x3C, 0x4E6650: 0x3D}
CWS = [0x37F, 0x7F, 0xC7F, 0x77F]


def open_emu():
    """Original task system; the parent type getters return RET[addr]."""
    e = init_emu()
    for a in (0x5034D0, 0x4C48C0):
        logfake(e, a, 0)
    e.baseline = e.snapshot()
    return e


def setup_open(e):
    setup_init(e)
    for i, a in enumerate(PARAM_GLOBALS):
        u32(e, a, 0x700 + i)
    root = e.call(0x4018F0)[0] & 0xFFFF
    parent = e.call(0x401900, cstr(e, b"Parent"), 0x40, root, 0)[0] & 0xFFFF
    RET[0x5034D0] = parent
    RET[0x4C48C0] = parent


def fake_emu(fakes, floats=()):
    """An Emu whose callees are loggers: fakes is {addr: nargs or (nargs, kw)},
    floats is {addr: nargs} for callees returning RET[addr] in st0."""
    e = Emu()
    for a, v in EVENTS.items():
        logfake(e, a, 0, ret=v)
    for a, v in fakes.items():
        n, kw = v if isinstance(v, tuple) else (v, {})
        logfake(e, a, n, **kw)
    for a, n in dict(floats).items():
        floatfake(e, a, n)
    e.baseline = e.snapshot()
    return e


def run(e, orig, ours, setup, rets=None, **kw):
    RET.clear()

    def s(e):
        RET.clear()
        RET.update(rets or {})
        return setup(e)
    kw.setdefault("check_eax", False)
    return check(e, orig, ours, setup=s, normalized=True, **kw)


def with_cw(e, target, cw, cache={}):
    """Entry that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    key = (id(e), addr, cw)
    if key not in cache:
        p = e.fake_top
        e.fake_top += 32
        e.write(p + 16, struct.pack("<H", cw))
        e.write(p, b"\xd9\x2d" + struct.pack("<I", p + 16) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        cache[key] = p
    return cache[key]


def children(e, parent, n, size=0x40, type_=0):
    out = []
    for _ in range(n):
        c = task(e, size, type_)
        add_child(e, parent, c)
        out.append(c)
    return out


def project(e):
    """View_ProjectPoint: out = in.xy / in.z * 300 + centre, logs the input."""
    out, src = e.arg(0), e.arg(1)
    x, y, z = struct.unpack("<3f", e.read(src, 12))
    if LOG:
        LOG[-1].append(("project", e.read(src, 12)))
    f32(e, out, x / z * 300.0 + 320.0, y / z * 300.0 + 240.0)
    e.ret(0)


def view(e, yaw, pitch, roll=0.0):
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    cr, sr = math.cos(roll), math.sin(roll)
    right = (cy * cr, sy * cr, sr)
    up = (-sy * cp - cy * sr * sp, cy * cp - sy * sr * sp, cr * sp)
    fwd = (sy * sp - cy * sr * cp, -cy * sp - sy * sr * cp, cr * cp)
    f32(e, 0xBCAAE0, *right, *up, *fwd)
    u32(e, 0xBCAB04, 7)
    f32(e, 0xBCAB28, 300.0, 300.0)
    f32(e, 0xBCABB8, 0.0, 0.0, 640.0, 480.0, 320.0, 240.0)
