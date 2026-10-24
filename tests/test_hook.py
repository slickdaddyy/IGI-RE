"""The DLL's own hook installer: version check, dinput.ini selection, dinput.log and the crash log."""
import re
import struct

import pytest

from emu import Emu

# (address, function, folder), registered in this order, so their numbers are 0..3
HOOKS = [
    (0x4D7620, "BoneDynCubeObj_LineHit", "haze"),
    (0x4D88D0, "BoneDynCubeObj_CollLine", "haze"),
    (0x512C30, "TerrainMesh_DrawD3D", "glowmesh"),
    (0x521E40, "Hemicube_DrawLightmapSet", "litmap"),
]
DLL_PATH = b"C:\\Games\\IGI\\PC\\dinput.dll"
LOG_HANDLE = 0x77


def cstr(e, p):
    return e.read_cstr(p).decode("latin1")


def put_cstr(e, p, s, size=None):
    data = s.encode("latin1")
    if size is not None:
        data = data[: size - 1]
    e.write(p, data + b"\0")
    return len(data)


def vsprintf(e):
    out, ap = [], e.arg(6)

    def field(m):
        nonlocal ap
        flag, width, conv = m.group(1), int(m.group(2) or 0), m.group(4)
        if conv == "%":
            return "%"
        v = e.u32(ap)
        ap += 4
        if conv == "s":
            s = cstr(e, v) if v else "(null)"
        else:
            s = {"d": str(struct.unpack("<i", struct.pack("<I", v))[0]), "u": str(v), "X": "%X" % v, "x": "%x" % v,
                 "c": chr(v & 0xFF)}[conv]
        return s.rjust(width, "0" if flag == "0" else " ")

    text = re.sub(r"%([-0]?)(\d*)(l?)([dusXxc%])", field, cstr(e, e.arg(4)))
    put_cstr(e, e.arg(2), text, e.arg(3))
    e.ret(len(text))


