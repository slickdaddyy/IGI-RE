"""HumanSoldierFemale (end), shared helpers for the test_rpg tests."""
import struct

import pytest
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP

from emu import Emu, compare

HANDLERS = 0xA96AE0


def f32(*v):
    return struct.pack("<%df" % len(v), *v)


def f64(*v):
    return struct.pack("<%dd" % len(v), *v)


def i32(*v):
    return struct.pack("<%di" % len(v), *v)


class Fakes:
    """Python replacements of original functions that log their arguments."""

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

    def add(self, addr, nargs=0, ret=0, deref=None, st0=None, fmt=None, mask=None, effect=None):
        """deref: {arg: byte count} logs the bytes instead of the pointer; mask: {arg: mask};
        st0: 'f' or 'd' returns ret in st(0); ret may be callable(e, args)."""
        self.rets[addr] = ret
        cell = self.code(b"\0" * 8) if st0 else None
        stub = None
        if st0:
            stub = self.code((b"\xd9\x05" if st0 == "f" else b"\xdd\x05") + struct.pack("<I", cell) + b"\xc3")
        fk = self

        def cb(uc, a, size, user):
            em = fk.e
            args = [em.arg(i) for i in range(nargs)]
            if fmt:
                rec = fmt(em, args)
            else:
                rec = []
                for i, v in enumerate(args):
                    if deref and i in deref:
                        rec.append(em.read(v, deref[i]))
                    elif mask and i in mask:
                        rec.append(v & mask[i])
                    else:
                        rec.append(em.normalize(v))
            fk.log.append((a, tuple(rec)))
            if effect:
                effect(em, args)
            r = fk.rets[a]
            if callable(r):
                r = r(em, args)
            if st0:
                em.write(cell, struct.pack("<f" if st0 == "f" else "<d", r))
                uc.reg_write(UC_X86_REG_EIP, stub)
            else:
                em.ret(r)
        self.e.uc.hook_add(UC_HOOK_CODE, cb, begin=addr, end=addr)
        return addr

    def stubs(self, n, nargs=2):
        """n logging functions, e.g. for a state table."""
        out = []
        for _ in range(n):
            out.append(self.add(self.code(b"\xc3"), nargs))
        return out


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
    """compare() plus equal logs of the Python fakes."""
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
    return res


def printf_fmt(e, args):
    """Human_printf(h, fmt, ...): the text and the argument words it uses."""
    text = e.read_cstr(args[1])
    n = 2 if b"f" in text.split(b"%", 1)[-1] else int(b"%" in text)
    return (args[0], text) + tuple(args[2:2 + n])


def ai_msg_fmt(data_bytes):
    """SoldierAI_Message(ai, msg): msg without its uninitialised word, data by content."""
    def fmt(e, args):
        w = struct.unpack("<7I", e.read(args[1], 28))
        data = e.read(w[4], data_bytes) if data_bytes else None
        return (args[0], w[0], w[1], w[2], data, w[5], w[6])
    return fmt


CWS = [0x37F, 0x7F]


# --- HumanSoldierFemale ---------------------------------------------------------

def female_emu():
    e = Emu()
    fk = Fakes(e)
    for a, n in ((0x45D360, 1), (0x460CF0, 1), (0x4610C0, 1), (0x4613D0, 1), (0x4614B0, 1), (0x462CF0, 1),
                 (0x462E50, 1), (0x463080, 1), (0x4538F0, 1), (0x464800, 2), (0x464BD0, 2), (0x463AD0, 3),
                 (0x4637C0, 3), (0x465970, 4), (0x4C1830, 1)):
        fk.add(a, n)
    fk.add(0x401A20, 1, mask={0: 0xFFFF})
    fk.add(0x4EC1E0, 2, effect=lambda em, a: em.write(a[0] + 8, struct.pack("<I", cfg["bits"])))
    fk.add(0x4D2FB0, 3, effect=lambda em, a: em.write(a[1], f32(*cfg["delta"])))
    fk.add(0x413A80, 0, ret=1.75, st0="f")
    fk.add(0x463680, 1, ret=0x1234)
    fk.add(0x460C80, 4, fmt=printf_fmt)
    fk.add(0x489C60, 2, ret=lambda em, a: cfg["part"])
    fk.add(0x489E10, 2, ret=0x5A5A0)
    fk.add(0x450D50, 2, fmt=lambda em, a: ai_msg_fmt(cfg["msgdata"])(em, a))
    fk.add(0x465800, 2, deref={0: 0}, effect=lambda em, a: em.write(a[0], f64(*cfg["cam"])))
    fk.add(0x4B3130, 1, deref={0: 24}, st0="d",
           ret=lambda em, a: sum(x * x for x in struct.unpack("<3d", em.read(a[0], 24))) ** 0.5)
    fk.add(0x416C90, 5, ret=lambda em, a: cfg["falloff"], st0="d")
    fk.add(0x4C3440, 1, ret=lambda em, a: cfg["pos"][a[0]])
    fk.add(0x4B3100, 1, deref={0: 24}, effect=lambda em, a: em.write(a[0], f64(*cfg["norm"])))
    e.baseline = e.snapshot()
    return e, fk


