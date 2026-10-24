import struct

import pytest
from emu import Emu, ErrorShown, compare, compare_hooked, source_hooks, STACK_BASE, STACK_SIZE, TRAP_BASE

# --- fake engine services -------------------------------------------------
#
# Files, resources and error boxes are replaced by Python code. Everything
# they do is logged into emulated memory, so compare() sees the calls, their
# order and the written file contents.

LOG_SIZE = 0x80000


class Fakes:
    def __init__(self, e):
        self.e = e
        self.res = {}
        self.files = {}
        self.fail_open = ()
        self.load_delta = 0
        self.log = e.alloc(LOG_SIZE)
        self.patch(0x4B1510, "File_Open", self.file_open)
        self.patch(0x4B1690, "File_Close", lambda e: self.record(b"CLOS", self.handle_name(e.arg(0))) or e.ret(0))
        self.patch(0x4B1700, "File_Write", self.file_write)
        self.patch(0x4B1AC0, "File_Delete", self.file_delete)
        self.patch(0x4B18B0, "File_Load", self.file_load)
        self.patch(0x4B1A30, "File_Unload", lambda e: self.record(b"UNLD") or e.ret(0))
        self.patch(0x4B5B60, "Resource_Exists", lambda e: e.ret(int(self.cstr(e.arg(0)) in self.res)))
        self.patch(0x4B5F00, "Resource_Load", self.res_load)
        self.patch(0x4B6380, "Resource_Unload", lambda e: self.record(b"RUNL", e.read_cstr(e.arg(0))) or e.ret(0))
        self.patch(0x4B63D0, "Resource_Flush", lambda e: self.record(b"RFLU", struct.pack("<I", e.arg(0))) or e.ret(0))
        self.patch(0x4BACA0, "Script_UnrefSymbol", lambda e: self.record(b"UREF", struct.pack("<I", e.arg(0))) or e.ret(0))
        self.callback = self.trap("Callback", self.on_callback)
        self.op_error = self.trap("OpError", self.on_op_error)
        e.call(0x4A537B)  # C runtime float formatting, normally set up at startup
        e.baseline = e.snapshot()

    def trap(self, name, fn):
        e = self.e
        addr = TRAP_BASE + len(e.imports) * 4
        e.imports.append(("fake", name))
        e.write(addr, b"\xc3")
        e.stub(name, fn)
        return addr

    def patch(self, addr, name, fn):
        trap = self.trap(name, fn)
        self.e.write(addr, b"\xe9" + struct.pack("<i", trap - addr - 5))

    def cstr(self, p):
        return self.e.read_cstr(p).decode("latin-1")

    def record(self, tag, data=b""):
        e = self.e
        used = e.u32(self.log)
        rec = tag + struct.pack("<I", len(data)) + data
        assert used + len(rec) < LOG_SIZE - 4
        e.write(self.log + 4 + used, rec)
        e.write(self.log, struct.pack("<I", used + len(rec)))

    def records(self):
        e = self.e
        data = e.read(self.log + 4, e.u32(self.log))
        out = []
        while data:
            n = struct.unpack("<I", data[4:8])[0]
            out.append((data[:4], data[8:8 + n]))
            data = data[8 + n:]
        return out

    def reset(self):
        self.files = {}
        self.fail_open = ()
        self.load_delta = 0

    def handle_name(self, h):
        return next((n.encode() for n, v in self.files.items() if v == h), b"?")

    def file_open(self, e):
        name = self.cstr(e.arg(0))
        if any(s in name for s in self.fail_open):
            self.record(b"OPNF", name.encode())
            return e.ret(0)
        h = e.alloc(0x10000)
        self.files[name] = h
        self.record(b"OPEN", name.encode())
        e.ret(h)

    def file_write(self, e):
        f, buf, n = e.arg(0), e.arg(1), e.arg(2)
        used = e.u32(f)
        e.write(f + 4 + used, e.read(buf, n))
        e.write(f, struct.pack("<I", used + n))
        e.ret(0)

    def file_delete(self, e):
        name = self.cstr(e.arg(0))
        self.record(b"DELE", name.encode())
        self.files.pop(name, None)
        e.ret(0)

    def file_load(self, e):
        name = self.cstr(e.arg(0))
        h = self.files.get(name)
        self.record(b"LOAD", name.encode())
        if not h:
            return e.ret(0)
        e.write(e.arg(1), struct.pack("<i", e.u32(h) + self.load_delta))
        e.ret(h + 4)

    def file_data(self, name):
        h = self.files[name]
        return self.e.read(h + 4, self.e.u32(h))

    def add_res(self, name, data):
        p = self.e.alloc(len(data) + 1)
        self.e.write(p, data + b"\0")
        self.res[name] = (p, len(data))
        return p

    def res_load(self, e):
        name = self.cstr(e.arg(0))
        self.record(b"RLOA", name.encode())
        if name not in self.res:
            return e.ret(0)
        p, n = self.res[name]
        if e.arg(1):
            e.write(e.arg(1), struct.pack("<I", n))
        e.ret(p)

    def on_callback(self, e):
        self.record(b"CALB", e.read_cstr(e.arg(0)) + struct.pack("<I", e.arg(1)))
        e.ret(0)

    def on_op_error(self, e):
        # an opcode handler that fails with the error code in its operand
        vm = e.arg(0)
        code, pc = e.u32(vm), e.u32(vm + 0x14)
        err = e.read(code + pc + 1, 1)[0]
        e.write(vm + 0x20, struct.pack("<I", err))
        e.write(vm + 0x10, struct.pack("<I", pc))
        e.ret(0)


