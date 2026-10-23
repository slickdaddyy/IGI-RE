import random
import struct

import pytest

from test_vehbase_util import (CWS, EDIT, RIGID, VEHICLE, add_child, check, check_error, child_list, ev, f32, f64,
                               make, new_task, rmatrix, set_handler)

e, fk = make()
LOG1 = fk.handler(1)
LOG2 = fk.handler(2)
LOG2W = fk.handler(2, deref={1: 4})
LOG2MSG = fk.handler(2, deref={1: 8})
LOG2B = fk.handler(2, fmt=lambda em, a: [em.read(a[1], 1)])
SETS = fk.handler(2, fmt=lambda em, a: [em.normalize(a[0]), em.u32(a[1])])


def answer(value):
    """A handler that writes value to *arg."""
    return fk.handler(2, fmt=lambda em, a: (em.write(a[1], struct.pack("<I", value)), [em.normalize(a[0])])[1])


ANSWER0 = answer(0)
ANSWER1 = answer(0x1234)


# --- weapon events ------------------------------------------------------------

@pytest.mark.parametrize("kids", [[], [1], [0], [1, 1], [0, 1, 2, 0], [1, 0, 1], [2, 2]])
def test_weapon_delete_all(kids):
    """kids: 0 = no handler, 1 = answers non-zero, 2 = answers zero"""
    def setup(em):
        o = new_task(em, 0x60, 0x40)
        for i, k in enumerate(kids):
            ty = 0x61 + i
            add_child(em, o, new_task(em, ty, 0x40))
            set_handler(em, ev(0xA76C03), ty, {0: 0, 1: ANSWER1, 2: ANSWER0}[k])
        return (o,)
    check(e, fk, 0x4F2290, "Weapon_DeleteAll", setup=setup, check_eax=False)


def weapon(em, slot):
    w = new_task(em, 0x60, 0x40)
    set_handler(em, ev(slot), 0x60, LOG2W)
    return w


def test_weapon_move():
    def setup(em):
        set_handler(em, ev(0xA76C09), 0x60, LOG2MSG)
        return new_task(em, 0x60, 0x40), 0x1111, 0x2222
    check(e, fk, 0x4F2310, "Weapon_Move", setup=setup, check_eax=False)


@pytest.mark.parametrize("full", [0, 1, 0x101])
def test_weapon_use_ammo(full):
    def setup(em):
        set_handler(em, ev(0xA76C01), 0x60, LOG2B)
        return new_task(em, 0x60, 0x40), full
    check(e, fk, 0x4F2350, "Weapon_UseAmmo", setup=setup, check_eax=False)


@pytest.mark.parametrize("h", ["answer", "silent"])
def test_weapon_drop(h):
    def setup(em):
        set_handler(em, ev(0xA76C08), 0x60, ANSWER1 if h == "answer" else LOG2)
        return (new_task(em, 0x60, 0x40),)
    check(e, fk, 0x4F2390, "Weapon_Drop", setup=setup)


def test_weapon_get_ammo():
    def setup(em):
        set_handler(em, ev(0xA76C10), 0x60, LOG2)
        return new_task(em, 0x60, 0x40), 0x4444
    check(e, fk, 0x4F23D0, "Weapon_GetAmmo", setup=setup, check_eax=False)


# --- Vehicle ------------------------------------------------------------------

def test_open():
    check(e, fk, 0x4F4190, "Vehicle_Open", check_eax=False)


def test_close():
    check(e, fk, 0x4F45B0, "Vehicle_Close", check_eax=False)


@pytest.mark.parametrize("addr,name", [(0x4F4160, "Vehicle_CreateHandler"), (0x4F4170, "Vehicle_DeleteHandler"),
                                       (0x4F4180, "Vehicle_RunHandler")])
def test_abstract(addr, name):
    check_error(e, addr, name, lambda em: (0x1234,))


@pytest.mark.parametrize("orig,ours,mask", [(0x4F45F0, "Vehicle_GetType", 0xFFFF),
                                            (0x4F4850, "Vehicle_GetExplodeEvent", 0xFF)])
def test_getters(orig, ours, mask):
    check(e, fk, orig, ours, eax_mask=mask)


def vehicle(em, seed=0, scale=(1.0, 2.0, 1.5)):
    rng = random.Random(seed)
    v = new_task(em, VEHICLE, 0x748)
    em.write(v + 0x20, f64(*[rng.uniform(-1e4, 1e4) for _ in range(3)]))
    em.write(v + 0x70, rmatrix(rng))
    em.write(v + 0x98, f32(*scale))
    return v


SCALES = [(1.0, 2.0, 1.5), (3.0, 2.0, 1.5), (1.0, 1.0, 1.0), (0.5, 0.25, 2.5), (float("nan"), 1.0, 2.0),
          (1.0, float("nan"), 2.0), (1.0, 2.0, float("nan"))]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("scale", SCALES)
