"""HumanSoldierPistol task: type setup, controls, use, update, damage."""
import struct

import pytest

from emu import Emu
from test_pistol import CWS, Fakes, check, f32, f64, i32

cfg = {}
TYPE = 0x47
TARGET_TYPE = 0x55


def c(key):
    return lambda em, a: cfg[key]


def ptr_diff(em, a):
    """ParamTable_Add(table, base, field, type, flags, range): field offset instead of two stack pointers."""
    return (a[0], (a[2] - a[1]) & 0xFFFFFFFF, a[3], a[4], a[5])


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


def use_msg_fmt(e, a):
    """the use message without its uninitialised second word"""
    m = e.read(a[1], 0x48)
    return (a[0], m[:4], m[8:])


def make():
    e = Emu()
    fk = Fakes(e)
    for a, n in ((0x45D360, 1), (0x460CF0, 1), (0x4610C0, 1), (0x4613D0, 1), (0x4614B0, 1), (0x462CF0, 1),
                 (0x462E50, 1), (0x463080, 1), (0x457810, 1), (0x464800, 2), (0x464BD0, 2), (0x463AD0, 3),
                 (0x4637C0, 3), (0x465970, 4), (0x4C1830, 1), (0x401FA0, 2), (0x401530, 3), (0x4EC190, 6),
                 (0x489C30, 2), (0x4D96F0, 1), (0x4C7560, 2), (0x4636A0, 2), (0x444BC0, 1), (0x447DA0, 1),
                 (0x4F2070, 3)):
        fk.add(a, n)
    fk.add(0x401A20, 1, mask={0: 0xFFFF})
    fk.add(0x401400, 3, mask={0: 0xFFFF, 1: 0xFF})
    fk.add(0x401900, 4, ret=TYPE, mask={2: 0xFFFF})
    fk.add(0x460BE0, 0, ret=0x13)
    for i, a in enumerate((0x4F1A60, 0x4F1A70, 0x4F40D0, 0x416840, 0x416860, 0x460C40, 0x47E990, 0x416810,
                           0x416800)):
        fk.add(a, 0, ret=0x30 + i)
    fk.add(0x460C50, 0, ret=0x6060)
    fk.add(0x4F1980, 2, ret=0x6161)
    fk.add(0x4F0FA0, 0, ret=0x6262)
    fk.add(0x4F1A80, 6, fmt=ptr_diff)
    fk.add(0x416920, 0, ret=0x9999)
    fk.add(0x4AF810, 3, fmt=lambda em, a: (em.read_cstr(a[0]), a[1], a[2]))
    fk.add(0x4EC1E0, 2, effect=lambda em, a: em.write(a[0] + 8, struct.pack("<I", cfg["bits"])))
    fk.add(0x4D2FB0, 3, effect=lambda em, a: em.write(a[1], f32(*cfg["delta"])))
    fk.add(0x463680, 1, ret=0x1234)
    fk.add(0x460C80, 4, fmt=printf_fmt)
    fk.add(0x489C60, 2, ret=c("part"))
    fk.add(0x489E10, 2, ret=0x5A5A0)
    fk.add(0x450D50, 2, fmt=lambda em, a: ai_msg_fmt(cfg["msgdata"])(em, a))
    fk.add(0x465800, 2, deref={0: 0}, effect=lambda em, a: em.write(a[0], f64(*cfg["cam"])))
    fk.add(0x4B3130, 1, deref={0: 24}, st0="d",
           ret=lambda em, a: sum(x * x for x in struct.unpack("<3d", em.read(a[0], 24))) ** 0.5)
    fk.add(0x416C90, 5, ret=c("falloff"), st0="d")
    fk.add(0x4C3440, 1, ret=lambda em, a: cfg["pos"][a[0]])
    fk.add(0x4B3100, 1, deref={0: 24}, effect=lambda em, a: em.write(a[0], f64(*cfg["norm"])))
    fk.add(0x463110, 1, ret=c("mode"))
    fk.add(0x413CC0, 2, ret=c("weapon"))
    fk.add(0x4F1050, 1, ret=0x4242)
    fk.add(0x4F1030, 1, ret=c("target"))
    handler = fk.stubs(1)[0]
    e.write(0xA96AE0 + 4 * 0x13, i32(handler))
    e.write(0xA96AE0 + 4 * (0x31 * 0x180 + 0x13), i32(handler))
    e.write(0xA96AE0 + 4 * (2 * 0x180 + 0x13), i32(handler))
    query = fk.add(fk.code(b"\xc3"), 2, fmt=lambda em, a: (a[0],),
                   effect=lambda em, a: em.write(a[1], i32(cfg["result"])))
    use = fk.add(fk.code(b"\xc3"), 2, fmt=use_msg_fmt)
    e.baseline = e.snapshot()
    return e, fk, query, use