class Host:
    """Fakes the Windows side of the DLL: the ini file, the log file, page protection."""

    def __init__(self, ini=None, fail_protect=(), exe_ok=True):
        self.ini = ini
        self.fail_protect = set(fail_protect)
        self.log = b""
        self.log_opened = False
        self.messages = []
        self.handler = None
        self.flushes = 0
        self.e = e = Emu()
        missing = [n for _, n, _ in HOOKS if n not in e.symbols or "hook_" + n not in e.symbols]
        if missing:
            pytest.skip("not in this DLL: " + ", ".join(missing))
        if not exe_ok:
            e.write(0x400000 + struct.unpack("<I", e.read(0x40003C, 4))[0] + 8, b"\0\0\0\0")  # TimeDateStamp
        self.strtok_next = 0
        stubs = {
            "GetModuleFileNameA": lambda e: e.ret(put_cstr(e, e.arg(1), DLL_PATH.decode(), e.arg(2)), 12),
            "GetModuleHandleA": lambda e: e.ret(0x400000 if e.arg(0) == 0 else 0, 4),
            "GetModuleHandleExA": lambda e: e.ret(0, 12),
            "GetPrivateProfileIntA": self.profile_int,
            "GetPrivateProfileStringA": self.profile_string,
            "CreateFileA": self.create_file,
            "WriteFile": self.write_file,
            "AddVectoredExceptionHandler": self.add_handler,
            "VirtualProtect": self.virtual_protect,
            "VirtualQuery": self.virtual_query,
            "GetCurrentProcess": lambda e: e.ret(0xFFFFFFFF),
            "FlushInstructionCache": self.flush,
            "GetLastError": lambda e: e.ret(5),
            "MessageBoxA": lambda e: (self.messages.append(cstr(e, e.arg(1))), e.ret(1, 16)),
            "__stdio_common_vsprintf": vsprintf,
            "strtok": self.strtok,
            "strchr": lambda e: self.find(e, cstr(e, e.arg(0)).find(chr(e.arg(1)))),
            "strrchr": lambda e: self.find(e, cstr(e, e.arg(0)).rfind(chr(e.arg(1)))),
            "strncmp": lambda e: e.ret((cstr(e, e.arg(0))[: e.arg(2)] != cstr(e, e.arg(1))[: e.arg(2)])),
            "atoi": lambda e: e.ret(int(re.match(r"\s*([-+]?\d*)", cstr(e, e.arg(0))).group(1) or 0)),
            "strtoul": lambda e: e.ret(int(cstr(e, e.arg(0)).strip() or "0", e.arg(2) or 10)),
        }
        e.stubs.update(stubs)
        for _, name, _ in HOOKS:
            e.call("hook_" + name)

    def find(self, e, i):
        e.ret(e.arg(0) + i if i >= 0 else 0)

    def strtok(self, e):
        p, delim = e.arg(0) or self.strtok_next, cstr(e, e.arg(1))
        if not p:
            return e.ret(0)
        while e.read(p, 1)[0] and chr(e.read(p, 1)[0]) in delim:
            p += 1
        if not e.read(p, 1)[0]:
            self.strtok_next = 0
            return e.ret(0)
        q = p
        while e.read(q, 1)[0] and chr(e.read(q, 1)[0]) not in delim:
            q += 1
        self.strtok_next = q + 1 if e.read(q, 1)[0] else 0
        e.write(q, b"\0")
        e.ret(p)

    def profile_int(self, e):
        assert cstr(e, e.arg(3)) == "C:\\Games\\IGI\\PC\\dinput.ini"
        v = (self.ini or {}).get(cstr(e, e.arg(1)))
        e.ret(int(v) if v is not None else e.arg(2), 16)

    def profile_string(self, e):
        v = (self.ini or {}).get(cstr(e, e.arg(1)), cstr(e, e.arg(2)))
        e.ret(put_cstr(e, e.arg(3), v, e.arg(4)), 24)

    def create_file(self, e):
        assert cstr(e, e.arg(0)) == "C:\\Games\\IGI\\PC\\dinput.log"
        self.log_opened = True
        e.ret(LOG_HANDLE, 28)

    def write_file(self, e):
        assert e.arg(0) == LOG_HANDLE
        self.log += bytes(e.read(e.arg(1), e.arg(2)))
        e.write(e.arg(3), struct.pack("<I", e.arg(2)))
        e.ret(1, 20)

    def add_handler(self, e):
        self.handler = e.arg(1)
        e.ret(1, 8)

    def virtual_protect(self, e):
        if e.arg(0) in self.fail_protect:
            return e.ret(0, 16)
        e.write(e.arg(3), struct.pack("<I", 0x20))
        e.ret(1, 16)

    def virtual_query(self, e):
        base, size = self.stack
        e.write(e.arg(1), struct.pack("<7I", base, base, 4, size, 0x1000, 4, 0x20000))
        e.ret(28, 12)

    def flush(self, e):
        self.flushes += 1
        e.ret(1, 12)

    def install(self):
        self.e.call("Hook_InstallAll", 0x10000000)
        return self.log.decode("latin1").splitlines()

    def patched(self):
        e = self.e
        out = []
        for addr, name, _ in HOOKS:
            if e.read(addr, 5) == b"\xe9" + struct.pack("<i", e.symbols[name] - addr - 5):
                out.append(name)
        return out


ALL = [n for _, n, _ in HOOKS]


def test_no_ini_installs_everything_silently():
    h = Host()
    assert h.install() == []
    assert h.patched() == ALL
    assert not h.log_opened and h.handler is None and h.flushes == 1


def test_turned_off():
    h = Host({"enabled": "0", "log": "1"})
    log = h.install()
    assert h.patched() == []
    assert log[-1] == "hooks turned off, nothing installed"


