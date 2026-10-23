import random
import struct

import pytest

from test_d3d_util import D3DEmu
from test_sysmisc_util import Com, Fakes, check, check_error, u32

DEV, STATE, CAPS, DEV2, DI, PREV, COUNT, DEADZONE = (0xA7B2E0, 0xA7B2F0, 0xA7B430, 0xA7B4E0, 0xA7B4F0, 0xA7B4F8,
                                                      0xA7B638, 0xA7B63C)
AXES, INPUT = 0x54E8A8, 0xBCAA00
DIERR_OBJECTNOTFOUND, DIERR_INPUTLOST = 0x80070002, 0x8007001E

e = D3DEmu()
fk = Fakes(e)
ST = {}


def hr(slot):
    return lambda em, a: ST.get("hr", lambda s, em, a: 0)(slot, em, a) & 0xFFFFFFFF


fk.add(0x48F090, 0, ret=0x400000)
fk.add(0x48F0A0, 0, ret=0x1234)

di = Com(fk, {2: 1, 3: 4, 4: 5}, derefs={3: {1: 16}})
dev = Com(fk, {0: 3, 2: 1, 3: 2, 6: 3, 7: 1, 8: 1, 9: 3, 11: 2, 13: 3, 25: 1},
          derefs={0: {1: 16}, 3: {1: 4}, 6: {2: lambda em, p: em.u32(p)}})
for _s in range(48):
    di.rets[_s] = hr(("di", _s))
    dev.rets[_s] = hr(_s)


def _create_device(em, a):
    em.write(a[2], u32(ST["devs"].pop(0)))


def _query(em, a):
    em.write(a[2], u32(ST["dev2"][a[0]]))


def _caps(em, a):
    em.write(a[1] + 0x10, u32(ST.get("buttons", 12)))


def _state(em, a):
    em.write(a[2], ST["states"].pop(0))


di.fns[3] = _create_device
dev.fns[0] = _query
dev.fns[3] = _caps
dev.fns[9] = _state


def _di_create(em):
    fk.log.append(("DirectInputCreateA", em.arg(0), em.arg(1), em.arg(2), em.arg(3)))
    if "di" in ST:
        em.write(em.arg(2), u32(ST["di"]))
    em.ret(ST.get("create_hr", 0), 16)


e.stub("DirectInputCreateA", _di_create)


def make_di(em, count):
    """A DirectInput object whose EnumDevices calls the callback for count devices."""
    obj = di.new(em)
    vt = em.u32(obj)
    insts = []
    for i in range(count):
        inst = em.alloc(0x40)
        em.write(inst, u32(0x244) + bytes(range(16 * i, 16 * i + 16)))
        insts.append(inst)
    code = b""
    for inst in insts:
        code += b"\xff\x74\x24\x10" + b"\x68" + u32(inst) + b"\xff\x54\x24\x14"
    p = em.alloc(len(code) + 5)
    code += b"\xe9" + struct.pack("<i", di.slots[4] - (p + len(code) + 5))
    em.write(p, code)
    em.write(vt + 16, u32(p))
    return obj


def make_devs(em, count):
    devs = [dev.new(em) for _ in range(count)]
    dev2 = [dev.new(em) for _ in range(count)]
    ST["devs"] = list(devs)
    ST["dev2"] = dict(zip(devs, dev2))
    return devs, dev2


def setup_init(em, count, hrf=None, create_hr=0):
    ST.clear()
    ST["hr"] = hrf or (lambda s, em, a: 0)
    ST["create_hr"] = create_hr
    make_devs(em, count)
    ST["di"] = make_di(em, count)
    em.write(DEADZONE, u32(1500))
    em.write(COUNT, u32(7))


def axis_of(em, a):
    return em.u32(a[2] + 8)


@pytest.mark.parametrize("count", [0, 1, 2, 4])
def test_init(count):
    check(e, fk, 0x509980, "Joystick_Init", setup=lambda em: setup_init(em, count), check_eax=False)


CASES = {
    "dataformat": lambda s, em, a: -1 if s == 11 and a[0] == ST["dev2"][ST["first"]] else 0,
    "coop": lambda s, em, a: 0x80004005 if s == 13 else 0,
    "noaxes": lambda s, em, a: DIERR_OBJECTNOTFOUND if s == 6 and a[1] == 4 and axis_of(em, a) in (0x10, 0x1C)
    else 0,
    "range": lambda s, em, a: 0x80004005 if s == 6 and a[1] == 4 and axis_of(em, a) == 0x14 else 0,
    "deadzone": lambda s, em, a: 1 if s == 6 and a[1] == 5 and axis_of(em, a) == 0x8 else 0,
    "autocenter": lambda s, em, a: 0x80004005 if s == 6 and a[1] in (6, 9) else 0,
}


