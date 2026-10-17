// Recover an observed leaf routine's bounded instruction flow, including long NOP runs.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.util.ArrayDeque;

public class RecoverLeafFunctionFlow extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException("Pass existing entry and exclusive analysis bound");
        Address entry = toAddr(Long.decode(args[0]));
        Address limit = toAddr(Long.decode(args[1]));
        if ((entry.getOffset() & 3) != 0 || (limit.getOffset() & 3) != 0 ||
            limit.compareTo(entry) <= 0 || limit.subtract(entry) > 0x10000)
            throw new IllegalArgumentException("Invalid bounded instruction region");
        Function function = getFunctionAt(entry);
        if (function == null) throw new IllegalArgumentException("Existing reviewed leaf entry required");
        AddressSet coverage = new AddressSet();
        ArrayDeque<Address> pending = new ArrayDeque<>();
        pending.add(entry);
        int count = 0;
        while (!pending.isEmpty()) {
            monitor.checkCancelled();
            Address address = pending.removeFirst();
            if (coverage.contains(address)) continue;
            if (address.compareTo(entry) < 0 || address.compareTo(limit) >= 0 || ++count > 4096)
                throw new IllegalStateException("Leaf flow left the explicit instruction bound");
            var block = currentProgram.getMemory().getBlock(address);
            if (block == null || !block.isInitialized() || !block.isExecute())
                throw new IllegalStateException("Leaf flow left executable initialized input");
            Function owner = getFunctionContaining(address);
            if (owner != null && !owner.equals(function))
                throw new IllegalStateException("Leaf flow entered another function");
            Instruction instruction = getInstructionAt(address);
            if (instruction == null) {
                disassemble(address);
                instruction = getInstructionAt(address);
            }
            if (instruction == null || instruction.getLength() != 4 || instruction.getFlowType().isCall())
                throw new IllegalStateException("Unsupported non-leaf or undecoded instruction at " + address);
            coverage.add(instruction.getMinAddress(), instruction.getMaxAddress());
            for (int slot = 1; slot <= instruction.getDelaySlotDepth(); slot++) {
                Address delay = address.add(slot * 4L);
                if (delay.compareTo(limit) >= 0) throw new IllegalStateException("Delay slot crosses bound");
                Instruction decoded = getInstructionAt(delay);
                if (decoded == null || decoded.getLength() != 4)
                    throw new IllegalStateException("Missing actual delay instruction");
                coverage.add(decoded.getMinAddress(), decoded.getMaxAddress());
            }
            if (instruction.getFallThrough() != null) pending.add(instruction.getFallThrough());
            for (Address target : instruction.getFlows()) pending.add(target);
        }
        function.setBody(coverage);
        println("Recovered bounded leaf " + entry + ": " + coverage.getNumAddresses() +
            " actual instruction bytes; max=" + coverage.getMaxAddress());
    }
}
