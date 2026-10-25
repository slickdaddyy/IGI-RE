#!/bin/sh
# usage: gh.sh Script.java [args...]  (runs on the saved IGI project, read-only unless the script changes things)
: "${GHIDRA_INSTALL_DIR:?set GHIDRA_INSTALL_DIR}"
: "${IGI_WORK:?set IGI_WORK to the folder holding the ghidra project}"
W=$IGI_WORK
D=$(cd "$(dirname "$0")" && pwd)
s=$1; shift
"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$W/ghidra" IGI -process IGI.exe -noanalysis -scriptPath "$D" -postScript "$s" "$@" 2>&1 | grep -E 'INFO  .*\(GhidraScript\)|^[^IWE]|ERROR|Exception' | sed -E 's/^INFO  [A-Za-z]+\.java> //; s/ \(GhidraScript\) *$//'
