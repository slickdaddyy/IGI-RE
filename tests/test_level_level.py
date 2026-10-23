"""Level task, script Task_New and level task ids (src/level/level.c)."""
import struct

import pytest

from emu import Emu, ErrorShown
from test_comprun_util import with_cw
from test_menu_util import LOG, add_child, check, cstr, logfake, logger, pyfake, set_handler, task, trap, u16, u32

ARGS = []
STATE = {}

LEVEL = 0xA758A4
FIRST = 0xA758A8
IDS = 0xA71890
NOTES = 0xA70CA8
STACK, STACK_TOP = 0xAFA6E0, 0xAFA7E0
ENABLED, INSERT, LINE, FILE = 0x54DD16, 0xA758BC, 0xA70C74, 0xA70C98
PAUSED = 0xA758BD


def log(*v):
    LOG[-1].append(v)


def new_task(tag):
    def fn(e):
        parent, type_, arg = e.arg(0), e.arg(1) & 0xFFFF, e.arg(2)
        log(tag, e.normalize(parent), type_, arg)
        e.ret(task(e, 0x40, type_, parent))
    return fn


def ref_add(e):
    t = e.arg(0)
    log("ref_add", e.normalize(t))
    r = e.alloc(4)
    u32(e, r, t)
    e.ret(r)


def get_arg_string(e):
    args, i, buf, size = (e.arg(n) for n in range(4))
    log("str", args, i, size)
    e.write(buf, ARGS[i][:size - 1] + b"\0")
    e.ret(buf)


def get_arg(tag, fn):
    def stub(e):
        args, i = e.arg(0), e.arg(1)
        log(tag, args, i)
        e.ret(fn(i))
    return stub


def param_call(e):
    field, type_, op, cur = e.arg(0), e.arg(1), e.arg(2) & 0xFF, e.arg(3)
    log("param", e.normalize(field), type_, op, e.read(cur, 16), e.read(ENABLED, 1))
    u32(e, cur, e.u32(cur) + STATE.get("step", 1))
    e.ret(0)


def script_run(e):
    log("run", e.arg(0), e.read_cstr(e.u32(FILE)), e.read(ENABLED, 1), e.u32(LINE), e.read(INSERT, 1),
        e.u32(FIRST))
    u32(e, FIRST, STATE.get("first", 0))
    e.ret(0)


def make_emu():
    e = Emu()
    pyfake(e, 0x4012A0, new_task("new"))
    pyfake(e, 0x4013B0, new_task("new_head"))
    logfake(e, 0x401780, 1)  # Task_DeleteChildren
    logfake(e, 0x4018C0, 1, masks={0: 0xFF})  # TaskEvent_Free
    logfake(e, 0x4018E0, 1)  # TaskEvent_Free2
    logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
    logfake(e, 0x401A70, 1, strings=(0,), effect=lambda e, a: STATE.get("type", 0x60) | 0xBEEF0000)  # TaskType_Find
    pyfake(e, 0x401AE0, ref_add)
    logfake(e, 0x401B20, 1)  # TaskRef_Remove
    logfake(e, 0x401D80, 3, masks={1: 0xFF}, derefs={2: 1})  # Task_Broadcast
    logfake(e, 0x4B0D10, 1)  # Mem_Free
    logfake(e, 0x4B80B0, 1, strings=(0,), effect=lambda e, a: STATE.get("script", 0))  # Script_Load
    logfake(e, 0x4B83D0, 1)  # Script_Free
    pyfake(e, 0x4B85B0, script_run)
    logfake(e, 0x4B8930, 2, strings=(0,))  # Script_SetSymbolContext
    pyfake(e, 0x4B8A20, get_arg("int", lambda i: ARGS[i]))
    pyfake(e, 0x4B8A80, get_arg_string)
    pyfake(e, 0x4B8AD0, get_arg("line", lambda i: 100 + i))
    pyfake(e, 0x4B8B00, get_arg("has", lambda i: int(i < len(ARGS))))
    logfake(e, 0x4C1780, 2, strings=(1,))  # TaskNote_Set
    logfake(e, 0x4CF9D0, 1)  # ModelFile_Free
    logfake(e, 0x4F1210, 0, ret=0x36)
    logfake(e, 0x4F1230, 0, ret=0x38)
    logfake(e, 0x4F1A40, 1)  # ParamType_Free
    logfake(e, 0x4F1A60, 0, ret=0xABCD34)  # Param_GetTableEvent
    logfake(e, 0x4F1A70, 0, ret=0xABCD35)  # Param_GetChangedEvent
    pyfake(e, 0x4F1AF0, param_call)
    logfake(e, 0x4F1B60, 0, ret=0xABCD22)  # ParamOp_Parse
    e.baseline = e.snapshot()
    return e


