import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class DpReport extends GhidraScript {
    private void words(String label, long addr, int n) throws Exception {
        Address a = toAddr(addr);
        StringBuilder sb = new StringBuilder();
        sb.append(label);
        for (int i = 0; i < n; i++) {
            int v = currentProgram.getMemory().getShort(a.add(i * 2L)) & 0xffff;
            sb.append(String.format(" %04x", v));
        }
        println(sb.toString());
    }

    private void xrefs(String label, long addr) throws Exception {
        Address a = toAddr(addr);
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(a);
        int n = 0;
        while (refs.hasNext() && n < 8) {
            Reference r = refs.next();
            println("XREF " + label + " " + r.getReferenceType().getName()
                    + " from " + r.getFromAddress());
            n++;
        }
        if (n == 0) {
            println("XREF " + label + " none");
        }
    }

    @Override
    public void run() throws Exception {
        println("LANG " + currentProgram.getLanguageID());
        println("BASE " + currentProgram.getImageBase());
        println("FUNCS " + currentProgram.getFunctionManager().getFunctionCount());
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            println("BLOCK " + b.getName() + " " + b.getStart() + " size " + b.getSize());
        }
        byte[] raw = new byte[8];
        currentProgram.getMemory().getBytes(toAddr(0x450f4aL), raw);
        println("MATTE_BYTES " + new String(raw, 0, 5, "ISO-8859-1"));
        words("PAL", 0x4501d2L, 32);
        words("ICONPTR", 0x453f9cL, 6);
        xrefs("Matte", 0x450f4aL);
        xrefs("Picture", 0x45031eL);
        Function entry = currentProgram.getFunctionManager().getFunctionAt(toAddr(0x400000L));
        println("ENTRY " + entry);
        if (entry != null) {
            DecompInterface di = new DecompInterface();
            di.openProgram(currentProgram);
            DecompileResults res = di.decompileFunction(entry, 20, monitor);
            if (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null) {
                String[] lines = res.getDecompiledFunction().getC().split("\n");
                int lim = Math.min(lines.length, 30);
                for (int i = 0; i < lim; i++) {
                    println("DEC " + lines[i]);
                }
            } else {
                println("DEC incomplete");
            }
            di.dispose();
        }
    }
}
