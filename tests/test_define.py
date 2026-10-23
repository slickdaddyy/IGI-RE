import struct

import pytest

from test_define_util import (ARGS, GROUP, INFO, LOG, RATE, REAL, RET, TEMPLATE, add_child, cstr, f32, f64, handler,
                              make_emu, run, set_handler, task, trap, u16, u32)

e = make_emu()

SOUND = dict(size=0x130, type=(0x54D9B4, 0x54D9B6), fields=dict(
    offset=0xA0, pos=0x88, vidx=0xB8, pidx=0xBC, sidx=0xC0, vol=0xC4, vol2=0xC8, pitch=0xCC, mind=0xD0, maxd=0xD4,
    range=0xD8, loop=0xDC, isdef=0xDD, stop=0xDE, name=0xDF, sample=0xFF, time=0x120, unk=0x124, bucket=0x128,
    hash=0x12C), fn=dict(
    Init=0x4E8390, Play=0x4E8520, Create=0x4E85E0, Reset=0x4E8780, Run=0x4E8840, Delete=0x4E88E0, Stop=0x4E8900,
    Write=0x4E8910, Find=0x4E8AA0, GetName=0x4E8B00, SetVolume=0x4E8B20, SetPitch=0x4E8B60, GetRange=0x4E8B80,
    GetDuration=0x4E8BD0, Shutdown=0x4E8C20, GetType=0x4E8C50, Update=0x4E8DC0), pre="SoundDefSound",
    define=(0x4E8C60, "DefineSound"), template=[(0x88, 0x56), ("s", 0xDF), ("s", 0xFF), (0x124, 0xC)])
TRIGGER = dict(size=0x138, type=(0x54DA60, 0x54DA62), fields=dict(
    offset=0xA0, pos=0x88, vol=0xB8, vol2=0xBC, pitch=0xC0, mind=0xC4, maxd=0xC8, range=0xCC, tidx=0xD0, vidx=0xD4,
    pidx=0xD8, sidx=0xDC, loop=0xE0, zero=0xE1, isdef=0xE2, name=0xE3, sample=0x103, active=0x123, stop=0x124,
    time=0x128, hold=0x12C, bucket=0x130, hash=0x134), fn=dict(
    Init=0x4E9520, Play=0x4E9760, Create=0x4E9830, Reset=0x4E99D0, Run=0x4E9AA0, Delete=0x4E9B60, Write=0x4E9B80,
    Find=0x4E9D30, GetName=0x4E9D90, SetPitch=0x4E9DB0, GetRange=0x4E9DD0, GetDuration=0x4E9E20, Stop=0x4E9E70,
    Shutdown=0x4E9E80, GetType=0x4E9EB0, Update=0x4EA040), pre="SoundDefTriggerOnce",
    define=(0x4E9EC0, "DefineTriggerOnce"), template=[(0x88, 0x5B), ("s", 0xE3), ("s", 0x103), (0x12C, 0xC)])
GRAPH = dict(size=0x188, type=(0x54DAD8, 0x54DADA), fields=dict(
    offset=0xA0, pos=0x88, vol=0xB8, vol2=0xBC, pitch=0xC0, mind=0xC8, maxd=0xCC, range=0xD0, isdef=0xD4, stop=0xD5,
    vidx=0xD8, pidx=0xDC, aidx=0xE0, sidx=0xE4, vgraph=0xE8, pgraph=0x114, name=0x140, sample=0x160, bucket=0x180,
    hash=0x184), fn=dict(
    Init=0x4EA3D0, Play=0x4EA560, Create=0x4EA600, Reset=0x4EA7B0, Run=0x4EA880, Delete=0x4EA8A0, Stop=0x4EA8C0,
    Write=0x4EA8D0, Find=0x4EAB30, GetName=0x4EAB90, SetPitch=0x4EAC10, GetRange=0x4EAC40, GetDuration=0x4EAC90,
    Shutdown=0x4EACE0, GetType=0x4EAD10), pre="SoundDefGraph",
    define=(0x4EAD20, "DefineGraph"),
    template=[(0x88, 0x3C), (0xC8, 0xD), (0xD8, 0x68), ("s", 0x140), ("s", 0x160), (0x180, 8)])
