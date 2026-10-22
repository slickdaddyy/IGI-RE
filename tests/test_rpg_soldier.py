"""HumanSoldierRPG task: type setup, handlers, damage."""
import struct

import pytest

from emu import Emu
from test_rpg import CWS, Fakes, ai_msg_fmt, check, f32, f64, i32, printf_fmt, soldier

cfg = {}
TYPE = 0x47


def c(key):
    return lambda em, a: cfg[key]


def ptr_diff(em, a):
    """ParamTable_Add(table, base, field, type, flags, range): field offset instead of two stack pointers."""
    return (a[0], (a[2] - a[1]) & 0xFFFFFFFF, a[3], a[4], a[5])


def make():
    e = Emu()
    fk = Fakes(e)
    for a, n in ((0x45D360, 1), (0x460CF0, 1), (0x4610C0, 1), (0x4613D0, 1), (0x4614B0, 1), (0x462CF0, 1),
                 (0x462E50, 1), (0x463080, 1), (0x463920, 1), (0x464800, 2), (0x464BD0, 2), (0x463AD0, 3),
                 (0x4637C0, 3), (0x465970, 4), (0x4C1830, 1), (0x401FA0, 2), (0x401530, 3), (0x4EC190, 6),
                 (0x489C30, 2), (0x4D96F0, 1), (0x489EA0, 4), (0x4D61D0, 5), (0x4C7140, 5), (0x4D38C0, 2)):
        fk.add(a, n)
    fk.add(0x401A20, 1, mask={0: 0xFFFF})
    fk.add(0x401400, 3, mask={0: 0xFFFF, 1: 0xFF})
    fk.add(0x401900, 4, ret=TYPE, mask={2: 0xFFFF})
    fk.add(0x460BE0, 0, ret=0x13)
    for i, a in enumerate((0x4F1A60, 0x4F1A70, 0x4F40D0, 0x416840, 0x416860, 0x460C40, 0x47E990)):
        fk.add(a, 0, ret=0x30 + i)
    fk.add(0x460C50, 0, ret=0x6060)
    fk.add(0x4F1980, 2, ret=0x6161)
    fk.add(0x4F0FA0, 0, ret=0x6262)
    fk.add(0x4F1A80, 6, fmt=ptr_diff)
    fk.add(0x416920, 0, ret=0x9999)
    fk.add(0x4D9620, 4, mask={2: 0xFFFF})
    fk.add(0x4D74F0, 3, ret=c("exists"))
    fk.add(0x4AF810, 2, fmt=lambda em, a: (em.read_cstr(a[0]), a[1]))
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
    handler = fk.stubs(1)[0]
    e.write(0xA96AE0 + 4 * 0x13, i32(handler))
    e.write(0xA96AE0 + 4 * (0x31 * 0x180 + 0x13), i32(handler))
    e.baseline = e.snapshot()
    return e, fk


re_, rf = make()


def test_open():
    check(re_, rf, 0x456AD0, "SoldierRPG_Open")


def test_create():
    def setup(e):
        cfg.clear()
        return soldier(e, 0xD40), 0x1234
    check(re_, rf, 0x456C70, "SoldierRPG_CreateHandler", setup=setup)


def test_close_gettype():
    def setup(e):
        e.write(0x5BDA94, struct.pack("<H", 0x47))
        e.write(0x5BDA98, i32(0x777))
    check(re_, rf, 0x4577E0, "SoldierRPG_Close", setup=setup)
    check(re_, rf, 0x457800, "SoldierRPG_GetType", setup=setup, check_eax=True, eax_mask=0xFFFF)


def test_get_param_table():
    def setup(e):
        e.write(0x5BDA98, i32(0x777))
        return 0, e.alloc(4)
    check(re_, rf, 0x456E40, "SoldierRPG_GetParamTable", setup=setup)


@pytest.mark.parametrize("spawned,idle,exists", [(0, -1, 1), (1, 5, 1), (0, 5, 0), (1, -1, 0)])
def test_params_changed(spawned, idle, exists):
    def setup(e):
        cfg.clear()
        cfg.update(exists=exists)
        h = soldier(e, 0xD40, fields=[(0xD01, bytes([spawned])), (0xCF8, i32(0x7FFF0003, idle))])
        m = e.alloc(0x60)
        e.write(m + 0x4C, i32(1, 2, 3, 4, 5))
        e.write(h + 0x70, i32(m))
        return h, 0x4321
    check(re_, rf, 0x456E50, "SoldierRPG_ParamsChanged", setup=setup)


