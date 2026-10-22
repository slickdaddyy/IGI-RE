import pytest

from test_soldier_util import LOG, NAN, RET, f32, fake, float_fake, new_emu, pyfake, run, trap, u32, u8

HANDLERS = 0xA96AE0
EVENT = 0x2B
HUMAN_TYPE = 0x31

e = new_emu()  # task type functions, ReadChannels faked
r = new_emu()  # ReadChannels itself


def param_add(em):
    a = [em.arg(i) for i in range(6)]
    LOG[-1].append((0x4F1A80, a[0], a[2] - a[1], a[3], a[4], a[5]))
    em.ret(0)


def read_channels(em, a):
    f32(em, a[0], *[RET.get("chan", 0.25) + i for i in range(40)])


def file_load(em, a):
    u32(em, a[1], RET.get("size", 0))
    return RET.get(0x4B18B0, 0)


def control(em):
    t, msg = em.arg(0), em.arg(1)
    LOG[-1].append(("control", t, em.u32(msg), em.read(em.u32(msg + 4), 160)))
    em.ret(0)


for addr, n, kw in (
        (0x401900, 4, dict(strings=(0,), masks={2: 0xFFFF}, ret=0x56)), (0x4018F0, 0, dict(ret=0xABCD0001)),
        (0x401400, 3, dict(masks={0: 0xFFFF, 1: 0xFF})), (0x401530, 3, dict(masks={0: 0xFFFF})),
        (0x401A20, 1, dict(masks={0: 0xFFFF})), (0x4C1830, 1, {}),
        (0x4F1A60, 0, dict(ret=0x7700A1)), (0x4F1A70, 0, dict(ret=0x7700A2)), (0x4F40D0, 0, dict(ret=0x77000 | EVENT)),
        (0x4F1950, 1, dict(ret=0x1950)), (0x4F1000, 0, dict(ret=0x1000)), (0x4F0FF0, 0, dict(ret=0xFF0)),
        (0x4F0FC0, 0, dict(ret=0xFC0)), (0x4F1320, 1, {}), (0x4F1340, 1, {}), (0x4F1400, 2, {}),
        (0x4F16C0, 1, {}), (0x46C0A0, 1, {}), (0x45E060, 2, dict(effect=read_channels, masks={0: 0}, also="HumanPlayerInput_ReadChannels")),
        (0x4B0C60, 2, dict(ret=0xC60000)), (0x4B0D10, 1, {}), (0x4B1510, 2, dict(strings=(0, 1))),
        (0x4B1700, 3, {}), (0x4B1690, 1, {}), (0x4B18B0, 2, dict(strings=(0,), effect=file_load, masks={1: 0})),
        (0x4B1A30, 1, {})):
    fake(e, addr, n, **kw)
pyfake(e, 0x4F1A80, param_add)
float_fake(e, 0x4F16E0, 2)
CONTROL = trap(e, "control", control)
e.baseline = e.snapshot()


def cheat(em, a):
    return RET.get(("key", a[0]), 0)


fake(r, 0x406220, 0, ret=0)
fake(r, 0x414FD0, 1, effect=cheat)
fake(r, 0x415020, 1, effect=cheat)
r.baseline = r.snapshot()


def test_open():
    run(e, 0x45DB70, "HumanPlayerInput_Open", lambda em: None)


def task(em, record=0, playback=0, name=b"demo.inp", played=0, recorded=0, frame=0, frames=0):
    t = em.alloc(0x1E4)
    em.write(t, bytes((i * 7 + 3) & 0xFF for i in range(0x1E4)))
    u8(em, t + 0x80, record, playback)
    em.write(t + 0x82, name + b"\0")
    u32(em, t + 0x184, played, recorded, frame, frames)
    return t


