// Regression fixture: a discontinuous function body in the original return42 ELF.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import java.util.Arrays;

public class SetSyntheticFragmentBody extends GhidraScript {
    @Override
    public void run() throws Exception {
        Address start = toAddr(0x1000);
        byte[] expected = {0x2a, 0, 2, 0x24, 8, 0, (byte)0xe0, 3, 0, 0, 0, 0};
        if (!Arrays.equals(getBytes(start, 12), expected))
            throw new IllegalStateException("This script only accepts our original synthetic fixture");
        disassemble(start);
        Function f = getFunctionAt(start);
        if (f == null) f = createFunction(start, "synthetic_fragment_body");
        if (f == null) throw new IllegalStateException("Could not create synthetic function");
        AddressSet body = new AddressSet(start, toAddr(0x1003));
        body.add(toAddr(0x1008), toAddr(0x100b));
        f.setBody(body);
        println("Synthetic body has two 4-byte chunks with a 4-byte hole");
    }
}
