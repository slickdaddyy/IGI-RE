import pytest

from emu import Emu, ErrorShown
from test_fxobj_util import init_emu, setup_init
from test_menu_util import add_child, logfake, task
from test_patrol_util import fake, list_add, list_init, run, u32

PATHS = 0x5BDA70
CMD_TYPE, PATH_TYPE = 0x50, 0x51

init = init_emu()

e = Emu()
logfake(e, 0x4C1780, 2, strings=(1,))  # Task_SetName
logfake(e, 0x4C1830, 1)  # QTaskList_Free
logfake(e, 0x401A20, 1, masks={0: 0xFFFF})  # TaskType_Free
fake(e, 0x4F1050, 1, effect=lambda e, a: e.u32(a[0] + 0x40))  # Task_GetId, id kept at +0x40 here


def test_command_open():
    run(init, 0x4533B0, "PatrolPathCommand_Open", setup_init)


def test_path_open():
    run(init, 0x453550, "PatrolPath_Open", setup_init)


def test_closes():
    def setup(e):
        u32(e, 0x53DFCC, 0x1234)
        u32(e, 0x53E29C, 0x4321)
        u32(e, 0x5BDA68, 0x777)
    run(e, 0x453520, "PatrolPathCommand_Close", setup)
    run(e, 0x453710, "PatrolPath_Close", setup)


def command(e, type_, arg=0, parent=None):
    c = task(e, 0x28, CMD_TYPE)
    u32(e, c + 0x20, type_, arg)
    if parent:
        add_child(e, parent, c)
    return c


def test_command_simple():
    run(e, 0x453470, "PatrolPathCommand_Create", lambda e: (command(e, 5, 5),))
    run(e, 0x453540, "PatrolPathCommand_GetType", lambda e: (command(e, 3, 5),), check_eax=True)

    def params(e):
        u32(e, 0x5BDA68, 0x5150)
        u32(e, 0x5BDA7C, 0x6160)
        out = e.alloc(4)
        return (command(e, 1), out)
    run(e, 0x453480, "PatrolPathCommand_GetParams", params)
    run(e, 0x453670, "PatrolPath_GetParams", params)


@pytest.mark.parametrize("t", range(-3, 11))
def test_command_changed(t):
    run(e, 0x453490, "PatrolPathCommand_Changed", lambda e: (command(e, t, 42),))


@pytest.mark.parametrize("t", range(-1, 9))
def test_command_name(t):
    run(e, 0x4534B0, "PatrolPathCommand_SetName", lambda e: (command(e, t, -7),))


def path(e, cmds=(), pid=-1, id_=5, listed=0):
    list_init(e, PATHS)
    p = task(e, 0x48, PATH_TYPE)
    u32(e, p + 0x20, 0, 0, pid, 0)
    e.write(p + 0x30, bytes([listed]))
    u32(e, p + 0x40, id_)
    for t in cmds:
        command(e, t, 0, p)
    return p


def test_path_create_delete():
    run(e, 0x453630, "PatrolPath_Create", lambda e: (path(e, pid=9, listed=1),))
    for listed in (0, 1):
        def setup(e, listed=listed):
            p = path(e, listed=listed)
            list_add(e, PATHS, p + 0x20)
            return (p,)
        run(e, 0x453650, "PatrolPath_Delete", setup)


@pytest.mark.parametrize("n", [0, 1, 4])
def test_count(n):
    run(e, 0x4536E0, "PatrolPath_CountCommands", lambda e: (path(e, [2] * n),), check_eax=True)


@pytest.mark.parametrize("id_,listed", [(-1, 0), (-1, 1), (7, 0), (7, 1)])
def test_path_changed(id_, listed):
    def setup(e):
        other = path(e, [1], pid=3)
        list_add(e, PATHS, other + 0x20)
        p = task(e, 0x48, PATH_TYPE)
        u32(e, p + 0x20, 0, 0, 2, 0)
        e.write(p + 0x30, bytes([listed]))
        u32(e, p + 0x40, id_)
        command(e, 2, 0, p)
        command(e, 6, 0, p)
        if listed:
            list_add(e, PATHS, p + 0x20)
        return (p,)
    run(e, 0x453680, "PatrolPath_Changed", setup)


def paths(e):
    list_init(e, PATHS)
    out = []
    for pid in (4, 8, 15):
        p = task(e, 0x48, PATH_TYPE)
        u32(e, p + 0x28, pid)
        list_add(e, PATHS, p + 0x20)
        out.append(p)
    return out


@pytest.mark.parametrize("pid", [4, 8, 15, 16])
def test_find(pid):
    run(e, 0x453840, "PatrolPath_Find", lambda e: (paths(e) and pid,), check_eax=True)
    run(e, 0x453870, "PatrolPath_FromNode", lambda e: (0x1000 + pid,), check_eax=True)


def route(e, cmds, cur=None, end=None):
    """A route on a new path with the given command types; cur/end are indexes."""
    p = path(e, cmds)
    c = []
    n = e.u32(p + 8)
    while e.u32(n):
        c.append(n)
        n = e.u32(n)
    r = e.alloc(0x1C)
    u32(e, r, p, c[cur] if cur is not None else 0, c[end] if end is not None else 0, 0x111, 0x222, 0, 0x333)
    return r, p, c


ROUTES = [([2, 3, 1], 0, None), ([2, 3, 1], 2, None), ([2, 3, 1], 2, 1), ([1, 6, 2], 0, None),
          ([1, 6, 2], 0, 1), ([1, 6, 6], 2, None), ([2, 6], 0, None), ([6, 6], 1, 1), ([6], 0, None),
          ([0, 4, 5, 7, 8], 1, None), ([0, 6, 3, 7], 0, None), ([6, 2], 1, 0)]


@pytest.mark.parametrize("cmds,cur,end", ROUTES)
def test_route(cmds, cur, end):
    for addr, name in ((0x453720, "PatrolRoute_Current"), (0x453730, "PatrolRoute_Next"),
                       (0x453760, "PatrolRoute_Advance")):
        run(e, addr, name, lambda e: (route(e, cmds, cur, end)[0],), check_eax=True)


def test_route_end_end():
    for target in (0x453760, "PatrolRoute_Advance"):
        e.reset()
        r = route(e, [2, 6, 6], 0)[0]
        with pytest.raises(ErrorShown) as err:
            e.call(target, r)
        assert str(err.value) == "END command following END command"


@pytest.mark.parametrize("cmds", [[2, 3, 6, 1, 6, 2], [6, 1], [0, 6]])
@pytest.mark.parametrize("n", [0, 1, 3, 9])
def test_seek_init(cmds, n):
    run(e, 0x453880, "PatrolRoute_Seek", lambda e: (route(e, cmds)[0], n), check_eax=True)

    def setup(e):
        r, p, c = route(e, cmds)
        list_init(e, 0x5BDA70)
        u32(e, p + 0x28, 77)
        list_add(e, 0x5BDA70, p + 0x20)
        return (r, 77, n)
    if n < len(cmds):
        run(e, 0x4537E0, "PatrolRoute_Init", setup)