emu = make_emu()
RUN = trap(emu, "run_handler", logger(emu, "run", 1))


def stop_next(e, a):
    """A run handler that deletes the next sibling."""
    u32(e, STACK + 4 * (e.u32(STACK_TOP) - 1), 0)


RUN_STOP = trap(emu, "run_stop", logger(emu, "run_stop", 1, effect=stop_next))
DA8 = trap(emu, "da8", logger(emu, "da8", 1))
DB0 = trap(emu, "db0", logger(emu, "db0", 1))


def give_table(e, a):
    log("table", e.normalize(a[0]))
    u32(e, a[1], STATE["table"])


TABLE = trap(emu, "table", lambda e: (give_table(e, [e.arg(0), e.arg(1)]), e.ret(0)))
emu.baseline = emu.snapshot()


def run(orig, ours, setup=None, args=(), state=None, **kw):
    ARGS[:] = args
    STATE.clear()
    STATE.update(state or {})
    kw.setdefault("check_eax", False)
    return check(emu, orig, ours, setup=setup, normalized=True, **kw)


def level(e, time=1000, tick=5, fps=30, ms=33):
    lv = task(e, 0x38, 0x40)
    u32(e, lv + 0x20, 0x11, time, tick, fps, ms, 0x22)
    u32(e, LEVEL, lv)
    return lv


def children(e, parent, handlers):
    out = []
    for i, h in enumerate(handlers):
        c = task(e, 0x20, 0x60 + i)
        add_child(e, parent, c)
        set_handler(e, 1, 0x60 + i, h)
        out.append(c)
    return out


KIDS = [[], [RUN], [RUN, 0, RUN], [RUN, RUN_STOP, RUN, RUN]]


@pytest.mark.parametrize("kids", KIDS)
@pytest.mark.parametrize("same", [0, 1])
def test_level_run(kids, same):
    def setup(e):
        lv = level(e)
        t = lv if same else task(e, 0x40, 0x41)
        # a walk that ran onto the list's tail sentinel would see these types
        u16(e, t + 0x28, 0x70)
        set_handler(e, 1, 0x70, RUN)
        set_handler(e, 1, 5, RUN)
        children(e, t, kids)
        u32(e, STACK_TOP, 2)
        return (t,)
    run(0x4F0580, "Level_Run", setup)


@pytest.mark.parametrize("kids", KIDS)
@pytest.mark.parametrize("paused", [0, 1])
def test_dynamic_run(kids, paused):
    def setup(e):
        t = task(e, 0x20, 0x41)
        children(e, t, kids)
        e.write(PAUSED, bytes([paused]))
        return (t,)
    run(0x4F0660, "Dynamic_Run", setup)


@pytest.mark.parametrize("addr,name,var", [
    (0x4F0600, "Level_Create", 0xA758A4), (0x4F06E0, "Dynamic_Create", 0xA758AC),
    (0x4F0700, "Idle_Create", 0xA758B4), (0x4F0720, "CameraBase_Create", 0xA758B8),
    (0x4F0610, "Level_Delete", 0xA758A4), (0x4F06F0, "Dynamic_Delete", 0xA758AC),
    (0x4F0710, "Idle_Delete", 0xA758B4), (0x4F0730, "CameraBase_Delete", 0xA758B8),
    (0x4F0620, "Static_Create", 0xA758B0), (0x4F0640, "Static_Delete", 0xA758B0),
    (0x4F0CE0, "Level_SetModels", 0xA70C94)])
@pytest.mark.parametrize("old", [0, 0x5000])
def test_set_globals(addr, name, var, old):
    def setup(e):
        u32(e, var, old)
        return (0x6000,)
    run(addr, name, setup)


@pytest.mark.parametrize("addr,name,var", [(0x4F0ED0, "Level_GetStatic", 0xA758B0),
                                           (0x4F0EE0, "Level_GetDynamic", 0xA758AC)])
def test_getters(addr, name, var):
    run(addr, name, lambda e: u32(e, var, 0x5A5A1234), check_eax=True)


def test_get_tick():
    run(0x4F1080, "Level_GetTick", lambda e: (level(e, tick=777), ())[1], check_eax=True)


def script_value(e):
    out = e.alloc(24)
    e.write(out, b"\xAA" * 24)
    return out, [(out + 4, out + 8), (out + 20, out + 24)]


def test_declare_parameters():
    ign = []

    def setup(e):
        out, ign[:] = script_value(e)
        return (out, 0x1234, 0x5678)
    run(0x4F0740, "Task_DeclareParameters", setup, ignore=ign, check_eax=True)