KINDS = {"sound": SOUND, "trigger": TRIGGER, "graph": GRAPH}
CWS = [0x37F, 0x7F]
PTYPE, OTHER = 0x50, 0x51


def fn(kind, name):
    k = KINDS[kind]
    return k["fn"][name], k["pre"] + "_" + name


def put(e, t, k, **kw):
    for key, v in kw.items():
        off = k["fields"][key]
        if key in ("offset", "pos"):
            e.write(t + off, f64(*v))
        elif key in ("vgraph", "pgraph"):
            e.write(t + off, f32(*v))
        elif key.endswith("idx") or key in ("bucket", "hash"):
            u32(e, t + off, v)
        elif key in ("name", "sample"):
            e.write(t + off, v + b"\0")
        elif key in ("loop", "zero", "isdef", "stop", "active"):
            e.write(t + off, bytes([v]))
        else:
            e.write(t + off, f32(v))


def make_def(e, kind, parent=0, **kw):
    """A definition task with every field set to something."""
    k = KINDS[kind]
    t = task(e, k["size"], 0x60, parent)
    e.write(t + 0x20, bytes((i * 7 + 3) & 0xFF for i in range(k["size"] - 0x20)))
    base = dict(offset=(1.5, -2.25, 3.125), pos=(10.0, 20.0, 30.0), vol=0.75, vol2=0.5, pitch=1.25, mind=2.0,
                maxd=40.0, range=40.0, isdef=0, stop=0, name=b"door_open", sample=b"door1.wav", vidx=-1, pidx=-1,
                sidx=-1, bucket=12, hash=0x1234)
    if kind == "sound":
        base.update(loop=0, time=0.5, unk=-1.0)
    if kind == "trigger":
        base.update(loop=0, zero=0, active=0, time=0.25, hold=-1.0, tidx=-1)
    if kind == "graph":
        base.update(aidx=-1, vgraph=[0.1 * i for i in range(11)], pgraph=[0.5 + 0.1 * i for i in range(11)])
    base.update(kw)
    put(e, t, k, **base)
    u32(e, t + 0x20, 12, 0x1234, 7, 0x12)  # the handle
    return t


# --- type setup ---------------------------------------------------------------------------------------------------


@pytest.mark.parametrize("kind", KINDS)
def test_init(kind):
    run(e, *fn(kind, "Init"))


@pytest.mark.parametrize("kind", KINDS)
def test_shutdown_type(kind):
    k = KINDS[kind]

    def setup(e):
        u16(e, k["type"][0], 0x44)
        u16(e, k["type"][1], 0x45)
    run(e, *fn(kind, "Shutdown"), setup)
    run(e, *fn(kind, "GetType"), setup, check_eax=True, eax_mask=0xFFFF)


@pytest.mark.parametrize("kind", KINDS)
def test_reset(kind):
    run(e, *fn(kind, "Reset"), lambda e: (make_def(e, kind),))


# --- update -------------------------------------------------------------------------------------------------------

INFO_OUT = [0, 0, 0]


def _info(e):
    LOG[-1].append(("info", e.arg(0)))
    e.write(e.arg(1), struct.pack("<3I", *INFO_OUT))
    e.ret(0)


INFO_HANDLER = trap(e, "info", _info)


def owner(e, info=True, pos=None, matrix=None, values=None, type_=PTYPE):
    """A parent task answering the info event."""
    p = task(e, 0x40, type_)
    set_handler(e, INFO, PTYPE, INFO_HANDLER if info else 0)
    INFO_OUT[0] = e.alloc(24) if pos else 0
    if pos:
        e.write(INFO_OUT[0], f64(*pos))
    INFO_OUT[1] = e.alloc(36) if matrix else 0
    if matrix:
        e.write(INFO_OUT[1], f32(*matrix))
    INFO_OUT[2] = 0
    if values is not None:
        INFO_OUT[2] = e.alloc(4 + 4 * len(values))
        u32(e, INFO_OUT[2], len(values))
        for i, v in enumerate(values):
            if isinstance(v, float):
                e.write(INFO_OUT[2] + 4 + 4 * i, f32(v))
            elif isinstance(v, bytes):
                u32(e, INFO_OUT[2] + 4 + 4 * i, cstr(e, v))
            else:
                u32(e, INFO_OUT[2] + 4 + 4 * i, v)
    return p