@pytest.mark.parametrize("case", list(CASES))
@pytest.mark.parametrize("count", [1, 3])
def test_init_devices(case, count):
    def setup(em):
        setup_init(em, count, CASES[case])
        ST["first"] = ST["devs"][0]
        em.write(AXES, b"".join(u32(o) + u32(1) for o in (0xC, 0x10, 0x14, 0, 4, 8, 0x18, 0x1C)))
    check(e, fk, 0x509980, "Joystick_Init", setup=setup, check_eax=False)


ERRORS = {
    "create": (dict(create_hr=0x80004005), None),
    "device": ({}, lambda s, em, a: 0x80004005 if s == ("di", 3) else 0),
    "query": ({}, lambda s, em, a: 0x80004002 if s == 0 else 0),
    "acquire": ({}, lambda s, em, a: 0x80004005 if s == 7 else 0),
    "caps": ({}, lambda s, em, a: 1 if s == 3 else 0),
}


@pytest.mark.parametrize("case", list(ERRORS))
def test_init_errors(case):
    kw, hrf = ERRORS[case]

    def setup(em):
        setup_init(em, 2, hrf, **kw)
    check_error(e, fk, 0x509980, "Joystick_Init", setup=setup)


@pytest.mark.parametrize("present", [False, True])
@pytest.mark.parametrize("ret", [0, 1, 0x80004005])
def test_acquire(present, ret):
    def setup(em):
        ST.clear()
        ST["hr"] = lambda s, em, a: ret
        d = dev.new(em)
        em.write(DEV + 8, u32(d))
        em.write(DEV2 + 8, u32(dev.new(em) if present else 0))
        return (2,)
    check(e, fk, 0x509CC0, "Joystick_Acquire", setup=setup)


def joy_state(rng):
    vals = [rng.choice([0, 1, -1, 127, -128, rng.randint(-1000, 1000)]) for _ in range(8)]
    povs = [rng.getrandbits(32) for _ in range(4)]
    buttons = bytes(rng.choice([0, 0x80, 0x7F, 0xFF, 0x01]) for _ in range(32))
    return struct.pack("<8i4I", *vals, *povs) + buttons


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("seed", range(12))
def test_poll(seed, cw):
    rng = random.Random(seed)
    count = rng.randint(0, 4)
    present = [rng.random() < 0.8 for _ in range(count)]
    states = [joy_state(rng) for _ in range(count)]
    olds = [joy_state(rng) for _ in range(count)]
    flags = [bytes([rng.randint(0, 1), 1, 0, 1, rng.randint(0, 1), rng.randint(0, 1), 0, 12]) for _ in range(count)]
    prev_buttons = [rng.choice([0, 5, 0xFFFFFFFF]) for _ in range(count)]
    fails = {i: rng.choice([0, 0, -1, DIERR_INPUTLOST]) for i in range(count)}

    def setup(em):
        ST.clear()
        ST["states"] = [s for s, p in zip(states, present) if p]
        devs = {}

        def hrf(s, em, a):
            if s == 9:
                return fails[devs[a[0]]]
            return 0
        ST["hr"] = hrf
        em.write(COUNT, u32(count))
        for i in range(count):
            d = dev.new(em)
            devs[d] = i
            em.write(DEV + 4 * i, u32(d))
            em.write(DEV2 + 4 * i, u32(d if present[i] else 0))
            em.write(STATE + 0x50 * i, olds[i])
            em.write(INPUT + 0x28 * i, flags[i] + b"\x33" * 0x18 + u32(prev_buttons[i]) + u32(0x77))
    check(e, fk, 0x509CF0, "Joystick_Poll", setup=setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("has_di", [False, True])
@pytest.mark.parametrize("count", [0, 3])
def test_shutdown(has_di, count):
    def setup(em):
        ST.clear()
        em.write(DI, u32(di.new(em) if has_di else 0))
        em.write(COUNT, u32(count))
        for i in range(count):
            em.write(DEV2 + 4 * i, u32(dev.new(em) if i != 1 else 0))
    check(e, fk, 0x509E90, "Joystick_Shutdown", setup=setup, check_eax=False)


@pytest.mark.parametrize("present", [False, True])
def test_init_input(present):
    def setup(em):
        em.write(DEV2 + 4, u32(0x5000 if present else 0))
        em.write(CAPS + 0x2C + 0x10, u32(0x1234))
        em.write(INPUT + 0x28, b"\x99" * 0x28)
        return (1,)
    check(e, fk, 0x509F00, "Joystick_InitInput", setup=setup, check_eax=False)


@pytest.mark.parametrize("case", [None] + list(CASES))
@pytest.mark.parametrize("count", [0, 2])
def test_enum_device(case, count):
    def setup(em):
        ST.clear()
        ST["hr"] = CASES.get(case, lambda s, em, a: 0)
        make_devs(em, 1)
        ST["first"] = ST["devs"][0]
        em.write(DI, u32(di.new(em)))
        em.write(COUNT, u32(count))
        em.write(DEADZONE, u32(900))
        inst = em.alloc(0x40)
        em.write(inst, u32(0x244) + bytes(range(16)))
        return inst, 0x77
    check(e, fk, 0x509A40, "Joystick_EnumDevice", setup=setup)
