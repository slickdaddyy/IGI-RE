# igi-decomp

A decompilation of Project I.G.I.: I'm Going In (Innerloop Studios, 2000).

The goal is a full source port of the game that runs on modern systems.
You need your own copy of the game. This repository contains no game files
or assets.

## How it works

Functions are rewritten one at a time. The build produces `dinput.dll`.
Placed next to `IGI.exe`, it replaces each rewritten function inside the
original exe, so the game stays playable while the work goes on.

## Supported version

EU `IGI.exe`, SHA-256
`ae93669a027d30b5f3f6c311594d9676acd72788dd7283b359f328265f6c3289`.
The DLL does nothing on other versions.

## Build

Needs CMake, Ninja and the i686 MinGW toolchain.

    cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/i686-mingw.cmake
    cmake --build build

Copy `build/dinput.dll` next to `IGI.exe`.

## Standalone build

The same build also makes `igidecomp.exe`. Put it in the game folder and run
it instead of `IGI.exe`, which it doesn't need. Its data is the game's own
layout: every global is a C definition (the block at the end of the file that
uses it, written by `tools/gendefs.py`) placed at its IGI.exe address, and
`src/exe/fill.c` (`tools/genfill.py`) holds the bytes no global names yet.
`tools/layoutcheck.py` checks the result against IGI.exe. After changing a
global's declaration, run `tools/gendefs.py IGI.exe`, build, run
`tools/genfill.py IGI.exe build/CMakeFiles/igidecomp.dir > src/exe/fill.c` and
build again. The original code range is filled with int3. All calls
link straight to the rewrite, and the data's function pointers (handler
tables, the lzss codec) are initialised with the rewritten functions, so
nothing is patched at startup. The C runtime the game code calls comes from `msvcrt.dll`.
Anything that still reaches for original code stops the game with "No
rewrite for IGI.exe code at ...". `igidecomp.ini` and `igidecomp.log` work
like the DLL's files below.

## Settings and log

Without a `dinput.ini` next to the DLL every rewritten function is used.
With one, you can turn functions off to find the one behind a crash:

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

A function that is turned off still gets called through our code by other
rewritten functions in its folder; turn off the whole folder to rule it out.
The crash lines in `dinput.log` name the function and the offset in the DLL.
They can also show exceptions the game catches and handles itself.

## Tests

Each rewritten function is run against the original code in an x86
emulator, and results and memory writes must match.

    pip install unicorn pefile pytest
    IGI_EXE=/path/to/IGI.exe pytest tests