fe, ff = female_emu()
cfg = {}


def soldier(e, size=0xD50, ai=True, fields=()):
    h = e.alloc(size)
    e.write(h + 0x20, f64(1000.5, -200.25, 30.125))
    e.write(h + 0x78, f32(0.8, -0.6, 0.0, 0.6, 0.8, 0.0, 0.0, 0.0, 1.0))
    e.write(h + 0xA0, f32(512.0))
    e.write(h + 0x254, f32(10.0, 100.0))
    for i in range(17):
        e.write(h + 0x260 + 8 * i, f32(1.5 + i, 0.5 + 0.25 * i))
    e.write(h + 0x2F4, struct.pack("<I", 0x45))
    e.write(h + 0x4EC, struct.pack("<I", e.alloc(0x100)))
    e.write(h + 0x664, f32(3.0, -2.0, -40.5))
    e.write(h + 0x710, f64(1234.5) + f32(0.25))
    e.write(h + 0x728, f32(1.1, 2.2, 3.3))
    e.write(h + 0x75C, f32(0.1, 0.2, 0.3))
    if ai:
        a = e.alloc(0x4000)
        e.write(h + 0x800, struct.pack("<I", a))
    e.write(h + 0xCF6, b"\x01")
    e.write(h + 0xD04, f32(0.33, 1.25))
    for off, data in fields:
        e.write((e.u32(h + 0x800) if off >= 0x10000 else h) + (off & 0xFFFF), data)
    return h


def test_female_close():
    def setup(e):
        e.write(0x5BDA84, struct.pack("<I", 0x777))
        e.write(0x5BDA88, struct.pack("<H", 0x41))
    check(fe, ff, 0x455960, "SoldierFemale_Close", setup=setup)


def test_female_gettype():
    def setup(e):
        e.write(0x5BDA88, struct.pack("<H", 0x41))
    check(fe, ff, 0x455980, "SoldierFemale_GetType", setup=setup, check_eax=True, eax_mask=0xFFFF)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("locked", [0, 1])
@pytest.mark.parametrize("scaled,ai3d38,attach,toggle,bits", [
    (0, 4, 0, 0.0, 0x10004), (0, 3, 1, 1.0, 0x10000), (1, 4, 0, 0.0, 0x4), (0, 0, 0, -0.5, 0x0),
    (0, None, 1, 0.0, 0x10005)])
def test_female_input(cw, locked, scaled, ai3d38, attach, toggle, bits):
    def setup(e):
        cfg.clear()
        cfg.update(bits=bits, delta=(13.5, -7.25, 0.875))
        fields = [(0x250, bytes([locked])), (0x52C, bytes([scaled])), (0x76C, i32(attach * 0x4444)),
                  (0xD00, b"\x01")]
        if ai3d38 is not None:
            fields.append((0x13D38, i32(ai3d38)))
        h = soldier(e, ai=ai3d38 is not None, fields=fields)
        e.write(h + 0x110, f32(*[100 + i for i in range(40)]))
        inp = [i * 0.5 for i in range(40)]
        inp[20] = toggle
        src = e.alloc(160)
        e.write(src, f32(*inp))
        msg = e.alloc(8)
        e.write(msg, struct.pack("<II", 0, src))
        return h, msg
    check(fe, ff, 0x455070, "SoldierFemale_InputHandler", setup=setup, cw=cw)


