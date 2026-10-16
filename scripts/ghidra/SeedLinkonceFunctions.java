// Seed only executable named linkonce sections; all generated analysis stays local.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.address.Address;

public class SeedLinkonceFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        int seeded = 0;
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            monitor.checkCancelled();
            if (!block.isExecute() || !block.getName().startsWith(".gnu.linkonce.t.")) continue;
            Address start = block.getStart();
            if (getFunctionAt(start) != null) continue;
            disassemble(start);
            String name = "linkonce_" + start.toString();
            if (createFunction(start, name) != null) seeded++;
        }
        println("Executable linkonce function seeds: " + seeded);
    }
}
