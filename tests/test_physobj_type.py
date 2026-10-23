import struct

import pytest

from emu import Emu
from test_physobj_util import CWS, Fakes, check, check_error, cstr, f32, set_handler, u32

COUNT, TYPES, EVENTS, LOD = 0xA5EFD8, 0xA5EFE0, 0x54DC30, 0xA5EFD4
SIZE = 0x2D8

e = Emu()
fk = Fakes(e)
ARGS = {}       # script argument index -> value
POINTS = {}     # model point -> xyz
EVENT_IDS = []


def arg_string(em, a):
    s = ARGS.get(a[1], b"")
    em.write(a[2], s[:a[3] - 1] + b"\0")


def next_event(em, a):
    EVENT_IDS.append(0x40 + len(EVENT_IDS))
    return EVENT_IDS[-1]


def get_point(em, a):
    f32(em, a[0], *POINTS.get(a[2], (0.0, 0.0, 0.0)))


fk.add(0x4F1A70, 0, ret=0x33)                                   # Param_GetChangedEvent
fk.add(0x401D80, 3, masks={1: 0xFF})                            # Task_Broadcast
fk.add(0x4017C0, 1, ret=next_event, masks={0: 0xFFFF})          # TaskEvent_Alloc
fk.add(0x4018C0, 1, masks={0: 0xFF})                            # TaskEvent_Free
fk.add(0x4B8890, 4)                                             # Script_RegisterFunction
fk.add(0x4B8A20, 2, ret=lambda em, a: ARGS.get(a[1], 0))        # Script_GetArgInt
fk.add(0x4B8A50, 2, st0="d", ret=lambda em, a: ARGS.get(a[1], 0.0))  # Script_GetArgReal
fk.add(0x4B8A80, 4, effect=arg_string)                          # Script_GetArgString
fk.add(0x4B0C60, 2, ret=0x2000F000)                             # Mem_Alloc
fk.add(0x4B80B0, 1, ret=0x2000E000, fmt=lambda em, a: (em.read_cstr(a[0]),))  # Script_Load
fk.add(0x4B85B0, 1)                                             # Script_Run
fk.add(0x4B83D0, 1)                                             # Script_Free
fk.add(0x4B1420, 3, fmt=lambda em, a: (em.read_cstr(a[0]), em.normalize(a[1]), a[2]))  # File_ScanDir
fk.add(0x48F340, 0, ret=0)                                      # Config_GetQvmOnly
fk.add(0x4CEC10, 1, ret=0x7777, fmt=lambda em, a: (em.read_cstr(a[0]),))  # VirModel_Find
fk.add(0x4D0970, 3, effect=get_point, fmt=lambda em, a: (em.u32(a[1] + 0x6C), a[2]))  # VirModel_GetPoint
SIZE_H = fk.handler(2, fmt=lambda m, a: (a[0], m.u32(a[1])), effect=lambda m, a: u32(m, a[1], 0x80))
PARSE_H = fk.handler(2, deref={1: 12})
FREE_H = fk.handler(2, fmt=lambda m, a: (a[0], m.u32(a[1])))
e.baseline = e.snapshot()


def reset_state():
    ARGS.clear()
    POINTS.clear()
    EVENT_IDS.clear()


def add_type(em, i, model, type_=0x55, size=(1.0, 2.0, 3.0), joints=()):
    t = TYPES + SIZE * i
    em.write(t, struct.pack("<H", type_))
    f32(em, t + 0xC, *size)
    em.write(t + 0x138, model + b"\0")
    u32(em, t + 0x148, len(joints))
    for k, p in enumerate(joints):
        u32(em, t + 0x14C + 0x18 * k, p)
    return t


@pytest.mark.parametrize("root", [0, 0x2000A000])
def test_lod_send_changed(root):
    def setup(em):
        u32(em, LOD, root)
    check(e, fk, 0x4EDCC0, "Lod_SendChanged", setup, check_eax=False)


def test_open():
    def setup(em):
        EVENT_IDS.clear()
    check(e, fk, 0x4EDCE0, "PhysicsObjType_Open", setup, check_eax=False)


def test_close():
    def setup(em):
        em.write(EVENTS, bytes([0x21, 0x22, 0x23]))
    check(e, fk, 0x4EDFB0, "PhysicsObjType_Close", setup, check_eax=False)


def script_args(joints, extra=True):
    reset_state()
    ARGS.update({0: 0x45, 1: 7, 2: 12.5, 3: 1.1, 4: 2.2, 5: 0.0, 6: b"short name", 7: b"x" * 300,
                 8: b"crate_big_model_name", 9: joints})
    i = 10
    for k in range(max(joints, 0)):
        ARGS.update({i: k + 3, i + 1: -k, i + 2: 100 + k, i + 3: 0.1 * k, i + 4: 1e30, i + 5: -2.5})
        i += 6
    ARGS.update({i: 3.75, i + 1: 1e-3})


@pytest.mark.parametrize("joints", [0, 3, -2, 16])
@pytest.mark.parametrize("handlers", [0, 1])
@pytest.mark.parametrize("count", [0, 2])
def test_define(joints, handlers, count):
    def setup(em):
        script_args(joints)
        em.write(EVENTS, bytes([0x35, 0x36, 0x37]))
        u32(em, COUNT, count)
        for i in range(count):
            add_type(em, i, b"model%d" % i)
        if handlers:
            set_handler(em, 0x36, 0x45, SIZE_H)
            set_handler(em, 0x35, 0x45, PARSE_H)
        ret = em.alloc(24)
        return ret, 0x1234, 0x2000C000
    check(e, fk, 0x4EDD20, "DefinePhysicsObjType", setup,
          ignore=[(0x20000004, 0x20000008), (0x20000014, 0x20000018)])