INPUT_BITS = [0, 0xFFFFFFFF, 0x0041C1E3, 0xFFE3E01C]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("bits", INPUT_BITS)
@pytest.mark.parametrize("flags,run", [(0, 0), (0x100, 1), (0x10000, 0), (0x10100, 1)])
@pytest.mark.parametrize("anim", [0.0, 2.5, 3.5, -7.0, 1e10])
def test_update_input(cw, bits, flags, run, anim):
    def setup(e):
        h = soldier(e, 0xD40, fields=[(0xD00, bytes([run]))])
        m = e.alloc(0x60)
        e.write(m, i32(0, flags))
        e.write(h + 0x70, i32(m))
        inp = [0.0] * 40
        for i in range(2, 10):
            inp[i] = 1.0 if (bits >> i) & 1 else 0.0
        inp[21] = float(bits & 1)
        inp[22] = anim
        e.write(h + 0x1B0, f32(*inp))
        e.write(h + 0x2F4, struct.pack("<I", bits))
        return h + 0x2EC, h
    check(re_, rf, 0x456CF0, "SoldierRPG_UpdateInput", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("locked,attach,toggle,bits", [(0, 0, 0.0, 0x10004), (1, 1, 1.0, 0x10000),
                                                       (0, 0, -0.5, 0x0)])
def test_input_handler(cw, locked, attach, toggle, bits):
    def setup(e):
        cfg.clear()
        cfg.update(bits=bits, delta=(13.5, -7.25, 0.875))
        h = soldier(e, 0xD40, fields=[(0x250, bytes([locked])), (0x76C, i32(attach * 0x4444))])
        inp = [i * 0.5 for i in range(40)]
        inp[20] = toggle
        src = e.alloc(160)
        e.write(src, f32(*inp))
        msg = e.alloc(8)
        e.write(msg, struct.pack("<II", 0, src))
        return h, msg
    check(re_, rf, 0x456FB0, "SoldierRPG_InputHandler", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("part,headshot,ai,kind,dmg,total", [
    (3, 0, 0, 1, 5.0, 10.0), (15, 1, 0, 2, 5.0, 10.0), (16, 0, 0, 2, 5.0, 10.0), (16, 1, None, 2, 95.0, 10.0),
    (2, 0, 0, 2, 400.0, 10.0), (5, 1, 1, 2, 5.0, 10.0), (4, 0, None, 1, 0.1, 99.9), (6, 0, 0, 1, 90.0, 0.0)])
def test_hit(cw, part, headshot, ai, kind, dmg, total):
    def setup(e):
        cfg.clear()
        cfg.update(part=part, msgdata=0)
        fields = [(0x254, f32(total))]
        if ai is not None:
            fields += [(0x13ABC, bytes([ai, headshot]))]
        h = soldier(e, 0xD40, ai=ai is not None, fields=fields)
        w = e.alloc(0x700)
        e.write(w + 0x610, i32(kind))
        hit = e.alloc(0x1C)
        e.write(hit, i32(7) + f32(dmg, 0.25, -0.5, 0.75) + struct.pack("<II", 0x3333, w))
        return h, hit
    check(re_, rf, 0x4572D0, "SoldierRPG_HitHandler", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("vuln,ai,cam,radius,falloff,total", [
    (0, 0, (0, 0, 0), 100.0, 1.0, 10.0), (1, 1, (0, 0, 0), 100.0, 1.0, 10.0),
    (1, 0, (1000.0, -200.0, 30.0), 100.0, 3.5, 10.0), (1, None, (1500.0, -200.0, 30.0), 300.0, 0.7, 10.0),
    (1, 0, (5000.0, -200.0, 30.0), 300.0, 0.0, 10.0), (1, 0, (1300.0, 90.0, 30.0), 200.0, 40.0, 90.0),
    (1, 0, (1300.0, 90.0, 30.0), 200.0, 1 / 3.0, 99.0)])
def test_blast(cw, vuln, ai, cam, radius, falloff, total):
    def setup(e):
        cfg.clear()
        cfg.update(cam=cam, falloff=falloff)
        fields = [(0xCF6, bytes([vuln])), (0x254, f32(total, 100.0))]
        if ai is not None:
            fields.append((0x13ABC, bytes([ai])))
        h = soldier(e, 0xD40, ai=ai is not None, fields=fields)
        pos = e.alloc(24)
        e.write(pos, f64(1100.0, -150.0, 70.0))
        b = e.alloc(0x18)
        e.write(b, i32(1, pos, 0) + f32(radius, 5.0) + i32(12))
        return h, b
    check(re_, rf, 0x457430, "SoldierRPG_BlastHandler", setup=setup, cw=cw)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("ai,norm,dmg,total", [
    (0, (0.6, 0.8, 0.0), 5.0, 10.0), (None, (-0.6, -0.8, 0.0), 5.0, 10.0), (0, (0.0, 0.0, 0.0), 500.0, 10.0),
    (1, (0.6, 0.8, 0.0), 5.0, 10.0), (0, (1.0, -0.75, 0.1), 0.1, 99.9)])
def test_punch(cw, ai, norm, dmg, total):
    def setup(e):
        cfg.clear()
        cfg.update(norm=norm, msgdata=24)
        fields = [(0x254, f32(total))]
        if ai is not None:
            fields.append((0x13ABC, bytes([ai])))
        h = soldier(e, 0xD40, ai=ai is not None, fields=fields)
        att = e.alloc(0x100)
        pa, pb = e.alloc(24), e.alloc(24)
        e.write(pa, f64(10.0, 20.0, 30.0))
        e.write(pb, f64(-5.0, 2.5, 30.0))
        cfg["pos"] = {h: pa, att: pb}
        p = e.alloc(8)
        e.write(p, f32(dmg) + i32(att))
        return h, p
    check(re_, rf, 0x457620, "SoldierRPG_PunchHandler", setup=setup, cw=cw)
