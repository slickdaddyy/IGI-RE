// Export function list and decompiled C for every function.
// Args: <outdir>
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.nio.file.*;

public class ExportAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        String out = getScriptArgs()[0];
        Path dir = Paths.get(out, "decomp");
        Files.createDirectories(dir);
        DecompInterface di = new DecompInterface();
        di.toggleCCode(true);
        di.openProgram(currentProgram);
        PrintWriter tsv = new PrintWriter(Files.newBufferedWriter(Paths.get(out, "functions.tsv")));
        tsv.println("entry\tname\tsize\tcallers\tcallees\tthunk");
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        int n = 0;
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            if (f.isExternal()) continue;
            String ep = f.getEntryPoint().toString();
            tsv.printf("%s\t%s\t%d\t%d\t%d\t%b%n", ep, f.getName(), f.getBody().getNumAddresses(),
                f.getCallingFunctions(monitor).size(), f.getCalledFunctions(monitor).size(), f.isThunk());
            if (f.isThunk()) continue;
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            String c = (r != null && r.decompileCompleted()) ? r.getDecompiledFunction().getC()
                : "// decompile failed: " + (r == null ? "null" : r.getErrorMessage()) + "\n";
            Files.writeString(dir.resolve(ep + ".c"), c);
            if (++n % 250 == 0) println("decompiled " + n);
        }
        tsv.close();
        di.dispose();
        println("done " + n);
    }
}
