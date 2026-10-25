// Find functions that are really switch cases: never called, only reached
// by a switch jump or from jump table data inside .text. Merge them back into the function
// before them. Args: <outdir> [fix]
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.nio.file.*;
import java.util.*;

public class FixCaseSplits extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        boolean fix = args.length > 1 && args[1].equals("fix");
        var text = currentProgram.getMemory().getBlock(".text");
        List<Function> sus = new ArrayList<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            Reference[] refs = getReferencesTo(f.getEntryPoint());
            if (refs.length == 0) continue;
            boolean caseOnly = true;
            for (Reference r : refs) {
                boolean fromText = text.contains(r.getFromAddress());
                boolean fromCode = getInstructionContaining(r.getFromAddress()) != null;
                boolean switchJump = r.getReferenceType() == RefType.COMPUTED_JUMP;
                if (!fromText || (fromCode && !switchJump)) { caseOnly = false; break; }
            }
            if (caseOnly) sus.add(f);
        }
        List<String> out = new ArrayList<>();
        Set<Function> parents = new LinkedHashSet<>();
        for (Function f : sus) {
            Function parent = getFunctionBefore(f.getEntryPoint());
            while (parent != null && sus.contains(parent)) parent = getFunctionBefore(parent.getEntryPoint());
            out.add(f.getEntryPoint() + "\t" + f.getName() + "\tparent\t" + (parent == null ? "?" : parent.getEntryPoint() + " " + parent.getName()));
            if (fix && parent != null) {
                currentProgram.getFunctionManager().removeFunction(f.getEntryPoint());
                parents.add(parent);
            }
        }
        for (Function p : parents) CreateFunctionCmd.fixupFunctionBody(currentProgram, p, monitor);
        Files.write(Paths.get(args[0], "case_splits.tsv"), out);
        println("case-only functions " + sus.size() + (fix ? " merged into " + parents.size() + " parents" : ""));
    }
}