def compare_error(e, orig, ours, setup=None, args=()):
    """Both versions must end in Error_Show with the same text and memory writes.
    The stack frame is abandoned there, so pointers into it only need to agree on being
    stack pointers: lists headed on the stack still link heap nodes to it."""
    results = []
    for target in (orig, ours):
        e.reset()
        a = (setup(e) or args) if setup else args
        before = e.snapshot()
        with pytest.raises(ErrorShown) as info:
            e.call(target, *a)
        words = {}
        for w in sorted({k & ~3 for k in e.changes(before)}):
            v = e.u32(w)
            words[w] = "stack" if STACK_BASE <= v < STACK_BASE + STACK_SIZE else v
        results.append((str(info.value), words))
    assert results[0] == results[1]
    return results[0][0]


emu = Emu()
fakes = Fakes(emu)


def cstr(e, s):
    if isinstance(s, str):
        s = s.encode()
    p = e.alloc(len(s) + 1)
    e.write(p, s + b"\0")
    return p


def u32(e, p):
    return e.u32(p)


def f32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


# --- script symbols -------------------------------------------------------

def script_setup(e):
    e.call(0x4B0BA0)
    buckets = e.alloc(12 * 31)
    for i in range(31):
        b = buckets + 12 * i
        e.write(b, struct.pack("<III", b + 4, 0, b))
    t = e.alloc(8)
    e.write(t, struct.pack("<II", buckets, 31))
    e.write(0xA43E88, struct.pack("<I", t))


INT_HANDLER = 0x4B7EA0


def add_var(e, name, kind=0, type_=1, size=4, value=b"", handler=INT_HANDLER, data=None):
    if data is None:
        data = e.alloc(max(size, 8, len(value)))
        e.write(data, value)
    e.call(0x4BAAC0, cstr(e, name), handler, data, size, kind, type_, 1)
    return data


