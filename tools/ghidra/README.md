# Ghidra scripts

Scripts used to find, name and export the functions of `IGI.exe`.

## Setup

1. Install Ghidra 12.1 and a JDK 21.
2. Import your EU `IGI.exe` into a Ghidra project called `IGI` inside a work folder and let auto analysis finish.
3. Run the scripts with `gh.sh`:

       export GHIDRA_INSTALL_DIR=/path/to/ghidra IGI_WORK=/path/to/work
       tools/ghidra/gh.sh FindPointerFuncs.java
       tools/ghidra/gh.sh ApplyNames.java symbols/names.tsv $IGI_WORK/export
       tools/ghidra/gh.sh ExportAll.java $IGI_WORK/export

`FindPointerFuncs` creates functions that are only reached through pointers (script handlers, task tables). Run it
twice, then run auto analysis again.

## Names

Each row of `symbols/names.tsv` is `address, name, source, kind`. The kind is `entry` for a function, `data` for a
global and `contain` for a label inside a function.

| source | meaning |
|---|---|
| manual | checked by reading the code |
| igipatch | hook names from IGIPatch by BlanknameES |

Names recovered straight from the exe (script symbol registration and `Name()` error messages) are found by
`ApplyNames.java` itself.
