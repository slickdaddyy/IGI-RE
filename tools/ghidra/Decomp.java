// Print decompiled C for the functions containing the given addresses.
// Args: <addr> [addr...]
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;

public class Decomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (String a : getScriptArgs()) {
            Function f = getFunctionContaining(toAddr(a));
            if (f == null) { println("no function at " + a); continue; }
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            println("==== " + f.getEntryPoint() + " " + f.getName() + "\n" + r.getDecompiledFunction().getC());
        }
        di.dispose();
    }
}
