import struct

import pytest

from emu import Emu
from test_win_util import api, check, check_raises, cstr, logged, pyfake, trap, u32

emu = Emu()

APP = 0x5C8BC0
WINPROCS = 0x5C8D28
WINPROC_LIST = 0x5C8DF0
RUNNING = 0x5C8DE8
ALT_DOWN = 0x5C8E04
TIMER_START = 0x5C8E34

# strtok keeps a pointer into the caller's stack buffer
STRTOK = [(0x936060, 0x936064)]

TICK = [5000]
STATE = {}

def game_start(e):
    if STATE.get("inactive"):
        u32(e, APP + 0x3C, 0)
    return STATE["start"]


pyfake(emu, 0x401000, "Game_Start", 0, before=game_start)
pyfake(emu, 0x401050, "Game_Update", 0, before=lambda e: e.write(RUNNING, bytes([STATE["keep"]])))
pyfake(emu, 0x4010B0, "Game_Shutdown", 0)
pyfake(emu, 0x498480, "DirectX_GetVersion", 2,
       before=lambda e: (u32(e, e.arg(0), STATE["dx"]), u32(e, e.arg(1), 2), 0)[2])


def peek(e):
    return 1 if STATE["msgs"] else 0


def get_message(e):
    m = STATE["msgs"].pop(0)
    e.write(e.arg(0), struct.pack("<6I", 0x9999, m, 0x33, 0, 0, 0))
    return 0 if m == 0x12 else 1


def wndclass(e):
    return (1, tuple(e.normalize(v) for v in struct.unpack("<10I", e.read(e.arg(0), 40))))


api(emu, "GetTickCount", 0, lambda: TICK[0])
for name, n, ret, before in [
        ("CreateMutexA", 3, 0x4400, None), ("GetLastError", 0, 0, lambda e: STATE["lasterr"]),
        ("MessageBoxA", 4, 1, None), ("CoInitialize", 1, 0, None), ("CoUninitialize", 0, 0, None),
        ("GetForegroundWindow", 0, 0x1111, None), ("LoadIconA", 2, 0x2222, None), ("LoadCursorA", 2, 0x3333, None),
        ("RegisterClassA", 1, 1, wndclass), ("GetSystemMetrics", 1, 0, lambda e: 1024 + e.arg(0) * 7),
        ("CreateWindowExA", 12, 0, lambda e: STATE["hwnd"]), ("ShowCursor", 1, 0, None), ("ShowWindow", 2, 0, None),
        ("Sleep", 1, 0, None), ("DestroyWindow", 1, 1, None), ("PeekMessageA", 5, 0, peek),
        ("GetMessageA", 4, 0, get_message), ("TranslateMessage", 1, 1, None), ("DispatchMessageA", 1, 0, None),
        ("SetForegroundWindow", 1, 1, None), ("UnregisterClassA", 2, 1, None), ("ReleaseMutex", 1, 1, None),
        ("PostQuitMessage", 1, 0, None), ("PostMessageA", 4, 1, None), ("DefWindowProcA", 4, 0x77, None)]:
    api(emu, name, n, ret, before=before)


def winprocs(e, fns=()):
    e.write(WINPROCS, b"\0" * 12 * 16)
    e.write(WINPROC_LIST, struct.pack("<3I", WINPROC_LIST + 4, 0, WINPROC_LIST))
    prev = WINPROC_LIST
    for i, fn in enumerate(fns):
        p = WINPROCS + 12 * i
        e.write(p, struct.pack("<3I", WINPROC_LIST + 4, prev, fn))
        u32(e, prev, p)
        u32(e, WINPROC_LIST + 8, p)
        prev = p


def test_timer():
    TICK[0] = 12345
    check(emu, 0x490360, "Timer_Open", check_eax=False)
    check(emu, 0x490370, "Timer_GetMs", setup=lambda e: u32(e, TIMER_START, 2000))


@pytest.mark.parametrize("addr,name", [(0x48F350, "Main_ParseWindowCB"), (0x48F920, "Main_ParseWinInputCB"),
                                       (0x48F930, "Main_ParseGermanyCB"), (0x48F950, "Main_ParseUSACB"),
                                       (0x48F960, "Main_ParseHardcoreGermanCB")])
