// Write every pointer Ghidra typed in .rdata/.data (top level or inside a struct/array) as
// "address<TAB>value" to the file given as the script argument.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.Pointer;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;
import java.io.PrintWriter;

public class ExportPointers extends GhidraScript {
    private PrintWriter out;

    private void walk(Data d) throws Exception {
        if (d.getDataType() instanceof Pointer) {
            Object v = d.getValue();
            out.println(d.getAddress() + "\t" + (v == null ? "0" : v.toString()));
            return;
        }
        for (int i = 0; i < d.getNumComponents(); i++)
            walk(d.getComponent(i));
    }

    @Override
    public void run() throws Exception {
        out = new PrintWriter(getScriptArgs()[0]);
        for (String name : new String[] {".rdata", ".data"}) {
            MemoryBlock b = currentProgram.getMemory().getBlock(name);
            DataIterator it = currentProgram.getListing().getDefinedData(b.getStart(), true);
            while (it.hasNext()) {
                Data d = it.next();
                if (!b.contains(d.getAddress())) break;
                walk(d);
            }
        }
        out.close();
        println("done");
    }
}
