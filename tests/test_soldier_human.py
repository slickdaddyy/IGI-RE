import pytest

from test_menu_util import add_child, task
from test_soldier_util import LOG, NAN, RET, f32, fake, float_fake, logger, new_emu, pyfake, run, soldier, trap, u32, u8, with_cw

HANDLERS = 0xA96AE0

e = new_emu()  # callers of Human_Reset and Human_RemoveRefs, which are faked here
r = new_emu()  # Human_Reset, Human_RemoveRefs and the small handlers

GETTERS = [0x4D14D0, 0x4F1A60, 0x4F1A70, 0x4F1FC0, 0x47CDC0, 0x416830, 0x416840, 0x416850, 0x4FDEF0, 0x416860,
           0x466230, 0x416880, 0x484D30, 0x450C80, 0x489620, 0x489610, 0x450CC0, 0x46D410, 0x46C970, 0x477C00,
           0x477C10, 0x477C20, 0x477C30, 0x477C40, 0x4FC840, 0x467D90, 0x487190, 0x4E6610, 0x477030, 0x5096A0,
           0x4CEA10, 0x4CEA20, 0x46AB20, 0x4F1010, 0x4F0F20, 0x4F0FD0, 0x4F0FA0]


def param_add(em):
    a = [em.arg(i) for i in range(6)]
    LOG[-1].append((0x4F1A80, a[0], a[2] - a[1], a[3], a[4], a[5]))
    em.ret(0)


def sprintf(em):
    buf, fmt, i = em.arg(0), em.arg(1), em.arg(2)
    text = em.read_cstr(fmt)
    LOG[-1].append((0x4A53B3, buf, text, i))
    em.write(buf, (text.decode() % i).encode() + b"\0")
    em.ret(0)


def name_len(em, a):
    return len(em.read_cstr(a[0])) + 0x100


for i, g in enumerate(GETTERS):
    fake(e, g, 0, ret=0xAB000040 + i)
for addr, n, kw in (
        (0x401F60, 1, {}), (0x4D9610, 0, dict(ret=0xFFFF0021)), (0x401900, 4, dict(strings=(0,), masks={2: 0xFFFF}, ret=0x57)),
        (0x401FA0, 2, dict(masks={0: 0xFFFF})), (0x4017C0, 1, dict(ret=0xFFFFFF33)), (0x4C1800, 1, dict(ret=0x1800)),
        (0x401400, 3, dict(masks={0: 0xFFFF, 1: 0xFF})), (0x401530, 3, dict(masks={0: 0xFFFF})),
        (0x4F1210, 0, dict(ret=0x1210)), (0x4F1220, 0, {}), (0x4F1950, 1, dict(ret=0x1950)),
        (0x504A30, 2, dict(strings=(0,), effect=name_len)), (0x4B8810, 2, dict(strings=(0,))),
        (0x4B8890, 4, dict(strings=(0,))), (0x416930, 1, dict(strings=(0,), effect=name_len)),
        (0x4169D0, 1, dict(strings=(0,), effect=name_len)), (0x489640, 1, dict(masks={0: 0xFFFF}, ret=0x9640)),
        (0x4F1030, 1, {}),
        (0x4C1790, 2, {}), (0x489680, 1, {}), (0x4168D0, 0, dict(ret=0x168D0)), (0x484DC0, 1, {}),
        (0x45ECD0, 1, dict(also="Human_Reset")), (0x4F2050, 1, {}), (0x464610, 1, dict(ret=0x4610)),
        (0x4E0CC0, 9, dict(ret=0xCC0)), (0x416FC0, 0, dict(ret=7)),
        (0x4015F0, 1, {}), (0x4C17C0, 2, {}), (0x4896A0, 1, {}), (0x484E00, 1, {}),
        (0x45EFD0, 1, dict(also="Human_RemoveRefs")), (0x4B6F30, 1, {}), (0x4F1050, 1, {})):
    fake(e, addr, n, **kw)
pyfake(e, 0x4F1A80, param_add)
pyfake(e, 0x4A53B3, sprintf)
CHILD_RUN = trap(e, "child_run", logger(e, "child_run", 1))
DELETE2 = trap(e, "delete2", logger(e, "delete2", 1))
e.baseline = e.snapshot()


def team(em, a):
    return RET[("team", a[0])]


for addr, n, kw in ((0x47CDD0, 1, {}), (0x463580, 1, {}), (0x401B20, 1, {}), (0x463150, 1, {}),
                    (0x416D20, 0, dict(ret=0x16D20)), (0x463130, 1, dict(effect=team))):
    fake(r, addr, n, **kw)
float_fake(r, 0x4B4770, 1)
r.baseline = r.snapshot()


