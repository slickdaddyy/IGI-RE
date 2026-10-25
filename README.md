<div align="center">

# igi-decomp

**A decompilation of Project I.G.I.: I'm Going In (Innerloop Studios, 2000): the whole game rewritten as readable C,
one verified function at a time.**

</div>

> [!IMPORTANT]
> **You need your own copy of the game.** This repository contains no game assets and no original code. The build
> runs only from the folder of an installed copy of the EU release (see [Target](#target)).

igi-decomp rewrites the code of `IGI.exe` in C, function by function, and checks every rewrite against the original
machine code. It is a **functional** decompilation: a rewrite does not have to compile to the same bytes, it has to
*behave* exactly like the original: same return value, same memory writes, same calls in the same order.

I made this in my free time, with AI tools (see the [AI disclosure](#ai-disclosure) at the end), because I.G.I. deserves
to stay preserved, so that it can be modernized to its core and played for years to come.

Every function is now rewritten, and the source builds a game executable on its own:

| | |
|---|---|
| Functions rewritten | 5,376 of 5,376 (100% of the game and engine code) |
| Functions with a real name | 5,376 |
| Globals | every one a typed C variable at its original address |
| Differential tests | 90,078 in 425 files, all passing |
| Missions | all 14 play the same as the original, checked side by side |

The Microsoft C runtime that was linked into `IGI.exe` is not decompiled; the system `msvcrt.dll` stands in for it.

## The goal

**A complete source port of I.G.I. for modern systems.** The game can already be built from this source alone as a
32-bit Windows program. Next comes the port itself: replacing the DirectX 7, DirectSound and Win32 layers with
portable ones so that it builds natively on Linux and macOS and as a 64-bit program
([docs/ROADMAP.md](docs/ROADMAP.md)).

## What you need

- The EU `IGI.exe` (SHA-256 `ae93669a027d30b5f3f6c311594d9676acd72788dd7283b359f328265f6c3289`) and its game folder.
  Other releases are not supported yet; the DLL refuses to install on them.
- Windows, or Linux with Wine. The development and the live tests are done under Wine 11.
- To build: CMake, Ninja and the i686 MinGW-w64 toolchain (`i686-w64-mingw32-gcc`).
- To run the tests: Python 3 with `unicorn`, `pefile` and `pytest`.

## Building

    cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/i686-mingw.cmake -DCMAKE_BUILD_TYPE=Release
    cmake --build build

The build makes two programs from the same source:

- **`build/igidecomp.exe`**, the game built from this source alone. Copy it into the game folder and run it in place of
  `IGI.exe`, which it doesn't need.
- **`build/dinput.dll`**, which loads into the original `IGI.exe` and replaces each original function with its rewrite.
  It is how every function was checked against the original, and the test suite still runs on it. Copy it next to
  `IGI.exe` and start the game as usual.

On Linux, run either one under Wine; the DLL needs `WINEDLLOVERRIDES=dinput=n,b`. The game's own `Window` switch runs it
in a window.

## Settings and log

`dinput.ini` next to the DLL (or `igidecomp.ini` next to the exe) turns rewritten functions off, which is how a crash is
traced to one function:

    [hooks]
    ; 0 runs the game's own code everywhere; the DLL only forwards DirectInput
    enabled=1
    ; use only these src folders, function names or 0x addresses
    only=
    ; leave these out
    skip=haze, TerrainMesh_DrawD3D, 0x521E40
    ; use only hooks first-last, numbered as in the log, for halving down
    range=
    ; 1 writes dinput.log with the hook counts and any crash, 2 also every hook
    log=1

Without the file every rewrite is used. A crash writes the faulting function, the registers and the rewritten
functions found on the stack to `dinput.log`. In `igidecomp.exe` there is no original code to fall back on, so only
`log` applies there.

## How it works

1. **Find every function.** Ghidra, plus scripts that recover functions only reached through pointers (script handlers,
   task tables), lists the exe's 5,376 game and engine functions ([docs/TARGET.md](docs/TARGET.md)).
2. **Rewrite it.** A C function in `src/<area>/`, with the original's calling convention, followed by
   `HOOK(0xADDRESS, Name)`. The DLL writes a jump at that address, so the game runs the rewrite while the original exe
   supplies everything else.
3. **Prove it.** `tests/` runs the original machine code and the rewrite side by side in an x86 emulator (Unicorn) from
   the same state, and compares the return value, every byte written, the stack cleanup and every call made. Float code
   is run under each x87 rounding mode the game uses ([docs/VERIFY.md](docs/VERIFY.md)).
4. **Play it.** Every mission is played by script with the original and with the rewrite, and the frames are compared.
   This catches what a per-function test can't see, such as ordering between systems.
5. **Cut the cord.** Every global became a typed C variable. `igidecomp.exe` links every call straight to the rewrite
   and defines all data in C at its original address, so nothing of `IGI.exe` is loaded
   ([docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)).

## Tests

    pip install unicorn pefile pytest
    cd tests
    IGI_EXE=/path/to/IGI.exe IGI_DLL=../build/dinput.dll pytest -q

The full suite takes about 10 minutes on four processes (`ls test_*.py | xargs -P 4 -n 1 pytest -q`). Point `IGI_DLL` at
a copy of the DLL for long runs, so that a rebuild doesn't swap the file under the running tests.

## Layout

| Path | What |
|---|---|
| `src/<area>/` | The rewrite, one folder per engine system (`sound`, `humanai`, `qvm`, ...), one C file per module, functions in the exe's order. `<area>types.h` holds the area's struct types. |
| `src/types.h` | Vectors, matrices, lists and pools, used everywhere. |
| `src/addr.c` | The original address of every global. |
| `src/hook.c`, `src/dllmain.c` | The DLL: hook installation, settings, crash log, the DirectInput forwarding. |
| `src/exe/` | What only `igidecomp.exe` needs: startup, links to the C runtime, `fill.c` (generated). |
| `tests/` | Differential tests, one or more files per area, and `emu.py`, the emulator harness. |
| `tools/` | Data generation and checks: `gendefs.py`, `genfill.py`, `layoutcheck.py`, `origcheck.py`. |
| `docs/` | Architecture, verification, the target exe, the roadmap. |

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) first. In short: behaviour must match the original exactly, every change comes
with a test that runs against the original, and no game files are ever committed.

## Target

The EU (and Chinese) release of `IGI.exe`, 1,384,448 bytes, linked 2000-11-23:

```
ae93669a027d30b5f3f6c311594d9676acd72788dd7283b359f328265f6c3289  IGI.exe
```

This exe carries a no-CD patch in its CD check; [docs/TARGET.md](docs/TARGET.md) has the details, the sections and the
code layout.

## Legal

- **No game assets.** This repository holds newly written source, tools and documentation. It contains no models,
  textures, sounds, levels, scripts or movies from the game, and no original machine code. The initial values of the
  exe's global data (numbers, tables, strings) are written into the source as C, as is usual for decompilation
  projects.
- **Your own copy.** To run or test anything you need the game, installed from your own copy. The project does not
  provide or point to game files.
- **Not affiliated.** This is a fan project. It is not affiliated with, endorsed by or sponsored by Innerloop Studios,
  Eidos Interactive or any current owner of the game. Project I.G.I. and related names are trademarks of their
  respective owners.

## Thanks

- BlanknameES for IGIPatch, whose hook names and disc check notes were a reference.
- The project-igi-internals-data project, for early function names.
- The Ghidra, Unicorn, MinGW-w64 and Wine projects, which this work stands on.

## License

GPL-2.0-or-later ([LICENSE](LICENSE)). The license covers this project's own source and tools; it grants no right in
the game.

## AI disclosure

AI coding tools were used in developing, debugging and testing this project. My focus has been on matching the
original game: every rewritten function is checked against the original machine code by the differential tests, and
the game is played side by side with the original to catch what the tests can't.
