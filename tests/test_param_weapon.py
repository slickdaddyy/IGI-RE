import pytest

from emu import Emu, compare
from test_menu_util import LOG, add_child, check, handler, set_handler, task, trap, u32

SELECT, DESELECT, STATE = 0x30, 0x31, 0x32
GUN, PISTOL, OTHER, SILENT = 5, 6, 7, 8
SET = 0x4F2070

e = Emu()
e.call(0x4B0BA0)  # Mem_Init
e.call(0x4B8B20)  # Script_Init
e.call(0x4010D0)  # Task_InitSystem
e.baseline = e.snapshot()


def state_handler(e):
    t = e.arg(0)
    LOG[-1].append(("state", t))
    u32(e, e.arg(1), t + 0x40)
    e.ret(0)


def silent_handler(e):
    LOG[-1].append(("silent", e.arg(0)))
    e.ret(0)


STATE_FN = trap(e, "state", state_handler)
SILENT_FN = trap(e, "silent", silent_handler)
for t in (GUN, PISTOL):
    set_handler(e, STATE, t, STATE_FN)
    set_handler(e, SELECT, t, handler(e, "select_%d" % t, 1))
set_handler(e, DESELECT, GUN, handler(e, "deselect", 1))
set_handler(e, STATE, SILENT, SILENT_FN)
e.write(0xA76C03, bytes([STATE]))
u32(e, 0xA76BFC, SELECT)
u32(e, 0xA76C04, DESELECT)
e.baseline = e.snapshot()


def test_init_events():
    compare(e, 0x4F1EE0, "Weapon_InitEvents", check_eax=False)


def test_free_events():
    def setup(e):
        e.call(0x4F1EE0)
    compare(e, 0x4F1F50, "Weapon_FreeEvents", setup=setup, check_eax=False)


@pytest.mark.parametrize("addr,name,mask", [
    (0x4F1FC0, "Weapon_GetSlotsEvent", 0xFF), (0x4F1FD0, "Weapon_GetStateEvent", 0xFF),
    (0x4F1FE0, "Weapon_GetMoveEvent", 0xFF), (0x4F1FF0, "Weapon_GetTriggerEvent", 0xFF),
    (0x4F2000, "Weapon_GetEvent3", 0xFF), (0x4F2010, "Weapon_GetEvent4", 0xFF),
    (0x4F2020, "Weapon_GetSelectEvent", 0xFFFFFFFF), (0x4F2030, "Weapon_GetDeselectEvent", 0xFFFFFFFF),
    (0x4F2040, "Weapon_GetEvent7", 0xFF)])
def test_getters(addr, name, mask):
    def setup(e):
        e.write(0xA76BFC, bytes(range(0xA1, 0xA1 + 0x18)))
    compare(e, addr, name, setup=setup, eax_mask=mask)


def test_slots_init():
    def setup(e):
        s = e.alloc(16)
        u32(e, s, 1, 2, 3, 4)
        return (s,)
    compare(e, 0x4F2050, "WeaponSlots_Init", setup=setup, check_eax=False)


def test_slots_get():
    def setup(e):
        s = e.alloc(12)
        u32(e, s, 0x11, 0x22, 0x33)
        return s, 2
    compare(e, 0x4F2060, "WeaponSlots_Get", setup=setup)


def weapon(e, type_, slot, unk8=0, owner=None):
    t = task(e, 0x50, type_)
    u32(e, t + 0x40, 0x99, slot, unk8)
    if owner:
        add_child(e, owner, t)
    return t


@pytest.mark.parametrize("old,new", [(None, None), (GUN, None), (PISTOL, None), (None, GUN), (GUN, PISTOL)])
def test_slots_set(old, new):
    def setup(e):
        s = e.alloc(12)
        if old:
            u32(e, s + 4, weapon(e, old, 1))
        return s, 1, weapon(e, new, 1) if new else 0
    check(e, SET, "WeaponSlots_Set", setup=setup, check_eax=False)


# children of the owner: (type, slot, unk8)
KIDS = [(OTHER, 1, 0), (GUN, 2, 0), (SILENT, 1, 0), (GUN, 1, 1), (PISTOL, 1, 0), (GUN, 2, 0), (GUN, 1, 0)]


@pytest.mark.parametrize("cur", [None, 1, 3, 4, 6])
@pytest.mark.parametrize("slot", [1, 2, 3])
def test_select_next(cur, slot):
    def setup(e):
        owner = task(e, 0x40)
        kids = [weapon(e, t, s, u, owner) for t, s, u in KIDS]
        slots = e.alloc(16)
        if cur is not None:
            u32(e, slots + 4 * slot, kids[cur])
        return owner, slots, slot
    check(e, 0x4F2140, "WeaponSlots_SelectNext", setup=setup, eax_mask=0xFF)


def test_select_next_no_children():
    def setup(e):
        return task(e, 0x40), e.alloc(16), 1
    check(e, 0x4F2140, "WeaponSlots_SelectNext", setup=setup, eax_mask=0xFF)
