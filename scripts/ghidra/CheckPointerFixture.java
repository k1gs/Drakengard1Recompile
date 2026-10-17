// Assert raw pointer candidate behavior without depending on automatic body analysis.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;

public class CheckPointerFixture extends GhidraScript {
    @Override
    public void run() throws Exception {
        if (!currentProgram.getName().equals("pointers.elf") ||
            currentProgram.getMemory().getInt(toAddr(0x3000)) != 0x2000)
            throw new IllegalStateException("Wrong fixture");
        Function target = getFunctionAt(toAddr(0x2000));
        if (target == null || getFunctionAt(toAddr(0x1004)) != null ||
            currentProgram.getFunctionManager().getFunctionCount() != 2)
            throw new IllegalStateException("Unexpected pointer candidates");
        target.setBody(new AddressSet(toAddr(0x2000), toAddr(0x200b)));
        println("Exactly one uncovered executable pointer candidate recovered");
    }
}
