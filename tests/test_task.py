import struct

import pytest

from emu import Emu, compare

emu = Emu()
emu.stub("MessageBoxA", lambda e: e.ret(1, 16))
emu.stub("OutputDebugStringA", lambda e: e.ret(0, 4))

TASK_INIT = 0x4010D0
TYPE_NEW = 0x401900
SET_HANDLER = 0x401400
SET_HANDLER2 = 0x401530
TASK_NEW = 0x4012A0
TASK_DELETE = 0x4015F0
MAP_SLOT = 0x401D30
REF_ADD = 0x401AE0
NO_TYPE = 0x181


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def logger(e, log):
    """x86 handler that appends its first argument to log[]; log[-1] word is the count."""
    count = log
    code = (b"\x8b\x44\x24\x04"                         # mov eax, [esp+4]
            + b"\x8b\x0d" + struct.pack("<I", count)    # mov ecx, [count]
            + b"\x89\x04\x8d" + struct.pack("<I", count + 4)  # mov [ecx*4+log], eax
            + b"\x41"                                   # inc ecx
            + b"\x89\x0d" + struct.pack("<I", count)    # mov [count], ecx
            + b"\xc3")
    p = e.alloc(len(code))
    e.write(p, code)
    return p


def deleter(e):
    """x86 handler that deletes the task it is called with."""
    code = b"\x8b\x44\x24\x04\x50\xba" + struct.pack("<I", TASK_DELETE) + b"\xff\xd2\x83\xc4\x04\xc3"
    p = e.alloc(len(code))
    e.write(p, code)
    return p


def script_table(e):
    e.call(0x4B0BA0)
    buckets = e.alloc(12 * 97)
    for i in range(97):
        b = buckets + 12 * i
        e.write(b, struct.pack("<III", b + 4, 0, b))
    table = e.alloc(8)
    e.write(table, struct.pack("<II", buckets, 97))
    e.write(0xA43E88, struct.pack("<I", table))


class World:
    """Task system set up by the original code: three types, handlers, a small tree."""

    def __init__(self, e, pooled=4, handlers=True):
        script_table(e)
        e.call(TASK_INIT)
        self.root_type = e.call(0x4018F0)[0] & 0xFFFF
        self.a = e.call(TYPE_NEW, cstr(e, b"TypeA"), 0x40, self.root_type, 0)[0] & 0xFFFF
        self.b = e.call(TYPE_NEW, cstr(e, b"TypeB"), 0x30, self.a, 0)[0] & 0xFFFF
        self.p = e.call(TYPE_NEW, cstr(e, b"Pooled"), 0x24, self.a, pooled)[0] & 0xFFFF
        self.log = e.alloc(4 * 256)
        self.handler = logger(e, self.log)
        if handlers:
            for slot in (0, 1, 2, 3):
                e.call(SET_HANDLER, self.a, slot, self.handler)
            e.call(MAP_SLOT, 1, 0)
            e.call(MAP_SLOT, 5, 3)
        self.root = e.call(TASK_NEW, 0, self.root_type, 0)[0]
        self.t1 = e.call(TASK_NEW, self.root, self.a, 0x111)[0]
        self.t2 = e.call(TASK_NEW, self.root, self.b, 0x222)[0]
        self.t3 = e.call(TASK_NEW, self.t1, self.p, 0x333)[0]
        self.t4 = e.call(TASK_NEW, self.t1, self.b, 0x444)[0]
        self.t5 = e.call(TASK_NEW, self.t1, self.p, 0x555)[0]
        self.t6 = e.call(TASK_NEW, self.t4, self.a, 0x666)[0]


def test_init():
    # the type name points at our own copy of "QTask"
    compare(emu, TASK_INIT, "Task_InitSystem", setup=script_table, check_eax=False, ignore=[(0xAF5EE0, 0xAF5EE4)])
    assert emu.read_cstr(emu.u32(0xAF5EE0)) == b"QTask"


def test_reset_all():
    compare(emu, 0x4011B0, "TaskType_ResetAll", setup=lambda e: World(e) and None, check_eax=False)
    compare(emu, 0x4011D0, "TaskEvent_ResetAll", setup=lambda e: World(e) and None, check_eax=False)


@pytest.mark.parametrize("parent", ["root", "a", "b", "none"])
@pytest.mark.parametrize("pool", [0, 3])
def test_type_new(parent, pool):
    def setup(e):
        w = World(e)
        par = NO_TYPE if parent == "none" else getattr(w, parent if parent != "root" else "root_type")
        return cstr(e, b"NewType"), 0x50, par, pool
    compare(emu, TYPE_NEW, "TaskType_New", setup=setup, eax_mask=0xFFFF)


