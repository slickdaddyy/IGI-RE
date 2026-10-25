// Function boundary audit. Flags entries that the previous instruction falls into
// (likely a split function or a case label), and prints totals.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.nio.file.*;
import java.util.*;

public class Audit extends GhidraScript {
    @Override
    public void run() throws Exception {
        List<String> sus = new ArrayList<>();
        int total = 0, named = 0, noRefs = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (f.isExternal()) continue;
            total++;
            if (!f.getName().startsWith("FUN_")) named++;
            if (getReferencesTo(f.getEntryPoint()).length == 0) noRefs++;
            Instruction prev = getInstructionBefore(f.getEntryPoint());
            if (prev == null) continue;
            if (!prev.getMaxAddress().next().equals(f.getEntryPoint())) continue;
            if (prev.getFlowType().isTerminal() || prev.getFlowType().isUnConditional()) continue;
            if (prev.getFlowType().isCall() && getFunctionAt(prev.getFlows().length > 0 ? prev.getFlows()[0] : f.getEntryPoint()) != null
                && getFunctionAt(prev.getFlows()[0]).hasNoReturn()) continue;
            sus.add(f.getEntryPoint() + "\t" + f.getName() + "\tfallthrough_from\t" + prev.getAddress() + "\t" + prev);
        }
        Files.write(Paths.get(getScriptArgs()[0], "audit_fallthrough.tsv"), sus);
        println("functions " + total + " named " + named + " no_refs " + noRefs + " fallthrough_suspects " + sus.size());
    }
}
