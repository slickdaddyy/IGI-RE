// Create functions at .text addresses that are referenced as data
// (pointer tables, pushed handler addresses) but are not inside a function yet.
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class FindPointerFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        MemoryBlock text = mem.getBlock(".text");
        Set<Address> cand = new TreeSet<>();
        // aligned dwords anywhere in the image that point into .text
        for (MemoryBlock b : mem.getBlocks()) {
            if (!b.isInitialized()) continue;
            Address a = b.getStart();
            while (a.compareTo(b.getEnd()) < 0) {
                try {
                    long v = mem.getInt(a) & 0xffffffffL;
                    if (v >= text.getStart().getOffset() && v < text.getEnd().getOffset()) cand.add(toAddr(v));
                } catch (Exception e) { }
                a = a.add(4);
            }
        }
        // operand references from instructions (push offset handler, mov [x], offset handler)
        InstructionIterator ii = currentProgram.getListing().getInstructions(true);
        while (ii.hasNext()) {
            Instruction ins = ii.next();
            for (int k = 0; k < ins.getNumOperands(); k++)
                for (Object o : ins.getOpObjects(k))
                    if (o instanceof ghidra.program.model.scalar.Scalar s) {
                        long v = s.getUnsignedValue();
                        if (v >= text.getStart().getOffset() && v < text.getEnd().getOffset()) cand.add(toAddr(v));
                    }
        }
        int made = 0, skipped = 0;
        for (Address a : cand) {
            if (getFunctionContaining(a) != null) continue;
            Instruction ins = getInstructionAt(a);
            if (ins == null) {
                if (getInstructionContaining(a) != null || getDataContaining(a) != null) { skipped++; continue; }
                if (!looksLikeEntry(a)) { skipped++; continue; }
                disassemble(a);
                if (getInstructionAt(a) == null) { skipped++; continue; }
            }
            if (new CreateFunctionCmd(a).applyTo(currentProgram, monitor) && getFunctionAt(a) != null) made++;
            else skipped++;
        }
        println("candidates " + cand.size() + " created " + made + " skipped " + skipped);
    }

    // MSVC6 entries: push ebp / sub esp / push reg / mov eax,[esp+..] etc. Reject padding.
    boolean looksLikeEntry(Address a) throws Exception {
        int b0 = getByte(a) & 0xff;
        if (b0 == 0xcc || b0 == 0x90 || b0 == 0x00) return false;
        int[] ok = {0x55, 0x83, 0x81, 0x53, 0x56, 0x57, 0x51, 0x8b, 0x6a, 0x68, 0xa1, 0x33, 0xb8, 0xd9, 0xdd, 0xc3, 0xe9, 0x64, 0xc7, 0x8d, 0x0f, 0x80, 0xa0, 0x52, 0x50, 0x31, 0x89, 0xff, 0xe8, 0x66, 0xc2};
        for (int x : ok) if (x == b0) return true;
        return false;
    }
}
