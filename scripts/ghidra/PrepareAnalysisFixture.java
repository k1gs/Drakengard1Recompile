// Establish deliberately incomplete analysis only for our exact synthetic fixtures.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import java.util.Arrays;

public class PrepareAnalysisFixture extends GhidraScript {
    @Override
    public void run() throws Exception {
        String name = currentProgram.getName();
        if (name.equals("delay-slot-exit.elf")) {
            int[] words = {0x18800004, 0x2484ffff, 0, 0, 0x08000400,
                0x24630001, 0x2402002a, 0x03e00008, 0x24420001};
            for (int i = 0; i < words.length; i++)
                if (currentProgram.getMemory().getInt(toAddr(0x1000 + i * 4)) != words[i])
                    throw new IllegalStateException("Wrong delay-slot exit fixture bytes");
            disassemble(toAddr(0x1000));
            Function entry = getFunctionAt(toAddr(0x1000));
            if (entry == null) entry = createFunction(toAddr(0x1000), "synthetic_entry");
            if (entry == null) throw new IllegalStateException("Cannot create fixture entry");
            entry.setBody(new AddressSet(toAddr(0x1000), toAddr(0x1017)));
            clearListing(toAddr(0x1018), toAddr(0x1023));
            println("Prepared truncated synthetic delay-slot exit");
            return;
        }
        boolean pointers = name.equals("pointers.elf");
        byte[] return42 = {0x2a, 0, 2, 0x24, 8, 0, (byte)0xe0, 3, 0, 0, 0, 0};
        byte[] branch = {3, 0, 0, 0x10, 0, 0, 0, 0};
        if (pointers) {
            if (!Arrays.equals(getBytes(toAddr(0x1000), 12), return42) ||
                !Arrays.equals(getBytes(toAddr(0x2000), 12), return42) ||
                currentProgram.getMemory().getInt(toAddr(0x3000)) != 0x2000)
                throw new IllegalStateException("Wrong pointer fixture bytes");
        } else if (!name.equals("shared-epilogue.elf") ||
                !Arrays.equals(getBytes(toAddr(0x1000), 8), branch) ||
                !Arrays.equals(getBytes(toAddr(0x1010), 12), return42)) {
            throw new IllegalStateException("Wrong shared epilogue fixture");
        }
        disassemble(toAddr(0x1000));
        Function entry = getFunctionAt(toAddr(0x1000));
        if (entry == null) entry = createFunction(toAddr(0x1000), "synthetic_entry");
        if (entry == null) throw new IllegalStateException("Cannot create fixture entry");
        entry.setBody(new AddressSet(toAddr(0x1000), toAddr(pointers ? 0x100b : 0x1007)));
        if (pointers) {
            currentProgram.getFunctionManager().removeFunction(toAddr(0x2000));
            clearListing(toAddr(0x2000), toAddr(0x200b));
        } else {
            currentProgram.getFunctionManager().removeFunction(toAddr(0x1010));
            disassemble(toAddr(0x1010));
        }
        println("Prepared original synthetic analysis fixture: " + name);
    }
}
