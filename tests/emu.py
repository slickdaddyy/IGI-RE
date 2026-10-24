"""Run original IGI.exe code and our rewritten code side by side in an x86 emulator."""
import os
import re
import struct
import subprocess

import pefile
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_UNMAPPED, UC_PROT_ALL
from unicorn.x86_const import *

STACK_BASE, STACK_SIZE = 0x00100000, 0x00100000
HEAP_BASE, HEAP_SIZE = 0x20000000, 0x04000000
TRAP_BASE, TRAP_SIZE = 0x70000000, 0x00010000
FAKE_BASE, FAKE_SIZE = 0x30000000, 0x00100000
CALL_LOG = FAKE_BASE + 0x80000
RETURN_MAGIC = TRAP_BASE + TRAP_SIZE - 0x10

GAME_EXE = os.environ.get("IGI_EXE", "")
OUR_DLL = os.environ.get("IGI_DLL", os.path.join(os.path.dirname(__file__), "..", "build", "dinput.dll"))


def _memcpy(e):
    e.write(e.arg(0), e.read(e.arg(1), e.arg(2)))
    e.ret(e.arg(0))


def _strcmp(e):
    x, y = e.read_cstr(e.arg(0)), e.read_cstr(e.arg(1))
    e.ret((x > y) - (x < y))


DEFAULT_STUBS = {
    "HeapAlloc": lambda e: e.ret(e.alloc(e.arg(2)), 12),
    "HeapFree": lambda e: e.ret(1, 12),
    "GetProcessHeap": lambda e: e.ret(1),
    "VirtualAlloc": lambda e: e.ret(e.alloc(e.arg(1)), 16),
    "malloc": lambda e: e.ret(e.alloc(e.arg(0))),
    "calloc": lambda e: e.ret(e.alloc(e.arg(0) * e.arg(1))),
    "free": lambda e: e.ret(0),
    "memcpy": _memcpy,
    "memmove": _memcpy,
    "memset": lambda e: (e.write(e.arg(0), bytes([e.arg(1) & 0xFF]) * e.arg(2)), e.ret(e.arg(0))),
    "strlen": lambda e: e.ret(len(e.read_cstr(e.arg(0)))),
    "strcpy": lambda e: (e.write(e.arg(0), e.read_cstr(e.arg(1)) + b"\0"), e.ret(e.arg(0))),
    "strcmp": _strcmp,
}


def _built(root):
    dirs = [d for d in os.environ.get("IGI_DIRS", "").split(";") if d]
    rel = os.path.relpath(root, os.path.join(os.path.dirname(__file__), "..", "src"))
    return not dirs or rel == "." or rel.split(os.sep)[0] in dirs


def source_hooks():
    """(address, function name) for every HOOK() line in src."""
    out = []
    for root, _, files in os.walk(os.path.join(os.path.dirname(__file__), "..", "src")):
        for f in files:
            if f.endswith(".c"):
                text = open(os.path.join(root, f)).read()
                if f"src{os.sep}" in root and not _built(root):
                    continue
                out += [(int(a, 16), n) for a, n in re.findall(r"^HOOK\((0x[0-9A-Fa-f]+), (\w+)\);", text, re.M)]
    return out


class ImportCalled(Exception):
    pass


class ErrorShown(Exception):
    """The game called Error_Show, which never returns."""


ERROR_SHOW = 0x4AF7B0


def align(n, a=0x1000):
    return (n + a - 1) & ~(a - 1)


