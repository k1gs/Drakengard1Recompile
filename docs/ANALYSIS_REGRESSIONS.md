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