pe, pf, QUERY, USE = make()


def soldier(e, ai=True, fields=()):
    h = e.alloc(0xD50)
    e.write(h + 0x20, f64(1000.5, -200.25, 30.125))
    e.write(h + 0x78, f32(0.8, -0.6, 0.0, 0.6, 0.8, 0.0, 0.0, 0.0, 1.0))
    e.write(h + 0xA0, f32(512.0))
    e.write(h + 0x254, f32(10.0, 100.0))
    for i in range(17):
        e.write(h + 0x260 + 8 * i, f32(1.5 + i, 0.5 + 0.25 * i))
    e.write(h + 0x2EC, i32(0x1111, 0x2222, 0x45, 3, 7, 0x3333))
    e.write(h + 0x4EC, struct.pack("<I", e.alloc(0x100)))
    e.write(h + 0x534, i32(*range(0x534, 0x550, 4)))
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


def test_open():
    check(pe, pf, 0x459880, "SoldierPistol_Open")


def test_create():
    check(pe, pf, 0x459A20, "SoldierPistol_CreateHandler", setup=lambda e: (soldier(e), 0x1234))


def test_close_gettype():
    def setup(e):
        e.write(0x5BDAA8, struct.pack("<H", 0x47))
        e.write(0x5BDAA4, i32(0x777))
    check(pe, pf, 0x45A7C0, "SoldierPistol_Close", setup=setup)
    check(pe, pf, 0x45A7E0, "SoldierPistol_GetType", setup=setup, check_eax=True, eax_mask=0xFFFF)


def test_get_param_table():
    def setup(e):
        e.write(0x5BDAA4, i32(0x777))
        return 0, e.alloc(4)
    check(pe, pf, 0x459F00, "SoldierPistol_GetParamTable", setup=setup)


@pytest.mark.parametrize("spawned", [0, 1])
def test_delete(spawned):
    check(pe, pf, 0x459EC0, "Soldier_DeleteHandler", setup=lambda e: (soldier(e, fields=[(0xD01, bytes([spawned]))]),))


def use_target(e, result, handler=True):
    t = e.alloc(0x40)
    e.write(t + 0x1C, struct.pack("<H", TARGET_TYPE))
    e.write(0xA96AE0 + 4 * (0x37 * 0x180 + TARGET_TYPE), i32(QUERY if handler else 0))
    e.write(0xA96AE0 + 4 * (0x38 * 0x180 + TARGET_TYPE), i32(USE))
    cfg.update(target=t, result=result)


def use_soldier(e):
    h = soldier(e)
    table = e.alloc(4 * 40)
    e.write(table, struct.pack("<40I", *STATE_STUBS))
    e.write(h + 0x2F0, i32(table))
    e.write(h + 0x7BC, i32(5))
    e.write(h + 0xD10, i32(0x99))
    return h


STATE_STUBS = pf.stubs(40)
pe.baseline = pe.snapshot()


@pytest.mark.parametrize("found,result,handler", [
    (0, 0, 1), (1, 0, 1), (1, 2, 1), (1, 3, 1), (1, 6, 1), (1, 4, 1), (1, 3, 0)])
def test_use_target(found, result, handler):
    def setup(e):
        cfg.clear()
        h = use_soldier(e)
        use_target(e, result, handler)
        if not found:
            cfg["target"] = 0
        return (h,)
    check(pe, pf, 0x459D50, "SoldierPistol_UseTarget", setup=setup)