def test_define_duplicate():
    def setup(em):
        script_args(1)
        ARGS[8] = b"model1"
        u32(em, COUNT, 3)
        for i in range(3):
            add_type(em, i, b"model%d" % i)
        return em.alloc(24), 0, 0x2000C000
    msg = check_error(e, 0x4EDD20, "DefinePhysicsObjType", setup)
    assert msg == "Physics obj model %s is defined multiple times."


@pytest.mark.parametrize("path", [b"LOCAL:phys/crate.qsc", b"LOCAL:phys/vssver.scc"])
def test_parse_config_file(path):
    check(e, fk, 0x4EDFE0, "PhysicsObjType_ParseConfigFile", lambda em: (cstr(em, path),), check_eax=False)


def test_parse_config_file_error():
    def setup(em):
        fk.rets[0x4B80B0] = 0
        return (cstr(em, b"LOCAL:phys/bad.qsc"),)
    try:
        check_error(e, 0x4EDFE0, "PhysicsObjType_ParseConfigFile", setup)
    finally:
        fk.rets[0x4B80B0] = 0x2000E000


def test_load():
    check(e, fk, 0x4EE030, "PhysicsObjLoad", lambda em: (cstr(em, b"LOCAL:physobj"),), check_eax=False)


@pytest.mark.parametrize("path,is_dir", [(b"LOCAL:phys/sub", 1), (b"LOCAL:phys/a.qsc", 0), (b"LOCAL:phys/a.QSC", 0),
                                         (b"LOCAL:phys/a.qvm", 0), (b"qsc", 0), (b"qvm", 0), (b"x.txt", 0),
                                         (b"LOCAL:phys/vssver.qsc", 0), (b"", 0)])
@pytest.mark.parametrize("qvm", [0, 1])
def test_visit(path, is_dir, qvm):
    def setup(em):
        fk.rets[0x48F340] = qvm
        ent = em.alloc(12)
        u32(em, ent, cstr(em, path), 0x1111)
        em.write(ent + 8, bytes([is_dir, 0xAA, 0xBB, 0xCC]))
        return (ent,)
    check(e, fk, 0x4EE050, "PhysicsObjLoad_Visit", setup, check_eax=False)


@pytest.mark.parametrize("addr,name", [(0x4EE0E0, "PhysicsObjType_GetParseEvent"),
                                       (0x4EE0F0, "PhysicsObjType_GetSizeEvent"),
                                       (0x4EE100, "PhysicsObjType_GetFreeEvent")])
def test_events(addr, name):
    check(e, fk, addr, name, lambda em: em.write(EVENTS, bytes([0x91, 0x92, 0x93])), eax_mask=0xFF)


def test_get_count():
    check(e, fk, 0x4EE110, "PhysicsObjType_GetCount", lambda em: u32(em, COUNT, 17))


@pytest.mark.parametrize("i", [0, 1, 37, -1])
def test_get(i):
    check(e, fk, 0x4EE120, "PhysicsObjType_Get", args=(i,))


@pytest.mark.parametrize("name", [b"crate", b"barrel", b"crat", b"crate2", b"", b"zz"])
@pytest.mark.parametrize("count", [0, 1, 3])
def test_find(name, count):
    def setup(em):
        u32(em, COUNT, count)
        for i, m in enumerate([b"crate", b"barrel", b"zz"][:count]):
            add_type(em, i, m)
        return (cstr(em, name),)
    check(e, fk, 0x4EE140, "PhysicsObjType_Find", setup)


NAN = float("nan")
PTS = [(1.0, -2.0, 3.0), (-4.5, 0.25, 7.0), (1e-30, 3e5, -1e5), (NAN, 1.0, 2.0), (2.0, NAN, -NAN)]


@pytest.mark.parametrize("cw", CWS)
@pytest.mark.parametrize("joints", [(), (0,), (1, 2), (0, 1, 2), (3, 0, 1), (1, 4, 2)])
def test_calc_size(cw, joints):
    def setup(em):
        reset_state()
        POINTS.update(enumerate(PTS))
        return (add_type(em, 0, b"crate", joints=joints),)
    check(e, fk, 0x4EE200, "PhysicsObjType_CalcSize", setup, check_eax=False, cw=cw)


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("size", [(1.0, 2.0, 3.0), (0.0, 2.0, 3.0), (1.0, -0.0, 3.0), (1.0, 2.0, 0.0),
                                  (NAN, 1.0, 1.0), (1e-40, -1.0, 5.0)])
def test_get_size(cw, size):
    def setup(em):
        reset_state()
        POINTS.update(enumerate(PTS))
        return (add_type(em, 0, b"crate", size=size, joints=(0, 1, 2)),)
    check(e, fk, 0x4EE1B0, "PhysicsObjType_GetSize", setup, cw=cw)


@pytest.mark.parametrize("count", [0, 1, 3])
def test_free_all(count):
    def setup(em):
        em.write(EVENTS, bytes([0x35, 0x36, 0x37]))
        u32(em, COUNT, count)
        for i in range(count):
            add_type(em, i, b"m%d" % i, type_=0x50 + i)
            set_handler(em, 0x37, 0x50 + i, FREE_H)
    check(e, fk, 0x4EE3B0, "PhysicsObjType_FreeAll", setup, check_eax=False)
