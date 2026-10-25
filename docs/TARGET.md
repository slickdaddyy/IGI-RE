# Target exe

| | |
|---|---|
| File | `IGI.exe`, EU/Chinese release |
| SHA-256 | `ae93669a027d30b5f3f6c311594d9676acd72788dd7283b359f328265f6c3289` |
| Size | 1,384,448 bytes |
| Linked | 2000-11-23 19:49:26 UTC |
| Image base | `0x400000`, x86, not packed |
| Version check | string `IGIMUTEX` at `0x541ECC` (same check as IGIPatch) |

## Sections

| name | start | size |
|---|---|---|
| .text | 0x401000 | 0x131510 |
| .rdata | 0x533000 | 0x28F6 |
| .data | 0x536000 | 0x6FC558 (0x1A000 initialised) |
| .rsrc | 0x833000 | 0x1E40 |

## Code layout

About 5,660 functions after recovering pointer-only ones.

| range | contents |
|---|---|
| 0x401000 - 0x4A5000 | game code: flow, config, menus, humans, AI, weapons, camera, WinMain, Direct3D |
| 0x4A5000 - 0x4AF7B0 | Microsoft C runtime (static) and import thunks |
| 0x4AF7B0 - 0x533000 | engine: errors, lists, memory, files, resources, QVM scripts, objects, terrain, models, sound, tasks, menus |

No other third party libraries are linked in.

## Imports

KERNEL32, USER32, GDI32, ole32, WINMM (mixer, MCI), DINPUT
(`DirectInputCreateA`, `DirectInputCreateEx`), DSOUND (ordinal 1 =
`DirectSoundCreate`), DDRAW (`DirectDrawEnumerateExA`, `DirectDrawCreateEx`),
AVIFIL32 and MSVFW32 (intro videos).

The DLL build loads as a `dinput.dll` proxy. DDraw wrappers like dgVoodoo and
DDrawCompat replace `ddraw.dll`, so both can be used at the same time.

## The exe is patched

This `IGI.exe` carries a no-CD patch in the CD check at `0x402F50`: the
bytes at `0x402F6A` (4), `0x402F74` (2) and `0x402F80` - `0x402F97` are
NOPs. They used to put the drive letter into the root path, skip drives
that are not CD-ROMs and build the path of `ymbe.afp` on that drive. As it
is, the check opens `.\ymbe.afp` in the game folder every time.

No other NOP runs inside functions were found. Other kinds of changes
(flipped jumps) cannot be ruled out without an untouched exe from a disc.

The game has four disc checks. IGIPatch (CDCheckPatch) disables all four
on an untouched exe, which confirms where they are:

| site | check |
|---|---|
| `0x402C32` in the module open list | call to the CD drive scan at `0x402F50` (patched here) |
| `0x4021E7` in `Flow_CreateHandler` | first byte of `anys.afp` must be `:` |
| `0x415F41` in `Game_CreateHandler` | first byte of `mpzm.afp` must be `^` |
| `0x4162A9` in `Game_RunHandler` | first byte of `ymbe.afp` must be `@` |

In this exe only the drive scan is patched; the other three pass because
the one byte marker files sit in the game folder.
