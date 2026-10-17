// Unclassified file-backed data words may hold callback pointers without Ghidra refs.
// Results are code candidates, not recovered symbols or validated call signatures.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;
import java.util.TreeMap;
import java.util.Map;

public class SeedDataPointers extends GhidraScript {
    private boolean eeCode(MemoryBlock block) {
        if (block == null || !block.isInitialized() || !block.isExecute()) return false;
        String name = block.getName();
        return !name.startsWith(".vutext") && !name.startsWith(".DVP.overlay");
    }

    @Override
    public void run() throws Exception {
        TreeMap<Long, Address> candidates = new TreeMap<>();
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || block.isExecute()) continue;
            long first = (block.getStart().getOffset() + 3) & ~3L;
            long last = block.getEnd().getOffset() - 3;
            for (long offset = first; offset <= last; offset += 4) {
                monitor.checkCancelled();
                Address source = block.getStart().getAddressSpace().getAddress(offset);
                long value = Integer.toUnsignedLong(currentProgram.getMemory().getInt(source));
                if ((value & 3) != 0) continue;
                Address target = toAddr(value);
                if (!eeCode(currentProgram.getMemory().getBlock(target)) ||
                    getFunctionContaining(target) != null) continue;
                candidates.putIfAbsent(value, source);
            }
        }
        int count = 0;
        for (Map.Entry<Long, Address> entry : candidates.entrySet()) {
            monitor.checkCancelled();
            Address target = toAddr(entry.getKey());
            if (getFunctionContaining(target) != null) continue;
            disassemble(target);
            if (getInstructionAt(target) == null) continue;
            if (createFunction(target, "data_pointer_code_" + target) != null) {
                count++;
                println("Candidate data pointer: " + entry.getValue() + " -> " + target);
            }
        }
        analyzeChanges(currentProgram);
        println("Created data-pointer code candidates: " + count);
    }
}
