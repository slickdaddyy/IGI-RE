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

## Tests

Each rewritten function is run against the original code in an x86
emulator, and results and memory writes must match.

    pip install unicorn pefile pytest
    IGI_EXE=/path/to/IGI.exe pytest tests
