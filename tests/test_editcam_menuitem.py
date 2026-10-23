"""MenuItem (0x505710 - 0x505C80) and Container (0x505C90 - 0x505D40)."""
import struct

import pytest

from emu import ErrorShown
from test_editcam_util import LOG, add_child, handler, make, run, set_handler, task, u16, u32, u8

FIND, SELECT, COMMIT, POINTER = 0x40, 0x41, 0x42, 0x43
PANEL, ITEM, OTHER = 0x50, 0x51, 0x52

emu = make()
set_handler(emu, FIND, ITEM, 0x505840)
set_handler(emu, SELECT, ITEM, 0x5057C0)
for t in (ITEM, OTHER):
    set_handler(emu, COMMIT, t, handler(emu, "commit%x" % t, 2))
    set_handler(emu, POINTER, t, handler(emu, "pointer%x" % t, 2))
on_event = handler(emu, "onEvent", 2, derefs={1: 28})
emu.baseline = emu.snapshot()


def events(e):
    u8(e, 0x54E71C, COMMIT, SELECT, FIND)
    u8(e, 0x54E70A, POINTER)
    u16(e, 0x54E720, ITEM)


def item(e, panel, id_, links=(0, 0, 0, 0), disabled=0, pressed=0, selected=0, handler=True):
    it = task(e, 0x54, ITEM, panel)
    u32(e, it + 0x38, id_, *links)
    u8(e, it + 0x4C, disabled, pressed, selected)
    u32(e, it + 0x50, on_event if handler else 0)
    if panel:
        add_child(e, panel, it)
    return it


def panel(e, sel=None, disabled=(), other=True):
    """Items 1..4 in a 2x2 grid (links up, down, left, right), and one child that is no item."""
    events(e)
    p = task(e, 0x40, PANEL)
    grid = {1: (0, 3, 0, 2), 2: (0, 4, 1, 0), 3: (1, 0, 0, 4), 4: (2, 0, 3, 0)}
    its = {}
    for i in (1, 2):
        its[i] = item(e, p, i, grid[i], i in disabled, 0, i == sel)
    if other:
        add_child(e, p, task(e, 0x40, OTHER, p))
    for i in (3, 4):
        its[i] = item(e, p, i, grid[i], i in disabled, 0, i == sel)
    return p, its


@pytest.mark.parametrize("pressed,selected", [(0, 0), (1, 0), (0, 1), (1, 1)])
@pytest.mark.parametrize("children", [0, 1, 3])
def test_commit(pressed, selected, children):
    def setup(e):
        events(e)
        it = item(e, 0, 7, pressed=pressed, selected=selected)
        for i in range(children):
            add_child(e, it, task(e, 0x40, [ITEM, OTHER, 0x53][i]))
        return it, 0x1234
    run(emu, 0x505710, "MenuItem_Commit", setup)


@pytest.mark.parametrize("same", [0, 1])
@pytest.mark.parametrize("keys", [0, 1, 2, 4, 8, 3, 0x10])
@pytest.mark.parametrize("has_handler", [0, 1])
def test_select(same, keys, has_handler):
    def setup(e):
        p, its = panel(e, sel=1)
        it = its[1]
        u32(e, it + 0x50, on_event if has_handler else 0)
        ev = e.alloc(28)
        u32(e, ev, 1 if same else 9, 0, 0, 0, 0, 0, keys)
        return it, ev
    run(emu, 0x5057C0, "MenuItem_Select", setup)


def test_identify():
    def setup(e):
        return 0x1357, e.alloc(4)
    run(emu, 0x505840, "MenuItem_Identify", setup)


@pytest.mark.parametrize("children", [0, 1, 3])
def test_forward_pointer(children):
    def setup(e):
        events(e)
        it = item(e, 0, 7)
        for i in range(children):
            add_child(e, it, task(e, 0x40, [ITEM, OTHER, 0x53][i]))
        return it, 0x4321
    run(emu, 0x505850, "MenuItem_ForwardPointer", setup)


def test_shutdown():
    run(emu, 0x5058E0, "MenuItem_Shutdown", events)


@pytest.mark.parametrize("other", [0, 1])
@pytest.mark.parametrize("empty", [0, 1])
def test_for_each(other, empty):
    log = handler(emu, "each", 2)
    emu.baseline = emu.snapshot()

    def setup(e):
        if empty:
            events(e)
            return task(e, 0x40, PANEL), log, 0x77
        p, _ = panel(e, other=other)
        return p, log, 0x77
    run(emu, 0x505920, "MenuItem_ForEach", setup)


