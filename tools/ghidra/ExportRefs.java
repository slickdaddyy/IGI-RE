// Export call graph and global data references per function.
// Args: <outdir>   writes calls.tsv (caller, callee) and data.tsv (function, address, access, label)
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.nio.file.*;

public class ExportRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String out = getScriptArgs()[0];
        var text = currentProgram.getMemory().getBlock(".text");
        try (PrintWriter calls = new PrintWriter(Files.newBufferedWriter(Paths.get(out, "calls.tsv")));
             PrintWriter data = new PrintWriter(Files.newBufferedWriter(Paths.get(out, "data.tsv")))) {
            calls.println("caller\tcallee");
            data.println("function\taddress\taccess\tlabel");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (f.isExternal() || f.isThunk()) continue;
                String ep = f.getEntryPoint().toString();
                for (Function c : f.getCalledFunctions(monitor))
                    calls.println(ep + "\t" + (c.isThunk() ? c.getThunkedFunction(true).getName() : c.getEntryPoint().toString()));
                java.util.Set<String> seen = new java.util.HashSet<>();
                for (var a : f.getBody().getAddresses(true)) {
                    for (Reference r : getReferencesFrom(a)) {
                        var to = r.getToAddress();
                        if (!to.isMemoryAddress() || text.contains(to) || !r.getReferenceType().isData()) continue;
                        String acc = r.getReferenceType().isWrite() ? "W" : r.getReferenceType().isRead() ? "R" : "A";
                        if (!seen.add(to + acc)) continue;
                        Symbol s = getSymbolAt(to);
                        data.println(ep + "\t" + to + "\t" + acc + "\t" + (s == null ? "" : s.getName()));
                    }
                }
            }
        }
        println("refs exported");
    }
}