def add_vars(e):
    """A set of script symbols, returns their data pointers by name."""
    script_setup(e)
    v = {}
    v["i"] = add_var(e, "i", value=struct.pack("<i", -1234))
    v["c"] = add_var(e, "c", size=1, value=b"\x7f")
    v["w"] = add_var(e, "w", size=2, value=b"\x01\x80")
    v["r"] = add_var(e, "r", type_=2, size=8, value=struct.pack("<d", 2.5), handler=0x4B7FE0)
    v["r2"] = add_var(e, "r2", type_=2, size=8, value=struct.pack("<d", -3.5), handler=0x4B7FE0)
    v["s"] = add_var(e, "s", type_=3, size=8, value=b"hello")
    v["k"] = add_var(e, "k", kind=2, value=struct.pack("<i", 77))
    e.call(0x4B8810, cstr(e, "K2"), 99)
    e.call(0x4B8890, cstr(e, "_Script_SetWarningLevel"), 0x4B8C60, 0, 1)
    e.call(0x4B8890, cstr(e, "BreakScript"), 0x4B8C10, 0, 0)
    return v


# --- programs -------------------------------------------------------------

def qvm_file(code, syms=(), strs=(), stack=0, types=0, unk08=5, version=8, magic=b"LOOP"):
    def table(items):
        offs, text = b"", b""
        for s in items:
            offs += struct.pack("<I", len(text))
            text += s + b"\0"
        return offs, text
    so, st = table(syms)
    ro, rt = table(strs)
    sym_off = 0x3C
    sym_text = sym_off + len(so)
    str_off = sym_text + len(st)
    str_text = str_off + len(ro)
    code_off = str_text + len(rt)
    hdr = magic + struct.pack("<14I", version, unk08, sym_off, sym_text, len(so), len(st), str_off, str_text,
                              len(ro), len(rt), code_off, len(code), stack, types)
    return hdr + so + st + ro + rt + code


def op(n, fmt="", *args):
    return bytes([n]) + struct.pack("<" + fmt, *args)


def push(v):
    return op(2, "i", v)


def pushf(v):
    return op(5, "f", v)


def pushs(s):
    return bytes([7]) + s + b"\0"


def pushi(s):
    return bytes([11]) + s + b"\0"


BRK, NOP, POP, ASSIGN, ADD = b"\0", b"\1", b"\x12", b"\x2a", b"\x19"


def handlers_setup(e):
    e.call(0x4BD430)
    e.call(0x4C0900)
    e.call(0x4C10E0)


def load_vm(e, code, stack=0x100, types=0x40, name="test.qvm", **kw):
    fakes.add_res(name, qvm_file(code, **kw))
    vm = e.call(0x4BD220, cstr(e, name), stack, types)[0]
    assert vm
    return vm


def vm_with(e, values, code=BRK, **kw):
    """A program whose stack holds values: ints, floats, (type, bytes) strings."""
    handlers_setup(e)
    vm = load_vm(e, code, **kw)
    for v in values:
        if isinstance(v, int):
            e.call(0x4BDA00, vm, v)
        elif isinstance(v, float):
            e.call(0x4BDA30, vm, f32(v))
        else:
            e.call(0x4BCE40, vm, cstr(e, v[1]), v[0])
    return vm


def setup_with(values, code=BRK, vars=True, **kw):
    def setup(e):
        fakes.reset()
        if vars:
            add_vars(e)
        return (vm_with(e, values, code, **kw),)
    return setup


# --- opcode table ---------------------------------------------------------

def test_qvm_set_handler():
    compare(emu, 0x4BD3D0, "Qvm_SetHandler", args=(7, 3, 0x12345678), check_eax=False)


@pytest.mark.parametrize("opc,slot", [(0, 0), (48, 1), (49, 0), (-1, 0), (3, 4), (3, -1), (3, 2), (0, 2), (7, 3)])
def test_qvm_call_handler(opc, slot):
    def setup(e):
        fakes.reset()
        e.call(0x4BD430)
        e.call(0x4BD3D0, 0, 2, 0)
        e.call(0x4BD3D0, 7, 3, 0)
        e.call(0x4BD3D0, 3, 2, fakes.callback)
        return opc, slot, cstr(e, "msg"), 0x55
    compare(emu, 0x4BD3F0, "Qvm_CallHandler", setup=setup, check_eax=False)


def test_qvm_call_handler_not_implemented():
    def setup(e):
        e.call(0x4BD430)
        return 5, 3, 0, 0
    assert compare_error(emu, 0x4BD3F0, "Qvm_CallHandler", setup=setup) == "Instruction not implemented."