@pytest.mark.parametrize("id_", [1, 3, 4, 9])
def test_find_by_id(id_):
    run(emu, 0x5059D0, "MenuItem_FindById", lambda e: (panel(e)[0], id_), check_eax=True)


@pytest.mark.parametrize("sel", [None, 2, 4])
def test_find_selected(sel):
    run(emu, 0x505A30, "MenuItem_FindSelected", lambda e: (panel(e, sel=sel)[0],), check_eax=True)


@pytest.mark.parametrize("empty", [0, 1])
def test_find_first(empty):
    def setup(e):
        events(e)
        return (task(e, 0x40, PANEL) if empty else panel(e)[0]),
    run(emu, 0x505A80, "MenuItem_FindFirst", setup, check_eax=True)


@pytest.mark.parametrize("hit", [0, 1])
def test_matchers(hit):
    def setup(e):
        p, its = panel(e, sel=2 if hit else None)
        s = e.alloc(12)
        u32(e, s, 2 if hit else 5, 0, 0)
        return its[2], s
    run(emu, 0x505A10, "MenuItem_MatchId", setup)
    run(emu, 0x505A60, "MenuItem_MatchSelected", setup)

    def setup_first(e):
        p, its = panel(e)
        s = e.alloc(12)
        u32(e, s, 0, 0 if hit else 0x999, 0)
        return its[3], s
    run(emu, 0x505AB0, "MenuItem_MatchFirst", setup_first)


@pytest.mark.parametrize("sel", [None, 1, 2, 3, 4])
@pytest.mark.parametrize("dir_", [-1, -2, -3, -4, 2, 3])
@pytest.mark.parametrize("disabled", [(), (2,), (3,), (2, 4)])
def test_move(sel, dir_, disabled):
    def setup(e):
        p, _ = panel(e, sel=sel, disabled=disabled)
        return p, dir_
    run(emu, 0x505AD0, "MenuItem_Move", setup, check_eax=True)


@pytest.mark.parametrize("dir_", [0, -5])
def test_move_bad_dir(dir_):
    for target in (0x505AD0, "MenuItem_Move"):
        emu.reset()
        p, _ = panel(emu, sel=1)
        with pytest.raises(ErrorShown) as err:
            emu.call(target, p, dir_)
        assert "illegal direction" in str(err.value)


def test_move_no_selection_bad_dir():
    run(emu, 0x505AD0, "MenuItem_Move", lambda e: (panel(e)[0], 0), check_eax=True)


def test_setup():
    def setup(e):
        it = task(e, 0x54, ITEM)
        e.write(it + 0x38, bytes(range(0x1C)))
        return it, 11, 12, 13, 14, 15, 0x3301, 0x5050
    run(emu, 0x505C10, "MenuItem_Setup", setup)


@pytest.mark.parametrize("addr,name", [(0x505C50, "MenuItem_GetFindEvent"), (0x505C60, "MenuItem_GetSelectEvent"),
                                       (0x505C70, "MenuItem_GetCommitEvent")])
def test_get_event(addr, name):
    run(emu, addr, name, events, check_eax=True, eax_mask=0xFF)


def test_get_type():
    run(emu, 0x505C80, "MenuItem_GetType", events, check_eax=True, eax_mask=0xFFFF)


def test_container():
    def setup(e):
        u16(e, 0xA7A7BC, 0x66)
    run(emu, 0x505C90, "Container_Init", setup)
    run(emu, 0x505D30, "Container_Shutdown", setup)
    run(emu, 0x505D40, "Container_GetType", setup, check_eax=True, eax_mask=0xFFFF)


def test_logs_calls():
    """The fakes really log: a select with a handler calls it once."""
    def setup(e):
        p, its = panel(e, sel=1)
        ev = e.alloc(28)
        u32(e, ev, 1, 0, 0, 0, 0, 0, 0)
        return its[1], ev
    run(emu, 0x5057C0, "MenuItem_Select", setup)
    assert LOG[-1] and LOG[-1][0][0] == "onEvent"
    assert struct.unpack("<I", LOG[-1][0][2][:4])[0] == 1
