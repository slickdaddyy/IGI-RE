import pytest

from emu import Emu, compare
from test_menu_util import LOG, add_child, check, set_handler, task, task_system, trap, u32

NOTES = 0xA70CA8
SET_ID = 0x4F1090
TASKREF_ADD = 0x401AE0
MEM_ALLOC = 0x4B0C60

e = Emu()
task_system(e)
for i in range(251):
    b = NOTES + 12 * i
    u32(e, b, b + 4, 0, b)


def name_handler(e):
    LOG[-1].append(("name", e.arg(0)))
    e.write(e.arg(1), b"Door_7\0")
    e.ret(0)


NAME = trap(e, "name", name_handler)
e.baseline = e.snapshot()


def tasks(e, n, ids=()):
    ts = [task(e, 0x40) for _ in range(n)]
    for i, t in ids:
        e.call(SET_ID, i, ts[t])
    return ts


def test_clear_task_id():
    def setup(e):
        tasks(e, 4, [(0, 0), (1, 1), (2, 2), (3, 3)])
        return (2,)
    compare(e, 0x4F10E0, "Level_ClearTaskId", setup=setup, check_eax=False)


@pytest.mark.parametrize("ids", [[(0, 0), (5, 1), (7, 3), (9, 4)], [(1, 2)], []])
def test_clear_task_id_tree(ids):
    def setup(e):
        ts = tasks(e, 5, ids)
        add_child(e, ts[0], ts[1])
        add_child(e, ts[0], ts[2])
        add_child(e, ts[2], ts[3])
        add_child(e, ts[0], ts[4])
        return (ts[0],)
    compare(e, 0x4F1110, "Level_ClearTaskIdTree", setup=setup, check_eax=False)


@pytest.mark.parametrize("paused,step", [(0, 0), (1, 0), (0, 1), (1, 1)])
def test_is_running(paused, step):
    def setup(e):
        e.write(0xA758BD, bytes([paused, step]))
    compare(e, 0x4F1160, "Level_IsRunning", setup=setup)


def add_note(e, t, text, dead=False):
    n = e.call(MEM_ALLOC, 0x50, 4)[0]
    ref = e.call(TASKREF_ADD, t)[0]
    e.write(n + 12, text + b"\0")
    u32(e, n + 8, ref)
    e.call(0x4AF8F0, NOTES + 12 * (t % 251), n)  # List_AddTail
    if dead:
        u32(e, ref, 0)


@pytest.mark.parametrize("case", ["found", "missing", "stale", "other"])
@pytest.mark.parametrize("addr,name", [(0x4F11A0, "TaskNote_Find"), (0x4F1180, "TaskNote_GetText")])
def test_task_note(addr, name, case):
    def setup(e):
        c, = tasks(e, 1)
        # b shares a's bucket
        a = e.alloc(0x10000)
        b = a + 251 * 0x100
        if case in ("stale", "found", "other"):
            add_note(e, b, b"dead", dead=True)
        if case != "missing":
            add_note(e, b, b"other")
        if case in ("found", "stale"):
            add_note(e, a, b"hello")
            add_note(e, c, b"c")
        return (a,)
    compare(e, addr, name, setup=setup)


@pytest.mark.parametrize("addr,name,mask", [
    (0x4F11F0, "Level_GetEventDA4", 0xFF), (0x4F1200, "Level_GetNameEvent", 0xFF),
    (0x4F1210, "Level_GetEventDA8", 0xFFFFFFFF), (0x4F1220, "Level_GetEventDAC", 0xFFFFFFFF),
    (0x4F1230, "Level_GetEventDB0", 0xFFFFFFFF), (0x4F17C0, "Level_Get", 0xFFFFFFFF)])
def test_getters(addr, name, mask):
    def setup(e):
        e.write(0x54DDA4, bytes(range(0x91, 0x91 + 16)))
        u32(e, 0xA758A4, 0x12345678)
    compare(e, addr, name, setup=setup, eax_mask=mask)


@pytest.mark.parametrize("n", [0, 1, 5])
def test_count_task_ids(n):
    def setup(e):
        tasks(e, 8, [(i, i) for i in range(n)] + [(n + 1, 7)])
    compare(e, 0x4F1240, "Level_CountTaskIds", setup=setup)


@pytest.mark.parametrize("case", ["handler", "id", "noid"])
def test_task_get_name(case):
    def setup(e):
        ts = tasks(e, 2, [(42, 0)] if case == "id" else [])
        if case == "handler":
            e.write(0x54DDA5, b"\x27")
            set_handler(e, 0x27, 0, NAME)
        out = e.alloc(128)
        return out, ts[0]
    check(e, 0x4F1270, "Task_GetName", setup=setup, check_eax=False)
