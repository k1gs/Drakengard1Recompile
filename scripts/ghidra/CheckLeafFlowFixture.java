// Exercise recovered real instruction flow, including pause/resume at a loop edge.
// @category DrakengardRecomp
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import ghidra.program.model.listing.Function;

public class CheckLeafFlowFixture extends GhidraScript {
    private void require(boolean value, String message) {
        if (!value) throw new IllegalStateException(message);
    }
    private void exercise(int initial, boolean pause) throws Exception {
        EmulatorHelper emulator = new EmulatorHelper(currentProgram);
        try {
            emulator.writeRegister(emulator.getPCRegister(), 0x1000);
            emulator.writeRegister("a0", initial);
            emulator.writeRegister("v0", 7);
            emulator.writeRegister("v1", 0);
            emulator.writeRegister("ra", 0x2000);
            int steps = 0;
            boolean paused = false;
            while (emulator.getExecutionAddress().getOffset() != 0x2000) {
                require(++steps <= 32, "Recovered leaf failed to return within its real loop bound");
                require(emulator.step(monitor), "Synthetic instruction failed: " + emulator.getLastError());
                if (pause && !paused && emulator.getExecutionAddress().getOffset() == 0x1000) {
                    require(emulator.readRegister("a0").intValue() == initial - 1,
                        "Branch delay instruction was lost before yield");
                    require(emulator.readRegister("v1").intValue() == 1 &&
                        emulator.readRegister("v0").intValue() == 7,
                        "Jump delay/epilogue executed incorrectly before yield");
                    paused = true; // resume by stepping the same architectural state
                }
            }
            require(!pause || paused, "Expected loop-edge pause was not exercised");
            require(emulator.readRegister("v0").intValue() == 43, "Return delay/epilogue result mismatch");
            require(emulator.readRegister("v1").intValue() == initial + 1,
                "Alternate delay-slot entry was skipped or executed twice");
            require(emulator.readRegister("a0").intValue() == -1, "Exit branch delay state mismatch");
        } finally { emulator.dispose(); }
    }
    @Override
    public void run() throws Exception {
        require(currentProgram.getName().equals("delay-slot-exit.elf"), "Synthetic fixture only");
        Function entry = getFunctionAt(toAddr(0x1000));
        require(entry != null && entry.getBody().contains(toAddr(0x1018), toAddr(0x1023)),
            "Missing reachable epilogue after alternate delay-slot entry");
        require(entry.getBody().getMinAddress().equals(toAddr(0x1000)) &&
            entry.getBody().getMaxAddress().equals(toAddr(0x1023)), "Recovery crossed the exact leaf boundary");
        require(getInstructionAt(toAddr(0x101c)).getFlowType().isTerminal(), "Expected actual return instruction");
        exercise(0, false);
        exercise(2, true);
        println("PASS delay-slot exit: immediate exit, busy loop, pause/resume, return and both delay effects");
    }
}
