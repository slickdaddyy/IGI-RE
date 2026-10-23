import random
import struct

from emu import Emu
from test_statgun_fakes import (CWS, SEEDS, Fakes, check, common, f64, i32, new_task, rfloats, rmatrix, rvec)

e = Emu()
fk = Fakes(e)
common(e, fk)
fk.add(0x4F1400, 2)
fk.add(0x471480, 1, fmt=lambda em, a: (em.read(a[0] + 0x60, 0x28),))
fk.add(0x4012A0, 3, ret=0x20F00000, fmt=lambda em, a: (a[0], a[1] & 0xFFFF, a[2]))


def test_get_params_changed():
    def setup(em):
        i32(em, 0x5BE348, 0x12345678)
        return 0, em.alloc(4)
    check(e, fk, 0x4723D0, "BinocularHolder_GetParams", setup=setup)
    check(e, fk, 0x4723E0, "BinocularHolder_Changed", args=(0x20001000,))


@SEEDS
def test_draw(seed):
    def setup(em):
        rng = random.Random(seed)
        em.write(0xBCAB08, rvec(rng))
        em.write(0xBCAAF8, rfloats(rng, 3))
        return (new_task(em, 0x30, 0x2E00),)
    check(e, fk, 0x472400, "BinocularOverlay_DrawHandler", setup=setup)


@CWS
@SEEDS
def test_use(seed, cw):
    def setup(em):
        rng = random.Random(seed)
        o = new_task(em, 0x30, 0x2E00)
        em.write(o + 0x38, rmatrix(rng))
        view = em.alloc(0x200)
        em.write(view + 0xE0, rvec(rng))
        em.write(view + 0xF8, rmatrix(rng))
        em.write(view + 0x1E4, rfloats(rng, 1))
        h = em.alloc(0x500)
        i32(em, h + 0x4EC, view)
        msg = em.alloc(0x30)
        i32(em, msg + 0x2C, h)
        return o, msg
    check(e, fk, 0x472440, "BinocularOverlay_UseHandler", setup=setup, cw=cw)


def test_close_new_set():
    def setup(em):
        em.write(0x5401F8, struct.pack("<HH", 0x51, 0x52))
        i32(em, 0x5BE348, 0x20005000)
    check(e, fk, 0x4725D0, "BinocularOverlay_Close", setup=setup)
    r, _ = check(e, fk, 0x472600, "BinocularOverlay_New", setup=setup, args=(0x20001000,), check_eax=True)

    def setup2(em):
        v = em.alloc(0x30)
        em.write(v, f64(1.5, -2.25, 3e9) + struct.pack("<f", 7.5))
        return em.alloc(0x100), v
    check(e, fk, 0x472630, "BinocularOverlay_SetPos", setup=setup2)
    check(e, fk, 0x472650, "BinocularOverlay_SetUnk7C", setup=setup2)
