// Negative ownership/bound/hash checks and actual caller/callee delay-slot execution.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import ghidra.program.model.address.AddressSet;

public class CheckFunctionFlowFixture extends GhidraScript {
    private void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }
    private void refused(String hash, String bound) throws Exception {
        var before = new AddressSet(getFunctionAt(toAddr(0x1000)).getBody());
        boolean rejected = false;
        try { runScript("RecoverFunctionFlow.java", new String[]{hash, "0x1000", bound}); }
        catch (IllegalArgumentException | IllegalStateException expected) { rejected = true; }
        require(rejected, "Unsafe recovery was accepted");
        require(before.equals(getFunctionAt(toAddr(0x1000)).getBody()), "Rejected recovery changed ownership");
        require(getInstructionAt(toAddr(0x1018)) == null, "Rejected recovery left decoded bytes");
    }
    private void exercise(int initial, boolean pause) throws Exception {
        EmulatorHelper emulator = new EmulatorHelper(currentProgram);
        try {
            emulator.writeRegister(emulator.getPCRegister(), 0x1000);
            emulator.writeRegister("a0", initial);
            emulator.writeRegister("v0", 99);
            emulator.writeRegister("v1", 0);
            emulator.writeRegister("ra", 0x3000);
            int steps = 0;
            boolean paused = false;
            while (emulator.getExecutionAddress().getOffset() != 0x3000) {
                require(++steps <= 48, "Caller failed to return within the real loop bound");
                require(emulator.step(monitor), "Emulation failed: " + emulator.getLastError());
                if (pause && emulator.getExecutionAddress().getOffset() == 0x1024) {
                    require(emulator.readRegister("v0").intValue() == 30 &&
                        emulator.readRegister("v1").intValue() == initial + 7,
                        "Call/alternate delay-slot effects lost before resume");
                    paused = true;
                }
            }
            require(!pause || paused, "Call continuation pause was not exercised");
            require(emulator.readRegister("v0").intValue() == 31 &&
                emulator.readRegister("v1").intValue() == initial + 7 &&
                emulator.readRegister("a0").intValue() == -1,
                "Caller/callee/return delay-slot results differ");
        } finally { emulator.dispose(); }
    }
    @Override
    public void run() throws Exception {
        require(currentProgram.getName().equals("call-slot-exit.elf"), "Synthetic fixture only");
        String hash = currentProgram.getExecutableSHA256();
        var callee = new AddressSet(getFunctionAt(toAddr(0x2000)).getBody());
        require(callee.equals(new AddressSet(toAddr(0x2000), toAddr(0x200b))),
            "Fixture callee is incomplete");
        // Ghidra nests script transactions: a negative rollback also aborts its
        // enclosing script at completion. Run positive recovery as a separate
        // headless postScript, then invoke this checker without arguments.
        if (getScriptArgs().length == 1 && getScriptArgs()[0].equals("negative")) {
            refused("0".repeat(64), "0x1030");
            refused(hash, "0x102c");
            // A foreign function inside the bound must not be swallowed.
            var foreign = createFunction(toAddr(0x1018), "synthetic_foreign");
            require(foreign != null, "Cannot create foreign owner negative control");
            foreign.setBody(new AddressSet(toAddr(0x1018), toAddr(0x101b)));
            clearListing(toAddr(0x1018), toAddr(0x101b));
            refused(hash, "0x1030");
            currentProgram.getFunctionManager().removeFunction(toAddr(0x1018));
            println("PASS bounded caller negative controls: hash, bound and foreign owner");
            return;
        }
        require(getFunctionAt(toAddr(0x1000)).getBody().equals(
            new AddressSet(toAddr(0x1000), toAddr(0x102f))), "Caller coverage differs from exact bound");
        require(callee.equals(getFunctionAt(toAddr(0x2000)).getBody()), "Callee ownership changed");
        exercise(0, false);
        exercise(2, true);
        println("PASS bounded caller: exact ownership, missing word, call, loop and resume");
    }
}
