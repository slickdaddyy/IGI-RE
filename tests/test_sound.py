import struct

import pytest

from emu import Emu
from test_menu_util import LOG, RET, add_child, check, cstr, f32, handler, logfake, set_handler, task, trap, u16, u32

FREE, COUNT, ACTIVE, NEXTID, RATE, INSTS = 0xA5E5EC, 0xA5E5F0, 0xA5E5F4, 0xA5E5F8, 0xA5E5FC, 0xA5E600
DISABLED, TASKS, DEFS = 0xA5E60A, 0xA5E60C, 0xA5E610
EVENTS = {0x4E65F0: 0x548812, 0x4E6610: 0x548813, 0x4E6630: 0x548814, 0x4E6650: 0x548815, 0x4E6670: 0x548816,
          0x4E6690: 0x548817, 0x4E66B0: 0x548818, 0x4E66D0: 0x54881B, 0x4E66F0: 0x548819, 0x4E6710: 0x54881A}
NAMES = {0x4E65F0: "Sound_GetListEvent", 0x4E6610: "Sound_GetInfoEvent", 0x4E6630: "Sound_GetFindEvent",
         0x4E6650: "Sound_GetNameEvent", 0x4E6670: "Sound_GetSetPosEvent", 0x4E6690: "Sound_GetSetVolumeEvent",
         0x4E66B0: "Sound_GetSetPitchEvent", 0x4E66D0: "Sound_GetSetGlobalEvent", 0x4E66F0: "Sound_GetRangeEvent",
         0x4E6710: "Sound_GetDurationEvent"}

# fakes of everything the sound system calls
e = Emu()
for addr, n, kw in [
        (0x4017C0, 1, {}), (0x401810, 1, {}), (0x4012A0, 3, {"derefs": {2: 5}, "masks": {1: 0xFFFF}}),
        (0x4C1790, 2, {}), (0x4C17C0, 2, {}), (0x4C1800, 1, {}), (0x4C1830, 1, {}),
        (0x4B8930, 2, {"strings": (0,)}), (0x4B80B0, 1, {"strings": (0,)}), (0x4B85B0, 1, {}), (0x4B83D0, 1, {}),
        (0x495DF0, 0, {}), (0x495F70, 2, {"strings": (0,)}), (0x4965A0, 1, {"strings": (0,)}),
        (0x496650, 1, {}), (0x496E40, 1, {}), (0x496E60, 2, {"derefs": {1: 0x58}}),
        (0x495CC0, 1, {}), (0x4B1420, 3, {"strings": (0,)}),
        (0x4B5B60, 1, {"strings": (0,)}), (0x4B5F00, 2, {"strings": (0,)}), (0x4B5D80, 3, {"strings": (0,)}),
        (0x4B6380, 1, {"strings": (0,)})]:
    logfake(e, addr, n, **kw)
logfake(e, 0x4966E0, 1, effect=lambda e, a: a[0] & 1)  # Voice_IsPlaying: odd voices play
logfake(e, 0x4B1EE0, 3, strings=(2,), masks={0: 0}, effect=lambda e, a: e.write(a[0], b"DIR:\0"))
EXT = [b".res"]
logfake(e, 0x4B1E90, 3, strings=(2,), masks={0: 0}, effect=lambda e, a: e.write(a[0], EXT[0] + b"\0"))


def run(orig, ours, setup=None, **kw):
    RET.clear()
    kw.setdefault("check_eax", False)
    return check(e, orig, ours, setup=setup, normalized=True, **kw)


@pytest.mark.parametrize("getter", sorted(EVENTS))
@pytest.mark.parametrize("value", [0xFF, 0x25])
def test_event_getter(getter, value):
    def setup(e):
        e.write(EVENTS[getter], bytes([value]))
        RET[0x4017C0] = 0x1234
    run(getter, NAMES[getter], setup, check_eax=True, eax_mask=0xFF)


@pytest.mark.parametrize("value", [0xFF, 0x25])
def test_stop_event(value):
    def setup(e):
        u32(e, 0x54881C, value)
        RET[0x401810] = 0x41
    run(0x4E6730, "Sound_GetStopEvent", setup, check_eax=True)


def insts(e, n):
    base = e.alloc(0x78 * n)
    return [base + 0x78 * i for i in range(n)]


def link(e, head, items):
    prev = 0
    for i, p in enumerate(items):
        u32(e, p, prev, items[i + 1] if i + 1 < len(items) else 0)
        prev = p
    u32(e, head, items[0] if items else 0)


@pytest.mark.parametrize("n", [0, 2])
def test_push(n):
    def setup(e):
        s = insts(e, 3)
        link(e, FREE, s[:n])
        return s[2], FREE
    run(0x4E6290, "SoundInst_Push", setup)


