import re
import subprocess

import pytest

from emu import GAME_EXE, source_hooks
from test_freecam_util import LOG, logfake, logger, new_emu, pyfake, run

CHAINS = {
    0x530270: "sub_530270", 0x5302A0: "sub_5302A0", 0x5309C0: "sub_5309C0", 0x530A20: "sub_530A20",
    0x5311E0: "sub_5311E0", 0x5311F0: "sub_5311F0", 0x531540: "sub_531540", 0x5315B0: "sub_5315B0",
    0x531D30: "sub_531D30", 0x531D70: "sub_531D70", 0x531DB0: "sub_531DB0", 0x531DC0: "sub_531DC0",
    0x531DD0: "sub_531DD0", 0x531DF0: "sub_531DF0", 0x5323C0: "sub_5323C0", 0x5323D0: "sub_5323D0",
    0x5323E0: "sub_5323E0", 0x5323F0: "sub_5323F0", 0x532400: "Core_Init", 0x532490: "Core_Shutdown",
    0x530960: "sub_530960", 0x530980: "sub_530980",
}
NARGS = {0x4AF8B0: 1, 0x4017C0: 1, 0x4018C0: 1}
OURS = {a: n for a, n in source_hooks()}


def callees(addr):
    """Call and tail jump targets of the original function at addr, in order."""
    out = subprocess.run(["i686-w64-mingw32-objdump", "-d", "--no-show-raw-insn", "--start-address=0x%x" % addr,
                          "--stop-address=0x%x" % (addr + 0x100), GAME_EXE], capture_output=True, text=True).stdout
    targets = []
    for line in out.splitlines():
        m = re.search(r"\s(call|jmp)\s+0x([0-9a-f]+)", line)
        if m:
            targets.append(int(m.group(2), 16))
            if m.group(1) == "jmp":
                break
        elif re.search(r"\sret", line):
            break
    return targets


def chain_emu(addr):
    e = new_emu()
    for t in set(callees(addr)):
        kw = {}
        if t == 0x4017C0:
            kw["effect"] = lambda e, a: 0x40 + len(LOG[-1])
        logfake(e, t, NARGS.get(t, 0), **kw)
        if t in OURS:
            pyfake(e, e.symbols[OURS[t]], logger(e, t, NARGS.get(t, 0), **kw))
    return e


@pytest.mark.parametrize("addr", sorted(CHAINS))
def test_chain(addr):
    e = chain_emu(addr)
    assert len(callees(addr)) >= 2

    def setup(e):
        e.write(0xA845E0, b"\x21\x22")
    run(e, addr, CHAINS[addr], setup)
