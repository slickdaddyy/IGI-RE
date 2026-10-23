import struct

import pytest

from emu import Emu
from test_animtrig_util import CWS, add_stubs, check_cw, logfn
from test_menu_util import LOG, RET, add_child, check as _check, cstr, f32, handler, logfake, set_handler, task, trap, u16, u32

INFO_EV, LIST_EV, FIND_EV = 0x40, 0x41, 0x42
EVENTS = {0x548812: LIST_EV, 0x548813: INFO_EV, 0x548814: FIND_EV, 0x548815: 0x43, 0x548816: 0x44,
          0x548817: 0x45, 0x548818: 0x46, 0x548819: 0x47, 0x54881A: 0x48, 0x54881B: 0x49}
OWNER_T, CHILD_T, CHILD2_T = 0x20, 0x21, 0x22
GROUP_T = 0x30


def check(em, orig, ours, setup=None, **kw):
    kw.setdefault("check_eax", False)
    return _check(em, orig, ours, setup, **kw)


def set_events(e):
    for a, v in EVENTS.items():
        e.write(a, bytes([v]))
    u32(e, 0x54881C, 0x4A)


e = Emu()
add_stubs(e)
INFO = {}


def info_handler(em):
    out = em.arg(1)
    words = [INFO.get(k, 0) for k in ("pos", "matrix", "params")]
    em.write(out, struct.pack("<3I", *words))
    em.ret(0)


INFO_FN = trap(e, "info", info_handler)


def d64(em, *v):
    p = em.alloc(8 * len(v))
    em.write(p, struct.pack("<%dd" % len(v), *v))
    return p


def sound_obj(em, vol=0.5, vol2=0.75, idx=(-1, -1, -1, -1), name=b"base"):
    s = task(em, 0x188, 0x50, task(em, 0x20, OWNER_T))
    em.write(s + 0xA0, struct.pack("<3d", 100.5, -20.25, 3.125))
    f32(em, s + 0xB8, vol, vol2, 1.5)
    f32(em, s + 0xC8, 64.0, 4096.0)
    u32(em, s + 0xD8, *idx)
    f32(em, s + 0xE8, *[0.1 * i + 0.05 for i in range(11)])
    f32(em, s + 0x114, *[0.5 + 0.13 * i for i in range(11)])
    em.write(s + 0x160, name + b"\0")
    u32(em, s + 0x28, 7)
    return s


CASES = {
    "none": (False, {}),
    "empty": (True, {}),
    "matrix": (True, {"matrix": [0.8, 0.6, 0.0, -0.6, 0.8, 0.0, 0.0, 0.0, 1.0]}),
    "pos": (True, {"pos": [1000.5, 2000.25, -30.0]}),
    "both": (True, {"pos": [1.0, 2.0, 3.0], "matrix": [0.36, 0.48, -0.8, -0.8, 0.6, 0.0, 0.48, 0.64, 0.6]}),
}


@pytest.mark.parametrize("case", sorted(CASES))
@pytest.mark.parametrize("cw", CWS[:2])
def test_update_params(case, cw):
    has, data = CASES[case]

    def setup(em):
        set_events(em)
        INFO.clear()
        if has:
            set_handler(em, INFO_EV, OWNER_T, INFO_FN)
        if "pos" in data:
            INFO["pos"] = d64(em, *data["pos"])
        if "matrix" in data:
            m = em.alloc(40)
            f32(em, m, *data["matrix"])
            INFO["matrix"] = m
        return (sound_obj(em),)
    check_cw(e, 0x4EAEB0, "SoundDefGraph_Update", cw, setup, check_eax=False)


PARAMS = [
    ((0, -1, -1, -1), [0.37]),
    ((0, -1, -1, -1), [-0.5]),
    ((0, 1, 2, -1), [1.5, 0.42, 0.25]),
    ((-1, 0, 1, -1), [0.0, 0.9]),
    ((-1, -1, -1, 0), ["base"]),
    ((-1, -1, -1, 1), [0.5, "other_name"]),
    ((-1, -1, -1, 0), [None]),
]