@pytest.mark.parametrize("which", [0, 1, 2])
def test_unlink(which):
    def setup(e):
        s = insts(e, 3)
        link(e, ACTIVE, s)
        return s[which], ACTIVE
    run(0x4E63B0, "SoundInst_Unlink", setup)


def active(e, specs):
    """specs: (flags, voice, volume, age, hash) per active sound."""
    s = insts(e, len(specs) + 1)
    link(e, ACTIVE, s[:len(specs)])
    link(e, FREE, s[len(specs):])
    for p, (flags, voice, vol, age, h) in zip(s, specs):
        u32(e, p + 8, flags, 0x11, h, 0x22, age, voice)
        f32(e, p + 0x20, vol, 1.0, 0.5)
    return s


SPECS = [(0, 0, 1.0, 0, 5), (0, 0x103, 1.0, 1, 6), (0, 0, 1.0, 2, 5), (1, 0, 1.0, 0, 7), (1, 0, 0.0, 0, 5),
         (1, 0x105, 1.0, 0, 5), (1, 0x107, 0.0, 0, 8), (1, 0x108, 1.0, 0, 5), (3, 0x109, -0.0, 0, 9)]


def test_update():
    run(0x4E62C0, "SoundSys_Update", lambda e: active(e, SPECS) and None)
    run(0x4E62B0, "SoundSysController_Run", lambda e: active(e, SPECS[:3]) and None)


def test_free_resource_entry():
    def setup(e):
        active(e, SPECS)
        name = cstr(e, b"LOCAL:sounds/ABCD.wav")
        h = e.call(0x497410, cstr(e, b"ABCD"))[0]
        for i, p in enumerate((e.u32(ACTIVE), e.u32(e.u32(ACTIVE) + 4))):
            u32(e, p + 0x10, h)
        p = e.alloc(8)
        u32(e, p, name, 0x99)
        return (p,)
    run(0x4E6830, "Sound_FreeResourceEntry", setup)


def test_def_create():
    def setup(e):
        u32(e, DEFS, 0x5555)
        t = task(e, 0xA0)
        RET[0x4B80B0] = 0x777
        return t, cstr(e, b"LOCAL:sounds/sounds.qsc")
    run(0x4E5EC0, "SoundSysDef_Create", setup)

    def setup0(e):
        r = setup(e)
        RET[0x4B80B0] = 0
        return r
    run(0x4E5EC0, "SoundSysDef_Create", setup0)


def test_small():
    run(0x4E5F50, "SoundSysDef_Delete", lambda e: (u32(e, DEFS, 0x5555), (0x1234,))[1])
    run(0x4E6000, "SoundSysDefParent_Create", lambda e: (task(e, 0xA0), cstr(e, b"abc/def")))
    run(0x4E6030, "Sound_SetRate", args=(30,))
    run(0x4E6050, "Sound_GetRate", lambda e: u32(e, RATE, 25), check_eax=True)
    run(0x4E6060, "Sound_SetUnusedByte", args=(0x101,))
    run(0x4E6530, "SoundSys_NewController", lambda e: u16(e, 0x548808, 0x44), args=(0x1000,))
    run(0x4E65D0, "SoundSys_NewDef", lambda e: (u16(e, 0x54880C, 0x45), (0x1000, cstr(e, b"x")))[1])
    run(0x4E6D20, "Sound_AddTask", lambda e: u32(e, TASKS, 0x6666), args=(0x1234,))
    run(0x4E67B0, "Sound_LoadResourceEntry", lambda e: (lambda p: (u32(e, p, cstr(e, b"a.wav"), 7), (p,))[1])(e.alloc(8)))


@pytest.mark.parametrize("dir_", [b"LOCAL:sounds/", b"LOCAL:sounds\\", b"MISSION:sounds"])
def test_load_defs(dir_):
    def setup(e):
        u16(e, 0x54880E, 0x46)
        u16(e, 0x54880C, 0x47)
        RET[0x4012A0] = 0x4321
        return 0x1000, cstr(e, dir_)
    run(0x4E6550, "SoundSys_LoadDefs", setup, check_eax=True)


@pytest.mark.parametrize("exists", [0, 1])
@pytest.mark.parametrize("fn", [(0x4E6750, "Sound_LoadResource"), (0x4E67D0, "Sound_FreeResource")])
def test_resource(exists, fn):
    def setup(e):
        RET[0x4B5B60] = exists
        return (cstr(e, b"LOCAL:sounds/x.res"),)
    run(fn[0], fn[1], setup)