def test_qvm_call_handler_name():
    def setup(e):
        e.call(0x4BD430)
        return 17, 1, e.alloc(16), 0
    compare(emu, 0x4BD3F0, "Qvm_CallHandler", setup=setup, check_eax=False)


def test_qvm_init():
    compare(emu, 0x4BD430, "Qvm_Init", check_eax=False)


def test_qvm_not_implemented():
    assert compare_error(emu, 0x4BC950, "Qvm_OpNotImplemented") == "Instruction not implemented."


# --- loading --------------------------------------------------------------

PROGRAM = push(1) + push(2) + ADD + POP + BRK


@pytest.mark.parametrize("stack,types", [(0, 0), (0x40, 0x10), (13, 0), (0, 7)])
@pytest.mark.parametrize("hdr", [dict(), dict(stack=0x200, types=0x20), dict(syms=[b"a", b"Task_New"]),
                                 dict(strs=[b"x", b"yy"], syms=[b"z"])])
def test_qvm_load(stack, types, hdr):
    def setup(e):
        fakes.reset()
        fakes.add_res("p.qvm", qvm_file(PROGRAM, **hdr))
        return cstr(e, "p.qvm"), stack, types
    compare(emu, 0x4BD220, "Qvm_Load", setup=setup)


@pytest.mark.parametrize("data", [
    qvm_file(PROGRAM, magic=b"LOOQ"), qvm_file(PROGRAM, magic=b"XOOP"), qvm_file(PROGRAM, magic=b"LXOP"),
    qvm_file(PROGRAM, magic=b"LOXP"), qvm_file(PROGRAM, version=7), qvm_file(PROGRAM, unk08=4),
    qvm_file(PROGRAM, unk08=6), qvm_file(b"")[:0x3B], None])
def test_qvm_load_bad(data):
    def setup(e):
        fakes.reset()
        if data is not None:
            fakes.add_res("p.qvm", data)
        return cstr(e, "p.qvm"), 0, 0
    compare(emu, 0x4BD220, "Qvm_Load", setup=setup)


def test_qvm_load_game_file():
    data = open(emu_exe_dir() + "/config.qvm", "rb").read()

    def setup(e):
        fakes.reset()
        fakes.add_res("config.qvm", data)
        return cstr(e, "config.qvm"), 0, 0
    compare(emu, 0x4BD220, "Qvm_Load", setup=setup)


def emu_exe_dir():
    import os
    from emu import GAME_EXE
    return os.path.dirname(GAME_EXE)


def test_qvm_free():
    def setup(e):
        fakes.reset()
        vm = load_vm(e, PROGRAM, syms=[b"a", b"b", b"c"])
        refs = u32(e, vm + 0x64)
        e.write(refs, struct.pack("<III", 0x1111, 0, 0x3333))
        return (vm,)
    compare(emu, 0x4BD380, "Qvm_Free", setup=setup, check_eax=False)


# --- stack ----------------------------------------------------------------

@pytest.mark.parametrize("v", [0, 1, -1, 0x7FFFFFFF])
def test_qvm_push_int(v):
    compare(emu, 0x4BDA00, "Qvm_PushInt", setup=lambda e: (vm_with(e, [5]), v), check_eax=False)


@pytest.mark.parametrize("bits", [f32(1.5), f32(-0.0), 0x7FC00000, 0x7F800001, 0xFF800000])
def test_qvm_push_float(bits):
    compare(emu, 0x4BDA30, "Qvm_PushFloat", setup=lambda e: (vm_with(e, [5]), bits), check_eax=False)


@pytest.mark.parametrize("s", [b"", b"a", b"abc", b"abcd", b"abcdefgh", b"Task_DeclareParameters"])
@pytest.mark.parametrize("t", [3, 4])
def test_qvm_push_string(s, t):
    compare(emu, 0x4BCE40, "Qvm_PushString", setup=lambda e: (vm_with(e, [1]), cstr(e, s), t), check_eax=False)