def table(e, flags):
    entries = e.alloc(16 * max(len(flags), 1))
    for i, f in enumerate(flags):
        u32(e, entries + 16 * i, 0x20 + 4 * i, 0x40 + i, 0x7700 + i, f | 0xF0)
    t = e.alloc(12)
    u32(e, t, len(flags), 8, entries)
    return t


NEW_TASK_CASES = {
    "plain": dict(args=[-1, b"Door", b"door_1", 5, 6]),
    "head": dict(insert=1, args=[-1, b"Door", b"door_1"]),
    "id": dict(id=7, args=[7, b"Door", b"d"], da8=True, db0=True),
    "id_taken": dict(id=7, taken=True, args=[7, b"Door", b"d"], da8=True, db0=True),
    "id_dead": dict(id=7, dead=True, args=[7, b"Door", b"d"], da8=True),
    "table": dict(id=3, insert=1, table=[1, 0, 1], args=[3, b"LongTypeName" * 10, b"n" * 80, b"a", 1, 2, 9],
                  da8=True, db0=True),
    "table_null": dict(table=None, args=[-1, b"T", b"n", 4]),
    "table_empty": dict(table=[], args=[2, b"T", b"n", 4, 5], step=2),
    "table_step": dict(table=[1, 1], step=2, args=[-1, b"T", b"n", 1, 2, 3, 4, 5]),
    "first_set": dict(first=0x7123, args=[-1, b"T", b"n"]),
}


@pytest.mark.parametrize("cw", [0x37F, 0x7F])
@pytest.mark.parametrize("case", list(NEW_TASK_CASES))
def test_new_task(case, cw):
    c = NEW_TASK_CASES[case]
    ign = []

    def setup(e):
        e.write(ENABLED, b"\x01")
        e.write(INSERT, bytes([c.get("insert", 0)]))
        u32(e, LINE, 55)
        u32(e, FILE, cstr(e, b"levels/1/objects.qsc"))
        u32(e, FIRST, c.get("first", 0))
        if c.get("taken") or c.get("dead"):
            r = e.alloc(4)
            u32(e, r, 0 if c.get("dead") else task(e, 0x20, 0x77))
            u32(e, IDS + 4 * c["id"], r)
        if "table" in c:
            STATE["table"] = table(e, c["table"]) if c["table"] is not None else 0
            set_handler(e, 0x34, 0x60, TABLE)
        if c.get("da8"):
            set_handler(e, 0x36, 0x60, DA8)
        if c.get("db0"):
            set_handler(e, 0x38, 0x60, DB0)
        out, ign[:] = script_value(e)
        args = e.alloc(12)
        u32(e, args, len(c["args"]))
        return out, task(e, 0x20, 0x41), args
    run(with_cw(emu, 0x4F0780, cw), with_cw(emu, "Level_NewTask", cw), setup, args=c["args"],
        state={"step": c.get("step", 1)}, ignore=ign, check_eax=True)


def error_args(targets, setup, match, show):
    """Run both targets into Error_Show and return what show(e) saw of its arguments for each."""
    out = []
    for target in targets:
        emu.reset()
        STATE.clear()
        LOG.append([])
        args = setup(emu)
        with pytest.raises(ErrorShown, match=match):
            emu.call(target, *args)
        out.append(show(emu))
    assert out[0] == out[1]


def test_new_task_disabled():
    def setup(e):
        e.write(ENABLED, b"\x00")
        u32(e, LINE, 55)
        u32(e, FILE, cstr(e, b"file.qsc"))
        out, _ = script_value(e)
        return out, 0, e.alloc(12)
    error_args((0x4F0780, "Level_NewTask"), setup, "Too few parameters in line %d of file",
               lambda e: (e.arg(1), e.read_cstr(e.arg(2))))


def notes(e, counts):
    """Initialise all note lists and add counts[i] notes to list i."""
    for i in range(251):
        lst = NOTES + 12 * i
        u32(e, lst, lst + 4, 0, lst)
    for i, n in enumerate(counts):
        lst = NOTES + 12 * (i * 37 % 251)
        for j in range(n):
            note = e.alloc(0x50)
            u32(e, note + 8, 0x9000 + 16 * i + j)
            last = e.u32(lst + 8)
            u32(e, note, lst + 4, last)
            u32(e, last, note)
            u32(e, lst + 8, note)


def ids(e, slots):
    for i in slots:
        u32(e, IDS + 4 * i, 0x8000 + i)


