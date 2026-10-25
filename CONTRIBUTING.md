# Contributing

## Your own copy

Never commit game files: no assets, no `IGI.exe` and no pieces of either.
`.gitignore` keeps the usual game file types out. Requests for game files or links to them are removed.

## A rewritten function

- Behaves exactly like the original: same return value, same memory writes, same calls in the same order, same stack
  cleanup. That includes the original's quirks: values kept at 80 bits, junk in the upper bits of small arguments,
  reads of uninitialised stack, out-of-bounds writes that land on the next global.
- Has `HOOK(0xADDRESS, Name);` right after its body. Functions under 5 bytes get no HOOK; the jump would not fit.
- Uses the original's calling convention: cdecl by default, `__stdcall`, `__fastcall` or `__thiscall` where the
  original pops its own arguments or takes them in registers. Returns `char` when the original only sets AL.
- Copies format strings exactly. Error paths that call `Error_Show` end in `for (;;);` like the original.
- Lives in `src/<area>/<module>.c`, in the same order as in the exe, named in the engine's style (`Module_Action`).
  Names found in the exe win. Function names are unique across all folders.

Float code needs care: the game runs the x87 at single precision, with truncation once the device is set up. gcc
assumes round-to-nearest, so it rewrites `(float)-x` and `-(a - b)` and folds constants at compile time, and each of
those changes the result under truncation. [docs/VERIFY.md](docs/VERIFY.md) has the details and the helpers.

## Types and globals

- Every struct type is defined once, in the `<area>types.h` of the area that implements it (`src/types.h` for vectors,
  matrices, lists and pools). Types-only headers hold no prototypes, so any file can include them. Keep the
  `_Static_assert` size and offset checks next to the definition.
- Unknown fields are `unk<offset>` (`unk1C`) until the code shows what they are.
- Every global is declared once (`extern`): in the one .c file that uses it, or in its owning area's `<area>vars.h`
  when other areas use it. Its address is in `src/addr.c`. After changing a declaration,
  regenerate the exe's data ([docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#data)).

## Tests

Every rewritten function gets a test in `tests/test_<area>*.py` that runs the original and the rewrite in the emulator
with `compare()`. Cover the normal path and every branch you can reach, and check that the test fails when the logic
is wrong. Tests that need state set it up with the original functions.

Before a pull request: both targets build with no new warnings, the tests of every area you touched pass, and
`tools/layoutcheck.py` and `tools/origcheck.py` print nothing.

## Comments and commits

Comments are few and short, only where the code does something surprising. Commit messages are short plain English,
one area per commit.
