// List functions nobody references, with the instruction before them.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class NoRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (f.isExternal() || getReferencesTo(f.getEntryPoint()).length > 0) continue;
            Instruction prev = getInstructionBefore(f.getEntryPoint());
            Function before = getFunctionBefore(f.getEntryPoint());
            println(f.getEntryPoint() + " " + f.getName() + " size " + f.getBody().getNumAddresses()
                + " | prev: " + prev + " | fn before: " + (before == null ? "" : before.getName()));
        }
    }
}
