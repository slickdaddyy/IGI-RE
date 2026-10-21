import struct

import pytest

from emu import Emu, TRAP_BASE, compare

LOG = []
CFG = {}


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def cstr(e, s):
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def pyfake(e, addr, name, fn):
    trap = TRAP_BASE + 4 * len(e.imports)
    e.imports.append(("fake", name))
    e.write(trap, b"\xc3")
    if addr:
        e.write(addr, b"\xe9" + struct.pack("<i", trap - addr - 5))
        e.uc.ctl_remove_cache(addr, addr + 5)
    e.stub(name, fn)
    return trap


def s(e, i):
    return e.read_cstr(e.arg(i))


def file_exists(e):
    LOG.append(("exists", s(e, 0)))
    e.ret(int(s(e, 0) in CFG.get("files", ())))


def qvm_load(e):
    LOG.append(("qvm_load", s(e, 0), e.arg(1), e.arg(2)))
    e.ret(CFG.get("qvm", 0))


def get_time(e):
    t = CFG.get("time", {}).get(s(e, 1), 0)
    e.write(e.arg(0), struct.pack("<q", t))
    LOG.append(("time", s(e, 1)))
    e.ret(0)


def compare_time(e):
    a, b = (struct.unpack("<q", e.read(e.arg(i), 8))[0] for i in (0, 1))
    LOG.append(("cmp", a, b))
    e.ret((a > b) - (a < b))


def resource_load(e):
    LOG.append(("res_load", s(e, 0)))
    if e.arg(1):
        e.write(e.arg(1), u32(0x321))
    e.ret(CFG.get("res", 0))


def logger(name, strs, ret=None):
    def fn(e):
        LOG.append((name,) + tuple(s(e, i) if i in strs else e.arg(i) for i in range(len(strs) or 1)))
        e.ret(CFG.get(ret, 0) if ret else 0)
    return fn


def error_handler(e):
    LOG.append(("error", s(e, 0)))
    e.ret(0)


le = Emu()
pyfake(le, 0x4B5B60, "File_Exists", file_exists)
pyfake(le, 0x4BD220, "Qvm_Load", qvm_load)
pyfake(le, 0x4B6440, "File_GetTime", get_time)
pyfake(le, 0x4B6490, "File_CompareTime", compare_time)
pyfake(le, 0x4B5F00, "Resource_Load", resource_load)
pyfake(le, 0x4B1AC0, "File_Delete", logger("delete", (0,)))
pyfake(le, 0x4BBCB0, "Qvm_Parse", logger("parse", (0, 1), "parse"))
pyfake(le, 0x4BB270, "Qvm_Assemble", logger("assemble", (0, 1), "assemble"))
pyfake(le, 0x4BD380, "Qvm_Free", logger("qvm_free", ()))
pyfake(le, 0x4B6380, "Resource_Unload", logger("res_unload", (0,)))
pyfake(le, 0x4BD080, "Qvm_Run", logger("qvm_run", (), "run"))
le.write(0xA43E70, u32(pyfake(le, 0, "ScriptErrorHandler", error_handler)))
le.write(0xA96598, u32(1))
le.call(0x4B0BA0)
le.baseline = le.snapshot()

QVM = 0x23000000
RES = 0x23800000
SRC = b"LOCAL:missions/test.qsc"
QVMP = b"LOCAL:missions/test.qvm"


def compare_logged(e, orig, ours, setup, **kw):
    runs = []

    def wrap(e):
        runs.append(list(LOG))
        r = setup(e)
        LOG.clear()
        return r
    compare(e, orig, ours, setup=wrap, **kw)
    assert runs[1] == LOG, "calls differ:\n orig %s\n ours %s" % (runs[1], LOG)


def config(compile, only, **kw):
    def setup(e):
        CFG.clear()
        CFG.update(kw)
        e.write(0x5C8BEC, u32(compile))
        e.write(0x5C8BF0, u32(only))
        e.write(0xA965A0, u32(1))
        return (cstr(e, SRC),)
    return setup


BOTH = (SRC, QVMP)
LOAD_CASES = [
    (0, 0, dict(files=(SRC,), res=RES)),
    (0, 0, dict(files=(), res=RES)),
    (0, 0, dict(files=(SRC,), res=0)),
    (1, 1, dict(files=BOTH, qvm=QVM)),
    (1, 1, dict(files=(SRC,), qvm=QVM)),
    (1, 1, dict(files=BOTH, qvm=0)),
    (1, 0, dict(files=BOTH, qvm=QVM, time={QVMP: 5, SRC: 3})),
    (1, 0, dict(files=BOTH, qvm=QVM, time={QVMP: 3, SRC: 3})),
    (1, 0, dict(files=BOTH, qvm=QVM, res=RES, time={QVMP: 2, SRC: 3})),
    (1, 0, dict(files=BOTH, qvm=QVM, res=RES, parse=1, time={QVMP: 2, SRC: 3})),
    (1, 0, dict(files=BOTH, qvm=QVM, res=RES, assemble=1, time={QVMP: 2, SRC: 3})),
    (1, 0, dict(files=(SRC,), qvm=0, res=RES)),
    (1, 0, dict(files=(), qvm=QVM, res=RES)),
    (1, 0, dict(files=BOTH, qvm=0, res=RES, time={QVMP: 5, SRC: 3})),
]


@pytest.mark.parametrize("compile,only,kw", LOAD_CASES)
def test_script_load(compile, only, kw):
    compare_logged(le, 0x4B80B0, "Script_Load", setup=config(compile, only, **kw))


@pytest.mark.parametrize("kw", [dict(files=()), dict(files=(SRC,), res=RES), dict(files=(SRC,), res=RES, parse=1),
                                dict(files=(SRC,), res=RES, assemble=1)])
def test_script_compile(kw):
    compare_logged(le, 0x4B8410, "Script_Compile", setup=config(0, 0, **kw), check_eax=False)


def script(e, qvm):
    p = e.call(0x4B0C60, 0x94, 4)[0]
    e.write(p, b"LOCAL:test.qsc\0")
    e.write(p + 0x8C, u32(qvm))
    if qvm:
        e.write(qvm + 0x268, b"Illegal instruction\0")
    return p


@pytest.mark.parametrize("qvm", [0, QVM])
def test_script_free(qvm):
    compare_logged(le, 0x4B83D0, "Script_Free", setup=lambda e: (script(e, qvm),), check_eax=False)


@pytest.mark.parametrize("fail", [0, 1])
def test_script_run_qvm(fail):
    def setup(e):
        CFG.clear()
        CFG["run"] = fail
        e.write(0xA965A4, u32(1))
        return (script(e, QVM),)
    compare_logged(le, 0x4B85B0, "Script_Run", setup=setup, check_eax=False)