def test_open():
    run(e, 0x45E450, "Human_Open", lambda em: None)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("x", [0.0, 1.0, 2.5, -3.7, 1e9, NAN, 0.3333333])
def test_cut_scene_delta(x, cw):
    with_cw(e, 0x45EAE0, cw)
    with_cw(e, "Human_GetCutSceneDeltaTime", cw)
    e.reset()
    out = e.alloc(24)

    def setup(em):
        p = em.alloc(24)
        em.write(p, b"\x5A" * 24)
        h = em.alloc(0x300)
        f32(em, h + 0x1B0, x)
        RET[0x4F1030] = h
        return (p,)
    run(e, 0x45EAE0, "Human_GetCutSceneDeltaTime", setup, cw=cw,
        ignore=((out + 4, out + 8), (out + 20, out + 24)))


def test_create():
    def setup(em):
        u32(em, 0x5BDC24, 0xC24)
        u32(em, 0x5BDB0C, 0x500)
        h, sm = soldier(em)
        return h, 0x1234
    run(e, 0x45EB30, "Human_CreateHandler", setup)


@pytest.mark.parametrize("delete,kids", [(1, ()), (0, ()), (0, (1,)), (0, (1, 0, 2)), (0, (0, 0))])
def test_run(delete, kids):
    def setup(em):
        h = task(em, 0xD00, 0x44)
        em.write(h + 0x20, bytes(range(256)) * 12)
        u8(em, h + 0x7C8, delete)
        u32(em, h + 0x8, h + 0xC, 0, h + 0x8)
        for i, k in enumerate(kids):
            c = task(em, 0x40, 0x60 + i, h)
            add_child(em, h, c)
            u32(em, HANDLERS + 4 * (0x180 + 0x60 + i), CHILD_RUN if k else 0)
        u32(em, 0xAFA7E0, 3)
        return (h,)
    run(e, 0x45EEA0, "Human_RunHandler", setup)


@pytest.mark.parametrize("id_,slot,handled", [(-1, 5, 1), (0, 5, 1), (12, 6, 0), (12, 7, 1)])
def test_delete(id_, slot, handled):
    def setup(em):
        u32(em, 0x5BDC24, 0xC24)
        h, sm = soldier(em)
        u32(em, h + 0x1C, 0x44)
        for i in range(32):
            u32(em, h + 0x854 + 4 * i, (0x1000 + i) if i % 3 == 0 else 0)
        RET[0x4F1050] = id_
        RET[0x4F1220] = slot
        u32(em, HANDLERS + 4 * (slot * 0x180 + 0x44), DELETE2 if handled else 0)
        return (h,)
    run(e, 0x45EF40, "Human_DeleteHandler", setup)


def test_reset():
    run(r, 0x45ECD0, "Human_Reset", lambda em: (soldier(em)[0],))


@pytest.mark.parametrize("mask", [0, 0x3F, 0x15, 0x2A])
def test_remove_refs(mask):
    def setup(em):
        h, sm = soldier(em)
        for i, off in enumerate((0x768, 0x7B0, 0x7B4, 0x838, 0x83C, 0x840)):
            u32(em, h + off, (0x100 + i) if mask >> i & 1 else 0)
        return (h,)
    run(r, 0x45EFD0, "Human_RemoveRefs", setup)


def test_clear_result():
    def setup(em):
        p = em.alloc(4)
        u32(em, p, 0x5A5A5A5A)
        return 0, p
    run(r, 0x45E3D0, "Human_ClearResult", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("which,special,rnd,chance,skip2,t1,t2", [
    (0, 0, 0, 0, 0, 0, 0), (1, 0, 0, 0, 0, 0, 0), (3, 1, 0.3, 0.5, 0, 0, 0), (3, 1, 0.5, 0.5, 0, 0, 0),
    (3, 1, 0.7, 0.5, 0, 0, 0), (3, 1, NAN, 0.5, 0, 0, 0), (3, 0, 0, 0, 0, 1, 1), (3, 0, 0, 0, 0x777, 1, 1),
    (3, 0, 0, 0, 0x777, 1, 2), (2, 0, 0, 0, 0x777, 1, 2)])
def test_target(which, special, rnd, chance, skip2, t1, t2, cw):
    def setup(em):
        h = 0x4040
        q = em.alloc(20)
        skips = [0x111, 0x222, skip2]
        if which < 3:
            skips[which] = h
        u32(em, q, 0x5A5A5A5A, *skips)
        f32(em, q + 16, chance)
        RET[0x463150] = special
        RET[0x4B4770] = rnd
        RET[("team", skip2)] = t1
        RET[("team", h)] = t2
        return h, q
    run(r, 0x45E3E0, "sub_45E3E0", setup, cw=cw)