INPUT_BITS = [0, 0xFFFFFFFF, 0x0041C1E3, 0xF003E01C, 0x0FFC1FE3]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("bits", INPUT_BITS)
@pytest.mark.parametrize("flags,run", [(0, 0), (0x100, 1), (0x10000, 0), (0x10100, 1)])
def test_update_input_bits(cw, bits, flags, run):
    def setup(e):
        cfg.clear()
        h = soldier(e, fields=[(0xD00, bytes([run]))])
        m = e.alloc(0x60)
        e.write(m, i32(0, flags))
        e.write(h + 0x70, i32(m))
        inp = [0.0] * 40
        for i in range(2, 10):
            inp[i] = 1.0 if (bits >> i) & 1 else 0.0
        for i in (21, 22, 25, 26, 27, 29, 30):
            inp[i] = -2.0 if (bits >> i) & 1 else 0.0
        e.write(h + 0x1B0, f32(*inp))
        e.write(h + 0x2F4, struct.pack("<I", bits))
        return h + 0x2EC, h
    check(pe, pf, 0x459AB0, "SoldierPistol_UpdateInput", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("anim,use,mode,weapon,wfound", [
    (2.5, 0.0, 0, 0.0, 0), (3.5, 0.0, 0, 0.0, 0), (-7.0, 0.0, 0, 0.0, 0), (1e10, 0.0, 0, 0.0, 0),
    (float("nan"), 0.0, 0, 0.0, 0), (0.0, 12.5, 0x24, 0.0, 0), (0.0, 12.5, 0x23, 0.0, 0),
    (0.0, -3.5, 1, 0.0, 0), (0.0, 0.0, 0, 4.5, 1), (0.0, 0.0, 0, 4.5, 0), (5.0, 9.0, 0, 6.0, 1)])
def test_update_input_values(cw, anim, use, mode, weapon, wfound):
    def setup(e):
        cfg.clear()
        cfg.update(mode=mode, weapon=0x8080 if wfound else 0, target=0)
        h = soldier(e)
        m = e.alloc(0x60)
        e.write(h + 0x70, i32(m))
        inp = [0.0] * 40
        inp[23], inp[24], inp[28] = anim, use, weapon
        e.write(h + 0x1B0, f32(*inp))
        return h + 0x2EC, h
    check(pe, pf, 0x459AB0, "SoldierPistol_UpdateInput", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("locked,attach,toggle,bits", [(0, 0, 0.0, 0x10004), (1, 1, 1.0, 0x10000),
                                                       (0, 0, -0.5, 0x0), (0, 1, float("nan"), 0x4)])
def test_input_handler(cw, locked, attach, toggle, bits):
    def setup(e):
        cfg.clear()
        cfg.update(bits=bits, delta=(13.5, -7.25, 0.875))
        h = soldier(e, fields=[(0x250, bytes([locked])), (0x76C, i32(attach * 0x4444))])
        e.write(h + 0x110, f32(*[100 + i for i in range(40)]))
        inp = [i * 0.5 for i in range(40)]
        inp[20] = toggle
        src = e.alloc(160)
        e.write(src, f32(*inp))
        msg = e.alloc(8)
        e.write(msg, struct.pack("<II", 0, src))
        return h, msg
    check(pe, pf, 0x459F10, "SoldierPistol_InputHandler", setup=setup, cw=cw)


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
    (4, 0, None, 1, 0.01, 10.0, 0.3), (15, 1, 0, 1, 33.3, 90.0, 0.31), (15, 1, 0, 1, 0.33, 10.0, 0.0),
    (15, 1, 0, 1, 0.1, 10.0, 0.23)])
def test_hit(cw, part, headshot, ai, kind, dmg, total, sum_):
    check(pe, pf, 0x45A230, "SoldierPistol_HitHandler", setup=hit_setup(part, headshot, ai, kind, dmg, total, sum_),
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
    (1, 0, (1300.0, 90.0, 30.0), 200.0, 0.01, 10.0, 0.0),
    (1, 0, (1300.0, 90.0, 30.0), 200.0, 0.11, 10.0, 0.0)])
def test_blast(cw, vuln, ai, cam, radius, falloff, total, sum_):
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
    check(pe, pf, 0x45A3D0, "SoldierPistol_BlastHandler", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("ai,norm,dmg,total,sum_", [
    (0, (0.6, 0.8, 0.0), 5.0, 10.0, 0.0), (None, (-0.6, -0.8, 0.0), 5.0, 10.0, 0.3),
    (0, (0.0, 0.0, 0.0), 500.0, 10.0, 0.0), (1, (0.6, 0.8, 0.0), 5.0, 10.0, 0.0),
    (0, (1.0, -0.75, 0.1), 0.2, 10.0, 0.0), (0, (1.0, -0.75, 0.1), 0.02, 10.0, 0.0)])
def test_punch(cw, ai, norm, dmg, total, sum_):
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
    check(pe, pf, 0x45A5E0, "SoldierPistol_PunchHandler", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("attach,toggle,bits,velz", [(0, 0.0, 0x10004, -40.5), (1, 1.0, 0x0, 12.25)])
def test_update(cw, attach, toggle, bits, velz):
    def setup(e):
        cfg.clear()
        cfg.update(bits=bits, delta=(13.5, -7.25, 0.875))
        h = soldier(e, fields=[(0x76C, i32(attach * 0x4444)), (0x200, f32(toggle)), (0x66C, f32(velz))])
        return (h,)
    check(pe, pf, 0x459F50, "SoldierPistol_Update", setup=setup, cw=cw)