STACKS = [[7], [1.25], [(3, b"text")], [(4, b"i")], [(7, b"abcdefgh")], [3, (3, b"xyz")]]


@pytest.mark.parametrize("values", STACKS)
@pytest.mark.parametrize("outs", [(1, 1, 1), (0, 0, 0), (1, 0, 1)])
def test_qvm_pop(values, outs):
    def setup(e):
        vm = vm_with(e, values)
        return (vm,) + tuple(e.alloc(4) if o else 0 for o in outs)
    compare(emu, 0x4BCCC0, "Qvm_Pop", setup=setup)


@pytest.mark.parametrize("t", [5, 6])
def test_qvm_pop_word_types(t):
    def setup(e):
        vm = vm_with(e, [9])
        e.write(u32(e, vm + 0x28) + 4 * u32(e, vm + 0x1C), struct.pack("<I", t))
        return vm, e.alloc(4), e.alloc(4), e.alloc(4)
    compare(emu, 0x4BCCC0, "Qvm_Pop", setup=setup)


@pytest.mark.parametrize("t", [0, 8])
def test_qvm_pop_bad(t):
    def setup(e):
        fakes.reset()
        vm = vm_with(e, [9])
        e.write(u32(e, vm + 0x28) + 4 * u32(e, vm + 0x1C), struct.pack("<I", t))
        return vm, 0, 0, 0
    assert compare_error(emu, 0x4BCCC0, "Qvm_Pop", setup=setup) == "Unknown data type."


def test_qvm_save_restore():
    compare(emu, 0x4BD9C0, "Qvm_SaveStack", setup=lambda e: (vm_with(e, [1, 2]),), check_eax=False)

    def setup(e):
        vm = vm_with(e, [1, 2])
        e.call(0x4BD9C0, vm)
        e.call(0x4BDA00, vm, 3)
        return (vm,)
    compare(emu, 0x4BD9E0, "Qvm_RestoreStack", setup=setup, check_eax=False)


@pytest.mark.parametrize("values", [[(6, b"")], [(4, b"i")], [(4, b"BreakScript")], [(4, b"nothere")], [5], [(3, b"i")]])
def test_qvm_pop_symbol(values):
    def setup(e):
        add_vars(e)
        vm = vm_with(e, [])
        for v in values:
            if v == (6, b""):
                sym = e.alloc(4)
                e.call(0x4BAB80, cstr(e, "i"), sym)
                e.call(0x4BEC10, vm, u32(e, sym))
            elif isinstance(v, int):
                e.call(0x4BDA00, vm, v)
            else:
                e.call(0x4BCE40, vm, cstr(e, v[1]), v[0])
        return vm, e.alloc(4)
    compare(emu, 0x4BCD90, "Qvm_PopSymbol", setup=setup)


@pytest.mark.parametrize("values,t", [([(3, b"str")], 3), ([(4, b"s")], 3), ([(4, b"i")], 3), ([(4, b"k")], 3),
                                      ([(4, b"BreakScript")], 3), ([(4, b"nope")], 3), ([7], 3), ([(4, b"name")], 4),
                                      ([(3, b"")], 3)])
def test_qvm_pop_string(values, t):
    def setup(e):
        vm = setup_with(values)(e)[0]
        return vm, e.alloc(4), e.alloc(4), t
    compare(emu, 0x4BCC00, "Qvm_PopString", setup=setup)


@pytest.mark.parametrize("values", [[5], [-7], [2.5], [3.5], [-2.5], [1e10], [float("nan")], [(4, b"i")], [(4, b"r")],
                                    [(4, b"r2")], [(4, b"s")], [(4, b"k")], [(4, b"K2")], [(4, b"BreakScript")],
                                    [(4, b"missing")], [(3, b"str")]])
def test_qvm_pop_number(values):
    def setup(e):
        vm = setup_with(values)(e)[0]
        return vm, e.alloc(4), e.alloc(4)
    compare(emu, 0x4BCEC0, "Qvm_PopNumber", setup=setup)