class Emu:
    cw = None  # x87 control word for call(); None keeps fninit's 0x37F

    def __init__(self, exe=GAME_EXE, dll=OUR_DLL):
        if not exe or not os.path.exists(exe):
            raise RuntimeError("set IGI_EXE to your EU IGI.exe")
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.imports = []
        self.heap_top = HEAP_BASE
        self.writable = []
        self.stubs = dict(DEFAULT_STUBS)
        self.uc.mem_map(STACK_BASE, STACK_SIZE, UC_PROT_ALL)
        self.uc.mem_map(HEAP_BASE, HEAP_SIZE, UC_PROT_ALL)
        self.uc.mem_map(TRAP_BASE, TRAP_SIZE, UC_PROT_ALL)
        self.uc.mem_map(FAKE_BASE, FAKE_SIZE, UC_PROT_ALL)
        self.fake_top = FAKE_BASE + 0x10
        self.fake_ret_at = {}
        self.images = []
        self.writable += [(HEAP_BASE, HEAP_SIZE), (CALL_LOG, FAKE_BASE + FAKE_SIZE - CALL_LOG)]
        self._load(exe)
        self.symbols = {}
        if dll and os.path.exists(dll):
            self._load(dll)
            self.symbols = self._dll_symbols(dll)
        self.uc.hook_add(UC_HOOK_CODE, self._trap, begin=TRAP_BASE, end=TRAP_BASE + TRAP_SIZE - 1)
        self.uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._unmapped)
        self.uc.hook_add(UC_HOOK_CODE, self._error_show, begin=ERROR_SHOW, end=ERROR_SHOW)
        self.baseline = self.snapshot()

    def _load(self, path):
        pe = pefile.PE(path)
        base = pe.OPTIONAL_HEADER.ImageBase
        size = align(pe.OPTIONAL_HEADER.SizeOfImage)
        self.images.append((base, base + size))
        self.uc.mem_map(base, size, UC_PROT_ALL)
        self.uc.mem_write(base, pe.header[: pe.OPTIONAL_HEADER.SizeOfHeaders])
        for s in pe.sections:
            data = s.get_data()[: max(s.Misc_VirtualSize, s.SizeOfRawData)]
            self.uc.mem_write(base + s.VirtualAddress, data)
            if s.Characteristics & 0x80000000:
                self.writable.append((base + s.VirtualAddress, align(max(s.Misc_VirtualSize, s.SizeOfRawData))))
        for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
            dll = entry.dll.decode().lower()
            for imp in entry.imports:
                name = imp.name.decode() if imp.name else "#%d" % imp.ordinal
                trap = TRAP_BASE + len(self.imports) * 4
                self.imports.append((dll, name))
                self.uc.mem_write(imp.address, struct.pack("<I", trap))
                self.uc.mem_write(trap, b"\xc3")

    def _dll_symbols(self, dll):
        out = subprocess.run(["i686-w64-mingw32-nm", dll], capture_output=True, text=True).stdout
        syms = {}
        for line in out.splitlines():
            parts = line.split()
            if len(parts) == 3:
                name = parts[2].lstrip("_").split("@")[0]
                # code wins over same-named ORIG pointers from other files
                if parts[1] in "Tt" or name not in syms:
                    syms[name] = int(parts[0], 16)
        return syms

    # --- imports -----------------------------------------------------------

    def arg(self, i):
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        return struct.unpack("<I", self.uc.mem_read(esp + 4 + 4 * i, 4))[0]

    def ret(self, value, pop=0):
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack("<I", self.uc.mem_read(esp, 4))[0]
        self.uc.reg_write(UC_X86_REG_EAX, value & 0xFFFFFFFF)
        self.uc.reg_write(UC_X86_REG_ESP, esp + 4 + pop)
        self.uc.reg_write(UC_X86_REG_EIP, ret)

    def alloc(self, size):
        p = self.heap_top
        self.heap_top = align(p + max(size, 1), 16)
        self.uc.mem_write(p, b"\0" * size)
        return p

    def _trap(self, uc, addr, size, user):
        if addr == RETURN_MAGIC:
            uc.emu_stop()
            return
        dll, name = self.imports[(addr - TRAP_BASE) // 4]
        stub = self.stubs.get(name)
        if stub:
            stub(self)
        else:
            uc.emu_stop()
            self.import_error = ImportCalled("%s!%s" % (dll, name))

    def _error_show(self, uc, addr, size, user):
        self.import_error = ErrorShown(self.read_cstr(self.arg(0)).decode("latin1"))
        uc.emu_stop()

    def stub(self, name, fn):
        """Emulate an imported function. fn(emu) reads args with emu.arg(i) and returns with emu.ret()."""
        self.stubs[name] = fn

    def _unmapped(self, uc, access, addr, size, value, user):
        self.mem_error = "unmapped access at 0x%x (eip 0x%x)" % (addr, uc.reg_read(UC_X86_REG_EIP))
        return False

    # --- memory ------------------------------------------------------------

    def read_cstr(self, p, limit=4096):
        out = bytearray()
        while len(out) < limit:
            b = self.uc.mem_read(p + len(out), 1)[0]
            if b == 0:
                break
            out.append(b)
        return bytes(out)

    def read(self, p, n):
        return bytes(self.uc.mem_read(p, n))

    def u32(self, p):
        return struct.unpack("<I", self.read(p, 4))[0]

    def write(self, p, data):
        self.uc.mem_write(p, data)

    def snapshot(self):
        regions = [(b, bytes(self.uc.mem_read(b, n))) for b, n in self.writable + [(STACK_BASE, STACK_SIZE)]]
        return regions, self.heap_top

    def restore(self, snap):
        regions, self.heap_top = snap
        for b, data in regions:
            self.uc.mem_write(b, data)

    def reset(self):
        self.restore(self.baseline)

    # --- calls -------------------------------------------------------------

    def call(self, fn, *args, ecx=0, edx=0, cc="cdecl", stack_fill=0xCC, limit=50_000_000):
        """Call fn (address or our symbol name). Returns (eax, edx, st0)."""
        addr = self.symbols[fn] if isinstance(fn, str) else fn
        self.import_error = self.mem_error = None
        esp = STACK_BASE + STACK_SIZE - 0x1000
        self.uc.mem_write(STACK_BASE, bytes([stack_fill]) * (esp - STACK_BASE))
        for v in reversed(args):
            esp -= 4
            self.uc.mem_write(esp, struct.pack("<I", v & 0xFFFFFFFF))
        esp -= 4
        self.uc.mem_write(esp, struct.pack("<I", RETURN_MAGIC))
        self.uc.mem_write(RETURN_MAGIC, b"\xc3")
        for reg, v in ((UC_X86_REG_ESP, esp), (UC_X86_REG_EBP, 0), (UC_X86_REG_ECX, ecx), (UC_X86_REG_EDX, edx), (UC_X86_REG_EAX, 0)):
            self.uc.reg_write(reg, v)
        # start every call with an empty x87 stack: fninit; [fldcw self.cw;] jmp addr
        code = b"\xdb\xe3"
        if self.cw is not None:
            code += b"\xd9\x2d" + struct.pack("<I", FAKE_BASE + 14)
        code += b"\xe9" + struct.pack("<i", addr - (FAKE_BASE + len(code) + 5))
        self.uc.mem_write(FAKE_BASE, code.ljust(14, b"\x90") + struct.pack("<H", self.cw or 0))
        self.uc.ctl_remove_cache(FAKE_BASE, FAKE_BASE + 16)
        try:
            self.uc.emu_start(FAKE_BASE, RETURN_MAGIC, count=limit)
        except UcError as e:
            raise RuntimeError(self.mem_error or "emulation error %s at eip 0x%x" % (e, self.uc.reg_read(UC_X86_REG_EIP)))
        if self.import_error:
            raise self.import_error
        if self.uc.reg_read(UC_X86_REG_EIP) != RETURN_MAGIC:
            raise RuntimeError("did not return (eip 0x%x)" % self.uc.reg_read(UC_X86_REG_EIP))
        self.popped = self.uc.reg_read(UC_X86_REG_ESP) - esp - 4
        top = (self.uc.reg_read(UC_X86_REG_FPSW) >> 11) & 7
        st0 = self.uc.reg_read(UC_X86_REG_FP0 + top)
        return self.uc.reg_read(UC_X86_REG_EAX), self.uc.reg_read(UC_X86_REG_EDX), st0

    def hook_table(self):
        # skip hooks whose source is newer than the DLL being tested
        return [(addr, self.symbols[fn]) for addr, fn in source_hooks() if fn in self.symbols]

    def install_hooks(self, only=None):
        """Patch jumps to our code into the emulated exe, like the real DLL does."""
        for addr, fn in self.hook_table():
            if only is None or addr in only:
                self.write(addr, b"\xe9" + struct.pack("<i", fn - addr - 5))
                self.uc.ctl_remove_cache(addr, addr + 5)

    def fake(self, addr, ret=0, nargs=0):
        """Replace the original function at addr with a stub that logs (addr, args) and returns ret.
        Lasts for the life of this Emu, so use a separate Emu for tests that fake callees."""
        count, log = CALL_LOG, CALL_LOG + 4
        code = b"\x8b\x0d" + struct.pack("<I", count)
        code += b"\xc7\x04\x8d" + struct.pack("<II", log, addr) + b"\x41"
        for i in range(nargs):
            code += b"\x8b\x44\x24" + bytes([4 + 4 * i]) + b"\x89\x04\x8d" + struct.pack("<I", log) + b"\x41"
        code += b"\x89\x0d" + struct.pack("<I", count) + b"\xb8" + struct.pack("<I", ret & 0xFFFFFFFF) + b"\xc3"
        stub = self.fake_top
        self.fake_top += (len(code) + 15) & ~15
        self.write(stub, code)
        self.fake_ret_at[addr] = stub + len(code) - 5
        self.write(addr, b"\xe9" + struct.pack("<i", stub - addr - 5))
        self.uc.ctl_remove_cache(addr, addr + 5)

    def set_fake_ret(self, addr, value):
        self.write(self.fake_ret_at[addr], struct.pack("<I", value & 0xFFFFFFFF))
        self.uc.ctl_remove_cache(self.fake_ret_at[addr], self.fake_ret_at[addr] + 4)

    def call_log(self):
        n = self.u32(CALL_LOG)
        return list(struct.unpack("<%dI" % n, self.read(CALL_LOG + 4, 4 * n)))

    def normalize(self, v):
        """Make a word comparable between original and rewritten code: our hooked functions
        map to the original address, pointers to C strings become the string."""
        if not hasattr(self, "_orig_of"):
            self._orig_of = {fn: addr for addr, fn in self.hook_table()}
        if v in self._orig_of:
            return self._orig_of[v]
        if any(lo <= v < hi for lo, hi in self.images):
            try:
                text = self.read_cstr(v, 256)
            except Exception:
                return v
            if len(text) >= 2 and all(32 <= c < 127 or c in (9, 10, 13) for c in text):
                return text
        return v

    def words(self, changed):
        """Changed bytes -> {aligned address: normalized word value now in memory}."""
        return {a: self.normalize(self.u32(a)) for a in sorted({k & ~3 for k in changed})}

    def changes(self, before, ignore=()):
        """Bytes in writable memory (not stack) that differ from snapshot `before`."""
        out = {}
        for (b, old), _ in zip(before[0], range(len(self.writable))):
            new = bytes(self.uc.mem_read(b, len(old)))
            if new == old:
                continue
            for page in range(0, len(old), 0x1000):
                if new[page:page + 0x1000] == old[page:page + 0x1000]:
                    continue
                for i in range(page, min(page + 0x1000, len(old))):
                    if old[i] != new[i] and not any(lo <= b + i < hi for lo, hi in ignore):
                        out[b + i] = new[i]
        return out


def compare(emu, orig_addr, ours, args=(), setup=None, ignore=(), check_eax=True, check_st0=False, eax_mask=0xFFFFFFFF,
            normalized=False, **kw):
    """Run original then ours from the same state. setup(emu) may return the call args.
    normalized=True compares memory and the fake call log word by word through emu.normalize."""
    results = []
    for target in (orig_addr, ours):
        emu.reset()
        call_args = args
        if setup:
            call_args = setup(emu) or args
        before = emu.snapshot()
        r = emu.call(target, *call_args, **kw)
        mem = emu.changes(before, ignore)
        if normalized:
            mem = emu.words(mem)
            mem["calls"] = [emu.normalize(v) for v in emu.call_log()]
        results.append((r, mem, emu.popped))
    (ro, mo, po), (rn, mn, pn) = results
    assert po == pn, "stack cleanup differs: orig pops %d, ours %d" % (po, pn)
    assert mo == mn, "memory differs: %s" % diff_text(mo, mn)
    if check_eax:
        assert ro[0] & eax_mask == rn[0] & eax_mask, "eax differs: orig 0x%x ours 0x%x" % (ro[0], rn[0])
    if check_st0:
        assert ro[2] == rn[2], "st0 differs: orig %r ours %r" % (ro[2], rn[2])
    return ro, rn


def diff_text(a, b, limit=16):
    if "calls" in a and a.get("calls") != b.get("calls"):
        return "calls differ:\n orig %s\n ours %s" % (a["calls"], b["calls"])
    keys = sorted(set(a) | set(b), key=str)
    bad = [k for k in keys if a.get(k) != b.get(k)][:limit]
    return ", ".join("%s: %s/%s" % (hex(k) if isinstance(k, int) else k, a.get(k), b.get(k)) for k in bad)


def compare_hooked(plain, hooked, addr, args=(), setup=None, ignore=(), check_eax=True):
    """Run original code on a clean exe and on one patched with our hooks.
    Memory is compared through normalize, so our handler pointers match the originals."""
    results = []
    for e in (plain, hooked):
        e.reset()
        call_args = setup(e) or args if setup else args
        before = e.snapshot()
        r = e.call(addr, *call_args)
        results.append((r[0], e.words(e.changes(before, ignore))))
    (ro, mo), (rn, mn) = results
    assert mo == mn, "memory differs: %s" % diff_text(mo, mn)
    if check_eax:
        assert ro == rn, "eax differs: 0x%x / 0x%x" % (ro, rn)
