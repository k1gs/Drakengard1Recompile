// Recover a reviewed MIPS function's bounded flow without traversing its callees.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.util.ArrayDeque;
import java.util.HashSet;

public class RecoverFunctionFlow extends GhidraScript {
    private AddressSet region;
    private Function function;

    private Instruction instructionAt(Address address) throws Exception {
        monitor.checkCancelled();
        if (!region.contains(address, address.add(3)))
            throw new IllegalStateException("Flow or delay slot left the explicit bound: " + address);
        var block = currentProgram.getMemory().getBlock(address);
        if (block == null || !block.isInitialized() || !block.isExecute() ||
                !block.contains(address.add(3)))
            throw new IllegalStateException("Not initialized executable input: " + address);
        Function owner = getFunctionContaining(address);
        if (owner != null && !owner.equals(function))
            throw new IllegalStateException("Flow entered another function: " + address);
        Instruction instruction = getInstructionAt(address);
        if (instruction == null) {
            new DisassembleCommand(address, region, true).applyTo(currentProgram, monitor);
            instruction = getInstructionAt(address);
        }
        if (instruction == null || instruction.getLength() != 4)
            throw new IllegalStateException("Undecoded aligned instruction: " + address);
        return instruction;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3 || !args[0].matches("[0-9a-fA-F]{64}") ||
                !args[0].equalsIgnoreCase(currentProgram.getExecutableSHA256()))
            throw new IllegalArgumentException("Pass verified executable SHA-256, existing entry and exclusive bound");
        Address entry = toAddr(Long.decode(args[1]));
        Address limit = toAddr(Long.decode(args[2]));
        if ((entry.getOffset() & 3) != 0 || (limit.getOffset() & 3) != 0 ||
                limit.compareTo(entry) <= 0 || limit.subtract(entry) > 0x10000)
            throw new IllegalArgumentException("Invalid bounded instruction region");
        region = new AddressSet(entry, limit.subtract(1));
        function = getFunctionAt(entry);
        if (function == null || !region.contains(function.getBody()))
            throw new IllegalArgumentException("Existing function must fit the reviewed bound");
        int transaction = currentProgram.startTransaction("Recover bounded function flow");
        boolean commit = false;
        try {
            AddressSet coverage = new AddressSet();
            // Delay-slot coverage does not mean we visited an alternate normal entry.
            HashSet<Address> visited = new HashSet<>();
            ArrayDeque<Address> pending = new ArrayDeque<>();
            pending.add(entry);
            int calls = 0;
            while (!pending.isEmpty()) {
                Address address = pending.removeFirst();
                if (!visited.add(address)) continue;
                if (visited.size() > 4096)
                    throw new IllegalStateException("Instruction traversal budget exceeded");
                Instruction instruction = instructionAt(address);
                var flow = instruction.getFlowType();
                coverage.add(instruction.getMinAddress(), instruction.getMaxAddress());
                for (int slot = 1; slot <= instruction.getDelaySlotDepth(); slot++) {
                    Instruction delay = instructionAt(address.add(slot * 4L));
                    if (!delay.getFlowType().isFallthrough())
                        throw new IllegalStateException("Control transfer in a delay slot");
                    coverage.add(delay.getMinAddress(), delay.getMaxAddress());
                }
                if (flow.isCall()) {
                    Address[] targets = instruction.getFlows();
                    if (flow.isComputed() || targets.length != 1 || instruction.getFallThrough() == null)
                        throw new IllegalStateException("Unsupported unresolved or non-returning call: " + address);
                    var block = currentProgram.getMemory().getBlock(targets[0]);
                    if (block == null || !block.isExecute() || !block.isInitialized() ||
                            (targets[0].getOffset() & 3) != 0)
                        throw new IllegalStateException("Call target is not executable aligned input");
                    calls++;
                    // Only the architectural return continuation belongs to this traversal.
                    pending.add(instruction.getFallThrough());
                } else {
                    if (flow.isTerminal()) {
                        var register = instruction.getRegister(0);
                        if (!instruction.getMnemonicString().equalsIgnoreCase("jr") ||
                                register == null || !register.getName().equals("ra"))
                            throw new IllegalStateException("Unsupported terminal instruction: " + address);
                    } else if (flow.isComputed()) {
                        throw new IllegalStateException("Unresolved computed flow: " + address);
                    }
                    if (instruction.getFallThrough() != null) pending.add(instruction.getFallThrough());
                    for (Address target : instruction.getFlows()) pending.add(target);
                }
            }
            if (!coverage.contains(function.getBody()))
                throw new IllegalStateException("Recovery would discard existing owned instruction bytes");
            function.setBody(coverage);
            commit = true;
            println("Recovered bounded function " + entry + ": " + coverage.getNumAddresses() +
                " actual instruction bytes; calls=" + calls + "; max=" + coverage.getMaxAddress());
        } finally {
            currentProgram.endTransaction(transaction, commit);
        }
    }
}
