import pytest

from test_soldier_util import NAN, RET, d64, f32, fake, new_emu, run, soldier, u32

e = new_emu()


def slots_get(em, a):
    return em.u32(a[0] + 4 * a[1])


def use_point(em, a):
    d64(em, a[1], *RET["use_pos"])
    f32(em, a[2], *RET["use_mat"])


def euler(em, a):
    for p, v in zip(a[1:], RET["euler"]):
        f32(em, p, v)


for addr, n, kw in (
        (0x489E70, 3, {}), (0x489EA0, 4, {}), (0x4F2060, 2, dict(effect=slots_get)), (0x4F2070, 3, {}),
        (0x4F2350, 2, dict(masks={1: 0xFF})), (0x4F2390, 1, {}), (0x4F2290, 1, {}), (0x489E10, 2, {}),
        (0x4D61D0, 5, {}), (0x48A330, 1, {}), (0x48A320, 1, {}), (0x463770, 1, {}), (0x4F0EE0, 0, dict(ret=0xEE0)),
        (0x4E6B00, 2, dict(strings=(1,))), (0x4E6C30, 2, dict(derefs={1: 24})), (0x416D20, 0, dict(ret=0x16D20)),
        (0x4B47C0, 2, {}), (0x45DAA0, 2, dict(derefs={1: 24}, also="Soldier_TurnToHit")), (0x45D5B0, 4, dict(also="Soldier_GetDeathVelocity")),
        (0x463760, 2, {}), (0x464700, 1, {}), (0x4635F0, 1, {}), (0x4638A0, 3, dict(strings=(1,))),
        (0x4D74F0, 3, {}), (0x4AF810, 2, dict(strings=(0,))), (0x4636E0, 1, dict(ret=0x36E0)),
        (0x444B30, 3, dict(effect=use_point, masks={1: 0, 2: 0})), (0x4631A0, 2, dict(derefs={1: 24})),
        (0x4B3C50, 4, dict(derefs={0: 36}, effect=euler, masks={1: 0, 2: 0, 3: 0})), (0x463220, 2, {}), (0x464720, 1, {}), (0x463310, 2, {}),
        (0x464710, 1, {}), (0x463600, 1, {}), (0x475340, 1, {}), (0x463620, 1, {}), (0x4636A0, 2, {}),
        (0x463550, 1, {})):
    fake(e, addr, n, **kw)
e.baseline = e.snapshot()

ENTERS = [
    (0x45B8D0, "Soldier_CrouchDownEnter", 13), (0x45B990, "Soldier_StandUpEnter", 14),
    (0x45BA50, "Soldier_RaiseEnter", 15), (0x45BB10, "Soldier_LowerEnter", 16),
    (0x45BBD0, "Soldier_CrouchEnter", 19), (0x45BC00, "Soldier_CrouchLeftEnter", 20),
    (0x45BC30, "Soldier_CrouchRightEnter", 21), (0x45BC60, "Soldier_CrouchFireEnter", 22),
    (0x45BC90, "Soldier_CrouchReloadEnter", 23), (0x45BD90, "Soldier_CrouchAimEnter", 24),
    (0x45BEA0, "Soldier_RiseAimEnter", 25), (0x45BF60, "Soldier_AimEnter", 26), (0x45C090, "Soldier_FireEnter", 27),
    (0x45C190, "Soldier_ReloadEnter", 28), (0x45C1C0, "Soldier_FallEnter", 29), (0x45C2E0, "Soldier_HitEnter", 30),
    (0x45C330, "Soldier_AimHitEnter", 31), (0x45CA50, "Soldier_UseEnter", 37),
]

RUNS = [
    (0x45B900, "Soldier_ToCrouchRun"), (0x45B9C0, "Soldier_StandUpRun"), (0x45BA80, "Soldier_RaiseRun"),
    (0x45BB40, "Soldier_LowerRun"), (0x45BCC0, "Soldier_CrouchReloadRun"), (0x45BDC0, "Soldier_CrouchAimRun"),
    (0x45BED0, "Soldier_RiseAimRun"), (0x45BF90, "Soldier_AimRun"), (0x45C0C0, "Soldier_FireRun"),
    (0x45C210, "Soldier_FallRun"), (0x45C370, "Soldier_AimHitRun"), (0x45C700, "Soldier_DieRun"),
    (0x45C740, "Human_DeadRun"), (0x45C7E0, "Soldier_AnimRun"), (0x45C9B0, "Soldier_MountRun"),
    (0x45CA80, "Human_UseRun"),
]

BITS = [0, 1, 0x10000, 0x20000, 0x30000, 4, 5, 4 | 0x10000, 4 | 0x20000, 4 | 0x30000, 4 | 0x200, 5 | 0x200,
        4 | 0x800, 5 | 0x800, 4 | 0x100000, 4 | 0x100200, 4 | 0x100800, 5 | 0x100000, 4 | 0x2000000,
        5 | 0x2000000, 5 | 0x2000200, 4 | 0x2000200, 4 | 0x40, 4 | 0x400000, 4 | 0x8000000, 4 | 0x10000000]


