# Original synthetic Ghidra regressions

These checks use invented MIPS instructions and data. No game/BIOS or generated game code
is required. Use the pinned Ghidra/R5900 plugin and Java environment configured locally.
All outputs are private in work/; commands require fresh fixture/project/output names.

## Raw data pointers

```powershell
python scripts/make_analysis_fixture.py pointers
& tools/ghidra_12.1.4_PUBLIC/support/analyzeHeadless.bat `
  work/ghidra-projects pointer-regression-002 `
  -import work/analysis-pointers-001/pointers.elf -processor r5900:LE:32:default `
  -noanalysis -scriptPath scripts/ghidra `
  -postScript PrepareAnalysisFixture.java -postScript SeedDataPointers.java `
  -postScript CheckPointerFixture.java `
  -postScript ExportRecompMap.java "$PWD/work/analysis-pointers-001/functions.csv"
```

Expected: exactly one new candidate, from data address `0x3000` to executable `0x2000`.
The table also contains a non-executable target, an unaligned target, an unmapped target,
and a target already inside the entry function. These must not become new functions.
The assertion script checks the function set and assigns the synthetic target's known body;
automatic body discovery is intentionally disabled. CSV must contain exactly:

```csv
Name,Start,End,Size
fn_00001000,0x00001000,0x0000100C,12
fn_00002000,0x00002000,0x0000200C,12
```

## Reachable shared epilogue

```powershell
python scripts/make_analysis_fixture.py shared-epilogue
& tools/ghidra_12.1.4_PUBLIC/support/analyzeHeadless.bat `
  work/ghidra-projects epilogue-regression-001 `
  -import work/analysis-shared-epilogue-001/shared-epilogue.elf `
  -processor r5900:LE:32:default -noanalysis -scriptPath scripts/ghidra `
  -postScript PrepareAnalysisFixture.java `
  -postScript ExportRecompMap.java "$PWD/work/analysis-shared-epilogue-001/functions.csv"
```

The entry branches over an eight-byte gap into valid instructions without a function owner.
Expected: 12 reachable unowned instruction bytes, two CSV records, and no gap bytes:

```csv
Name,Start,End,Size
fn_00001000,0x00001000,0x00001008,8
fn_00001010,0x00001010,0x0000101C,12
```

The exporter follows existing explicit flow edges from owned instructions, then follows
existing unowned instructions, fallthrough and delay slots. It never fills a bounding span
or invents instructions for undefined bytes. Explicit branch destinations split fragments
so the runtime can dispatch to each actual entry.

The previous discontinuous-body fixture (SetSyntheticFragmentBody.java) must still export
only `[0x1000,0x1004)` and `[0x1008,0x100c)`. Its unowned gap has no explicit incoming branch,
so the new reachability pass must leave it out. This was rerun successfully locally.

## Validation limits

These are local Ghidra checks, separate from Python/CI and native gameplay. The raw pointer
pass can still discover false code candidates from ordinary numeric data. Native execution
must validate real call paths; generated code and loader success do not establish a menu.

## Explicit selection and bounded long-NOP leaf flow

`ExportRecompMap.java OUTPUT.csv [ENTRY ...]` can restrict export to exact existing function
entries. Unknown/duplicate entries are rejected. Without entries its original full export is retained;
selected functions still include explicitly reached shared code and preserve holes.

`RecoverLeafFunctionFlow.java ENTRY EXCLUSIVE_BOUND` follows actual instructions from an existing
leaf entry through fallthrough, branch edges and delay slots. It rejects calls, other owners,
uninitialized/non-executable input and flow outside the explicit bound. The limit is an analysis
bound, not a request to include every byte inside a bounding interval.

```powershell
python scripts/make_analysis_fixture.py long-nop
# Import this original fixture with the pinned R5900 processor and seed entry 0x1000.
# Then run RecoverLeafFunctionFlow.java 0x1000 0x110c before exporting entry 0x1000.
```

Expected coverage: exactly `[0x1000,0x110c)`, 268 bytes including 64 original NOPs and the real
return/delay slot. This fixture passed locally. Selected and full pointer exports produced the
expected one/two records, and both shared-epilogue modes retained the eight-byte hole.

## Exit into a backward jump's delay slot

An EE conditional branch may target the delay instruction of a different jump.
On that incoming path, the instruction executes once and falls through normally;
the other jump must not take effect. Automatic analysis can retain the slot but
leave its ordinary fallthrough undefined, producing an incomplete leaf map.

```powershell
python scripts/make_analysis_fixture.py delay-slot-exit
& tools/ghidra_12.1.4_PUBLIC/support/analyzeHeadless.bat `
  work/ghidra-projects delay-slot-regression-001 `
  -import work/analysis-delay-slot-exit-001/delay-slot-exit.elf `
  -processor r5900:LE:32:default -noanalysis -scriptPath scripts/ghidra `
  -postScript PrepareAnalysisFixture.java `
  -postScript RecoverLeafFunctionFlow.java 0x1000 0x1024 `
  -postScript CheckLeafFlowFixture.java `
  -postScript ExportRecompMap.java "$PWD/work/analysis-delay-slot-exit-001/functions.csv" 0x1000
```

Use the pinned JDK21/Ghidra environment and fresh project/output names. The
preparation script deliberately omits `[0x1018,0x1024)` from the function.
Without recovery, the behavioral check reports `Missing reachable epilogue`.
Headless can return exit0 after a script exception: inspect the actual PASS/error
markers and export, not just the process exit code.

After bounded recovery, the map is exactly `[0x1000,0x1024)`,36 instruction bytes.
`CheckLeafFlowFixture` executes the synthetic instructions with the pinned Ghidra
emulator: immediate exit and two busy iterations both return43, preserving branch
and return delay effects. It pauses at the loop edge with a0=1/v1=1/v0=7, then
continues the same architectural state to a0=-1/v1=3/v0=43. An extra or omitted
execution of the alternate-entry delay instruction changes those results.
This is analysis/control-flow evidence, not a full native scheduler test.

The observed Europe leaf at0x297E44 used this recovery with exclusive bound
0x2983E8. Its old map stopped at0x298154 and also held two separate reachable
fragments. The recovered map owns the real complete leaf through its return and
stack-restoring delay instruction; unrelated functions are unchanged. The current
runner consumes fresh `lift-008`, generated from `functions-ghidra-009.csv`.
Inputs, generated units, Ghidra projects and instruction exports remain private.

For the already verified local Europe project, the read-only recovery/export
and fresh lift used for this checkpoint were:

```powershell
& tools/ghidra_12.1.4_PUBLIC/support/analyzeHeadless.bat `
  work/ghidra-projects drakengard-eu-001 -process SLES_523.22 -readOnly -noanalysis `
  -scriptPath scripts/ghidra -postScript RecoverLeafFunctionFlow.java 0x297E44 0x2983E8 `
  -postScript ExportRecompMap.java "$PWD/work/functions-ghidra-009.csv"
python scripts/lift.py --input-complete --map work/functions-ghidra-009.csv --run-id lift-008
```

These paths must be fresh when regenerating; never overwrite old evidence or
modify original input/upstream/generated files. This is a full map export, not
the optional selected-entry export used by the synthetic check.