def test_get_info(scale, cw):
    def setup(em):
        return vehicle(em, 1, scale), em.alloc(0x58)
    check(e, fk, 0x4F42D0, "Vehicle_GetInfo", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("same", [0, 1])
def test_coll_filter(same):
    def setup(em):
        v = vehicle(em)
        msg = em.alloc(8)
        em.write(msg, struct.pack("<II", 0xCCCCCCCC, v if same else v + 4))
        return v, msg
    check(e, fk, 0x4F4390, "Vehicle_CollFilter", setup=setup, check_eax=False)


def test_parse_args():
    def setup(em):
        msg = em.alloc(12)
        em.write(msg, struct.pack("<III", 0x1010, 0x2020, 5))
        return 0, msg
    check(e, fk, 0x4F43B0, "Vehicle_ParseArgs", setup=setup, check_eax=False)


def test_config_size():
    check(e, fk, 0x4F43D0, "Vehicle_GetConfigSize", setup=lambda em: (0, em.alloc(4)), check_eax=False)


@pytest.mark.parametrize("parts", [None, [], [0x61], [0x61, 0x62, 0x61]])
@pytest.mark.parametrize("addr,name,slot", [(0x4F43E0, "Vehicle_Activate", 0x548176),
                                            (0x4F44A0, "Vehicle_Deactivate", 0x548177)])
def test_activate(addr, name, slot, parts):
    def setup(em):
        v = vehicle(em)
        em.write(v + 0xE8, b"\0" * 4)
        if parts is not None:
            c = new_task(em, 0x60, 0x40)
            em.write(v + 0xE8, struct.pack("<I", c))
            child_list(em, c, ev(slot), LOG2, parts)
        set_handler(em, ev(slot), RIGID, LOG2)
        return v, 0x3333
    check(e, fk, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("hits", [None, [], [0], [1], [1, 0, 1, 1]])
def test_count_in_criteria(hits):
    check(e, fk, 0x4F4560, "Vehicle_CountInCriteria", setup=lambda em: count_setup(em, hits), check_eax=False)


def count_setup(em, hits):
    lst = 0
    vs = []
    if hits is not None:
        lst = em.alloc(8 + 4 * len(hits))
        em.write(lst, struct.pack("<II", len(hits), 0))
        for i in range(len(hits)):
            vs.append(vehicle(em, i))
            em.write(lst + 8 + 4 * i, struct.pack("<I", vs[-1]))
    em.write(0xA76C40, struct.pack("<I", lst))
    fk.rets[0x5042B0] = lambda em, a: hits[vs.index(a[1] - 0x20)]
    msg = em.alloc(8)
    em.write(msg, struct.pack("<II", 0xC0C0, 7))
    return (msg,)


def test_reset_controls():
    check(e, fk, 0x4F4600, "Vehicle_ResetControls", setup=lambda em: (vehicle(em),), check_eax=False)


@pytest.mark.parametrize("parts", [0, 1])
def test_free(parts):
    def setup(em):
        v = vehicle(em)
        em.write(v + 0xE8, struct.pack("<I", 0x4567 if parts else 0))
        return (v,)
    check(e, fk, 0x4F4640, "Vehicle_Free", setup=setup, check_eax=False)


@pytest.mark.parametrize("parts", [0, 1])
def test_run_parts(parts):
    def setup(em):
        v = vehicle(em)
        em.write(v + 0xE8, b"\0" * 4)
        if parts:
            c = new_task(em, 0x61, 0x40)
            em.write(v + 0xE8, struct.pack("<I", c))
            set_handler(em, 1, 0x61, LOG1)
        return (v,)
    check(e, fk, 0x4F4660, "Vehicle_RunParts", setup=setup, check_eax=False)


def test_init():
    def setup(em):
        fk.rets[0x4E0930] = 0x6543
        return (vehicle(em),)
    check(e, fk, 0x4F4680, "Vehicle_Init", setup=setup, check_eax=False)


def test_save_state():
    def setup(em):
        v = vehicle(em, 3)
        msg = em.alloc(12)
        st = em.alloc(0x70)
        em.write(st, bytes(range(0x70)))
        em.write(msg, struct.pack("<III", st, 0x5555, em.alloc(24)))
        return v, msg
    check(e, fk, 0x4F46E0, "Vehicle_SaveState", setup=setup, check_eax=False)


def test_load_state():
    def setup(em):
        v = vehicle(em, 3)
        st = em.alloc(0x70)
        em.write(st, bytes(range(0x70, 0xE0)))
        msg = em.alloc(4)
        em.write(msg, struct.pack("<I", st))
        return v, msg
    check(e, fk, 0x4F4780, "Vehicle_LoadState", setup=setup, check_eax=False)


def test_new_parts():
    def setup(em):
        fk.rets[0x4E0930] = 0x6543
        return (vehicle(em),)
    check(e, fk, 0x4F4800, "Vehicle_NewParts", setup=setup, check_eax=False)


@pytest.mark.parametrize("parts", [0, 1])
def test_delete_parts(parts):
    def setup(em):
        v = vehicle(em)
        em.write(v + 0xE8, struct.pack("<I", 0x4567 if parts else 0))
        return (v,)
    check(e, fk, 0x4F4820, "Vehicle_DeleteParts", setup=setup, check_eax=False)