def base(em, bits=4, state=5, seed=1):
    h, sm = soldier(em, bits, state, seed)
    hit = em.alloc(16)
    u32(em, hit, 0x111, 0x222, 0x333, 0x444)
    RET[0x489E10] = hit
    return h, sm


@pytest.mark.parametrize("addr,name,state", ENTERS)
def test_enter(addr, name, state):
    def setup(em):
        h, sm = base(em, state=state)
        return sm, h
    run(e, addr, name, setup)


@pytest.mark.parametrize("bits", BITS)
@pytest.mark.parametrize("addr,name", RUNS)
@pytest.mark.parametrize("variant", [0, 1, 2])
def test_run(addr, name, bits, variant):
    def setup(em):
        h, sm = base(em, bits, seed=variant + 3)
        f32(em, h + 0x1C8, (0.0, 1.0, NAN)[variant])        # fire input
        u32(em, h + 0xD14, (0, 19, 20)[variant])             # hit frames
        u32(em, h + 0x6EC, (0, cstr_ground(em), cstr_ground(em))[variant])
        u32(em, h + 0xD0C, (-1, 5, 0)[variant])               # script anim
        f32(em, h + 0x210, (0.0, 2.0, 2.0)[variant])          # use input
        f32(em, h + 0x170, (0.0, 0.0, 1.0)[variant])
        RET[0x4E6B00] = (0, 0x6B00, 0x6B01)[variant]
        return sm, h
    run(e, addr, name, setup)


def cstr_ground(em):
    g = em.alloc(0x140)
    em.write(g + 0x114, b"land_dirt\0")
    return g


DIES = [(0x45C440, "Soldier_DieEnter"), (0x45C5A0, "Soldier_CrouchDieEnter"), (0x45C650, "Soldier_AimDieEnter")]


@pytest.mark.parametrize("addr,name", DIES)
@pytest.mark.parametrize("ai,death", [(0, 0), (1, -1), (1, 0x33)])
@pytest.mark.parametrize("kind", [-1, 0, 1, 2, 3])
@pytest.mark.parametrize("zone", [3, 15, 16])
@pytest.mark.parametrize("rnd,turn,weapon", [(0, 0, 0), (1, 1, 0x4C4C)])
def test_die(addr, name, ai, death, kind, zone, rnd, turn, weapon):
    def setup(em):
        h, sm = base(em, 5)
        hit = em.alloc(16)
        u32(em, hit, 0x111, 0x222, 0x333, 0x444)
        RET[0x489E10] = hit
        RET[0x4B47C0] = rnd
        RET[0x45DAA0] = turn
        a = 0
        if ai:
            a = em.alloc(0x3E00)
            u32(em, a + 0x3AD0, death)
        u32(em, h + 0x800, a)
        u32(em, h + 0x4C4, weapon)
        u32(em, h + 0x2E8, zone)
        u32(em, h + 0xD1C, kind)
        d64(em, h + 0xD20, 1.0, -2.0, 3.0)
        return sm, h
    run(e, addr, name, setup)


@pytest.mark.parametrize("anim,has", [(-1, 1), (7, 1), (7, 0)])
def test_anim_enter(anim, has):
    def setup(em):
        h, sm = base(em, 4, 35)
        u32(em, h + 0xD0C, anim)
        RET[0x4D74F0] = has
        return sm, h
    run(e, 0x45C750, "Soldier_AnimEnter", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("pos,mat,yaw", [
    ((1000.5, -2000.25, 33.0), (1, 0.3, 0, 0, -0.7, 0, 0, 0, 1), 0.1),
    ((-7.1, 3.3, 9e5), (0.1234567, 0.7654321, 0.3, 0.2, 0.3333333, 0.1, 0, 0, 1), -2.9),
])
def test_mount_enter(pos, mat, yaw, cw):
    def setup(em):
        h, sm = base(em, 4, 36)
        d64(em, h + 0x20, 11.0, 22.0, -123.456)
        RET["use_pos"] = pos
        RET["use_mat"] = mat
        RET["euler"] = (0.5, 0.25, yaw)
        return sm, h
    run(e, 0x45C8B0, "Soldier_MountEnter", setup, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
def test_die_run_float(cw):
    def setup(em):
        h, sm = base(em, 5)
        d64(em, h + 0x20, 1234.567, -0.001, 1e9 + 0.3)
        d64(em, h + 0xD38, 0.1, 3.3e-3, -7.77)
        return sm, h
    run(e, 0x45C700, "Soldier_DieRun", setup, cw=cw)
