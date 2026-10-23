"""SubLevel and EditVariable."""
import pytest

from test_levelflow_util import (RET, check_init, handler, kids, logfake, make_emu, make_init_emu, run, set_handler,
                                 task, u32)

emu = make_emu()
logfake(emu, 0x401780, 1)  # Task_DeleteChildren
logfake(emu, 0x4B1510, 2, strings=(0, 1), effect=lambda e, a: RET.get("file", 0))  # File_Open
logfake(emu, 0x4B1690, 1)  # File_Close
logfake(emu, 0x4B80B0, 1, strings=(0,), effect=lambda e, a: RET.get("script", 0))  # Script_Load
logfake(emu, 0x4B8930, 2, strings=(0,))  # Script_SetSymbolContext
logfake(emu, 0x4B85B0, 1)  # Script_Run
logfake(emu, 0x4B83D0, 1)  # Script_Free
RUN = handler(emu, "run", 1)
DAC = handler(emu, "dac", 1)
emu.baseline = emu.snapshot()
init = make_init_emu()


@pytest.mark.parametrize("addr,name", [(0x488620, "SubLevel_Init"), (0x488800, "EditVariable_Init")])
def test_init(addr, name):
    check_init(init, addr, name)


@pytest.mark.parametrize("addr,name,globs", [(0x4887E0, "SubLevel_Close", (0x5C8A90, 0x5C8A94)),
                                             (0x488B90, "EditVariable_Close", (0x5410D0, 0x5C8A9C))])
def test_close(addr, name, globs):
    def setup(e):
        for i, g in enumerate(globs):
            u32(e, g, 0xABCD0010 + i)
    run(emu, addr, name, setup)


@pytest.mark.parametrize("addr,name,glob", [(0x488770, "SubLevel_GetParams", 0x5C8A94),
                                            (0x488A90, "EditVariable_GetParams", 0x5C8A9C)])
def test_get_params(addr, name, glob):
    def setup(e):
        u32(e, glob, 0x9876)
        return 0, e.alloc(4)
    run(emu, addr, name, setup)


# SubLevel

def sublevel(e, path=b"LOCAL:missions/location0/level1/sub.qsc"):
    s = task(e, 0x120, 0x53)
    e.write(s + 0x20, path + b"\0")
    return s


def test_sublevel_create():
    run(emu, 0x4886F0, "SubLevel_Create", lambda e: (sublevel(e),))


@pytest.mark.parametrize("n", [0, 1, 3])
def test_sublevel_run(n):
    def setup(e):
        s = sublevel(e)
        kids(e, s, n, 1, RUN)
        return (s,)
    run(emu, 0x488700, "SubLevel_Run", setup)


@pytest.mark.parametrize("path,file,script", [(b"", 0, 0), (b"a.qsc", 0, 0), (b"a.qsc", 0x7000, 0),
                                              (b"a.qsc", 0x7000, 0x7100)])
def test_sublevel_load(path, file, script):
    def setup(e):
        RET.update(file=file, script=script)
        return (sublevel(e, path),)
    run(emu, 0x488780, "SubLevel_Load", setup)


# EditVariable

def editvar(e, value=5, registered=0):
    v = task(e, 0xE0, 0x54)
    u32(e, v + 0xD8, value)
    e.write(v + 0xDC, bytes([registered]))
    return v


def test_editvar_create():
    run(emu, 0x488980, "EditVariable_Create", lambda e: (editvar(e, 7, 1),))


@pytest.mark.parametrize("has", [(0, 0), (1, 0), (0, 1), (1, 1)])
@pytest.mark.parametrize("evals", [(1.0, 2.0), (-3.5, 2.5), (0.49, 1e6), (2.5, -7.0)])
def test_editvar_run(has, evals):
    def setup(e):
        RET.update(has=list(has), evals=[v for h, v in zip(has, evals) if h])
        return (editvar(e),)
    run(emu, 0x4889B0, "EditVariable_Run", setup)


@pytest.mark.parametrize("id_", [-1, 0, 3])
@pytest.mark.parametrize("handled", [0, 1])
def test_editvar_delete(id_, handled):
    def setup(e):
        RET["id"] = id_
        set_handler(e, 0x37, 0x54, DAC if handled else 0)
        return (editvar(e),)
    run(emu, 0x488A30, "EditVariable_Delete", setup)


def test_editvar_changed():
    run(emu, 0x488AA0, "EditVariable_Changed", lambda e: (editvar(e),))


@pytest.mark.parametrize("registered", [0, 1])
def test_editvar_activate(registered):
    run(emu, 0x488AD0, "EditVariable_Activate", lambda e: (editvar(e, registered=registered),))


@pytest.mark.parametrize("registered", [0, 1])
def test_editvar_deactivate(registered):
    run(emu, 0x488B30, "EditVariable_Deactivate", lambda e: (editvar(e, registered=registered),))