@pytest.mark.parametrize("n", range(len(PARAMS)))
@pytest.mark.parametrize("cw", CWS[:2])
def test_update_params_values(n, cw):
    idx, vals = PARAMS[n]

    def setup(em):
        set_events(em)
        INFO.clear()
        set_handler(em, INFO_EV, OWNER_T, INFO_FN)
        p = em.alloc(4 + 4 * len(vals))
        u32(em, p, len(vals))
        for i, v in enumerate(vals):
            if isinstance(v, float):
                f32(em, p + 4 + 4 * i, v)
            else:
                u32(em, p + 4 + 4 * i, cstr(em, v.encode()) if v else 0)
        INFO["params"] = p
        return (sound_obj(em, idx=idx),)
    check_cw(e, 0x4EAEB0, "SoundDefGraph_Update", cw, setup, check_eax=False)


@pytest.mark.parametrize("vols", [(-0.5, 2.0), (1.0, 0.0), (1.25, -0.0), (0.3, 1.0)])
def test_update_params_clamp(vols):
    def setup(em):
        set_events(em)
        return (sound_obj(em, *vols),)
    check(e, 0x4EAEB0, "SoundDefGraph_Update", setup, check_eax=False)


# --- SoundDefGroup handlers on hand made tasks ---

def group(em, name=b"grp", keep=1, parent=None, children=0, types=(CHILD_T,)):
    g = task(em, 0x60, GROUP_T)
    em.write(g + 0x20, bytes([keep, 0]) + name + b"\0")
    if parent:
        add_child(em, parent, g)
    kids = []
    for i in range(children):
        c = task(em, 0x60, types[i % len(types)])
        add_child(em, g, c)
        kids.append(c)
    return g, kids


@pytest.mark.parametrize("arg", [None, (b"engine", 0), (b"x" * 31, 1)])
def test_create(arg):
    def setup(em):
        g = task(em, 0x60, GROUP_T)
        em.write(g + 0x20, b"\xAA" * 0x40)
        if arg is None:
            return g, 0
        d = em.alloc(0x60)
        em.write(d + 0x22, arg[0] + b"\0")
        a = em.alloc(8)
        u32(em, a, d, arg[1] | 0x5500)
        return g, a
    check(e, 0x4EB360, "SoundDefGroup_Create", setup)


e2 = Emu()
add_stubs(e2)
RUN_FN = handler(e2, "run", 1)
logfake(e2, 0x4015F0, 1)


@pytest.mark.parametrize("children,keep", [(0, 0), (0, 1), (2, 0), (3, 1)])
def test_run(children, keep):
    def setup(em):
        g, kids = group(em, keep=keep, children=children, types=(CHILD_T, CHILD2_T))
        set_handler(em, 1, CHILD_T, RUN_FN)
        return (g,)
    check(e2, 0x4EB3D0, "SoundDefGroup_Run", setup)


def test_stop():
    check(e, 0x4EB460, "SoundDefGroup_Stop", lambda em: (group(em)[0],))


LIST_FN = handler(e, "list", 2)


@pytest.mark.parametrize("depth,children,pos", [(0, 0, 0), (2, 0, 0), (1, 2, 0), (3, 1, 1), (2, 0, 2)])
def test_list(depth, children, pos):
    def setup(em):
        set_events(em)
        parent = task(em, 0x20, OWNER_T)
        sibs = [group(em, b"s%d" % i, parent=parent)[0] for i in range(3)]
        g, kids = group(em, b"middle", children=children)
        # pos: 0 last child, 1 first child, 2 only child of an empty-tailed parent
        if pos == 0:
            add_child(em, parent, g)
        elif pos == 1:
            u32(em, g, sibs[0], parent + 8)
            u32(em, parent + 8, g)
            u32(em, sibs[0] + 4, g)
            u32(em, g + 0x14, parent)
        else:
            p2 = task(em, 0x20, OWNER_T)
            add_child(em, p2, g)
        set_handler(em, LIST_EV, CHILD_T, LIST_FN)
        buf = em.alloc(0x400)
        em.write(buf, b"start\r\n\0")
        a = em.alloc(8)
        u32(em, a, buf, depth)
        return g, a
    check(e, 0x4EB470, "SoundDefGroup_Write", setup)


