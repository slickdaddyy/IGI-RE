// Apply names from names_in.tsv plus names recovered from the binary
// (script symbol registration and "Name()" error strings).
// Args: <names.tsv> <outdir>   writes names_applied.tsv
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.lang.Register;
import java.nio.file.*;
import java.util.*;
import java.util.regex.*;

public class ApplyNames extends GhidraScript {
    Map<Address, Integer> prio = new HashMap<>();
    List<String> log = new ArrayList<>();
    static Map<String, Integer> P = Map.of("manual", 4, "binary", 3, "igipatch", 2, "natives", 1);

    boolean isDefault(String n) {
        return n.startsWith("FUN_") || n.startsWith("LAB_") || n.startsWith("DAT_") || n.startsWith("thunk_FUN_");
    }

    String clean(String n) { return n.replaceAll("[^A-Za-z0-9_]", "_"); }

    Function ensureFunction(Address a) {
        Function f = getFunctionAt(a);
        if (f != null) return f;
        if (getInstructionAt(a) == null) disassemble(a);
        new CreateFunctionCmd(a).applyTo(currentProgram, monitor);
        return getFunctionAt(a);
    }

    void name(Address a, String n, String src, boolean func) throws Exception {
        n = clean(n);
        int p = P.get(src);
        Integer had = prio.get(a);
        if (had != null && had >= p) return;
        String cur;
        if (func) {
            Function f = ensureFunction(a);
            if (f == null) { log.add(a + "\t" + n + "\t" + src + "\tFAILED_NO_FUNCTION"); return; }
            cur = f.getName();
            if (had == null && !isDefault(cur) && p < 4) return;
            f.setName(n, SourceType.USER_DEFINED);
        } else {
            Symbol s = getSymbolAt(a);
            cur = s == null ? "" : s.getName();
            if (had == null && s != null && !isDefault(cur) && s.getSource() != SourceType.DEFAULT) return;
            createLabel(a, n, true, SourceType.USER_DEFINED);
        }
        prio.put(a, p);
        log.add(a + "\t" + n + "\t" + src + "\t" + (func ? "func" : "data"));
    }

    // Values pushed for the first n args of a cdecl/stdcall call. null if unknown.
    Long[] pushedArgs(Instruction call, int n) {
        Long[] out = new Long[n];
        Map<String, Long> regs = new HashMap<>();
        List<Object> pushes = new ArrayList<>();
        Instruction i = call.getPrevious();
        int steps = 0;
        // walk back collecting pushes; remember mov reg,imm seen after the push (earlier in code)
        List<Instruction> seq = new ArrayList<>();
        while (i != null && steps++ < 40 && seq.size() < 64) {
            if (i.getFlowType().isCall() || i.getFlowType().isJump() || i.getFlowType().isTerminal()) break;
            seq.add(i);
            i = i.getPrevious();
        }
        Collections.reverse(seq);
        List<Long> vals = new ArrayList<>();
        for (Instruction x : seq) {
            String m = x.getMnemonicString();
            if (m.equals("MOV") || m.equals("LEA")) {
                Object[] o0 = x.getOpObjects(0), o1 = x.getOpObjects(1);
                if (o0.length == 1 && o0[0] instanceof Register) {
                    String r = ((Register) o0[0]).getName();
                    Long v = null;
                    if (o1.length == 1 && o1[0] instanceof Scalar) v = ((Scalar) o1[0]).getUnsignedValue();
                    else if (o1.length == 1 && o1[0] instanceof Address) v = ((Address) o1[0]).getOffset();
                    if (v != null) regs.put(r, v); else regs.remove(r);
                }
            } else if (m.equals("PUSH")) {
                Object[] o = x.getOpObjects(0);
                Long v = null;
                if (o.length == 1 && o[0] instanceof Scalar) v = ((Scalar) o[0]).getUnsignedValue();
                else if (o.length == 1 && o[0] instanceof Address) v = ((Address) o[0]).getOffset();
                else if (o.length == 1 && o[0] instanceof Register) v = regs.get(((Register) o[0]).getName());
                vals.add(v);
            } else if (m.equals("XOR")) {
                Object[] o0 = x.getOpObjects(0), o1 = x.getOpObjects(1);
                if (o0.length == 1 && o1.length == 1 && o0[0].equals(o1[0]) && o0[0] instanceof Register)
                    regs.put(((Register) o0[0]).getName(), 0L);
            }
        }
        // last push = first arg
        for (int k = 0; k < n && k < vals.size(); k++) out[k] = vals.get(vals.size() - 1 - k);
        return out;
    }

