# How rewrites are verified

A rewrite counts as done when it behaves exactly like the original. Three checks prove it: differential tests in an
emulator, the hooked game itself, and side by side play of every mission.

## Differential tests

`tests/emu.py` loads the player's `IGI.exe` and the built `dinput.dll` into a 32-bit x86 emulator (Unicorn).
`compare()` runs the original function and the rewrite from the same state and requires:

- the same return value (and `st(0)` for float functions, when asked);
- the same stack cleanup;
- the same memory changes, byte for byte, everywhere the emulator tracks;
- the same calls, in the same order with the same arguments, for callees replaced by fakes.

```python
from emu import Emu, compare

emu = Emu()

def test_list_add_tail():
    def setup(e):
        lst, _ = list_with_nodes(e, 3)
        return lst, e.alloc(16)
    compare(emu, 0x4AF8F0, "List_AddTail", setup=setup, check_eax=False)
```

(from `tests/test_core.py`; the address is the original's, the name the rewrite's.)

Useful pieces of the harness:

- `Emu.fake(addr, ret, nargs)` replaces an original callee with a stub that logs its arguments, so a test can check what
  a function calls without running it. `stub(name, fn)` emulates an imported system function in Python.
- `call()` puts known values in `ebx`, `esi` and `edi` and fails if the function doesn't restore them.
- `compare(..., normalized=True)` compares pointers through `normalize`, so the rewrite's function pointers match the
  original's.
- `compare_hooked()` runs a larger original function once on the clean exe and once with every hook installed, which
  checks how the rewrites work together.
- After each call the emulator restores only the 64 KB pages that were written, which keeps the full suite to about
  10 minutes.

Tests set up state with the original functions where they can, cover every branch they can reach, and should fail when
the logic is wrong.

## Floats

The game runs the x87 at single precision (control word `0x7F`), and with truncation (`0xC7F`) once the Direct3D device
is set up. Float tests run under each control word the game uses, plus round-down (`0x77F`), which catches negations
done at the wrong point:

```python
@pytest.fixture(autouse=True, params=[0x37F, 0x7F, 0xC7F, 0x77F], ids=hex)
def cw(request):
    Emu.cw = request.param
    yield
    Emu.cw = None
```

Things that came out of this:

- gcc assumes round-to-nearest. It turns `(float)-x` into `-(float)x` and `-(a - b)` into `b - a`, and folds constant
  expressions at compile time; under truncation each of those changes the result. Negate with an `fchs` helper (see
  `x87_neg` in `src/res/qvertex.c`), and keep values that the original computes at run time opaque to the compiler.
- Under `excess-precision=standard` a literal like `0.1f` is evaluated as a long double. Keep the exe's constants as
  `static const float` or `double`.
- Inlined 3x3 matrix products add their terms in an order that changes per function and per row. Work from the
  disassembly, not from a decompiler's pseudo-C.
- The original often keeps intermediates at 80 bits, treats NaN in specific ways, pushes junk in the upper bits of small
  arguments and reads uninitialised stack. All of it is copied; tests skip only the words that are truly undefined.

## In the game

Some bugs only the running game shows: arguments the original reuses from the stack of an earlier call, script
handlers that must return their out pointer in `eax`, string literals the game writes to. The DLL runs the rewrite
inside the real game, and `dinput.ini` halves the set of hooks down to the one behind a crash.

Every mission is then played by a script (pistol, walk, fire, turn, knife), once with the original exe and once with
`igidecomp.exe`, and frames at fixed points are compared. Differences are looked at by eye: enemies that react
differently from run to run and key timing account for the small ones.

## The exe's data

`tools/layoutcheck.py` checks every global's address and every data word of `igidecomp.exe` against `IGI.exe`, with
function pointers matched to the rewrite of the function at the original address. `tools/origcheck.py` checks that
every `ORIG` name matches the function rewritten at that address, because the standalone build links by name.
