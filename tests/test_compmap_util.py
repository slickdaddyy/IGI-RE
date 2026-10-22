"""Helpers shared by the test_compmap tests (computer screens)."""
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import Emu, ErrorShown, compare

HANDLERS = 0xA96AE0
DISPLAY = 0xC28B40
GAME = 0xBC2360
MAP_ENABLED = 0x53FD98
DRAW_EVENT = 0x5488C8
DRAW_SLOT = 0x23
MENU = 0x5BE2B0
MAP = 0x5BDD98
COMPUTER = 0x5BE2CC
MOUSE = 0xC28F8C
ZOOM = 0x5BDDA0
MAP_POS = 0x5BDD78
RES = 0x5BDDB0
FONT = 0x7100


def u32(e, a, *vals):
    e.write(a, struct.pack("<%dI" % len(vals), *[v & 0xFFFFFFFF for v in vals]))


def f32(e, a, *vals):
    e.write(a, struct.pack("<%df" % len(vals), *vals))


def d64(e, a, *vals):
    e.write(a, struct.pack("<%dd" % len(vals), *vals))


def u16(e, a, v):
    e.write(a, struct.pack("<H", v & 0xFFFF))


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


class Fakes:
    """Python replacements of original functions that log their arguments.
    ret may be a value or fn(emu, args); st0 returns ret as a float in st(0).
    masks maps an arg index to a mask (u16 and u8 args are pushed with junk above)."""

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

    def add(self, addr, nargs=0, ret=0, deref=None, strings=(), st0=None, fmt=None, quiet=False, masks=None):
        fk = self
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = self.code(b"\xdd\x05" + struct.pack("<I", cell) + b"\xc3") if st0 else None

        def cb(uc, a, size, user):
            em = fk.e
            args = [em.arg(i) for i in range(nargs)]
            if masks:
                args = [v & masks.get(i, 0xFFFFFFFF) for i, v in enumerate(args)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = []
                for i, v in enumerate(args):
                    if i in strings:
                        rec.append(em.read_cstr(v) if v else None)
                    elif deref and i in deref:
                        rec.append(em.read(v, deref[i]) if v else None)
                    else:
                        rec.append(em.normalize(v))
            if not quiet:
                fk.log.append((a, tuple(rec)))
            r = fk.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r or 0)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def handler(self, nargs=1, deref=None, ret=0):
        """A function pointer that only logs."""
        return self.add(self.code(b"\xc3"), nargs, ret=ret, deref=deref)


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
    """compare() plus equal logs of the Python fakes. cw: x87 control word to run with."""
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
    return res, logs[0]


def check_error(e, orig, ours, setup):
    msgs = []
    for target in (orig, ours):
        e.reset()
        args = setup(e) or ()
        try:
            e.call(target, *args)
        except ErrorShown as err:
            msgs.append(str(err))
        else:
            raise AssertionError("no error shown")
    assert msgs[0] == msgs[1]
    return msgs[0]


def set_handler(e, slot, type_, fn):
    u32(e, HANDLERS + 4 * (slot * 0x180 + type_), fn)


def task(e, size, type_=0, parent=0):
    """A zeroed task with an empty child list."""
    t = e.alloc(size)
    u32(e, t + 8, t + 0xC, 0, t + 8)
    u32(e, t + 0x14, parent)
    u16(e, t + 0x1C, type_)
    return t


def add_child(e, parent, child):
    """Append child to parent's child list (List_AddTail)."""
    lst = parent + 8
    tailpred = e.u32(lst + 8)
    u32(e, child, lst + 4, tailpred)
    u32(e, tailpred, child)
    u32(e, lst + 8, child)
    u32(e, child + 0x14, parent)


def text_new(e, args):
    t = e.alloc(0x28)
    u32(e, t + 0x20, 0x5A5A)
    u32(e, t + 0x24, 0x100)
    return t


def task_new(e, args):
    t = task(e, 0x100, args[1] & 0xFFFF, args[0])
    return t


def res_load(e, args):
    return cstr(e, b"res:" + e.read_cstr(args[0]))


def make_emu(fakes=True):
    """An Emu with a display, a game block, a menu manager and the common fakes."""
    e = Emu()
    fk = Fakes(e)
    u32(e, DISPLAY + 4, 800, 600)
    game = e.alloc(0x200)
    u32(e, GAME, game)
    u32(e, game + 0x120, 0x7000)
    u32(e, DRAW_EVENT, DRAW_SLOT)
    if fakes:
        fk.add(0x4012A0, 3, ret=task_new, masks={1: 0xFFFF})                    # Task_New
        fk.add(0x4B5F00, 2, ret=res_load, strings=(0,))      # Resource_Load
        fk.add(0x418E40, 2, ret=lambda e, a: FONT + len(e.read_cstr(a[1])), strings=(1,))  # FindFont
        fk.add(0x4B6EC0, 4, ret=text_new, strings=(3,))      # Text_New
        fk.add(0x4B7DF0, 1, ret=37)                          # Text_GetWidth
        fk.add(0x4B7E00, 1, ret=13)                          # Text_GetHeight
        fk.add(0x4B6CE0, 1, ret=lambda e, a: 20 + (a[0] & 0x1F))  # Sprite_Width
        fk.add(0x4B6D00, 1, ret=lambda e, a: 9 + (a[0] & 0xF))    # Sprite_Height
        fk.add(0x4B4910, 10)                                 # Draw_RectZ
        fk.add(0x4B50B0, 11)                                 # QVertex_DrawFrame
        fk.add(0x4B5730, 15)                                 # QVertex_DrawLine
        fk.add(0x4B6F50, 1, fmt=lambda e, a: (e.normalize(a[0]), e.read(a[0], 0x28)))  # Text_DrawAlt
        fk.add(0x4B6F30, 1)                                  # Text_Free
        fk.add(0x4B6E90, 2, strings=(1,))                    # Text_SetString
        fk.add(0x4E7200, 3, deref={2: 12}, strings=(0,))     # play sound
        fk.add(0x4B7050, 2, ret=lambda e, a: 7 * len(e.read_cstr(a[1])) + 3, strings=(1,))  # Sprite_StringWidth
        fk.add(0x4B70F0, 2, ret=lambda e, a: 14 + len(e.read_cstr(a[1])) % 3, strings=(1,))  # Sprite_GetHeight
    e.baseline = e.snapshot()
    return e, fk


def make_menu(e):
    """A ComputerMenu task in gMenu."""
    m = task(e, 0x3C)
    u32(e, MENU, m)
    return m