@pytest.mark.parametrize("orig,ours", [(SET_HANDLER, "TaskType_SetHandler"), (SET_HANDLER2, "TaskType_SetHandler2")])
@pytest.mark.parametrize("on", ["root_type", "a", "b"])
@pytest.mark.parametrize("slot", [1, 7])
def test_set_handler(orig, ours, on, slot):
    def setup(e):
        w = World(e)
        e.call(SET_HANDLER, w.b, slot, 0x401F30)
        return getattr(w, on), slot, 0x401F40
    compare(emu, orig, ours, setup=setup, check_eax=False)


def test_masks():
    def setup(e):
        w = World(e)
        return (w.a,)
    compare(emu, 0x4014E0, "TaskType_GetMask", setup=setup, eax_mask=0xFFFF)
    compare(emu, 0x4014B0, "TaskType_UpdateMasks", setup=lambda e: World(e) and None, check_eax=False)
    compare(emu, MAP_SLOT, "TaskEvent_MapSlot", setup=lambda e: (World(e), (2, 7))[1], check_eax=False)
    compare(emu, 0x401D50, "TaskEvent_UnmapSlot", setup=lambda e: (World(e), (5,))[1], check_eax=False)
    compare(emu, 0x401D50, "TaskEvent_UnmapSlot", setup=lambda e: (World(e), (9,))[1], check_eax=False)


@pytest.mark.parametrize("orig,ours", [(TASK_NEW, "Task_New"), (0x4013B0, "Task_NewHead")])
@pytest.mark.parametrize("kind", ["a", "b", "p", "root_type"])
@pytest.mark.parametrize("has_parent", [True, False])
def test_task_new(orig, ours, kind, has_parent):
    def setup(e):
        w = World(e, pooled=3)
        return (w.t1 if has_parent else 0), getattr(w, kind), 0x999
    compare(emu, orig, ours, setup=setup)


def test_task_alloc_pool_full():
    def setup(e):
        w = World(e, pooled=2)
        return w.t2, w.p
    compare(emu, 0x4012F0, "Task_Alloc", setup=setup)


@pytest.mark.parametrize("which", ["t1", "t2", "t3", "t4", "t5", "t6", "root"])
@pytest.mark.parametrize("refs", [False, True])
def test_task_delete(which, refs):
    def setup(e):
        w = World(e)
        if refs:
            e.call(REF_ADD, w.t3)
            e.call(REF_ADD, getattr(w, which))
            e.call(REF_ADD, w.t2)
        return (getattr(w, which),)
    compare(emu, TASK_DELETE, "Task_Delete", setup=setup, check_eax=False)


@pytest.mark.parametrize("which", ["root", "t1", "t2"])
def test_delete_children(which):
    compare(emu, 0x401780, "Task_DeleteChildren", setup=lambda e: (getattr(World(e), which),), check_eax=False)


def test_unlink_and_move():
    compare(emu, 0x4015D0, "Task_Unlink", setup=lambda e: (World(e).t4,), check_eax=False)

    def setup(e):
        w = World(e)
        return w.t6, w.t2
    compare(emu, 0x401BB0, "Task_MoveAfter", setup=setup, check_eax=False)


def test_events():
    compare(emu, 0x4017C0, "TaskEvent_Alloc", setup=lambda e: (World(e).a,))
    compare(emu, 0x401810, "TaskEvent_Alloc2", setup=lambda e: (World(e).b,))
    compare(emu, 0x401860, "TaskEvent_Set", setup=lambda e: (World(e).a, 9), check_eax=False)
    compare(emu, 0x401890, "TaskEvent_Set2", setup=lambda e: (World(e).a, 12), check_eax=False)
    compare(emu, 0x4018C0, "TaskEvent_Free", setup=lambda e: (World(e) and 2,), check_eax=False)
    compare(emu, 0x4018E0, "TaskEvent_Free2", setup=lambda e: (World(e) and 3,), check_eax=False)
    compare(emu, 0x4018F0, "Task_GetRootType", setup=lambda e: World(e) and None, eax_mask=0xFFFF)
    compare(emu, 0x401A60, "Task_GetCount", setup=lambda e: World(e) and None)


def test_type_free_find_name():
    compare(emu, 0x401A20, "TaskType_Free", setup=lambda e: (World(e).p,), check_eax=False)
    compare(emu, 0x401A20, "TaskType_Free", setup=lambda e: (World(e).b,), check_eax=False)
    compare(emu, 0x401A70, "TaskType_Find", setup=lambda e: (World(e) and cstr(e, b"TypeB"),), eax_mask=0xFFFF)
    compare(emu, 0x401A70, "TaskType_Find", setup=lambda e: (World(e) and cstr(e, b"QTask"),), eax_mask=0xFFFF)
    compare(emu, 0x401B90, "TaskType_GetName", setup=lambda e: (World(e).b,))


