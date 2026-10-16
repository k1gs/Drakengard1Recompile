// Discover uncovered executable entry points from existing Ghidra data references.
// These are analysis candidates, not claims of recovered API names or correctness.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class SeedReferencedCode extends GhidraScript {
    @Override
    public void run() throws Exception {
        AddressSet executable = new AddressSet();
        for (MemoryBlock block : currentProgram.getMemory().getBlocks())
            if (block.isExecute()) executable.add(block.getStart(), block.getEnd());
        AddressIterator destinations = currentProgram.getReferenceManager()
            .getReferenceDestinationIterator(executable, true);
        int count = 0;
        while (destinations.hasNext()) {
            monitor.checkCancelled();
            Address address = destinations.next();
            if ((address.getOffset() & 3) != 0 || getFunctionContaining(address) != null) continue;
            boolean dataReference = false;
            ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(address);
            while (references.hasNext()) {
                Reference reference = references.next();
                if (reference.getReferenceType().isData()) dataReference = true;
            }
            if (!dataReference) continue;
            disassemble(address);
            if (getInstructionAt(address) == null) continue;
            if (createFunction(address, "referenced_code_" + address) != null) {
                count++;
                println("Candidate executable entry from data reference: " + address);
            }
        }
        analyzeChanges(currentProgram);
        println("Created referenced-code candidates: " + count);
    }
}