def test_create():
    run(e, 0x45DCB0, "HumanPlayerInput_CreateHandler", lambda em: (task(em),))


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("record,playback,frame,frames,expr,speed", [
    (0, 0, 0, 0, 0, 0.0), (0, 1, 2, 5, 1, 2.5), (0, 1, 5, 5, 1, 3.5), (0, 1, 0, 0, 0, 0.0),
    (1, 0, 3, 3, 1, -1.5), (1, 0, 9000, 9000, 0, 0.0), (1, 1, 1, 0, 0, 0.0), (1, 1, 1, 4, 1, 1e10)])
@pytest.mark.parametrize("handled", [0, 1])
def test_run(record, playback, frame, frames, expr, speed, handled, cw):
    def setup(em):
        human = em.alloc(0x40)
        u32(em, human + 0x1C, HUMAN_TYPE)
        played = em.alloc(160 * 6)
        f32(em, played, *[i * 0.5 for i in range(240)])
        rec = em.alloc(160 * 6)
        t = task(em, record, playback, played=played, recorded=rec - 160 * max(frame - 3, 0), frame=frame,
                 frames=frames)
        u32(em, t + 0x14, human)
        u32(em, HANDLERS + 4 * (EVENT * 0x180 + HUMAN_TYPE), CONTROL if handled else 0)
        RET[0x4F16C0] = expr
        RET[0x4F16E0] = speed
        return (t,)
    run(e, 0x45DCF0, "HumanPlayerInput_RunHandler", setup, cw=cw)


@pytest.mark.parametrize("record,playback,played,recorded,frames,opened", [
    (0, 0, 0, 0, 0, 0), (1, 0, 0, 0xAB0000, 0, 0), (1, 0, 0, 0xAB0000, 12, 0xF11E), (1, 1, 0x5000, 0xAB0000, 12, 0),
    (0, 1, 0x5000, 0, 3, 0), (0, 1, 0, 0, 3, 0), (1, 0, 0, 0, 7, 0xF11E)])
def test_delete(record, playback, played, recorded, frames, opened):
    def setup(em):
        RET[0x4B1510] = opened
        return (task(em, record, playback, b"rec_" + b"x" * 200, played, recorded, 0, frames),)
    run(e, 0x45DE20, "HumanPlayerInput_DeleteHandler", setup)


def test_get_params():
    def setup(em):
        u32(em, 0x5BDAC8, 0xAC8AC8)
        return 0, em.alloc(4)
    run(e, 0x45DF40, "HumanPlayerInput_GetParams", setup)


@pytest.mark.parametrize("record,playback,data,size", [
    (0, 0, 0, 0), (1, 1, 0, 0), (0, 1, 0, 0), (0, 1, 0xDA7A, 1600), (0, 1, 0xDA7A, 1759), (0, 1, 0xDA7A, 0xFFFFFFF0)])
def test_params_changed(record, playback, data, size):
    def setup(em):
        RET[0x4B18B0] = data
        RET["size"] = size
        return (task(em, record, playback, b"walk.rec"),)
    run(e, 0x45DF50, "HumanPlayerInput_ParamsChanged", setup)


def test_close():
    def setup(em):
        u32(em, 0x5BDAC8, 0xAC8)
        u32(em, 0x5BDAC4, 0xFFFF0042)
    run(e, 0x45E040, "HumanPlayerInput_Close", setup)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("bits", [0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA, 0x12345678])
@pytest.mark.parametrize("invert,speed", [(0, 0.5), (1, 0.01), (5, 3.0), (0, NAN), (0, 1.0), (0, 0.05)])
def test_read_channels(bits, invert, speed, cw):
    def setup(em):
        prof = em.alloc(0x540)
        u32(em, prof + 0x538, invert)
        f32(em, prof + 0x53C, speed)
        RET[0x406220] = prof
        RET[("key", 0x4E)] = 1
        RET[("key", 0x37)] = 0x80
        RET[("key", 0x21)] = -3
        f32(em, 0xBC20F4, 12.75, -3.3)
        u32(em, 0xBC210C, bits)
        out = em.alloc(160)
        em.write(out, b"\xEE" * 160)
        return out, 0x1234
    run(r, 0x45E060, "HumanPlayerInput_ReadChannels", setup, cw=cw)
