import random
import struct

import pytest

from emu import Emu, ErrorShown, compare

emu = Emu()
emu.stub("MessageBoxA", lambda e: e.ret(1, 16))
emu.stub("OutputDebugStringA", lambda e: e.ret(0, 4))

# callees replaced by logging stubs
fk = Emu()
FAKES = {0x4C7140: 5, 0x4C4950: 3, 0x4C7760: 1, 0x4CEC10: 1, 0x4C6940: 4, 0x4E5610: 7, 0x4B80B0: 1,
         0x4B85B0: 1, 0x4B83D0: 1, 0x4B1420: 3, 0x4B8A80: 4, 0x4B8A20: 2}
for addr, n in FAKES.items():
    fk.fake(addr, nargs=n)

HANDLERS = 0xA96AE0
EVENTS = 0x547F3C
DEFS, DEF_COUNT = 0xB97E00, 0xA4433C


def rbytes(n, seed):
    r = random.Random(seed)
    return bytes(r.randrange(256) for _ in range(n))


def f32(v):
    return struct.pack("<f", v)


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def code(e, c):
    p = e.alloc(len(c))
    e.write(p, c)
    return p


def set_handler(e, slot, type_, fn):
    e.write(HANDLERS + 4 * (slot * 0x180 + type_), struct.pack("<I", fn))


def clear_flag_handler(e):
    """x86 handler that writes 0 to the int its second argument points at."""
    return code(e, b"\x8b\x44\x24\x08\xc7\x00\x00\x00\x00\x00\xc3")


def logger(e, deref=False):
    """x86 handler that appends (task, arg) to a log, or (task, *arg) for stack args."""
    log = e.alloc(4 + 8 * 32)
    c = (b"\x8b\x0d" + struct.pack("<I", log)           # mov ecx, [log]
         + b"\x8b\x44\x24\x04"                          # mov eax, [esp+4]
         + b"\x89\x04\xcd" + struct.pack("<I", log + 4)  # mov [ecx*8+log+4], eax
         + b"\x8b\x44\x24\x08"                          # mov eax, [esp+8]
         + (b"\x8b\x00" if deref else b"")              # mov eax, [eax]
         + b"\x89\x04\xcd" + struct.pack("<I", log + 8)  # mov [ecx*8+log+8], eax
         + b"\x41"                                      # inc ecx
         + b"\x89\x0d" + struct.pack("<I", log)          # mov [log], ecx
         + b"\xc3")
    return code(e, c)


def make_model(e, radius=2.5, parts=None):
    """VirModel: model[0] -> +0xC -> +0xC -> radius at +0x78, mesh parts at +0x2C."""
    c = e.alloc(0x80)
    e.write(c + 0x78, f32(radius))
    b = e.alloc(0x10)
    e.write(b + 0xC, struct.pack("<I", c))
    a = e.alloc(0x10)
    e.write(a + 0xC, struct.pack("<I", b))
    model = e.alloc(8)
    e.write(model, struct.pack("<I", a))
    if parts is not None:
        d = e.alloc(0x40)
        for i, (verts, tris) in enumerate(parts):
            vp = e.alloc(16 * max(len(verts), 1))
            for j, v in enumerate(verts):
                e.write(vp + 16 * j, struct.pack("<4f", *v, 0.0))
            tp = e.alloc(8 * max(len(tris), 1))
            for j, t in enumerate(tris):
                e.write(tp + 8 * j, struct.pack("<4H", *t, 0))
            e.write(d + 0x20 * i, struct.pack("<II8xII", len(tris), len(verts), tp, vp))
        e.write(c + 0x2C, struct.pack("<I", d))
    return model


def make_obj(e, scale=(1.0, 1.0, 1.0), model=0, type_=0x40):
    o = e.alloc(0xF0)
    e.write(o, rbytes(0xF0, o))
    e.write(o + 0x8, struct.pack("<III", o + 0xC, 0, o + 0x8))
    e.write(o + 0x1C, struct.pack("<H", type_))
    e.write(o + 0x20, struct.pack("<3d", 10.5, -3.25, 7.0))
    e.write(o + 0x6C, struct.pack("<I", model))
    r = random.Random(o)
    m = [r.uniform(-1, 1) for _ in range(9)]
    e.write(o + 0x70, struct.pack("<9fI", *m, 0))
    e.write(o + 0x98, struct.pack("<3f", *scale))
    e.write(o + 0xA4, struct.pack("<I", 0))
    return o


def add_children(e, parent, types):
    lst = parent + 8
    kids = []
    for t in types:
        k = e.alloc(0x20)
        e.write(k + 0x1C, struct.pack("<H", t))
        kids.append(k)
    nodes = kids + [lst + 4]
    prev = lst
    e.write(lst, struct.pack("<I", nodes[0]))
    for k, nxt in zip(kids, nodes[1:]):
        e.write(k, struct.pack("<II", nxt, prev))
        prev = k
    e.write(lst + 8, struct.pack("<I", prev))
    return kids


# --- trivial accessors -----------------------------------------------------

