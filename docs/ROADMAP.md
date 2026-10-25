# Roadmap

## Done

1. **Every function rewritten.** All 5,376 functions of the game and engine code (0x401000 - 0x4A5000 and
   0x4AF7B0 - 0x533000), each with a differential test against the original, including the large ones set aside at
   first (magicobj collision, the QVM, terrain and lightmap rendering). The few paths that can't be tested (ones that
   read uninitialised stack or crash in the original) are documented.
2. **Nothing missed.** Every byte of `.text` that no function covers was checked: switch tables and tails of known
   functions, C runtime code, and three dead blocks (among them the CD drive scan that the no-CD patch bypasses). The
   standalone build proves it again: every call has to link to a rewrite.
3. **Names.** Every function, global and struct type has a real name, and every type is defined once. Where even the
   original never reads some state, the name says so.
4. **Live testing.** The DLL runs the rewrite in the real game under Wine. Menus, cutscenes, all 14 missions and the
   20 weapons look and play the same with the rewrite on and off. The bugs only the game showed (stack arguments the
   original reuses, script handlers that return their out pointer, string literals the game writes to) are fixed.
5. **Data.** Every global is a typed C variable with its initial value, at its original address
   ([ARCHITECTURE.md](ARCHITECTURE.md#data)).
6. **Standalone exe.** `igidecomp.exe` is the game built from this source alone, with the system `msvcrt.dll` in place
   of the C runtime that was linked into `IGI.exe`.

## Open

- **Struct fields.** Fields are named where the code shows what they are. About 300 that the code touches are still
  `unk<offset>`; the code never touches the rest one by one.
- **Other releases.** Only the EU exe is supported. Other versions need their own address map, or a check that their
  code matches.
- **Free the layout.** `igidecomp.exe` keeps `IGI.exe`'s data layout because a few code paths rely on one global
  sitting next to another. Finding each global's true extent (the test suite can record every data byte the code
  touches) would let the data go anywhere, which anyone porting the code to other platforms or to 64-bit will need.