@pytest.mark.parametrize("has,pre", [(False, 0), (True, 0), (True, 1), (False, 1)])
def test_info(has, pre):
    def setup(em):
        set_events(em)
        INFO.clear()
        INFO["pos"] = 0
        INFO["matrix"] = 0x1234
        parent = task(em, 0x20, OWNER_T)
        g, _ = group(em, parent=parent)
        if has:
            set_handler(em, INFO_EV, OWNER_T, INFO_FN)
        out = em.alloc(12)
        u32(em, out, 0x5555 if pre else 0, 0, 0)
        return g, out
    check(e, 0x4EB620, "SoundDefGroup_Info", setup)


FIND_FN = handler(e, "find", 2, derefs={1: 8})


@pytest.mark.parametrize("name,found,children", [(b"grp", 0, 0), (b"zzz", 0, 2), (b"grp", 1, 2), (b"gr", 0, 0)])
def test_find(name, found, children):
    def setup(em):
        set_events(em)
        g, _ = group(em, children=children)
        set_handler(em, FIND_EV, CHILD_T, FIND_FN)
        a = em.alloc(8)
        u32(em, a, cstr(em, name), 0x777 if found else 0)
        return g, a
    check(e, 0x4EB670, "SoundDefGroup_Find", setup)


@pytest.mark.parametrize("name", [b"", b"abc"])
def test_get_name(name):
    def setup(em):
        g, _ = group(em, name)
        out = em.alloc(4)
        u32(em, out, 0x99)
        return g, out
    check(e, 0x4EB740, "SoundDefGroup_GetName", setup)


def test_set_pos():
    def setup(em):
        g, _ = group(em)
        return g, d64(em, 1.5, 2.5, -3.5)
    check(e, 0x4EB760, "SoundDefGroup_SetPos", setup)


FWD = [(0x4EB780, "SoundDefGroup_SetVolume", 0x45), (0x4EB810, "SoundDefGroup_SetPitch", 0x46),
       (0x4EB8A0, "SoundDefGroup_SetGlobal", 0x49), (0x4EB930, "SoundDefGroup_GetRange", 0x47),
       (0x4EB9C0, "SoundDefGroup_GetDuration", 0x48)]
FWD_FN = handler(e, "fwd", 2)


@pytest.mark.parametrize("n", range(len(FWD)))
@pytest.mark.parametrize("children", [0, 3])
def test_forward(n, children):
    orig, ours, ev = FWD[n]

    def setup(em):
        set_events(em)
        g, _ = group(em, children=children, types=(CHILD_T, CHILD2_T))
        set_handler(em, ev, CHILD_T, FWD_FN)
        return g, 0x4321
    check(e, orig, ours, setup)


def test_get_type():
    check(e, 0x4EBA80, "SoundDefGroup_GetType", lambda em: u16(em, 0x54DB4A, 0x77), eax_mask=0xFFFF, check_eax=True)


# --- init / shutdown / DefineGroup with logging fakes ---

e3 = Emu()
add_stubs(e3)
logfake(e3, 0x4018F0, 0, ret=0x11)
logfake(e3, 0x401900, 4, strings=(0,), masks={2: 0xFFFF}, ret=0x31)
logfake(e3, 0x401400, 3, masks={0: 0xFFFF, 1: 0xFF})
logfake(e3, 0x401530, 3, masks={0: 0xFFFF})
logfake(e3, 0x401A20, 1, masks={0: 0xFFFF})
logfake(e3, 0x4017C0, 1, masks={0: 0xFFFF}, ret=0x3C)
logfake(e3, 0x4018C0, 1, masks={0: 0xFF})
logfake(e3, 0x4B8930, 2, strings=(0,))
logfake(e3, 0x496E00, 3, derefs={0: 40, 1: 24, 2: 24})


def test_init():
    check(e3, 0x4EB1D0, "SoundDefGroup_Init", set_events, normalized=True, check_eax=False)


