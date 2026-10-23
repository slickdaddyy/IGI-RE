"""Game_Update (0x401050) and Game_Quit (0x4010A0): setjmp/longjmp through the jmp_buf at 0x54F8E0."""
import struct

import pytest

from emu import Emu, compare

GAME_UPDATE, GAME_QUIT = 0x401050, 0x4010A0
JMP_BUF = 0x54F8E0
FLOW = 0x567C8C          # read by Flow_Get (0x402880)
RUNNING = 0x5C8DE8       # AppMain_SetRunning (0x48F480) stores here
RUN_HANDLERS = 0xA970E0  # task handler slot 1
TYPE = 0x2A
# Words that hold a code address of whoever called setjmp: the return address in the
# jmp_buf and the copy longjmp hands to the CRT's debugger notification (0x543F14 + 4).
CODE_WORDS = [(JMP_BUF + 0x14, JMP_BUF + 0x18), (0x543F18, 0x543F1C)]


def make_emu(hooks=None):
    e = Emu()
    # fs:[0] (the SEH chain head read by _setjmp3 and longjmp) lives at linear address 0 here
    e.uc.mem_map(0, 0x1000)
    e.writable.append((0, 0x1000))
    if hooks:
        e.install_hooks(only=hooks)
    e.baseline = e.snapshot()
    return e


e = make_emu()
# a run handler that only logs its argument
LOGGER = e.fake_top
e.fake_top += 16
e.fake(LOGGER, 0, 1)
e.baseline = e.snapshot()


def setup_flow(em, handler, seh=0xFFFFFFFF):
    task = em.alloc(0x60)
    em.write(task + 0x1C, struct.pack("<H", TYPE))
    em.write(FLOW, struct.pack("<I", task))
    em.write(RUN_HANDLERS + 4 * TYPE, struct.pack("<I", handler))
    em.write(RUNNING, b"\x01")
    if seh != 0xFFFFFFFF:
        rec = em.alloc(0x10)
        em.write(rec, struct.pack("<4I", 0xFFFFFFFF, 0x401234, 0x536100, seh))
        seh = rec
    em.write(0, struct.pack("<I", seh))


@pytest.mark.parametrize("seh", [0xFFFFFFFF, 3])
def test_game_update_runs_flow(seh):
    compare(e, GAME_UPDATE, "Game_Update", setup=lambda em: setup_flow(em, LOGGER, seh), ignore=CODE_WORDS,
            check_eax=False, normalized=True)
    assert e.call_log() == [LOGGER, e.u32(FLOW)]
    assert e.read(RUNNING, 1) == b"\x01"


@pytest.mark.parametrize("seh", [0xFFFFFFFF, 3])
def test_game_update_quit(seh):
    # the flow handler is the original Game_Quit, which longjmps back into Game_Update
    compare(e, GAME_UPDATE, "Game_Update", setup=lambda em: setup_flow(em, GAME_QUIT, seh), ignore=CODE_WORDS,
            check_eax=False, normalized=True)
    assert e.read(RUNNING, 1) == b"\x00"
    assert e.call_log() == []


@pytest.mark.parametrize("seh", [0xFFFFFFFF, 3])
def test_game_quit(seh):
    # the original Game_Update with the original or our Game_Quit as the flow handler
    runs = []

    def setup(em):
        setup_flow(em, GAME_QUIT if not runs else em.symbols["Game_Quit"], seh)
        runs.append(1)

    compare(e, GAME_UPDATE, GAME_UPDATE, setup=setup, check_eax=False, normalized=True)
    assert e.read(RUNNING, 1) == b"\x00"


def test_game_update_quit_hooked():
    plain, hooked = make_emu(), make_emu({GAME_UPDATE, GAME_QUIT})
    out = []
    for em in (plain, hooked):
        em.reset()
        setup_flow(em, GAME_QUIT, 5)
        before = em.snapshot()
        em.call(GAME_UPDATE)
        out.append(em.words(em.changes(before, CODE_WORDS)))
        assert em.read(RUNNING, 1) == b"\x00"
    assert out[0] == out[1]
