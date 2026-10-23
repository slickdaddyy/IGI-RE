"""The level task types end to end: original code against the exe with our level hooks."""
from emu import Emu, diff_text
from test_menu_util import task_system

plain = Emu()
hooked = Emu()
hooked.install_hooks()


def level_session(e):
    e.reset()
    task_system(e)
    before = e.snapshot()
    out = []
    e.call(0x4EE970)  # level init: task types, parameter types, Task_New
    types = [e.u32(0x54DD0C + 2 * i) & 0xFFFF for i in range(4)]
    level = e.call(0x4F0CB0, 0)[0]  # Level_New
    kids = [e.call(0x4012A0, level, t, 0)[0] for t in types[1:]]
    dyn_kid = e.call(0x4012A0, kids[1], e.call(0x4018F0)[0] & 0xFFFF, 0)[0]
    out += [e.call(0x4F0ED0)[0], e.call(0x4F0EE0)[0]]
    e.call(0x4F1090, 5, kids[0])
    e.call(0x4F1090, 0xFFF, dyn_kid)
    out += [e.call(0x4F1030, i)[0] for i in (5, 0xFFF, 6)]
    out += [e.call(0x4F1050, t)[0] for t in kids + [dyn_kid]]
    for _ in range(3):
        e.call(0x4F0580, level)  # Level_Run
    out.append(e.call(0x4F1080)[0])
    e.call(0x4F0E90)  # Level_Clear
    out += [e.call(0x4F1030, 5)[0], e.call(0x4F0ED0)[0], e.call(0x4F0EE0)[0]]
    e.call(0x4F0A60)  # Level_Shutdown
    return out, e.words(e.changes(before))


def test_level_session():
    ro, mo = level_session(plain)
    rn, mn = level_session(hooked)
    assert mo == mn, "memory differs: %s" % diff_text(mo, mn)
    assert ro == rn
