import struct

import pytest

import test_qvm as q
from emu import compare_hooked
from test_qvm import compare, compare_error, cstr, emu, plain_hooked  # noqa: F401
from test_qvm_gen import SOURCES, gen_setup, script

LOG_FN, LOG_DATA, ERR_FN, ERR_DATA = 0xA43ED4, 0xA43EC8, 0xA43ED8, 0xA43EC0


def generate(src):
    """Assembler text from the original QSC compiler."""
    emu.reset()
    gen_setup(emu)
    emu.call(0x4BBCB0, cstr(emu, "LOCAL:gen.qas"), script(emu, src))
    text = q.fakes.file_data("LOCAL:gen.qas")
    emu.reset()
    return text


GENERATED = [generate(s) for s in SOURCES[:13]]

HANDWRITTEN = [
    b"start:\n  PUSH 1\n  PUSH 0x7fff\n  PUSH -200000\n  PUSH 0\n  PUSH 1\n  PUSH -1\n  ADD\n  BRA start\n  BRK\n",
    b"  PUSHF 1.5\n  PUSHF 0.0\n  PUSHF -2.25\n  pushs \"a\"\n  PUSHS \"a\"\n  pushi Foo\n  PUSHI Foo\n  NOP\n  BRK\n",
    b"  BT later\n  BF later\nlater:\n  RET\n  JSR\n  BLK\n  BRK\n",
    b"  CALL (A1, A2)\nA1:\n  BRK\nA2:\n  BRK\n",
]


def asm_setup(text, out="LOCAL:out.qvm", callbacks=True, src="LOCAL:test.qas", have_src=True,
              fail_open=(), load_delta=0):
    def setup(e):
        q.fakes.reset()
        q.handlers_setup(e)
        e.call(0x4B8A00, 0x800)
        e.call(0x4B89F0, 0x2000)
        if callbacks:
            e.write(LOG_FN, struct.pack("<I", q.fakes.callback))
            e.write(LOG_DATA, struct.pack("<I", 0x1111))
            e.write(ERR_FN, struct.pack("<I", q.fakes.callback))
            e.write(ERR_DATA, struct.pack("<I", 0x2222))
        if have_src:
            q.fakes.add_res(src, text)
        q.fakes.fail_open = fail_open
        q.fakes.load_delta = load_delta
        return cstr(e, out), cstr(e, src)
    return setup


@pytest.mark.parametrize("text", GENERATED + HANDWRITTEN)
def test_qvmasm_assemble(text):
    compare(emu, 0x4BB270, "QvmAsm_Assemble", setup=asm_setup(text))
    if text in GENERATED:
        assert q.fakes.file_data("LOCAL:out.qvm")[:4] == b"LOOP"


@pytest.mark.parametrize("out", ["out.qvm", "LOCAL:dir/sub/out.qvm", "LOCAL:out.qvm"])
def test_qvmasm_assemble_paths(out):
    compare(emu, 0x4BB270, "QvmAsm_Assemble", setup=asm_setup(GENERATED[0], out=out, callbacks=False))


@pytest.mark.parametrize("text", [
    b"  FOO 1\n  BRK\n",
    b"  BRA nowhere\n  BRK\n",
    b"  BRA nowhere\n  BF nowhere\n  BT elsewhere\n  BRK\n",
    b"x:\nx:\n  BRK\n",
    b"",
])
def test_qvmasm_assemble_errors(text):
    compare(emu, 0x4BB270, "QvmAsm_Assemble", setup=asm_setup(text))


def test_qvmasm_assemble_open_fails():
    compare(emu, 0x4BB270, "QvmAsm_Assemble", setup=asm_setup(GENERATED[0], fail_open=("out.qvm",)))
    compare(emu, 0x4BB270, "QvmAsm_Assemble", setup=asm_setup(GENERATED[0], fail_open=("qvmbin",)))
    compare(emu, 0x4BB270, "QvmAsm_Assemble", setup=asm_setup(GENERATED[0], have_src=False))


def test_qvmasm_assemble_error_box():
    msg = compare_error(emu, 0x4BB270, "QvmAsm_Assemble", setup=asm_setup(b"  FOO\n", callbacks=False))
    assert msg == "%s"
    msg = compare_error(emu, 0x4BB270, "QvmAsm_Assemble", setup=asm_setup(GENERATED[1], load_delta=1))
    assert msg.startswith("INTERNAL ERROR")


@pytest.mark.parametrize("cb", [True, False])
def test_qvmasm_log(cb):
    def setup(e):
        if cb:
            e.write(LOG_FN, struct.pack("<I", q.fakes.callback))
            e.write(LOG_DATA, struct.pack("<I", 0x1111))
        return cstr(e, "%s at %08x\n"), cstr(e, "label"), 0x1234
    compare(emu, 0x4BBC30, "QvmAsm_Log", setup=setup, check_eax=False)


def test_qvmasm_init():
    def setup(e):
        q.handlers_setup(e)
        for a in range(0xA43EC0, 0xA43EDC, 4):
            e.write(a, struct.pack("<I", a))
    compare(emu, 0x4BBC80, "QvmAsm_Init", setup=setup, check_eax=False)


def test_qvmasm_game_like_file():
    """The assembled program loads and runs."""
    setup = asm_setup(GENERATED[1])
    emu.reset()
    emu.call("QvmAsm_Assemble", *setup(emu))
    data = q.fakes.file_data("LOCAL:out.qvm")
    hdr = struct.unpack("<4s14I", data[:0x3C])
    assert hdr[0] == b"LOOP" and hdr[11] + hdr[12] == len(data)


@pytest.mark.parametrize("i", range(len(GENERATED)))
def test_qvm_hooked_compile_assemble(plain_hooked, i):
    (plain, pf), (hooked, hf) = plain_hooked
    saved = q.fakes

    def use(e):
        q.fakes = pf if e is plain else hf

    def gen(e):
        use(e)
        gen_setup(e)
        return cstr(e, "LOCAL:test.qas"), script(e, SOURCES[i])

    def asm(e):
        use(e)
        return asm_setup(GENERATED[i])(e)
    try:
        compare_hooked(plain, hooked, 0x4BBCB0, setup=gen)
        compare_hooked(plain, hooked, 0x4BB270, setup=asm)
    finally:
        q.fakes = saved