def hit_setup(part, headshot, ai, weapon_kind, dmg, total, sum_):
    def setup(e):
        cfg.clear()
        cfg.update(part=part, msgdata=0)
        fields = [(0x254, f32(total)), (0xD08, f32(sum_))]
        if ai is not None:
            fields += [(0x13ABC, bytes([ai, headshot]))]
        h = soldier(e, ai=ai is not None, fields=fields)
        w = e.alloc(0x700)
        e.write(w + 0x610, i32(weapon_kind))
        hit = e.alloc(0x1C)
        e.write(hit, i32(7) + f32(dmg, 0.25, -0.5, 0.75) + struct.pack("<II", 0x3333, w))
        return h, hit
    return setup


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("part,headshot,ai,kind,dmg,total,sum_", [
    (3, 0, 0, 1, 5.0, 10.0, 0.1), (15, 1, 0, 2, 5.0, 10.0, 0.0), (16, 0, 0, 2, 5.0, 10.0, 0.5),
    (16, 1, None, 2, 95.0, 10.0, 0.0), (2, 0, 0, 2, 400.0, 10.0, 0.0), (5, 1, 1, 2, 5.0, 10.0, 0.0),
    (4, 0, None, 1, 0.01, 10.0, 0.3), (15, 1, 0, 1, 33.3, 90.0, 0.31)])
def test_female_hit(cw, part, headshot, ai, kind, dmg, total, sum_):
    check(fe, ff, 0x4553E0, "SoldierFemale_HitHandler", setup=hit_setup(part, headshot, ai, kind, dmg, total, sum_),
          cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("vuln,ai,cam,radius,falloff,total,sum_", [
    (0, 0, (0, 0, 0), 100.0, 1.0, 10.0, 0.0),
    (1, 1, (0, 0, 0), 100.0, 1.0, 10.0, 0.0),
    (1, 0, (1000.0, -200.0, 30.0), 100.0, 3.5, 10.0, 0.0),
    (1, None, (1500.0, -200.0, 30.0), 300.0, 0.7, 10.0, 0.0),
    (1, 0, (5000.0, -200.0, 30.0), 300.0, 0.0, 10.0, 0.0),
    (1, 0, (2800.0, 90.0, 30.0), 300.0, 2.0, 10.0, 0.2),
    (1, 0, (1300.0, 90.0, 30.0), 200.0, 40.0, 90.0, 0.0),
    (1, 0, (1300.0, 90.0, 30.0), 200.0, 0.01, 10.0, 0.0)])
def test_female_blast(cw, vuln, ai, cam, radius, falloff, total, sum_):
    def setup(e):
        cfg.clear()
        cfg.update(cam=cam, falloff=falloff)
        fields = [(0xCF6, bytes([vuln])), (0x254, f32(total)), (0xD08, f32(sum_))]
        if ai is not None:
            fields.append((0x13ABC, bytes([ai])))
        h = soldier(e, ai=ai is not None, fields=fields)
        pos = e.alloc(24)
        e.write(pos, f64(1100.0, -150.0, 70.0))
        b = e.alloc(0x18)
        e.write(b, i32(1, pos, 0) + f32(radius, 5.0) + i32(12))
        return h, b
    check(fe, ff, 0x455570, "SoldierFemale_BlastHandler", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("ai,norm,dmg,total,sum_", [
    (0, (0.6, 0.8, 0.0), 5.0, 10.0, 0.0), (None, (-0.6, -0.8, 0.0), 5.0, 10.0, 0.3),
    (0, (0.0, 0.0, 0.0), 500.0, 10.0, 0.0), (1, (0.6, 0.8, 0.0), 5.0, 10.0, 0.0),
    (0, (1.0, -0.75, 0.1), 0.2, 10.0, 0.0)])
def test_female_punch(cw, ai, norm, dmg, total, sum_):
    def setup(e):
        cfg.clear()
        cfg.update(norm=norm, msgdata=24)
        fields = [(0x254, f32(total)), (0xD08, f32(sum_))]
        if ai is not None:
            fields.append((0x13ABC, bytes([ai])))
        h = soldier(e, ai=ai is not None, fields=fields)
        att = e.alloc(0x100)
        pa, pb = e.alloc(24), e.alloc(24)
        e.write(pa, f64(10.0, 20.0, 30.0))
        e.write(pb, f64(-5.0, 2.5, 30.0))
        cfg["pos"] = {h: pa, att: pb}
        p = e.alloc(8)
        e.write(p, f32(dmg) + i32(att))
        return h, p
    check(fe, ff, 0x455780, "SoldierFemale_PunchHandler", setup=setup, cw=cw)