@pytest.mark.parametrize("orig,ours,mask", [
    (0x4C48C0, "RigidDynCubeObj_GetType", 0xFFFF),
    (0x4C56D0, "RigidDynCubeObj_GetEvent0", 0xFF),
    (0x4C56E0, "RigidDynCubeObj_GetEvent1", 0xFF),
    (0x4C56F0, "RigidDynCubeObj_GetEvent2", 0xFF),
    (0x4C5700, "RigidDynCubeObj_GetEvent3", 0xFF),
    (0x4C5800, "RigidDynCubeObj_GetEvent4", 0xFF),
])
def test_getters(orig, ours, mask):
    def setup(e):
        e.write(EVENTS, bytes([0x21, 0x22, 0x23, 0x24, 0x25, 0, 0x34, 0x12]))
    compare(emu, orig, ours, setup=setup, eax_mask=mask)


def test_get_position():
    compare(emu, 0x4C3440, "DynCubeObj_GetPosition", args=(0x20001000,))


def test_set_model():
    compare(emu, 0x4C48D0, "RigidDynCubeObj_SetModel", setup=lambda e: (make_obj(e), 0x1234), check_eax=False)


@pytest.mark.parametrize("same", [False, True])
def test_is_other(same):
    def setup(e):
        o = make_obj(e)
        arg = e.alloc(8)
        e.write(arg, struct.pack("<II", 0xAAAAAAAA, o if same else o + 4))
        return o, arg
    compare(emu, 0x4C35D0, "RigidDynCubeObj_CollFilter", setup=setup, check_eax=False)


# --- create, info, init ------------------------------------------------------

def test_init():
    compare(fk, 0x4C5710, "RigidDynCubeObj_Init", setup=lambda e: (0x77, make_obj(e), 0x5555), check_eax=False,
            normalized=True)


@pytest.mark.parametrize("radius", [2.5, 0.1])
def test_create(radius):
    def setup(e):
        model = make_model(e, radius)
        fk.set_fake_ret(0x4CEC10, model)
        o = make_obj(e)
        arg = e.alloc(0x60)
        e.write(arg, rbytes(0x60, arg))
        e.write(arg, struct.pack("<II", cstr(e, b"box_01"), 0x33))
        return o, arg
    compare(fk, 0x4C3010, "RigidDynCubeObj_CreateHandler", setup=setup, check_eax=False, normalized=True)


def test_create_null():
    compare(fk, 0x4C3010, "RigidDynCubeObj_CreateHandler", setup=lambda e: (make_obj(e), 0), check_eax=False,
            normalized=True)


def test_create_missing_model():
    for target in (0x4C3010, "RigidDynCubeObj_CreateHandler"):
        fk.reset()
        fk.set_fake_ret(0x4CEC10, 0)
        o = make_obj(fk)
        arg = fk.alloc(0x60)
        fk.write(arg, struct.pack("<I", cstr(fk, b"nothere")))
        with pytest.raises(ErrorShown, match='VirModel "%s" not available'):
            fk.call(target, o, arg)


SCALES = [(1, 2, 3), (3, 2, 1), (2, 3, 1), (2, 2, 2), (5, 1, 5), (1, 5, 5), (0.3, 0.7, 0.5), (-1, -2, -3)]


@pytest.mark.parametrize("scale", SCALES)
@pytest.mark.parametrize("name", [True, False])
def test_get_info(scale, name):
    def setup(e):
        o = make_obj(e, scale, make_model(e, 1.75))
        info = e.alloc(0x54)
        e.write(info, b"\x55" * 0x54)
        e.write(info + 0x50, struct.pack("<I", e.alloc(16) if name else 0))
        return o, info
    compare(emu, 0x4C3110, "RigidDynCubeObj_GetInfo", setup=setup, check_eax=False)


def test_get_info_no_model():
    def setup(e):
        o = make_obj(e, (1.5, 0.5, 2.5), 0)
        info = e.alloc(0x54)
        return o, info
    compare(emu, 0x4C3110, "RigidDynCubeObj_GetInfo", setup=setup, check_eax=False)


@pytest.mark.parametrize("scale", SCALES[:4])
def test_handler_4C4780(scale):
    def setup(e):
        e.write(0xA44344, struct.pack("<I", 0xDEAD0000))
        o = make_obj(e, scale, make_model(e, 3.0))
        arg = e.alloc(0x40)
        e.write(arg, rbytes(0x40, arg))
        return o, arg
    compare(fk, 0x4C4780, "RigidDynCubeObj_MoveHandler", setup=setup, check_eax=False, normalized=True)


# --- children broadcast ------------------------------------------------------

