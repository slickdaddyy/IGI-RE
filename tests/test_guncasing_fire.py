"""Gun_FireHandler, with the real random numbers and matrix helpers."""
import random
import struct

import pytest

from test_guncasing_util import CWS, Emu, Fakes, check, f32, f64, new_task, add_child, rmatrix, rvec, u32

e = Emu()
fk = Fakes(e)
GT, OT = 0x3E, 0x41
OWNER, PLAYER = 0x5E000000, 0x5F000000
R = {}
s = lambda em, p: em.read_cstr(p)


def get_point(em, a):
    em.write(a[0], f64(11.5 * a[2], -3.25 * a[2], 7.0 + a[2]))
    return (a[1], a[2])


def task_ref(em, a):
    p = em.alloc(4)
    em.write(p, u32(a[0]))
    return p


fk.add(0x414E20, 3, fmt=get_point)
fk.add(0x4F0EE0, 0, ret=OWNER)
fk.add(0x4E6B00, 2, ret=lambda em, a: R["snd"], fmt=lambda em, a: (a[0], s(em, a[1])))
fk.add(0x4E6C00, 1)
fk.add(0x4E6C30, 2)
fk.add(0x401AE0, 1, ret=task_ref)
fk.add(0x401B20, 1)
fk.add(0x4015F0, 1)
fk.add(0x416A70, 1, ret=lambda em, a: R["player"])
fk.add(0x4E75A0, 0, ret=4321)
# the aim matrix's last word is uninitialised stack in the original
fk.add(0x413D10, 9, fmt=lambda em, a: (a[0], a[1], em.read(a[2], 24), a[3], em.read(a[4], 12), em.read(a[5], 0x24),
                                       a[6], a[7], a[8]))
fk.add(0x4012A0, 3, ret=lambda em, a: R["casing"], fmt=lambda em, a: (a[0], a[1] & 0xFFFF, a[2]))
fk.add(0x4CEC10, 1, ret=0x5D000000, fmt=lambda em, a: (s(em, a[0]),))
fk.add(0x4C48D0, 2)
e.baseline = e.snapshot()


def setup_fire(rng, **kw):
    def setup(em):
        game = em.alloc(0x40)
        em.write(0x57BABC, u32(game))
        em.write(game + 0x2C, u32(*[rng_.getrandbits(32) for _ in range(3)]))
        wt = em.alloc(0x6E8)
        em.write(wt + 0x61C, b"rifle01\0")
        em.write(wt + 0x62C, kw.get("casing", b"shell01") + b"\0")
        em.write(wt + 0x63C, u32(7, kw.get("pellets", 1), kw.get("rate", 600), 30, kw.get("burst", 1)))
        em.write(wt + 0x664, f32(*kw.get("spread", (rng_.uniform(0, 3), rng_.uniform(0, 6)))))
        em.write(wt + 0x6B0, b"shot_a\0")
        em.write(wt + 0x6C0, b"shot_b\0")
        owner = new_task(em, OT, 0x40)
        g = new_task(em, GT, 0x1B0)
        add_child(em, owner, g)
        em.write(g + 0x20, rvec(rng_))
        em.write(g + 0x70, rmatrix(rng_))
        em.write(g + 0x128, u32(wt, kw.get("ammo", 0x7700)))
        loop = 0
        if kw.get("loop") is not None:
            loop = em.alloc(4)
            em.write(loop, u32(kw["loop"]))
        smoke = 0
        if kw.get("smoke") is not None:
            smoke = em.alloc(4)
            em.write(smoke, u32(kw["smoke"]))
        em.write(g + 0x13C, u32(kw.get("cooldown", 0), kw.get("gburst", 0), kw.get("rounds", 5)))
        em.write(g + 0x148, f32(kw.get("spin", 0.5)))
        em.write(g + 0x190, u32(smoke, loop))
        R.update(snd=kw.get("snd", 0x5C000000), player=owner if kw.get("player", 1) else PLAYER,
                 casing=new_task(em, 0x43, 0x128))
        msg = em.alloc(0x2C)
        em.write(msg, bytes([kw.get("trigger", 1)]))
        if kw.get("muzzle"):
            p = em.alloc(24)
            em.write(p, rvec(rng_))
            em.write(msg + 4, u32(p))
        if kw.get("aim"):
            p = em.alloc(0x28)
            em.write(p, rmatrix(rng_))
            em.write(msg + 8, u32(p))
        if kw.get("vel"):
            p = em.alloc(12)
            em.write(p, f32(*[rng_.uniform(-50, 50) for _ in range(3)]))
            em.write(msg + 0xC, u32(p))
        em.write(msg + 0x28, u32(kw.get("unk28", 3)))
        return (g, msg)
    rng_ = None

    def wrapped(em):
        nonlocal rng_
        rng_ = random.Random(rng)
        return setup(em)
    return wrapped


def run(cw=None, **kw):
    check(e, fk, 0x478BA0, "Gun_FireHandler", setup_fire(kw.pop("seed", 0), **kw), check_eax=False, cw=cw)


@CWS
@pytest.mark.parametrize("seed", range(16))
def test_spread(seed, cw):
    r = random.Random(seed)
    run(cw, seed=seed, pellets=r.choice([1, 2, 8]), gburst=r.choice([0, 3, 4]), muzzle=r.random() < 0.5,
        aim=r.random() < 0.5, vel=r.random() < 0.5, player=r.random() < 0.7, rate=r.choice([60, 450, 900, 1100]),
        spin=r.choice([0.0, 0.5, 0.99, 1.0]), smoke=r.choice([None, 0, 0x7800]), ammo=r.choice([0, 0x7700]),
        rounds=r.choice([1, 5, -1]))


@CWS
@pytest.mark.parametrize("spread", [(0.0, 0.0), (-2.0, -4.0), (0.0, -0.0), (-0.0, 1.0)])
@pytest.mark.parametrize("gburst", [0, 5])
def test_spread_signs(spread, gburst, cw):
    run(cw, seed=gburst, spread=spread, gburst=gburst, pellets=3)


@pytest.mark.parametrize("casing,player,pellets", [(b"", 1, 1), (b"shell", 0, 1), (b"shell", 1, 0)])
def test_casing(casing, player, pellets):
    run(seed=7, casing=casing, player=player, pellets=pellets)


# trigger, wt burst, gun burst, cooldown, ammo, rounds, loop sound, played sound
STATES = [(t, b, gb, cd, am, rd, lp, sn)
          for t in (0, 1) for b, gb in ((1, 0), (1, 1), (3, 1), (3, 3)) for cd in (0, 2)
          for am, rd in ((0, 0), (0x7700, 0), (0x7700, 4)) for lp in (None, 0, 0x7900) for sn in (0, 0x5C000000)]


@pytest.mark.parametrize("i", range(len(STATES)))
def test_states(i):
    t, b, gb, cd, am, rd, lp, sn = STATES[i]
    run(seed=i, trigger=t, burst=b, gburst=gb, cooldown=cd, ammo=am, rounds=rd, loop=lp, snd=sn)
