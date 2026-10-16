// Add evidence-backed code entry points discovered during bounded runtime analysis.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class SeedAddressFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String argument : getScriptArgs()) {
            Address address = toAddr(Long.decode(argument));
            MemoryBlock block = currentProgram.getMemory().getBlock(address);
            if (block == null || !block.isExecute() || (address.getOffset() & 3) != 0)
                throw new IllegalArgumentException("Not an executable aligned address: " + argument);
            disassemble(address);
            if (getFunctionAt(address) == null && createFunction(address, "callback_" + address) == null)
                throw new IllegalStateException("Could not create function at " + argument);
            println("Evidence-backed entry point: " + address);
        }
        analyzeChanges(currentProgram);
    }
}
