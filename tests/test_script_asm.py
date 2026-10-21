import struct

import pytest

from emu import Emu, ErrorShown, TRAP_BASE, compare, diff_text

emu = Emu()
LOG = []


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


def error_handler(e):
    LOG.append((e.read_cstr(e.arg(0)), e.arg(1)))
    e.ret(0)


HANDLER = pyfake(emu, 0, "AsmErrorHandler", error_handler)
TOKENS = []


def read_token(e):
    LOG.append(("read", e.arg(0), e.arg(1)))
    if not TOKENS:
        e.ret(0)
        return
    e.write(e.arg(0) + 0x2C4, u32(TOKENS.pop(0)))
    e.ret(1)


pyfake(emu, 0x4C0780, "AsmLexer_Read", read_token)
emu.baseline = emu.snapshot()


def new_list(e, p):
    e.write(p, u32(p + 4) + u32(0) + u32(p))


def new_asm(e, handler=True):
    e.call(0x4B0BA0)
    a = e.alloc(0x50)
    e.write(a + 0x1C, u32(e.call(0x4C0360, 31)[0]))
    for off in (0x20, 0x2C, 0x38, 0x44):
        new_list(e, a + off)
    e.write(0xA43ED8, u32(HANDLER if handler else 0))
    e.write(0xA43EC0, u32(0x5150))
    for n in (b"start", b"done:"):
        e.call(0x4BAE00, a, cstr(e, n))
    e.write(e.call(0x4BAFA0, a, cstr(e, b"done"))[0] + 0x14, u32(1))
    for s in (b'"one"', b'"two"'):
        e.call(0x4BB030, a, cstr(e, s))
    for s in (b"GOStart", b"IntVar"):
        e.call(0x4BB180, a, cstr(e, s))
    return a


def compare_logged(e, orig, ours, setup, **kw):
    runs = []

    def s(e):
        runs.append(list(LOG))
        r = setup(e)
        LOG.clear()
        return r
    compare(e, orig, ours, setup=s, **kw)
    assert runs[1] == LOG


def same_error(e, orig, ours, setup):
    out = []
    for target in (orig, ours):
        e.reset()
        a = setup(e)
        before = e.snapshot()
        with pytest.raises(ErrorShown) as info:
            e.call(target, *a)
        out.append((str(info.value), e.changes(before)))
    assert out[0] == out[1], diff_text(out[0][1], out[1][1])


def test_add_arg_count_fixup():
    compare(emu, 0x4BADC0, "Asm_AddArgCountFixup", setup=lambda e: (new_asm(e), 0x1234))


@pytest.mark.parametrize("name", [b"new", b"new:", b"start", b"start:", b"done", b"done:", b"", b":", b"a:b"])
def test_define_label(name):
    compare_logged(emu, 0x4BAE00, "Asm_DefineLabel", setup=lambda e: (new_asm(e), cstr(e, name)))


def test_define_label_no_handler():
    same_error(emu, 0x4BAE00, "Asm_DefineLabel", setup=lambda e: (new_asm(e, False), cstr(e, b"done")))


def test_error():
    compare_logged(emu, 0x4BAF30, "Asm_Error", setup=lambda e: (new_asm(e), cstr(e, b"x %d %s"), 5, cstr(e, b"y")),
                   check_eax=False)


def test_error_no_handler():
    same_error(emu, 0x4BAF30, "Asm_Error", setup=lambda e: (new_asm(e, False), cstr(e, b"x %d"), 5))


@pytest.mark.parametrize("name", [b"start", b"done", b"done:", b"missing"])
def test_find_label(name):
    compare(emu, 0x4BAFA0, "Asm_FindLabel", setup=lambda e: (new_asm(e), cstr(e, name)))


@pytest.mark.parametrize("name", [b"start", b"done", b"later", b"later:"])
def test_add_label_ref(name):
    compare_logged(emu, 0x4BAFD0, "Asm_AddLabelRef", setup=lambda e: (new_asm(e), cstr(e, name)))


STRINGS = [b'"one"', b'"two"', b'"three"', b'""', b'"a\\nb\\tc\\rd\\\\e\\"f"', b'"\\q"', b'"x"']


@pytest.mark.parametrize("s", STRINGS)
def test_add_string(s):
    compare(emu, 0x4BB030, "Asm_AddString", setup=lambda e: (new_asm(e), cstr(e, s)))


@pytest.mark.parametrize("s", STRINGS)
def test_unescape(s):
    def setup(e):
        buf = e.alloc(64)
        e.write(buf, b"\x55" * 64)
        return buf, cstr(e, s)
    compare(emu, 0x4BB110, "Asm_Unescape", setup=setup, check_eax=False)


@pytest.mark.parametrize("s", [b"GOStart", b"IntVar", b"Other", b""])
def test_add_name(s):
    compare(emu, 0x4BB180, "Asm_AddName", setup=lambda e: (new_asm(e), cstr(e, s)))


@pytest.mark.parametrize("seq", [[], [0], [1, 1, 2], [1, 1], [3, 1]])
def test_next_token(seq):
    def setup(e):
        TOKENS[:] = seq
        return e.alloc(0x300), e.alloc(16)
    compare_logged(emu, 0x4BB240, "Asm_NextToken", setup=setup)