@pytest.mark.parametrize(
    "ini, want",
    [
        ({"only": "haze"}, ALL[:2]),
        ({"only": "glowmesh, litmap"}, ALL[2:]),
        ({"skip": "haze"}, ALL[2:]),
        ({"skip": "TerrainMesh_DrawD3D"}, [ALL[0], ALL[1], ALL[3]]),
        ({"skip": "0x4d88d0;0x521E40"}, [ALL[0], ALL[2]]),
        ({"only": "haze litmap", "skip": "BoneDynCubeObj_LineHit"}, ALL[1:2] + ALL[3:]),
        ({"range": "1-2"}, ALL[1:3]),
        ({"range": "2"}, ALL[2:3]),
        ({"range": "1-"}, ALL[1:]),
        ({"range": "0-2", "skip": "glowmesh"}, ALL[:2]),
    ],
)
def test_selection(ini, want):
    h = Host(ini)
    h.install()
    assert h.patched() == want


def test_log_lists_hooks_and_failures():
    h = Host({"log": "2", "skip": "litmap"}, fail_protect=[0x4D88D0])
    log = h.install()
    assert log[0].endswith(" hooks in this DLL")
    assert log[1].startswith("enabled=1 only= skip=litmap range=0-")
    assert "failed 1 4D88D0 BoneDynCubeObj_CollLine (error 5)" in log
    assert "0 4D7620 haze BoneDynCubeObj_LineHit" in log
    assert "2 512C30 glowmesh TerrainMesh_DrawD3D" in log
    assert log[-1] == "installed 2, failed 1"
    assert h.patched() == [ALL[0], ALL[2]]
    assert h.handler is not None


def test_log_1_has_no_hook_list():
    h = Host({"log": "1"})
    log = h.install()
    assert len(log) == 3 and log[-1] == "installed 4, failed 0"


def test_unsupported_exe():
    h = Host({"log": "1"}, exe_ok=False)
    h.e.uc.mem_unmap(0x541000, 0x1000)  # the marker is never read for another exe
    log = h.install()
    assert h.patched() == []
    assert log[-1] == "unsupported IGI.exe, nothing installed"
    assert h.messages == ["Unsupported IGI.exe version. The EU version is required."]


def test_crash_log():
    h = Host({"log": "1"})
    h.install()
    e = h.e
    fn = e.symbols["TerrainMesh_DrawD3D"]
    caller = e.symbols["Hemicube_DrawLightmapSet"]
    stack = e.alloc(0x100)
    h.stack = (stack, 0x100)
    e.write(stack, struct.pack("<4I", 0x1234, caller + 0x21, 0x4D7625, fn + 3))
    record = e.alloc(0x50)
    e.write(record, struct.pack("<5I2I", 0xC0000005, 0, 0, fn + 0x10, 2, 1, 0x18))
    ctx = e.alloc(0x2CC)
    e.write(ctx + 0x1C, struct.pack("<I", 0xC7F))
    e.write(ctx + 0x9C, struct.pack("<6I", 6, 5, 2, 4, 3, 1))  # edi esi ebx edx ecx eax
    e.write(ctx + 0xB4, struct.pack("<2I", 7, fn + 0x10))
    e.write(ctx + 0xC4, struct.pack("<I", stack))
    ptrs = e.alloc(8)
    e.write(ptrs, struct.pack("<2I", record, ctx))
    before = len(h.log)
    assert e.call(h.handler, ptrs, cc="stdcall")[0] == 0  # EXCEPTION_CONTINUE_SEARCH
    assert e.popped == 4
    lines = h.log[before:].decode("latin1").splitlines()
    dll = lambda a: "dinput.dll+0x%X" % (a - 0x10000000)
    assert lines == [
        "exception C0000005 at TerrainMesh_DrawD3D+0x10 (%s)" % dll(fn + 0x10),
        "  writing 0x00000018",
        "  eax 00000001 ebx 00000002 ecx 00000003 edx 00000004 esi 00000005 edi 00000006 ebp 00000007 "
        "esp %08X fpu cw C7F" % stack,
        "  stack %08X: Hemicube_DrawLightmapSet+0x21 (%s)" % (stack + 4, dll(caller + 0x21)),
        "  stack %08X: TerrainMesh_DrawD3D+0x3 (%s)" % (stack + 12, dll(fn + 3)),
    ]
    # breakpoints and other non-error codes are left alone
    e.write(record, struct.pack("<I", 0x80000003))
    before = len(h.log)
    e.call(h.handler, ptrs, cc="stdcall")
    assert h.log[before:] == b""