def test_scan():
    run(0x4E68D0, "SoundLoad", lambda e: (cstr(e, b"LOCAL:common/sounds"),))
    run(0x4E69C0, "SoundFree", lambda e: (cstr(e, b"LOCAL:common/sounds"),))


@pytest.mark.parametrize("ext", [b".res", b".wav", b".WAV", b".txt", b""])
@pytest.mark.parametrize("isdir", [0, 1])
@pytest.mark.parametrize("fn", [(0x4E68F0, "SoundLoad_File"), (0x4E69E0, "SoundFree_File")])
def test_scan_file(ext, isdir, fn):
    def setup(e):
        EXT[0] = ext
        RET[0x4B5B60] = 1
        f = e.alloc(12)
        u32(e, f, cstr(e, b"LOCAL:sounds/a" + ext), 0x1234, isdir)
        return (f,)
    run(fn[0], fn[1], setup)


@pytest.mark.parametrize("n", [0, 1, 3])
def test_params_new(n):
    def setup(e):
        e.call(0x4B0BA0)
    run(0x4E6AB0, "SoundParams_New", setup, args=(n,))


FOUND = [0]


def _find(e):
    t, arg = e.arg(0), e.arg(1)
    LOG[-1].append(("find", t, e.read_cstr(e.u32(arg))))
    if FOUND[0]:
        u32(e, arg + 4, FOUND[0])
    e.ret(0)


FIND_NONE = handler(e, "none", 2, derefs={1: 8})
FIND_SOUND = trap(e, "find", _find)
STOP = handler(e, "stop", 1)
SETTERS = {n: handler(e, "set%d" % n, 2, derefs={1: n}) for n in (24, 8, 4, 1)}


@pytest.mark.parametrize("case", ["ok", "nochild", "missing", "disabled", "empty", "noname"])
def test_play(case):
    def setup(e):
        u32(e, 0x548814, 0x30)
        defs = [task(e, 0x40, 0x50), task(e, 0x40, 0x51)]
        sound = task(e, 0x40, 0x52)
        if case != "nochild":
            for i in range(3):
                add_child(e, sound, task(e, 0x40, 0x60 + i))
        lst = e.alloc(16)
        u32(e, lst, 0 if case == "empty" else 2, 8, *defs)
        u32(e, DEFS, lst)
        e.write(DISABLED, bytes([case == "disabled"]))
        set_handler(e, 0x30, 0x50, FIND_NONE)
        set_handler(e, 0x30, 0x51, FIND_SOUND)
        FOUND[0] = 0 if case == "missing" else sound
        RET[0x4012A0] = 0x8888
        return 0x1000, cstr(e, b"" if case == "noname" else b"gunshot")
    run(0x4E6B00, "Sound_Play", setup, check_eax=True)


@pytest.mark.parametrize("has", [0, 1])
@pytest.mark.parametrize("ev", [0xFF, 0x31])
def test_stop(has, ev):
    def setup(e):
        u32(e, 0x54881C, ev)
        RET[0x401810] = 0x32
        t = task(e, 0x40, 0x53)
        set_handler(e, 0x31, 0x53, STOP if has else 0)
        set_handler(e, 0x32, 0x53, STOP if has else 0)
        return (t,)
    run(0x4E6C00, "Sound_Stop", setup)


def test_setters():
    def prep(e, var, ev, size):
        e.write(var, bytes([ev]))
        t = task(e, 0x40, 0x54)
        set_handler(e, ev, 0x54, SETTERS[size])
        return t
    run(0x4E6C30, "Sound_SetPos", lambda e: (prep(e, 0x548816, 0x33, 24), cstr(e, bytes(range(1, 25)))))
    run(0x4E6C60, "Sound_SetVolume", lambda e: (prep(e, 0x548817, 0x34, 8), 0x3F000000, 0xBF800000))
    run(0x4E6CA0, "Sound_SetPitch", lambda e: (prep(e, 0x548818, 0x35, 4), 0x3FC00000))
    run(0x4E6CE0, "Sound_SetGlobal", lambda e: (prep(e, 0x54881B, 0x36, 1), 0x1201))


@pytest.mark.parametrize("n", [0, 1, 3])
def test_def_find(n):
    def setup(e):
        u32(e, 0x548814, 0x30)
        d = task(e, 0xA0, 0x50)
        for i in range(n):
            add_child(e, d, task(e, 0x40, 0x51 + (i & 1)))
        set_handler(e, 0x30, 0x51, FIND_NONE)
        set_handler(e, 0x30, 0x52, 0)
        arg = e.alloc(8)
        u32(e, arg, cstr(e, b"name"), 0)
        return d, arg
    run(0x4E5F70, "SoundSysDef_Find", setup)
