"""Helpers shared by the test_computer tests."""
import random
import struct

from emu import STACK_BASE, STACK_SIZE

from test_menu_util import (LOG, RET, add_child, check, cstr, f32, handler, logfake, pyfake, set_handler,  # noqa: F401
                            task, u16, u32)
from test_vehicle_util import d64, floatfake, init_emu, run, setup_init, vfake  # noqa: F401

# event getters outside the range and the slots they return
EVENTS = {0x416840: 0x30, 0x416850: 0x31, 0x416860: 0x32, 0x416880: 0x33, 0x489610: 0x34, 0x4D9FB0: 0x35,
          0x4D14D0: 0x36, 0x4E7530: 0x37, 0x4F1A60: 0x38, 0x4F1A70: 0x39}
# type getters outside the range: they return RET[addr] (a type made by setup_open)
TYPES = (0x504DB0, 0x4D9FA0, 0x4C48C0, 0x5034D0, 0x4F45F0, 0x42BC20, 0x447D20)
# param type getters used by the param tables
PARAMS = {0x4F0F00: 0x710, 0x4F0F50: 0x711, 0x4F0FA0: 0x712, 0x4F0FC0: 0x713, 0x4F0FE0: 0x714, 0x4F0FF0: 0x715,
          0x4F1010: 0x716}
CW = {}


def with_cw(e, target, cw):
    """Entry that loads an x87 control word and jumps to target."""
    addr = e.symbols[target] if isinstance(target, str) else target
    if (addr, cw) not in CW:
        p = e.fake_top
        e.fake_top += 32
        e.write(p + 16, struct.pack("<H", cw))
        e.write(p, b"\xd9\x2d" + struct.pack("<I", p + 16) + b"\xe9" + struct.pack("<i", addr - (p + 11)))
        e.baseline = e.snapshot()
        CW[(addr, cw)] = p
    return CW[(addr, cw)]


def make_emu():
    """An Emu with the original task system and the outside getters as loggers."""
    e = init_emu()
    for a, v in EVENTS.items():
        logfake(e, a, 0, ret=v)
    for a in TYPES:
        logfake(e, a, 0)
    for a, v in PARAMS.items():
        logfake(e, a, 0, ret=v)
    e.baseline = e.snapshot()
    return e


def setup_open(e):
    """Task system plus one parent type returned by every type getter."""
    setup_init(e)
    root = e.call(0x4018F0)[0] & 0xFFFF
    parent = e.call(0x401900, cstr(e, b"Parent"), 0x40, root, 0)[0] & 0xFFFF
    for a in TYPES:
        RET[a] = parent


def isa_fake(e):
    """TaskType_IsA answers RET[('isa', type)]."""
    logfake(e, 0x401CF0, 2, masks={0: 0xFFFF, 1: 0xFFFF},
            effect=lambda e, a: RET.get(("isa", a[0] & 0xFFFF), 0))


def task_array(e, items):
    """A TaskArray: count, unused, then the tasks."""
    p = e.alloc(8 + 4 * len(items))
    u32(e, p, len(items), 0, *items)
    return p


def mat(e, p, rng, unk=3):
    f32(e, p, *[rng.uniform(-1, 1) for _ in range(9)])
    u32(e, p + 0x24, unk)


def obj(e, size, type_, scale=(1.0, 2.0, 1.5), seed=1):
    """A RigidDynCubeObj style task with position, model, matrix and scale set."""
    rng = random.Random(seed)
    t = task(e, size, type_)
    d64(e, t + 0x20, 1000.0, -2000.0, 30.5)
    u32(e, t + 0x68, 0x6868, 0x6C6C)
    mat(e, t + 0x70, rng)
    f32(e, t + 0x98, *scale)
    return t


def com(e, name, nargs, effects=None, derefs=None, size=64):
    """A fake COM object: vtable slot i logs (name.i, args) and pops nargs[i] (with this, default 1).
    effects[i](e, args) may write outputs and return the HRESULT; derefs[i] maps an argument
    index to a byte count logged instead of the pointer."""
    from test_menu_util import trap
    effects = effects or {}
    derefs = derefs or {}
    obj = e.alloc(8)
    vt = e.alloc(4 * size)
    u32(e, obj, vt)
    for i in range(size):
        def fn(e, i=i):
            n = nargs.get(i, 1)
            args = [e.arg(k) for k in range(n)]
            if LOG:
                d = derefs.get(i, {})
                LOG[-1].append(("%s.%d" % (name, i),) + tuple(e.read(a, d[k]) if k in d else
                                                              "stack" if STACK_BASE <= a < STACK_BASE + STACK_SIZE
                                                              else e.normalize(a) for k, a in enumerate(args)))
            r = effects[i](e, args) if i in effects else 0
            e.ret(r or 0, 4 * n)
        u32(e, vt + 4 * i, trap(e, "com_%s_%d" % (name, i), fn))
    return obj
