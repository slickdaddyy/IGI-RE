"""Helpers for the Cabinet, Switch and Door tests (no tests in here)."""
import random
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import Emu, compare

HANDLERS = 0xA96AE0
TICK_INFO = 0xBC2360
NAN = float("nan")

# getters of event slots, task types and param types, each faked to return 0x20 + its index
GETTERS = [0x4F1A60, 0x4F1A70, 0x4F1210, 0x4F1220, 0x4167F0, 0x416800, 0x416810, 0x416820, 0x416840, 0x4C5800,
           0x467D90, 0x47E6F0, 0x450CA0, 0x5096A0, 0x4D9FB0, 0x4E0830, 0x4C48C0, 0x4F0F50, 0x4F0F70, 0x4F0FC0,
           0x4F0FD0, 0x4F1000, 0x4F1010]
EV_CHANGED = 0x20 + GETTERS.index(0x4F1A70)
EV_DAC = 0x20 + GETTERS.index(0x4F1220)
EV_MOVE = 0x20 + GETTERS.index(0x4C5800)
EV_MAGIC_MOVE = 0x20 + GETTERS.index(0x4D9FB0)


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

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None):
        """deref: {arg index: byte count} logs the bytes instead of the pointer.
        st0: 'd' returns rets[addr] in st(0). ret may be a function (emu, args) -> value."""
        e = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code(b"\xdd\x05" + struct.pack("<I", cell) + b"\xc3") if st0 else None

        def cb(uc, a, size, user):
            em = e.e
            args = [em.arg(i) for i in range(nargs)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = [em.read(v, deref[i]) if deref and i in deref else em.normalize(v) for i, v in enumerate(args)]
            e.log.append((a, tuple(rec)))
            r = e.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r or 0)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def handler(self, nargs, deref=None, key=0):
        """A task handler that only logs; the same one for the same arguments."""
        k = (nargs, str(deref), key)
        if k not in self.tramps:
            self.tramps[k] = self.add(self.code(b"\xc3"), nargs, deref=deref)
        return self.tramps[k]


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


def check(e, fk, orig, ours, setup=None, cw=None, **kw):
    """compare() plus equal logs of the fakes. cw: x87 control word to run with
    (0x7F is the single precision the game runs at)."""
    logs = []
    if cw is not None:
        orig, ours = with_cw(e, fk, orig, cw), with_cw(e, fk, ours, cw)

    def s(em):
        r = setup(em) if setup else None
        fk.log = []
        logs.append(fk.log)
        return r
    kw.setdefault("check_eax", False)
    res = compare(e, orig, ours, setup=s, normalized=True, **kw)
    assert logs[0] == logs[1], "fake calls differ:\n orig %s\n ours %s" % (logs[0], logs[1])
    return res, logs[0]


def masked(*masks):
    def fmt(em, args):
        return tuple(em.normalize(a & (masks[i] if i < len(masks) else 0xFFFFFFFF)) for i, a in enumerate(args))
    return fmt


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def bits(v):
    return struct.unpack("<I", struct.pack("<I", v))[0]


def u32(e, p, *v):
    e.write(p, struct.pack("<%dI" % len(v), *[x & 0xFFFFFFFF for x in v]))


def new_task(e, type_, size):
    t = e.alloc(size)
    e.write(t + 8, struct.pack("<III", t + 0xC, 0, t + 8))
    e.write(t + 0x1C, struct.pack("<H", type_))
    return t


def add_child(e, parent, child):
    tailpred = e.u32(parent + 0x10)
    e.write(child, struct.pack("<II", parent + 0xC, tailpred))
    e.write(tailpred, struct.pack("<I", child))
    e.write(parent + 0x10, struct.pack("<I", child))
    e.write(child + 0x14, struct.pack("<I", parent))


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


def children(e, fk, t, slot, n, nargs=2, deref=None, type0=0x40):
    """n children of t whose slot handler logs."""
    h = fk.handler(nargs, deref=deref)
    for i in range(n):
        c = new_task(e, type0 + i, 0x40)
        add_child(e, t, c)
        set_handler(e, slot, type0 + i, h)


def rmatrix(rng, unk=None):
    return f32(*[rng.uniform(-1.5, 1.5) for _ in range(9)]) + struct.pack("<i", rng.randrange(100) if unk is None
                                                                          else unk)


def rvec(rng, scale=5000.0):
    return f64(*[rng.uniform(-scale, scale) for _ in range(3)])


def obj(e, type_, size, rng, scale=(1.0, 2.0, 1.5)):
    """A SectorInteriorObject with a model, matrix and scale."""
    t = new_task(e, type_, size)
    e.write(t + 0x20, rvec(rng))
    u32(e, t + 0x68, 0x6868, 0x6C6C)
    e.write(t + 0x70, rmatrix(rng))
    e.write(t + 0x98, f32(*scale))
    return t


SCALES = [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 1.0, 1.0), (0.5, 0.25, 2.5), (NAN, 1.0, 2.0), (1.0, NAN, 2.0),
          (1.0, 2.0, NAN)]