MATRIX = (0.36, 0.48, -0.8, -0.8, 0.6, 0.0, 0.48, 0.64, 0.6)
MATRIX2 = (0.9137, -0.3218, 0.2479, 0.4011, 0.7123, -0.5761, -0.0679, 0.6237, 0.7786)
ODD = (1.2345678, -2.718281828, 3.14159265)
VALUES = [0.625, 1.5, b"door2.wav", b"door1.wav", 0, 3.0, 0.0, 255.6, -1.0, 2.5, 256.0]
UPDATE_CASES = {
    "none": dict(info=False),
    "empty": dict(),
    "pos": dict(pos=(100.5, -7.25, 3e5)),
    "matrix": dict(pos=(100.5, -7.25, 3e5), matrix=MATRIX),
    "matrix2": dict(pos=(0.1, 0.2, 0.3), matrix=MATRIX2, fields=dict(offset=ODD)),
    "matrix3": dict(matrix=MATRIX2, fields=dict(offset=(1e3 / 7, -1e4 / 3, 0.1))),
    "values": dict(values=VALUES, fields=dict(vidx=0, pidx=1, sidx=2)),
    "same_sample": dict(values=VALUES, fields=dict(vidx=-1, pidx=1, sidx=3)),
    "null_sample": dict(values=VALUES, fields=dict(sidx=4)),
    "clamp_low": dict(fields=dict(vol=-0.5, vol2=1.5)),
    "clamp_nan": dict(fields=dict(vol=float("nan"), vol2=-0.0)),
    "clamp_high": dict(fields=dict(vol=1.0000001, vol2=0.0)),
}


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("case", UPDATE_CASES)
@pytest.mark.parametrize("kind", ["sound", "trigger"])
def test_update(kind, case, cw):
    c = UPDATE_CASES[case]

    def setup(e):
        p = owner(e, c.get("info", True), c.get("pos"), c.get("matrix"), c.get("values"))
        return (make_def(e, kind, p, **c.get("fields", {})),)
    run(e, *fn(kind, "Update"), setup, cw=cw)


# trigger value, triggerOnZero, active, hold, time, loop, playing
TRIGGERS = [
    (5, 0, 0, -1.0, 0.0, 0, 0), (6, 0, 0, -1.0, 0.0, 0, 0), (7, 0, 0, -1.0, 0.0, 0, 0), (8, 0, 0, -1.0, 0.0, 0, 0),
    (10, 0, 0, -1.0, 0.0, 0, 0), (9, 0, 0, -1.0, 0.0, 0, 0), (6, 1, 0, -1.0, 0.0, 0, 0), (5, 1, 0, -1.0, 0.0, 0, 0),
    (5, 0, 1, -1.0, 0.0, 0, 1), (5, 0, 1, -1.0, 0.0, 0, 0), (6, 0, 1, -1.0, 0.0, 0, 0), (6, 0, 1, -1.0, 0.0, 1, 1),
    (6, 0, 1, 2.0, 1.0, 0, 0), (6, 0, 1, 2.0, 2.0, 0, 0), (5, 0, 1, 2.0, 3.0, 0, 1), (-1, 0, 1, 0.0, 0.0, 0, 0),
    (-1, 0, 0, 0.0, 0.0, 0, 0), (6, 0, 1, float("nan"), 0.0, 0, 1),
]


@pytest.mark.parametrize("cw", CWS + [0xC7F, 0x77F])
@pytest.mark.parametrize("trig", TRIGGERS)
def test_trigger(trig, cw):
    idx, zero, active, hold, time, loop, playing = trig

    def setup(e):
        RET[0x4E6FA0] = playing
        p = owner(e, values=VALUES)
        return (make_def(e, "trigger", p, tidx=idx, zero=zero, active=active, hold=hold, time=time, loop=loop),)
    run(e, *fn("trigger", "Update"), setup, cw=cw)


# --- create / define ----------------------------------------------------------------------------------------------