def test_qvm_dump_stack():
    values = [1, 2.5, (3, b"str"), (4, b"name"), (7, b"datablock")]
    compare(emu, 0x4BC960, "Qvm_DumpStack", setup=setup_with(values), check_eax=False)

    def setup(e):
        vm = vm_with(e, [1, 2])
        e.write(u32(e, vm + 0x28) + 4 * u32(e, vm + 0x1C), struct.pack("<I", 5))
        return (vm,)
    compare(emu, 0x4BC960, "Qvm_DumpStack", setup=setup, check_eax=False)
    compare(emu, 0x4BC960, "Qvm_DumpStack", setup=setup_with([]), check_eax=False)


# --- running --------------------------------------------------------------

def run_setup(code, values=(), **kw):
    def setup(e):
        fakes.reset()
        add_vars(e)
        handlers_setup(e)
        vm = load_vm(e, code, **kw)
        e.call(0x4BD3D0, 47, 0, fakes.op_error)
        for v in values:
            e.call(0x4BDA00, vm, v)
        return (vm,)
    return setup


def call(name, *args_code):
    """PUSHI name; CALL with each argument's code, which ends in BRK."""
    head = pushi(name)
    start = len(head) + 5 + 4 * len(args_code) + 5
    pcs, body = [], b""
    for a in args_code:
        pcs.append(start + len(body))
        body += a + BRK
    end = start + len(body)
    return head + op(24, "i" + "i" * len(pcs), len(pcs), *pcs) + op(20, "i", end - (len(head) + 5 + 4 * len(pcs))) + body


PROGRAMS = [
    push(1) + push(2) + ADD + POP + BRK,
    pushi(b"i") + push(41) + ASSIGN + POP + BRK,
    pushi(b"r") + pushf(1.5) + ASSIGN + POP + BRK,
    pushi(b"s") + pushs(b"new") + ASSIGN + POP + BRK,
    pushi(b"k") + push(1) + ASSIGN + BRK,
    call(b"_Script_SetWarningLevel", push(2)),
    call(b"_Script_SetWarningLevel", pushi(b"i")),
    call(b"_Script_SetWarningLevel", pushf(3.7)),
    call(b"BreakScript") + BRK,
    pushi(b"undefined") + POP + BRK,
    push(0) + op(21, "i", 10) + push(5) + BRK + push(6) + BRK,
    op(20, "i", 6) + NOP + BRK + NOP + BRK,
]


@pytest.mark.parametrize("code", PROGRAMS)
def test_qvm_run(code):
    compare(emu, 0x4BD080, "Qvm_Run", setup=run_setup(code))


@pytest.mark.parametrize("err", list(range(1, 17)))
def test_qvm_run_errors(err):
    compare(emu, 0x4BD080, "Qvm_Run", setup=run_setup(push(7) + bytes([47, err]) + BRK, syms=[b"x"]))


def test_qvm_run_args_overflow():
    compare(emu, 0x4BD080, "Qvm_Run", setup=run_setup(bytes([47, 13]) + struct.pack("<i", 99) + BRK))


def test_qvm_execute():
    def setup(e):
        vm = run_setup(push(1) + push(2) + BRK + push(3) + BRK)(e)[0]
        e.write(vm + 0x14, struct.pack("<I", 5))
        return (vm,)
    compare(emu, 0x4BCFD0, "Qvm_Execute", setup=setup, check_eax=False)

    def setup_err(e):
        vm = run_setup(push(1) + BRK)(e)[0]
        e.write(vm + 0x20, struct.pack("<I", 3))
        return (vm,)
    compare(emu, 0x4BCFD0, "Qvm_Execute", setup=setup_err, check_eax=False)


# --- opcodes --------------------------------------------------------------

def at_pc(code, pc, values=(), strings=()):
    def setup(e):
        vm = run_setup(code, values)(e)[0]
        for t, s in strings:
            e.call(0x4BCE40, vm, cstr(e, s), t)
        e.write(vm + 0x14, struct.pack("<I", pc))
        return (vm,)
    return setup