@pytest.mark.parametrize("flag", [0, 1])
@pytest.mark.parametrize("kids", [[], [0x41], [0x41, 0x42, 0x41, 0x43]])
def test_handler_4C4660(flag, kids):
    def setup(e):
        e.write(0x548176, b"\x31")
        log = logger(e)
        set_handler(e, 0x31, 0x41, log)
        set_handler(e, 0x31, 0x43, log)
        o = make_obj(e, (2, 1, 3), make_model(e, 4.0))
        add_children(e, o, kids)
        arg = e.alloc(12)
        e.write(arg, struct.pack("<III", flag, 0x99, 1))
        return o, arg
    compare(fk, 0x4C4660, "RigidDynCubeObj_ActivateHandler", setup=setup, check_eax=False, normalized=True)


# --- 2D draw -------------------------------------------------------------------

PARTS = [
    ([(1.0, 2.0, 3.0), (-1.5, 0.25, 2.0), (4.0, -3.0, 0.5), (0.1, 0.2, 0.3)], [(0, 1, 2), (1, 2, 3), (3, 0, 2)]),
    ([(5.0, 5.0, 5.0), (-2.0, 1.0, 0.0), (0.0, 0.0, 9.0)], [(2, 1, 0)]),
]


@pytest.mark.parametrize("case", ["draw", "hidden", "shown", "nomodel", "empty"])
def test_draw(case):
    def setup(e):
        e.write(0x54875E, b"\x32")
        parts = PARTS if case != "empty" else [([], []), ([], [])]
        model = 0 if case == "nomodel" else make_model(e, 1.0, parts)
        o = make_obj(e, (1.5, 0.75, 2.0), model, type_=0x44)
        if case == "hidden":
            set_handler(e, 0x32, 0x44, clear_flag_handler(e))
        if case == "shown":
            set_handler(e, 0x32, 0x44, logger(e, deref=True))
        center = e.alloc(16)
        e.write(center, struct.pack("<2d", 3.5, -1.25))
        view = e.alloc(16)
        e.write(view, struct.pack("<iiIf", 0x700, 256, center, 40.0))
        return o, view
    compare(fk, 0x4C31F0, "RigidDynCubeObj_DrawCellMask", setup=setup, check_eax=False, normalized=True)


# --- script side ---------------------------------------------------------------

@pytest.mark.parametrize("handlers", [False, True])
def test_define_magic_obj(handlers):
    def setup(e):
        ret = e.alloc(24)
        e.call(0x4B0BA0)
        e.write(EVENTS, bytes([0x35, 0x36]))
        e.write(DEF_COUNT, struct.pack("<I", 3))
        fk.set_fake_ret(0x4B8A20, 0x45)
        if handlers:
            set_handler(e, 0x36, 0x45, code(e, b"\x8b\x44\x24\x08\xc7\x00\x40\x00\x00\x00\xc3"))
            set_handler(e, 0x35, 0x45, logger(e))
        return ret, 0, 0x1234
    compare(fk, 0x4C4520, "DefineMagicObj", setup=setup, normalized=True,
            ignore=[(0x20000004, 0x20000008), (0x20000014, 0x20000018)])


@pytest.mark.parametrize("path,ok", [(b"LOCAL:magicobj/rigid.qvm", 1), (b"LOCAL:magicobj/vssver.scc", 1),
                                     (b"LOCAL:magicobj/bad.qsc", 0)])
def test_parse_config_file(path, ok):
    def setup(e):
        fk.set_fake_ret(0x4B80B0, 0x2000F000 if ok else 0)
        return (cstr(e, path),)
    if ok:
        compare(fk, 0x4C48E0, "RigidDynCubeObj_ParseConfigFile", setup=setup, check_eax=False, normalized=True)
        return
    for target in (0x4C48E0, "RigidDynCubeObj_ParseConfigFile"):
        fk.reset()
        with pytest.raises(ErrorShown, match="Couldn't load script: %s"):
            fk.call(target, *setup(fk))


def test_magic_obj_load():
    compare(fk, 0x4C4930, "MagicObjLoad", setup=lambda e: (cstr(e, b"LOCAL:magicobj"),), check_eax=False,
            normalized=True)


# --- registration with the real task system ---------------------------------

def task_system(e):
    e.call(0x4B0BA0)
    buckets = e.alloc(12 * 97)
    for i in range(97):
        b = buckets + 12 * i
        e.write(b, struct.pack("<III", b + 4, 0, b))
    table = e.alloc(8)
    e.write(table, struct.pack("<II", buckets, 97))
    e.write(0xA43E88, struct.pack("<I", table))
    e.call(0x4010D0)
    e.call(0x4E5370)
    e.write(0xA5E408, b"\x20")
    e.write(0x548176, b"\x21")


def test_open():
    compare(emu, 0x4C3450, "RigidDynCubeObj_Open", setup=task_system, check_eax=False, normalized=True)


def test_close():
    def setup(e):
        task_system(e)
        e.call(0x4C3450)
        e.write(DEF_COUNT, struct.pack("<I", 4))
        for i, has in enumerate([1, 0, 1, 1]):
            p = e.call(0x4B0C60, 16, 4)[0] if has else 0
            e.write(DEFS + 0x28 * i + 0x24, struct.pack("<I", p))
    compare(emu, 0x4C4820, "RigidDynCubeObj_Close", setup=setup, check_eax=False, normalized=True)
