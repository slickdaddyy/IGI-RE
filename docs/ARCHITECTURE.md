# Architecture

One source tree builds two programs. `dinput.dll` runs the rewrite inside the original `IGI.exe`, one function at a
time; it is what every function was checked with. `igidecomp.exe` is the game built from the source alone.

## The DLL

`IGI.exe` loads `dinput.dll` from its folder in place of the system DirectInput. The DLL forwards the real DirectInput
calls to the system copy (`src/dllmain.c`, `src/dinput.def`), checks that the exe is the supported one (PE
timestamp, image size and the `IGIMUTEX` marker string), and then writes a jump at the start of every original
function it has a rewrite for (`src/hook.c`). Each `HOOK(0xADDRESS, Name)` line in the source registers one.

Code in one area calls another area's functions through `ORIG`:

    ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));

In the DLL, `ORIG` is a pointer to the original address, so a call runs the hooked original, which jumps to the
rewrite unless that hook is turned off. That is what lets `dinput.ini` turn functions off one by one
([README](../README.md#settings-and-log)) while the game keeps working. Rewritten functions used by address, as in
handler tables, are written `FN(0xADDRESS, Name)`: the DLL keeps the number, which its tests compare.

Globals are `extern` declarations, and `src/addr.c` gives each one its address in `IGI.exe`
(`AT(name, 0xADDRESS)`, an absolute symbol). In the DLL the rewrite works on the original exe's own data.

## The standalone exe

With `IGI_STANDALONE` (the `igidecomp` target):

- `ORIG` is a plain declaration and `FN` the function itself, so every call and every function pointer links straight
  to the rewrite. The linker proves that each one has a rewrite. `src/exe/link.c` maps the few names that are only
  aliases, and binds the C runtime calls of the game code to `msvcrt.dll`, the closest thing to the static MSVC 6
  runtime linked into `IGI.exe` (same `FILE` layout).
- The exe is linked at `0x400000`. Its first section covers the original code range and is filled with `int3`, so
  anything that still reached for original code would stop the game with "No rewrite for IGI.exe code at ...".
- Every global is defined in C at its original address (below). The game's own data layout is kept for now: some
  code relies on one global sitting next to another.

## Data

Each `.c` file that uses globals ends with a generated block, after the line

    /* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */

that defines them with their types and initial values. `tools/gendefs.py` reads the declared types from the
compiler's debug info and the values from `IGI.exe`, and writes them as C: numbers, strings, structs and arrays, a
pointer to a function as that function, a pointer to data as the global it points at. In the DLL these lines are only
declarations (`BSS`, `DATA`, `INIT` and friends in `src/igi.h`).

Nearby globals of a file form one section that the linker places at the address where their run starts.
`src/exe/fill.c` (`tools/genfill.py`) fills the few bytes between them that no global names, and
`tools/layoutcheck.py` checks every address and every data word of the built exe against `IGI.exe`.

After changing a global's declaration:

    python tools/gendefs.py /path/to/IGI.exe
    cmake --build build
    python tools/genfill.py /path/to/IGI.exe build/CMakeFiles/igidecomp.dir > src/exe/fill.c
    cmake --build build
    python tools/layoutcheck.py /path/to/IGI.exe build/igidecomp.exe

The first build may fail to link while `fill.c` is out of date; the second must not. `layoutcheck.py` prints nothing
when the layout matches.

## Source layout

`src/<area>/` follows the engine's own modules: one folder per system, one C file per module, functions in the order
they have in the exe. The first areas (`core`, `task`, `flow`, `script`) are the engine's base: memory, lists, files,
the task system and the script VM. Most other areas call each other only through `ORIG`, so the DLL can be built with
only the area being worked on (`-DIGI_DIRS=sound`).

The platform layers are `src/win` (window, input, timer, movies) and `src/d3d` (DirectDraw, Direct3D 7,
DirectSound). They are what the port replaces ([ROADMAP.md](ROADMAP.md)).