@pytest.mark.parametrize("case", ["null", "def", "inst"])
@pytest.mark.parametrize("kind", KINDS)
def test_create(kind, case):
    def setup(e):
        p = owner(e, pos=(1.0, 2.0, 3.0))
        t = task(e, KINDS[kind]["size"], 0x60, p)
        e.write(t + 0x20, b"\xAB" * (KINDS[kind]["size"] - 0x20))
        if case == "null":
            return t, 0
        d = make_def(e, kind, 0, sample=b"a_long_sample_name.wav")
        arg = e.alloc(8)
        u32(e, arg, d, 1 if case == "def" else 0x100)
        return t, arg
    run(e, *fn(kind, "Create"), setup)


@pytest.mark.parametrize("kind", KINDS)
def test_define(kind):
    k = KINDS[kind]

    def setup(e):
        ARGS.clear()
        ARGS.update({0: b"wind_loop", 1: b"wind.wav", 2: 1.5, 3: -2.0, 4: 1e3, 5: 0.8, 6: 3.0, 7: 50.0})
        for i in range(8, 40):
            ARGS[i] = i * 0.25 if kind == "graph" and i >= 12 else i - 9
        if kind == "sound":
            ARGS[11] = 0.5
        if kind == "trigger":
            ARGS[8] = 2.5
        TEMPLATE[:] = k["template"]
        e.write(e.out, b"\x11" * 24)
        return e.out, 0x2000, 0x3000
    run(e, *k["define"], setup, check_eax=True, ignore=[(e.out + 4, e.out + 8), (e.out + 0x14, e.out + 0x18)])


# --- run ----------------------------------------------------------------------------------------------------------

