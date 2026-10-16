// Export function metadata only into an explicitly selected local file.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressRangeIterator;
import java.util.TreeMap;
import java.util.ArrayList;
import java.util.Map;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.io.BufferedWriter;

public class ExportRecompMap extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Pass a fresh local CSV path");
        Path output = Path.of(args[0]);
        Files.createDirectories(output.toAbsolutePath().getParent());
        int count = 0;
        int functionCount = 0;
        TreeMap<Long, Integer> events = new TreeMap<>();
        FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
        while (functions.hasNext()) {
            monitor.checkCancelled();
            Function f = functions.next();
            if (f.isExternal() || f.getBody().isEmpty()) continue;
            functionCount++;
            events.merge(f.getEntryPoint().getOffset(), 0, Integer::sum);
            AddressRangeIterator ranges = f.getBody().getAddressRanges();
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