    String cstr(long off) {
        try {
            Address a = toAddr(off);
            StringBuilder sb = new StringBuilder();
            for (int k = 0; k < 128; k++) {
                byte b = getByte(a.add(k));
                if (b == 0) break;
                if (b < 0x20 || b > 0x7e) return null;
                sb.append((char) b);
            }
            return sb.length() == 0 ? null : sb.toString();
        } catch (Exception e) { return null; }
    }

    boolean inText(long off) {
        var b = currentProgram.getMemory().getBlock(toAddr(off));
        return b != null && b.getName().equals(".text");
    }

    boolean inData(long off) {
        var b = currentProgram.getMemory().getBlock(toAddr(off));
        return b != null && (b.getName().equals(".data") || b.getName().equals(".rdata"));
    }

    @Override
    public void run() throws Exception {
        String out = getScriptArgs()[1];
        List<String[]> rows = new ArrayList<>();
        for (String l : Files.readAllLines(Paths.get(getScriptArgs()[0]))) rows.add(l.split("\t"));

        // 1. manual first so registrar names exist
        for (String[] r : rows)
            if (r[2].equals("manual"))
                name(toAddr(Long.parseLong(r[0], 16)), r[1], "manual", r.length < 4 || !r[3].equals("data"));

        // 2. script registration: callers of Script_RegisterSymbol wrappers
        long[] funcRegs = {0x4b8890L, 0x4b88f0L};
        long[] varRegs = {0x4b7e80L, 0x4b7ee0L, 0x4b7fa0L, 0x4b7fc0L};
        int nf = 0, nv = 0, nu = 0;
        for (int pass = 0; pass < 2; pass++) {
            long[] regs = pass == 0 ? funcRegs : varRegs;
            for (long ra : regs) {
                for (Reference ref : getReferencesTo(toAddr(ra))) {
                    if (!ref.getReferenceType().isCall()) continue;
                    Instruction call = getInstructionAt(ref.getFromAddress());
                    if (call == null) continue;
                    Long[] a = pushedArgs(call, 2);
                    String s = a[0] == null ? null : cstr(a[0]);
                    if (s == null || a[1] == null || a[1] == 0) { nu++; continue; }
                    if (pass == 0 && inText(a[1])) { name(toAddr(a[1]), s, "binary", true); nf++; }
                    else if (pass == 1 && inData(a[1])) { name(toAddr(a[1]), "g" + s, "binary", false); nv++; }
                    else nu++;
                }
            }
        }
        println("registered funcs " + nf + " vars " + nv + " unresolved " + nu);

        // 3. "Name()" strings -> function that references them
        Pattern pn = Pattern.compile("^([A-Za-z][A-Za-z0-9]*_[A-Za-z0-9_]+)\\(\\)");
        DataIterator di = currentProgram.getListing().getDefinedData(true);
        int ne = 0;
        while (di.hasNext()) {
            Data d = di.next();
            if (!(d.getValue() instanceof String)) continue;
            Matcher m = pn.matcher((String) d.getValue());
            if (!m.find()) continue;
            Set<Function> fs = new HashSet<>();
            for (Reference r : getReferencesTo(d.getAddress())) {
                Function f = getFunctionContaining(r.getFromAddress());
                if (f != null) fs.add(f);
            }
            if (fs.size() == 1) { name(fs.iterator().next().getEntryPoint(), m.group(1), "binary", true); ne++; }
            else log.add(d.getAddress() + "\t" + m.group(1) + "\tbinary\tAMBIGUOUS_" + fs.size());
        }
        println("error-string names " + ne);

        // 4. IGIPatch then natives
        for (String src : new String[]{"igipatch", "natives"}) {
            for (String[] r : rows) {
                if (!r[2].equals(src)) continue;
                Address a = toAddr(Long.parseLong(r[0], 16));
                if (r[3].equals("contain")) {
                    Function f = getFunctionContaining(a);
                    if (f == null) { log.add(a + "\t" + r[1] + "\t" + src + "\tFAILED_NOT_IN_FUNCTION"); continue; }
                    a = f.getEntryPoint();
                }
                name(a, r[1], src, true);
            }
        }
        Files.write(Paths.get(out, "names_applied.tsv"), log);
        println("applied " + prio.size() + " log " + log.size());
    }
}
