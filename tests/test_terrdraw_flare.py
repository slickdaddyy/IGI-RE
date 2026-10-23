"""Hermite curves, viewport slots, lens flares (0x518130 - 0x5183C1)."""
import random
import struct

import pytest

from test_d3d_util import D3DEmu, check, logged, pyfake, trap, u32
from test_dxinit_util import check_cw

e = D3DEmu()
CWS = [0x37F, 0x7F]

pyfake(e, 0x512820, "Glow_Init", 0)
pyfake(e, 0x512920, "Glow_Shutdown", 0)

DRAW = trap(e, "Flare.draw", logged("Flare.draw", 3, pop=0,
                                    before=lambda em: (None, (em.read(em.arg(0), 12), em.read(em.arg(1), 8)))))


def f64(em, p, *v):
    em.write(p, struct.pack("<%dd" % len(v), *v))


def vals(seed):
    rng = random.Random(seed)
    return [rng.choice([0.0, 1.0, -2.5, 1e-3, 3.3, rng.uniform(-1e4, 1e4), rng.uniform(-1, 1)]) for _ in range(8)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(8))
def test_hermite_init(seed, cw):
    v = vals(seed)

    def setup(em):
        h = em.alloc(32)
        return (h,) + tuple(x for d in v[:4] for x in struct.unpack("<II", struct.pack("<d", d)))
    check_cw(e, 0x518130, "Hermite_Init", cw, setup=setup, check_eax=False)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(8))
@pytest.mark.parametrize("addr,name", [(0x518190, "Hermite_Eval"), (0x5181C0, "Hermite_Slope")])
def test_hermite_eval(addr, name, seed, cw):
    v = vals(seed)

    def setup(em):
        h = em.alloc(32)
        f64(em, h, *v[:4])
        return (h,) + struct.unpack("<II", struct.pack("<d", v[4]))
    check_cw(e, addr, name, cw, setup=setup, check_eax=False, check_st0=True)


SLOTS = 0xA800C0


@pytest.mark.parametrize("used", [b"", b"\x01", b"\x01\x01\x00\x01", b"\x00\x01", b"\x01" * 15])
def test_view_slot_alloc(used):
    def setup(em):
        em.write(SLOTS, used + b"\0")
        return (em.alloc(4),)
    check(e, 0x5181F0, "ViewSlot_Alloc", setup=setup, check_eax=False)


def test_view_slot_misc():
    check(e, 0x518230, "ViewSlot_SetCurrent", args=(7,), check_eax=False)
    check(e, 0x518240, "ViewSlot_GetCurrent", setup=lambda em: u32(em, 0xA800BC, 0x1234))

    def setup(em):
        em.write(SLOTS, b"\x01" * 8)
        return (3,)
    check(e, 0x518250, "ViewSlot_Free", setup=setup, check_eax=False)


def test_lens_flare_init():
    log = check(e, 0x518260, "LensFlare_Init", check_eax=False)
    assert log == [("Glow_Init", ())]


def test_lens_flare_shutdown():
    def setup(em):
        em.call(0x518260)
    log = check(e, 0x518290, "LensFlare_Shutdown", setup=setup, check_eax=False)
    assert log[-1] == ("Glow_Shutdown", ())


VIEW = 0xBCABB8


def flare_list(em, n, rng):
    nodes = [em.alloc(0x24) for _ in range(n)]
    for i, p in enumerate(nodes):
        nxt = nodes[i + 1] if i + 1 < n else 0
        prv = nodes[i - 1] if i else 0
        em.write(p, struct.pack("<IIfIfffII", nxt, prv, rng.uniform(-0.5, 1.5), 0x55, rng.uniform(0, 2),
                                rng.uniform(0, 2), rng.uniform(0, 2), 0x66, DRAW))
    return nodes


POSITIONS = [(320.0, 240.0), (-15.9, 100.0), (-16.0, 100.0), (655.9, 10.0), (656.0, 10.0), (10.0, -16.0),
             (10.0, -15.5), (10.0, 495.9), (10.0, 496.0), (float("nan"), 3.0), (3.0, float("nan")),
             (0.1, 0.2), (639.75, 479.5)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("pos", range(len(POSITIONS)))
@pytest.mark.parametrize("n,start", [(1, 0), (3, 0), (4, 2), (4, 3)])
def test_lens_flare_draw(n, start, pos, cw):
    def setup(em):
        rng = random.Random(n * 100 + pos)
        em.write(VIEW, struct.pack("<4f", 0.0, 0.0, 640.0, 480.0))
        nodes = flare_list(em, n, rng)
        p = em.alloc(8)
        em.write(p, struct.pack("<2f", *POSITIONS[pos]))
        c = em.alloc(12)
        em.write(c, struct.pack("<3f", rng.uniform(0, 1), rng.uniform(0, 1), rng.uniform(0, 1)))
        return nodes[start], p, c
    log = check_cw(e, 0x5182B0, "LensFlare_Draw", cw, setup=setup, check_eax=False)
    if pos == 0:
        assert len(log) == n


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("seed", range(6))
def test_lens_flare_draw_view(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        em.write(VIEW, struct.pack("<4f", rng.uniform(-50, 50), rng.uniform(-50, 50), rng.uniform(100, 900),
                                   rng.uniform(100, 700)))
        nodes = flare_list(em, 3, rng)
        p = em.alloc(8)
        em.write(p, struct.pack("<2f", rng.uniform(-100, 1000), rng.uniform(-100, 800)))
        c = em.alloc(12)
        em.write(c, struct.pack("<3f", 0.25, 0.5, 1.0))
        return nodes[0], p, c
    check_cw(e, 0x5182B0, "LensFlare_Draw", cw, setup=setup, check_eax=False)