def tick(e, v):
    p = e.u32(TICK_INFO)
    if not p:
        p = e.alloc(0x80)
        u32(e, TICK_INFO, p)
    u32(e, p + 0x68, v)


def show_pos(em, a, i):
    return em.read(a[i], 24)


def make_emu():
    e = Emu()
    fk = Fakes(e)
    for i, g in enumerate(GETTERS):
        fk.add(g, 0, ret=0x20 + i)
    fk.add(0x401900, 4, ret=0x7777, fmt=masked(~0, ~0, 0xFFFF))  # TaskType_New
    fk.add(0x401400, 3, fmt=masked(0xFFFF, 0xFF))  # TaskType_SetHandler
    fk.add(0x401530, 3, fmt=masked(0xFFFF))  # TaskType_SetHandler2
    fk.add(0x401A20, 1, fmt=masked(0xFFFF))  # TaskType_Free
    fk.add(0x4F1950, 1, ret=0x1950)  # ParamTable_New
    fk.add(0x4F1A80, 6, fmt=lambda em, a: (a[0], (a[2] - a[1]) & 0xFFFFFFFF, a[3], a[4], em.normalize(a[5])))
    fk.add(0x4168A0, 0, ret=0x1680)  # Game_GetTaskList0
    fk.add(0x416920, 0, ret=0x9999)  # Game_GetWorld
    fk.add(0x4C1790, 2)  # QTaskList_Add
    fk.add(0x4C17C0, 2)  # QTaskList_Remove
    fk.add(0x4C1830, 1)  # QTaskList_Free
    fk.add(0x4F1050, 1)  # Task_GetId
    fk.add(0x4015F0, 1)  # Task_Delete
    fk.add(0x4DCDD0, 1)
    fk.add(0x4C7560, 2)  # DynCube_Remove
    fk.add(0x4CEC10, 1, ret=0xC0DE, fmt=lambda em, a: (em.read(a[0], 16),))  # VirModel_Find
    fk.add(0x4C48D0, 2)  # RigidDynCubeObj_SetModel
    fk.add(0x4D0950, 1, ret=3.25, st0="d")  # VirModel_GetRadius
    fk.add(0x4C6940, 4, fmt=lambda em, a: (a[0], em.normalize(a[1]), show_pos(em, a, 2), a[3]))  # DynCube_Move
    fk.add(0x4C7140, 5, fmt=lambda em, a: (a[0], em.normalize(a[1]), show_pos(em, a, 2), a[3], a[4]))  # DynCube_Add
    fk.add(0x4C5710, 3)  # RigidDynCubeObj_Init
    fk.add(0x4C3440, 1, ret=0x3440)  # DynCubeObj_GetPosition
    fk.add(0x4DCF00, 3)
    fk.add(0x4DCCB0, 3)  # Sector_AddObject
    fk.add(0x4B3BE0, 2)  # Matrix_SetRotZ
    fk.add(0x4B3790, 3)  # Matrix_RotateZ
    fk.add(0x4E0930, 2)
    fk.add(0x4C4950, 3)
    fk.add(0x4871C0, 2)
    fk.add(0x401CC0, 2, fmt=masked(~0, 0xFFFF))  # Task_FindAncestor
    fk.add(0x4F16C0, 1)  # VarString_HasExpr
    fk.add(0x4F16E0, 2, ret=0.0, st0="d")  # VarString_EvalReal
    fk.add(0x4F1320, 1)  # VarString_Init
    fk.add(0x4F1340, 1)  # VarString_Free
    fk.add(0x4F1400, 2)  # VarString_Bind
    fk.add(0x4E6B00, 2, fmt=lambda em, a: (em.normalize(a[0]), em.read_cstr(a[1])))  # Sound_Play
    fk.add(0x4E6C30, 2, fmt=lambda em, a: (a[0], show_pos(em, a, 1)))  # Sound_SetPos
    fk.add(0x4E6C00, 1)  # Sound_Stop
    fk.add(0x4B7EE0, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))  # Script_RegisterBool8
    fk.add(0x4B7FA0, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))  # Script_RegisterInt2
    fk.add(0x4B80A0, 1, fmt=lambda em, a: (em.read_cstr(a[0]),))  # Script_RemoveSymbol

    def get_name(em, a):
        em.write(a[0], b"Thing_12\0")
    fk.add(0x4F1270, 2, ret=get_name, fmt=lambda em, a: (a[1],))  # Task_GetName

    def point(em, a):
        em.write(a[0], fk.point)
    fk.point = f64(10.0, -20.0, 30.0)
    fk.add(0x414E20, 3, ret=point, fmt=lambda em, a: (a[1], a[2]))  # Model_GetPoint
    fk.add(0x416AD0, 3, fmt=lambda em, a: (a[0], a[1], show_pos(em, a, 2)))
    e.baseline = e.snapshot()
    return e, fk