@pytest.mark.parametrize("orig,ours", [(0x401BE0, "Task_FindChild"), (0x401C10, "Task_FindLastChild"),
                                       (0x401C40, "Task_FindNext"), (0x401C80, "Task_FindPrev"),
                                       (0x401CC0, "Task_FindAncestor")])
@pytest.mark.parametrize("task", ["root", "t1", "t3", "t4", "t5", "t6"])
@pytest.mark.parametrize("kind", ["a", "b", "p", "root_type"])
def test_find(orig, ours, task, kind):
    def setup(e):
        w = World(e)
        return getattr(w, task), getattr(w, kind)
    compare(emu, orig, ours, setup=setup)


@pytest.mark.parametrize("t,base", [("b", "a"), ("b", "root_type"), ("a", "b"), ("p", "b"), ("a", "a")])
def test_is_a(t, base):
    def setup(e):
        w = World(e)
        return getattr(w, t), getattr(w, base)
    compare(emu, 0x401CF0, "TaskType_IsA", setup=setup, eax_mask=0xFF)


@pytest.mark.parametrize("orig,ours,extra", [(0x401D80, "Task_Broadcast", ()), (0x401E30, "Task_BroadcastChildren", ()),
                                             (0x401E70, "Task_BroadcastType", ("b",))])
@pytest.mark.parametrize("slot", [1, 9])
@pytest.mark.parametrize("delete_on", [None, "b", "p"])
def test_broadcast(orig, ours, extra, slot, delete_on):
    def setup(e):
        w = World(e)
        e.call(SET_HANDLER, w.root_type, slot, w.handler)
        if delete_on:
            e.call(SET_HANDLER, getattr(w, delete_on), slot, deleter(e))
        args = (w.root, slot) + tuple(getattr(w, x) for x in extra) + (0x77,)
        return args
    compare(emu, orig, ours, setup=setup, check_eax=False)


def test_refs():
    def setup(e):
        w = World(e)
        return (w.t2,)
    compare(emu, REF_ADD, "TaskRef_Add", setup=setup)

    def setup_remove(which):
        def setup(e):
            w = World(e)
            refs = [e.call(REF_ADD, t)[0] for t in (w.t1, w.t2, w.t3)]
            e.write(refs[2], b"\0\0\0\0") if which == 3 else None
            return (refs[min(which, 2)],)
        return setup
    for which in (0, 1, 2, 3):
        compare(emu, 0x401B20, "TaskRef_Remove", setup=setup_remove(which), check_eax=False)


def test_shutdown():
    def setup(e):
        w = World(e, handlers=False)
        for t in (w.t1, w.t2, w.root):
            e.call(TASK_DELETE, t)
        for t in (w.a, w.b, w.p):
            e.call(0x401A20, t)
    compare(emu, 0x4011F0, "Task_ShutdownSystem", setup=setup, check_eax=False)
    compare(emu, 0x401230, "TaskType_WarnLeaks", setup=lambda e: World(e) and None, check_eax=False)
    compare(emu, 0x401270, "TaskEvent_WarnLeaks", setup=lambda e: World(e) and None, check_eax=False)


def test_small_setters():
    compare(emu, 0x401F30, "Task_SetUnk567C60", args=(0x1234,), check_eax=False)
    compare(emu, 0x401F40, "Task_ClearPair", setup=lambda e: (e.alloc(4), e.alloc(4)), check_eax=False)
    compare(emu, 0x401F60, "Task_SetGlobalFlags", args=(0x30,), check_eax=False)
    compare(emu, 0x401F80, "Task_ClearGlobalFlags", setup=lambda e: (e.call(0x401F60, 0xF0), (0x30,))[1], check_eax=False)
    compare(emu, 0x401FA0, "TaskType_SetFlags", setup=lambda e: (World(e).b, 0x44), check_eax=False)


def scenario(e):
    w = World(e)
    e.call(SET_HANDLER, w.b, 9, deleter(e))
    e.call(0x401D80, w.root, 1, 0x77)
    e.call(0x401D80, w.root, 9, 0x77)
    e.call(REF_ADD, w.t5)
    e.call(TASK_DELETE, w.root)
    e.call(0x4011F0)


def test_hooked_scenario():
    hooked = Emu()
    hooked.stubs = emu.stubs
    hooked.install_hooks()
    results = []
    for e in (emu, hooked):
        e.reset()
        before = e.snapshot()
        scenario(e)
        results.append(e.changes(before, ignore=[(0xAF5EE0, 0xAF5EE4)]))
    assert results[0] == results[1]
    assert len(results[0]) > 1000