@pytest.mark.parametrize("code,values,strings", [
    (op(20, "i", 100), [], []),
    (op(20, "i", -5), [], []),
    (op(21, "i", 30), [0], []), (op(21, "i", 30), [1], []),
    (op(22, "i", 30), [0], []), (op(22, "i", 30), [-3], []),
    (op(21, "i", 30), [], [(4, b"r")]), (op(22, "i", 30), [], [(4, b"r")]),
    (op(21, "i", 30), [], [(4, b"s")]), (op(22, "i", 30), [], [(3, b"s")]),
    (op(21, "i", 30), [], [(4, b"zz")]),
    (op(19, "i", 30), [], []),
])
def test_qvm_op_branch(code, values, strings):
    compare(emu, 0x4BD730, "Qvm_OpBranch", setup=at_pc(code + BRK, 0, values, strings), check_eax=False)


@pytest.mark.parametrize("bits", [f32(0.0), f32(-0.0), f32(0.5), 0x7FC00000, 0xFFC00000, 0x7F800000])
@pytest.mark.parametrize("opc", [21, 22])
def test_qvm_op_branch_float(bits, opc):
    def setup(e):
        vm = at_pc(op(opc, "i", 30) + BRK, 0)(e)[0]
        e.call(0x4BDA30, vm, bits)
        return (vm,)
    compare(emu, 0x4BD730, "Qvm_OpBranch", setup=setup, check_eax=False)


@pytest.mark.parametrize("values,strings", [([4], []), ([], [(3, b"abc")])])
def test_qvm_op_pop(values, strings):
    compare(emu, 0x4BD860, "Qvm_OpPop", setup=at_pc(POP + BRK, 0, values, strings), check_eax=False)


@pytest.mark.parametrize("code", [
    call(b"_Script_SetWarningLevel", push(3)),
    call(b"_Script_SetWarningLevel", pushs(b"x")),
    call(b"BreakScript"),
    call(b"i"),
    call(b"nothere"),
])
def test_qvm_op_call(code):
    name_len = code[1:].index(b"\0") + 2
    compare(emu, 0x4BD880, "Qvm_OpCall", setup=at_pc(code + BRK, name_len, [],
                                                       [(4, code[1:name_len - 1])]), check_eax=False)


def test_qvm_op_call_empty_stack():
    def setup(e):
        vm = at_pc(op(24, "i", 0) + BRK, 0, [9])(e)[0]
        return (vm,)
    compare(emu, 0x4BD880, "Qvm_OpCall", setup=setup, check_eax=False)


ASSIGNS = [
    (b"i", [5], []), (b"i", [], [(3, b"s")]), (b"c", [0x1FF], []), (b"w", [0x18001], []), (b"i", 2.5, []),
    (b"i", 3.5, []), (b"r", [7], []), (b"r", 1.25, []), (b"r", [], [(3, b"s")]),
    (b"s", [], [(3, b"a long string value")]), (b"s", [], [(3, b"abc")]), (b"s", [1], []),
    (b"k", [1], []), (b"BreakScript", [1], []), (b"nope", [1], []), (b"i", [], [(4, b"BreakScript")]),
    (b"i", [], [(4, b"r")]), (b"i", [], [(4, b"nope")]),
]


@pytest.mark.parametrize("target,values,strings", ASSIGNS)
def test_qvm_op_assign(target, values, strings):
    def setup(e):
        vm = at_pc(ASSIGN + BRK, 0, [], [(4, target)])(e)[0]
        if isinstance(values, float):
            e.call(0x4BDA30, vm, f32(values))
        else:
            for v in values:
                e.call(0x4BDA00, vm, v)
        for t, s in strings:
            e.call(0x4BCE40, vm, cstr(e, s), t)
        return (vm,)
    compare(emu, 0x4BDA60, "Qvm_OpAssign", setup=setup, check_eax=False)