def test_shutdown():
    def setup(e):
        notes(e, [0, 1, 3, 2])
        ids(e, [0, 5, 0xFFF])
        for i, a in enumerate(range(0xA70C68, 0xA70CA8, 4)):
            u32(e, a, 0x100 + i)
        for i, a in enumerate(range(0xA7186C, 0xA71890, 4)):
            u32(e, a, 0x200 + i)
        for i, a in enumerate(range(0xA75890, 0xA758A4, 4)):
            u32(e, a, 0x300 + i)
        u16(e, 0x54DD0C, 0x41, 0x42, 0x43, 0x44, 0x45)
        e.write(0x54DDA4, b"\x21\x22")
        u32(e, 0x54DDA8, 0x23, 0x24, 0x25)
    run(0x4F0A60, "Level_Shutdown", setup)


def test_task_note_free():
    def setup(e):
        notes(e, [2])
        return (e.u32(NOTES),)
    run(0x4F0C90, "TaskNote_Free", setup)


def test_level_new():
    run(0x4F0CB0, "Level_New", lambda e: (u16(e, 0x54DD0C, 0x48), (0x6000,))[1], check_eax=True)


def test_free_models():
    run(0x4F0CD0, "Level_FreeModels", lambda e: u32(e, 0xA70C94, 0x6100))


@pytest.mark.parametrize("script", [0, 0x6200])
@pytest.mark.parametrize("insert", [0, 1, 0x101])
def test_run_script(script, insert):
    def setup(e):
        u32(e, LINE, 9)
        u32(e, FIRST, 0x6400)
        return 0x6300, cstr(e, b"menu.qsc"), insert
    run(0x4F0CF0, "Task_RunScript", setup, state={"script": script, "first": 0x6500}, check_eax=True)


@pytest.mark.parametrize("fps", [30, 7, -3])
def test_level_load(fps):
    def setup(e):
        level(e)
        ids(e, [1, 0x800])
        return cstr(e, b"missions/2/objects.qsc"), fps
    run(0x4F0D40, "Level_Load", setup, state={"script": 0x6200})


def test_level_load_missing():
    error_args((0x4F0D40, "Level_Load"), lambda e: (level(e), (cstr(e, b"nope.qsc"), 30))[1],
               "Couldn't load script: %s", lambda e: (e.read_cstr(e.arg(1)), LOG[-1]))


def test_level_load_objects():
    # the script file name is left pointing at a dead stack buffer
    run(0x4F0E10, "LevelLoad", lambda e: (level(e), (cstr(e, b"LOCAL:missions/location0/level1"), 25))[1],
        state={"script": 0x6200}, ignore=[(FILE, FILE + 4)])


@pytest.mark.parametrize("a", [(0x1234, 0x101, 77, 0x200), (0x1234, 0, -1, 1)])
def test_script_init(a):
    run(0x4F0E50, "ScriptInit", lambda e: (u32(e, FIRST, 0x99), a)[1])


@pytest.mark.parametrize("has_level", [0, 1])
def test_clear(has_level):
    def setup(e):
        if has_level:
            level(e)
        ids(e, [0, 3, 0xFFE])
    run(0x4F0E90, "Level_Clear", setup)


def id_table(e):
    """Slot 2 holds a dead task, slot 4 and 0xFFF live ones."""
    r = e.alloc(12)
    u32(e, r, 0, 0x7000, 0x7100)
    u32(e, IDS + 8, r)
    u32(e, IDS + 16, r + 4)
    u32(e, IDS + 4 * 0xFFF, r + 8)
    u32(e, IDS + 4 * 0x1000, r + 4)  # past the end


@pytest.mark.parametrize("i", [-1, 0, 2, 4, 0xFFF, 0x1000, 0x7FFFFFFF])
def test_find_by_id(i):
    run(0x4F1030, "Task_FindById", lambda e: (id_table(e), (i,))[1], check_eax=True)


@pytest.mark.parametrize("t", [0, 0x7000, 0x7100, 0x7200])
def test_get_id(t):
    run(0x4F1050, "Task_GetId", lambda e: (id_table(e), (t,))[1], check_eax=True)


@pytest.mark.parametrize("i", [0, 2, 3, 0xFFF])
def test_set_task_id(i):
    def setup(e):
        id_table(e)
        u32(e, IDS + 4 * 0xFFF, 0)
        return i, 0x7300
    run(0x4F1090, "Level_SetTaskId", setup)


def test_set_task_id_taken():
    error_args((0x4F1090, "Level_SetTaskId"), lambda e: (id_table(e), (4, 0x7300))[1],
               "LevelQTaskID %d already defined", lambda e: (e.arg(1), LOG[-1]))
