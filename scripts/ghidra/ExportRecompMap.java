// Export function metadata only into an explicitly selected local file.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressRangeIterator;
import java.util.TreeMap;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashSet;
import java.util.ArrayDeque;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.io.BufferedWriter;

public class ExportRecompMap extends GhidraScript {
    private final HashSet<Long> selected = new HashSet<>();
    private boolean selectedFunction(Function f) {
        return f != null && (selected.isEmpty() || selected.contains(f.getEntryPoint().getOffset()));
    }
    private void addInstruction(AddressSet coverage, Instruction instruction) {
        if (instruction.getLength() != 4 || (instruction.getAddress().getOffset() & 3) != 0)
            throw new IllegalStateException("Not an aligned EE instruction: " + instruction.getAddress());
        coverage.add(instruction.getMinAddress(), instruction.getMaxAddress());
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        selected.clear();
        if (args.length < 1) throw new IllegalArgumentException("Pass a fresh local CSV path and optional exact function entries");
        for (int i = 1; i < args.length; i++) {
            long entry = Long.decode(args[i]);
            if (getFunctionAt(toAddr(entry)) == null || !selected.add(entry))
                throw new IllegalArgumentException("Missing or duplicate selected function: " + args[i]);
        }
        Path output = Path.of(args[0]);
        Files.createDirectories(output.toAbsolutePath().getParent());
        int count = 0;
        int functionCount = 0;
        TreeMap<Long, Integer> events = new TreeMap<>();
        AddressSet executable = new AddressSet();
        for (MemoryBlock block : currentProgram.getMemory().getBlocks())
            if (block.isExecute() && block.isInitialized())
                executable.add(block.getStart(), block.getEnd());
        // Shared epilogues can have valid incoming branches and instructions but
        // no Ghidra function owner. Include only code reached by explicit owned
        // flow edges, never the whole bounding span or arbitrary unowned bytes.
        ArrayDeque<Address> pending = new ArrayDeque<>();
        InstructionIterator instructions = currentProgram.getListing().getInstructions(executable, true);
        while (instructions.hasNext()) {
            monitor.checkCancelled();
            Instruction instruction = instructions.next();
            if (!selectedFunction(getFunctionContaining(instruction.getAddress()))) continue;
            for (Address target : instruction.getFlows())
                if (executable.contains(target) && getFunctionContaining(target) == null &&
                    getInstructionAt(target) != null) {
                    pending.add(target);
                    events.merge(target.getOffset(), 0, Integer::sum);
                }
        }
        AddressSet extra = new AddressSet();
        while (!pending.isEmpty()) {
            monitor.checkCancelled();
            Address address = pending.removeFirst();
            if (extra.contains(address) || !executable.contains(address) ||
                getFunctionContaining(address) != null) continue;
            Instruction instruction = getInstructionAt(address);
            if (instruction == null) continue;
            addInstruction(extra, instruction);
            for (int slot = 1; slot <= instruction.getDelaySlotDepth(); slot++) {
                Address delay = address.add(slot * 4L);
                Instruction delayInstruction = getInstructionAt(delay);
                if (delayInstruction != null && executable.contains(delay) &&
                    getFunctionContaining(delay) == null) addInstruction(extra, delayInstruction);
            }
            if (instruction.getFallThrough() != null) pending.add(instruction.getFallThrough());
            for (Address target : instruction.getFlows())
                if (executable.contains(target) && getFunctionContaining(target) == null) {
                    pending.add(target);
                    events.merge(target.getOffset(), 0, Integer::sum);
                }
        }
        AddressRangeIterator extraRanges = extra.getAddressRanges();
        while (extraRanges.hasNext()) {
            AddressRange range = extraRanges.next();
            events.merge(range.getMinAddress().getOffset(), 1, Integer::sum);
            events.merge(range.getMaxAddress().getOffset() + 1, -1, Integer::sum);
        }
        println("Reachable unowned instruction bytes: " + extra.getNumAddresses());
        FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
        while (functions.hasNext()) {
            monitor.checkCancelled();
            Function f = functions.next();
            if (!selectedFunction(f)) continue;
            if (f.isExternal() || f.getBody().isEmpty()) continue;
            AddressSetView codeBody = f.getBody().intersect(executable);
            if (codeBody.isEmpty()) continue;
            functionCount++;
            if (codeBody.contains(f.getEntryPoint()))
                events.merge(f.getEntryPoint().getOffset(), 0, Integer::sum);
            AddressRangeIterator ranges = codeBody.getAddressRanges();
            while (ranges.hasNext()) {
                AddressRange range = ranges.next();
                long start = range.getMinAddress().getOffset();
                long end = range.getMaxAddress().getOffset() + 1;
                if ((start & 3) != 0 || (end & 3) != 0 || end <= start)
                    throw new IllegalStateException("Non-instruction-aligned body range: " + range);
                events.merge(start, 1, Integer::sum);
                events.merge(end, -1, Integer::sum);
            }
        }
        // Partition the union of real body ranges at every function/range boundary.
        // This preserves all code fragments and never translates holes between them.
        ArrayList<Long> boundaries = new ArrayList<>(events.keySet());
        int depth = 0;
        long coveredBytes = 0;
        try (BufferedWriter writer = Files.newBufferedWriter(output, StandardCharsets.UTF_8,
                StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE)) {
            writer.write("Name,Start,End,Size\n");
            for (int i = 0; i + 1 < boundaries.size(); i++) {
                monitor.checkCancelled();
                long start = boundaries.get(i);
                long end = boundaries.get(i + 1);
                depth += events.get(start);
                if (depth <= 0) continue;
                writer.write(String.format("fn_%08X,0x%08X,0x%08X,%d\n",
                    start, start, end, end - start));
                coveredBytes += end - start;
                count++;
            }
        }
        println("Ghidra functions: " + functionCount + "; disjoint code records: " + count +
            "; covered bytes: " + coveredBytes + " -> " + output);
    }
}