# stop, loop, duration, time, playing
RUNS = [(1, 0, 1.0, 0.0, 0), (0, 1, 1.0, 5.0, 0), (0, 0, 2.0, 1.0, 0), (0, 0, 1.0, 1.0, 0), (0, 0, 1.0, 2.0, 1),
        (0, 0, float("nan"), 0.0, 0), (0, 0, 0.5, 0.25, 1)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("r", RUNS)
def test_run_sound(r, cw):
    stop, loop, dur, time, playing = r

    def setup(e):
        REAL[0x496DB0] = dur
        RET[0x4E6FA0] = playing
        u32(e, RATE, 30)
        return (make_def(e, "sound", owner(e), stop=stop, loop=loop, time=time),)
    run(e, *fn("sound", "Run"), setup, cw=cw)


CHILD = handler(e, "child", 1)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("children", [0, 1, 3])
@pytest.mark.parametrize("state", ["stop", "active", "idle"])
def test_run_trigger(state, children, cw):
    def setup(e):
        u32(e, RATE, 25)
        p = owner(e)
        t = make_def(e, "trigger", p, stop=state == "stop", active=state == "active", hold=5.0, time=1.0)
        for i in range(children):
            add_child(e, t, task(e, 0x40, 0x61 + (i & 1)))
        set_handler(e, 1, 0x61, CHILD)
        set_handler(e, 1, 0x62, 0)
        return (t,)
    run(e, *fn("trigger", "Run"), setup, cw=cw)


@pytest.mark.parametrize("stop", [0, 1])
def test_run_graph(stop):
    run(e, *fn("graph", "Run"), lambda e: (make_def(e, "graph", owner(e), stop=stop),))


@pytest.mark.parametrize("isdef", [0, 1])
@pytest.mark.parametrize("kind", KINDS)
def test_delete_stop(kind, isdef):
    run(e, *fn(kind, "Delete"), lambda e: (make_def(e, kind, isdef=isdef),))
    run(e, *fn(kind, "Stop"), lambda e: (make_def(e, kind),))


# --- play ---------------------------------------------------------------------------------------------------------

# isDef, defs arg, flags, distance, same hash, loop, active
PLAYS = [(1, 1, 2, 0.0, 1, 0, 1), (1, 0, 2, 10.0, 1, 0, 1), (0, 1, 0, 1e9, 0, 1, 1), (0, 0, 2, 163839.0, 0, 0, 1),
         (0, 0, 2, 163841.0, 1, 1, 1), (0, 0, 0x12, 163840.0, 1, 0, 1), (0, 0, 0x10, 5.0, 0, 0, 0)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("p", PLAYS)
@pytest.mark.parametrize("kind", KINDS)
def test_play(kind, p, cw):
    isdef, defs, flags, dist, same, loop, active = p

    def setup(e):
        kw = dict(isdef=isdef, pos=(dist, 0.0, 0.0), range=40.0, hash=0x1234 if same else 0x999, bucket=44)
        if kind != "graph":
            kw["loop"] = loop
        if kind == "trigger":
            kw["active"] = active
        t = make_def(e, kind, **kw)
        u32(e, t + 0x2C, flags)
        listener = e.alloc(0x80)
        arg = e.alloc(8)
        u32(e, arg, listener, defs | 0x100)
        return t, arg
    run(e, *fn(kind, "Play"), setup, cw=cw)


# --- events -------------------------------------------------------------------------------------------------------


@pytest.mark.parametrize("indent", [0, 2])
@pytest.mark.parametrize("last", [0, 1])
@pytest.mark.parametrize("kind", KINDS)
def test_write(kind, indent, last):
    def setup(e):
        p = task(e, 0x40, GROUP)
        t = make_def(e, kind, p, loop=1, zero=1) if kind == "trigger" else make_def(e, kind, p)
        other = task(e, 0x40, 0x60, p)
        u32(e, p + 8, t if last else other)
        u32(e, p + 0x10, t if last else other)
        u32(e, t + 4, p + 8)
        u32(e, other + 4, p + 8)
        buf = e.alloc(0x1000)
        e.write(buf, b"DefineGroup(\"x\",\r\n\0")
        arg = e.alloc(8)
        u32(e, arg, buf, indent)
        return t, arg
    run(e, *fn(kind, "Write"), setup)


@pytest.mark.parametrize("case", ["match", "other", "found"])
@pytest.mark.parametrize("kind", KINDS)
def test_find_name(kind, case):
    def setup(e):
        t = make_def(e, kind)
        arg = e.alloc(8)
        u32(e, arg, cstr(e, b"door_open" if case != "other" else b"door_opem"), 0x5555 if case == "found" else 0)
        return t, arg
    run(e, *fn(kind, "Find"), setup)
    for name in (b"", b"x"):
        run(e, *fn(kind, "GetName"), lambda e: (make_def(e, kind, name=name), e.alloc(4)))


@pytest.mark.parametrize("kind", KINDS)
def test_setters(kind):
    def vol(a, b):
        return lambda e: (make_def(e, kind), cstr(e, f32(a, b)))
    setvol = (0x4E8B20, "SoundDefSound_SetVolume") if kind == "sound" else (0x4EABD0, "SoundDefGraph_SetVolume")
    for a, b in ((0.25, 0.5), (-0.1, 0.0), (0.0, -2.0), (float("nan"), 1.0)):
        run(e, *setvol, vol(a, b))
    run(e, *fn(kind, "SetPitch"), lambda e: (make_def(e, kind), cstr(e, f32(1.75))))
    run(e, 0x4EABB0, "SoundDef_SetPos", lambda e: (make_def(e, kind), cstr(e, f64(1.0, -2.0, 3.5))))
    for on in (0, 1, 0x100):
        for flags in (0x10, 0x12):
            def setup(e, on=on, flags=flags):
                t = make_def(e, kind)
                u32(e, t + 0x2C, flags)
                return t, cstr(e, struct.pack("<I", on)[:1])
            run(e, 0x4EAC20, "SoundDef_SetGlobal", setup)


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("parent", ["none", "group", "other"])
@pytest.mark.parametrize("cur", [0.0, 45.0, 1e6])
@pytest.mark.parametrize("kind", KINDS)
def test_range(kind, parent, cur, cw):
    def setup(e):
        p = 0 if parent == "none" else task(e, 0x40, GROUP if parent == "group" else OTHER)
        t = make_def(e, kind, p, offset=(3.0, 4.0, 1e-3), range=40.0)
        return t, cstr(e, f32(cur))
    run(e, *fn(kind, "GetRange"), setup, cw=cw)


@pytest.mark.parametrize("cw", CWS + [0xC7F, 0x77F])
@pytest.mark.parametrize("dur", [0.0, 1.23456789, 2.5, 100.0])
@pytest.mark.parametrize("cur", [-5, 40])
@pytest.mark.parametrize("kind", KINDS)
def test_duration(kind, dur, cur, cw):
    def setup(e):
        REAL[0x496DB0] = dur
        u32(e, RATE, 30)
        return make_def(e, kind), cstr(e, struct.pack("<i", cur))
    run(e, *fn(kind, "GetDuration"), setup, cw=cw)