def test_qvm_op_assign_symbol_target():
    def setup(e):
        vm = at_pc(ASSIGN + BRK, 0)(e)[0]
        sym = e.alloc(4)
        e.call(0x4BAB80, cstr(e, "i"), sym)
        e.call(0x4BEC10, vm, u32(e, sym))
        e.call(0x4BDA00, vm, 12)
        return (vm,)
    compare(emu, 0x4BDA60, "Qvm_OpAssign", setup=setup, check_eax=False)


# --- script arguments -----------------------------------------------------

def args_setup(arg_codes, t=None):
    """A ScriptArgs as OpCall makes it, for code with one argument block each."""
    def setup(e):
        code = b""
        pcs = []
        for a in arg_codes:
            pcs.append(len(code))
            code += a + BRK
        vm = run_setup(code + BRK)(e)[0]
        e.write(vm + 0x14, struct.pack("<I", len(code)))
        pc_arr = e.alloc(4 * len(pcs) + 4)
        e.write(pc_arr, struct.pack("<%dI" % len(pcs), *pcs))
        args = e.alloc(20)
        e.write(args, struct.pack("<IIIII", len(pcs), 1, 0, vm, pc_arr))
        return args
    return setup


ARG_CODES = [push(42), pushf(2.5), pushf(-1.5), pushs(b"text"), pushi(b"i"), pushi(b"r"), pushi(b"s"),
             pushi(b"BreakScript"), pushi(b"nope"), b"", push(1) + push(2), push(3) + POP]


@pytest.mark.parametrize("code", ARG_CODES)
def test_qvm_get_int_arg(code):
    compare(emu, 0x4BCAB0, "Qvm_GetIntArg", setup=lambda e: (args_setup([NOP, code])(e), 1))


@pytest.mark.parametrize("code", ARG_CODES)
def test_qvm_get_real_arg(code):
    compare(emu, 0x4BD010, "Qvm_GetRealArg", setup=lambda e: (args_setup([code])(e), 0), check_eax=False,
            check_st0=True)


@pytest.mark.parametrize("code", ARG_CODES)
@pytest.mark.parametrize("size", [1, 3, 64])
def test_qvm_get_string_arg(code, size):
    if code in (pushi(b"i"), pushi(b"r")) and size > 1:
        pytest.skip("a number variable leaves the string pointer uninitialised")
    compare(emu, 0x4BD040, "Qvm_GetStringArg", setup=lambda e: (args_setup([code])(e), 0, e.alloc(64), size))


def test_qvm_get_arg_bad_type():
    def setup(e):
        args = args_setup([push(1)])(e)
        return e.alloc(4), 4, args, 0, 4
    assert compare_error(emu, 0x4BCAE0, "Qvm_GetArg", setup=setup) == "Unknown data type."


@pytest.mark.parametrize("t", [1, 2, 3])
def test_qvm_get_arg_error(t):
    def setup(e):
        args = args_setup([push(1) + bytes([47, 3])])(e)
        return e.alloc(8), 8, args, 0, t
    compare(emu, 0x4BCAE0, "Qvm_GetArg", setup=setup)


def test_qvm_return_zero():
    compare(emu, 0x4BD070, "Qvm_GetArgLine")


# --- whole programs with every hook installed ------------------------------

QVM_HOOKS = {a for a, n in source_hooks() if 0x4BB270 <= a < 0x4BDC00}


@pytest.fixture(scope="module")
def plain_hooked():
    plain = Emu()
    plain_f = Fakes(plain)
    hooked = Emu()
    hooked_f = Fakes(hooked)
    hooked.install_hooks(only=QVM_HOOKS)
    return (plain, plain_f), (hooked, hooked_f)


@pytest.mark.parametrize("code", PROGRAMS)
def test_qvm_hooked_run(plain_hooked, code):
    global fakes
    (plain, pf), (hooked, hf) = plain_hooked
    saved = fakes
    try:
        def setup(e):
            global fakes
            fakes = pf if e is plain else hf
            return run_setup(code)(e)
        compare_hooked(plain, hooked, 0x4BD080, setup=setup)
    finally:
        fakes = saved