def test_shutdown():
    check(e3, 0x4EBA50, "SoundDefGroup_Shutdown", lambda em: u16(em, 0x54DB48, 0x31, 0x31), check_eax=False)


ARGS = {}
OUT = 0x23F00000
logfn(e3, 0x4B8A80, lambda em, a: [a[1], a[3]], 4,
      ret=lambda em, a: em.write(a[2], ARGS["name"] + b"\0"))
logfn(e3, 0x4B8B00, lambda em, a: [a[1]], 2, ret=lambda em, a: int(a[1] < ARGS["n"]))
logfn(e3, 0x4B8A20, lambda em, a: [a[1]], 2, ret=5)
logfn(e3, 0x4012A0, lambda em, a: [em.normalize(a[0]), a[1] & 0xFFFF,
                                   em.read_cstr(em.u32(a[2]) + 0x22), em.read(a[2] + 4, 1)], 3, ret=0x2000)


@pytest.mark.parametrize("n", [1, 2, 4])
def test_define_group(n):
    def setup(em):
        ARGS["name"] = b"group_%d" % n
        ARGS["n"] = n
        u16(em, 0x54DB4A, 0x31)
        return OUT, 0x1000, 0x3000
    check(e3, 0x4EBA90, "DefineGroup", setup, ignore=[(OUT + 4, OUT + 8), (OUT + 0x14, OUT + 0x18)], check_eax=True)


# --- microphone ---

def test_mic_init():
    check(e3, 0x4EBC60, "SoundSysMicrophone_Init", normalized=True, check_eax=False)


def test_mic_shutdown():
    def setup(em):
        u16(em, 0x54DB68, 0x44)
        em.write(0x54DB6A, b"\x29")
    check(e3, 0x4EBDB0, "SoundSysMicrophone_Shutdown", setup)


def test_mic_set():
    def setup(em):
        m = task(em, 0x68)
        mat = em.alloc(40)
        f32(em, mat, *[0.1 * i for i in range(10)])
        return m, mat, d64(em, 5.0, 6.0, 7.0)
    check(e3, 0x4EBBF0, "SoundSysMicrophone_Set", setup)


e4 = Emu()
add_stubs(e4)
MIC_FN = handler(e4, "mic", 2, derefs={1: 5})
RUN4_FN = handler(e4, "run", 1)
logfake(e4, 0x4012A0, 3, masks={1: 0xFFFF})


@pytest.mark.parametrize("ok", [0, 1])
def test_mic_new(ok):
    def setup(em):
        u16(em, 0x54DB68, 0x44)
        RET[0x4012A0] = em.alloc(0x68) if ok else 0
        return 0x5000, 0x77
    check(e4, 0x4EBBA0, "SoundSysMicrophone_New", setup, check_eax=True)


@pytest.mark.parametrize("sounds,children", [(0, 0), (3, 0), (2, 2)])
def test_mic_run(sounds, children):
    def setup(em):
        em.write(0x54DB6A, b"\x29")
        lst = em.alloc(8 + 4 * max(sounds, 1))
        u32(em, lst, sounds, 8)
        for i in range(sounds):
            t = task(em, 0x20, CHILD_T + i % 2)
            u32(em, lst + 8 + 4 * i, t)
        set_handler(em, 0x29, CHILD_T, MIC_FN)
        set_handler(em, 0x29, CHILD2_T, MIC_FN)
        u32(em, 0xA5E60C, lst)
        m = task(em, 0x68, 0x44)
        for i in range(children):
            add_child(em, m, task(em, 0x20, CHILD_T))
        set_handler(em, 1, CHILD_T, RUN4_FN)
        return (m,)
    check(e4, 0x4EBCD0, "SoundSysMicrophone_Run", setup, check_eax=False)


def test_mic_getters():
    check(e4, 0x4EBDD0, "SoundSysMicrophone_GetPlayEvent", lambda em: em.write(0x54DB6A, b"\x29"), eax_mask=0xFF, check_eax=True)
    check(e4, 0x4EBDE0, "SoundSysMicrophone_GetPos", args=(0x1000,), check_eax=True)