def test_parse_callbacks(addr, name):
    def setup(e):
        u32(e, APP + 0x1C, 1)
        u32(e, APP + 0x40, 1)
        return (0,)
    check(emu, addr, name, setup=setup, check_eax=False)


@pytest.mark.parametrize("cmdline", [b"", b"-window", b"window", b"WINDOW foo", b"\tWindowed\t-x Window",
                                     b"Win", b"a b c Window2"])
def test_parse_cmdline(cmdline):
    def setup(e):
        u32(e, APP + 8, cstr(e, cmdline))
        cb = trap(e, "cb", logged("cb", 1, pop=0, before=lambda e: (0, e.read_cstr(e.arg(0)))))
        return (cstr(e, b"Window"), cb)
    check(emu, 0x48F360, "AppMain_ParseCmdLineArgs", setup=setup, check_eax=False, ignore=STRTOK)


@pytest.mark.parametrize("v", [0, 1, 0x1FF])
def test_set_running(v):
    check(emu, 0x48F480, "AppMain_SetRunning", args=(v,), check_eax=False)


@pytest.mark.parametrize("used", [0, 3, 15])
def test_add_winproc(used):
    check(emu, 0x48F490, "AppMain_AddWinProcCallback",
          setup=lambda e: (winprocs(e, [0x401000 + i for i in range(used)]), (0x402000,))[1], check_eax=False)


def test_add_winproc_full():
    check_raises(emu, 0x48F490, "AppMain_AddWinProcCallback",
                 setup=lambda e: (winprocs(e, [0x401000 + i for i in range(16)]), (0x402000,))[1])


@pytest.mark.parametrize("which", [0, 2, 4])
def test_remove_winproc(which):
    check(emu, 0x48F4F0, "AppMain_RemoveWinProcCallback",
          setup=lambda e: (winprocs(e, [0x401000 + i for i in range(5)]), (0x401000 + which,))[1], check_eax=False)


def test_remove_winproc_missing():
    check_raises(emu, 0x48F4F0, "AppMain_RemoveWinProcCallback",
                 setup=lambda e: (winprocs(e, [0x401000]), (0x402000,))[1])


MSGS = [(1, 0), (2, 0), (0x1C, 1), (0x104, 0x73), (0x104, 0x12), (0x104, 0x41), (0x105, 0x12), (0x105, 0x41),
        (0x100, 0x41)]


@pytest.mark.parametrize("msg,wparam", MSGS)
@pytest.mark.parametrize("running,cb_ret", [(0, 1), (1, 1), (1, 0)])
@pytest.mark.parametrize("alt", [0, 1])
def test_winproc(msg, wparam, running, cb_ret, alt):
    def setup(e):
        cb = trap(e, "wp", logged("wp", 4, cb_ret, pop=0))
        winprocs(e, [cb, cb])
        e.write(RUNNING, bytes([running]))
        u32(e, ALT_DOWN, alt)
        u32(e, APP + 4, 0x4545)
        return (0x4545, msg, wparam, 0x99)
    check(emu, 0x48F970, "AppMain_WinProc", setup=setup)


@pytest.mark.parametrize("case", ["mutex", "exists", "nowindow", "nostart", "loop", "inactive", "quit"])
@pytest.mark.parametrize("cmdline", [b"", b"Window WinInput Germany USA HardcoreGerman"])
def test_winmain(case, cmdline):
    def setup(e):
        STATE.clear()
        STATE.update(start=case not in ("nostart", "nowindow"), keep=0, dx=0x700, msgs=[],
                     lasterr=183 if case == "exists" else 0, hwnd=0 if case == "nowindow" else 0x5151)
        if case == "quit":
            STATE.update(keep=1, msgs=[0x200, 0x12])
        if case == "mutex":
            api(e, "CreateMutexA", 3, 0)
        else:
            api(e, "CreateMutexA", 3, 0x4400)
        if case == "inactive":
            STATE.update(inactive=True, msgs=[0x12])
        winprocs(e)
        return (0x400000, 0, cstr(e, cmdline), 10)
    check(emu, 0x48F530, "AppMain_WinMain", setup=setup, ignore=STRTOK)
