# Continuation handoff: nonzero ADPCM, startup logos, EE return failure

## Current continuation: intra BDEC, real scratchpad transport, automatic startup

This section supersedes the older IPU checkpoint below. Preserve all pre-existing
INTC/write-watch/signed-branch work and `.vscode/`. Implementation used no Computer
Use, upstream edits or generated-code edits. The user subsequently authorized
publishing this source checkpoint; `.vscode/` and private evidence stay excluded.
The user explicitly authorized
automating the no-card startup warning; `scripts/native_start.py --run N` now
supplies bounded ordinary Left/Cross and verifies SIO2 press/release. It sends no
more startup buttons after the first real IPU command. Default ceiling180s,
five attempts; no opt-in/ended run/missing reports/timeouts fail explicitly.

Implemented `native/runner/ipu_bdec.h` and the BDEC state machine in `ee_ipu.h`:
MPEG2 intra, IVF1, normal scan, frame/field DCT, DC predictors, VLC/escape,
inverse quantization and an original mathematical inverse DCT. The transform is
functional Q20 separable cosine arithmetic; bit-exact PS2 IDCT rounding is not
claimed. MPEG1, IVF0, alternate scan and non-intra decode still refuse. Ten
redundant long AC encodings accepted by the pinned reference are covered.

Output is the real Y16x16/Cb8x8/Cr8x8 sample layout, clipped0..255 and widened
to16 bits,768 bytes per macroblock. An eight-QW output FIFO and48-QW staged
macroblock feed from-IPU DMA/CPU128 reads. `ee_ipu_dma.h` now drains output and
input fairly; `firmware_functions.cpp` binds the actual runtime scratchpad.
SPR MADR0x80001800 writes scratchpad0x1800 rather than aliased main RAM.
Empty/partial transfers retain state. Completion and IRQ edges depend on real
data transfer and trailing stream/TOP handling. Unsupported malformed run
overflow fails closed rather than using permissive reference implicit ends.

Validation: native Release builds001/002 passed; final diagnostics build003
also passed. Bootstrap CTest8/8 (`work/ipu-bdec-bootstrap-test-005.log`), Python55
(`work/ipu-bdec-python-test-002.log`), strict IOP CTest, doctor and public-tree gate
passed. Private exhaustive AC comparison matched all65536 prefixes (65520 valid,
16 invalid); `work/ipu-bdec-research-001/compare_prefixes.py` reproduces comparison
against pinned PCSX2v2.8.2 table data using raw harness output003. Report:
`work/ipu-bdec-research-001/report-final.json`. Initial long-code defects were
fixed before the native runs. Synthetic coverage includes observable DC/AC
pixels, predictor continuity, starvation, FB-once, field DCT, FIFO pressure,
reset, partial DMA, exact scratch bytes, unchanged main RAM and invalid SPR aliases.

Native214/215 used binary SHA
`03ab86191fa2902e0da569ebdbf46d666254bac0c20888a860216a667fd33d1a`.
Both exited6 at the fixed512M IOP budget before IPU commands. In214 an automation
UTF8/CRLF offset mistake delayed Cross32s; actual Left/Cross SIO2 reports still
occurred. Its final GS image was the no-card Yes/No warning, not a menu. The
corrected short sequence in215 also did not establish warning acceptance.
Neither run verifies BDEC. Frame1200 is not a semantic readiness marker.

Native216 uses diagnostics build003 SHA
`4a2c5fa2ac54fa8a85681d825a28b437e8ef52aef6d613f2826c46ce2395247c`,
300s ceiling/512M/host pad and `DRAKENGARD_FRAME_INTERVAL=600`. This optional
diagnostic saves at most32 direct GS images; it does not change guest pixels or
state. The startup helper started at frame1800; real press/releases were checked.
Direct GS image1200 showed Accessing,1800 the no-card warning,3000 Square Enix.
Thus216 actually passed the warning. These are raw GS observations, not an
independent host-window inspection and not menu/gameplay evidence.
Native216 completed: exit6, `timed_out=false`. The helper observed actual IPU
startup and stopped; all ten startup press/release pairs of report transitions
were checked (five Left/Cross attempts). BDEC passed the old213 refusal and
logged at least512 real48-QW transfers, alternating scratchpad0x1800/0x3300.
Their logged FNV1a64 `55376818ea38df25` independently matches a macroblock with
Y=16/Cb=Cr=128, consistent with the black initial movie image. This hash check
is not a general pixel/IDCT equivalence proof. Final direct GS frame was black,
640x508; no menu, rendered nonblack movie or gameplay is claimed.

First next refusal: VDEC command `0x34000000`, table1 after the intra path.
The existing guard proves a non-I PCT (>1), but the exact PCT was not logged;
do not label it P or B without observation. Next experiment: add a diagnostic
of CTRL/PCT at that refusal, then implement the observed macroblock-type table
and follow the next real non-intra/motion contract. Next unused run217; verify
directory/processes first. Use the same bounded launcher profile and run
`python scripts/native_start.py --run 217` alongside it. Evidence:
`work/native-run-216/{runtime.log,result.json,pad-control-events.jsonl}`,
`work/ipu-bdec-research-001/run216-startup.log`, and periodic/final GS images.

Do not publish or claim a complete MPEG decoder. Audio, menu, movement/combat,
saves and recovery remain unverified. No forced ready flags, RA repair, invented
stream prefix or dummy DMA completion was introduced. Evidence stays ignored.

## Current continuation: IPU transport/VLC and control without Computer Use

This section supersedes the historical sections below. The existing INTC/W1C
work was preserved; original inputs, upstream and generated lifts remain unchanged.
The user now explicitly requests **no Computer Use**. Long-term renderer direction
is Vulkan; no renderer migration is part of this device change.

- The previous agent's IPU model/DMA headers were drafts: MMIO integration was
  incomplete. `cmake/memory.cmake` now includes `ipu.cmake` and routes 32/64-bit
  registers, CPU128 input, real completion causes3/4 and IRQ8 through the owner.
- BCLR/FDEC/SETIQ/SETVQ/SETTH operate on real FIFO/buffer data. FDEC skips FB
  once, then peeks32 bits without consuming them; TOP is latched. Normal DMA
  respects backpressure and master/suspend. Zero normal QWC means65536 quadwords.
  Empty from-IPU output stalls; no decoded output is fabricated.
- Native210 passed the old start-code-search loop using real DMA/header bits and
  table loading. It stopped with a generic unsupported-command refusal. Native211
  identified the next real command as `0x30000000` (VDEC MBAI), exit6/no timeout.
- Added original VDEC table0 prefix decoder with all33 independent MPEG code
  vectors, escape/stuffing, ECD/error and staged TOP refill. Native212 passed this
  path and reached `0x34000000` (MBT) with PCT=I, exit6/no timeout. Implemented the
  two I-picture MBT codes; their DATA is the modes mask, unlike MBAI's length/token.
  Other picture types and motion/DMVector tables remain explicit refusals.
- Added opt-in `--pad-control` and `scripts/native_pad.py`: bounded, sequenced
  host-side digital-controller commands. Duplicate file reads never extend holds.
  This supplies normal snapshots to DualShock2/SIO2; no Windows input automation,
  guest RAM changes or ready-flag injection. Native212 confirmed FF7F/BFFF and
  release reports. Input events stay in each run's `pad-control-events.jsonl`.
- Latest native build005 and bootstrap build/test008 passed. Bootstrap CTest5/5
  includes signed branches, IPU transport, VLC and host controller. Python51 passed
  (`work/ipu-python-test-002.log`) in normal Windows; sandbox atomic replacement
  was denied and recorded, not hidden. Strict IOP CTest and doctor passed too.
- Native213 completed the latest actual experiment (300s ceiling,512M,
  `--pad-control`). It passed I-MBT (`DATA=1`, BP0x50) and stopped at BDEC
  **0x2C010000**, exit6 with `timed_out=false`. It received Left/Cross via the
  direct controller tool after the runner's frame1200 diagnostic marker.

Evidence: `work/native-run-210/` through `work/native-run-213/`,
`work/native-pad-210/input.json`, `work/native-pad-211/input.json`, and the direct
control event logs in212/213. Native210 executable SHA:
`252f0ed34df33a1c78dd19d49520e52464d2106df03b6753e24d5bd5a15cbff7`;
native211: `6f8446842a279d0401fe5cef3d6dbb8730756eba5e5b6a0752e3a231b59e433e`;
native212: `71a19b668c4d7cd528fbf84e4f5d5e08e596540dbcb693b0347c42a6d94e6d00`.
Native213 executable SHA:
`09c9df4e0bed30101cb761ea97f0dff30378273672a5aebeed3b424753894bab`.
Final GS images210 and212 were visually inspected and black. The last actual
window inspection before the no-Computer-Use request showed Project Dragonsphere,
not a menu. Later direct GS images do not establish independent host-window review.

The next work is the confirmed BDEC command from native213. Its flags are FB0,
QSC1, DT0, DCR1 and MBI1. CTRL was0x01620008: PCT=I, MPEG2, IVF1, QST1,
IDP2, ordinary scan. These fields need true VLC/DC/AC, inverse quantization,
inverse transform and output-FIFO state; existing table/VDEC support is not enough.
The guest armed from-IPU DMA with MADR0x80001800 and QWC0x30 before BDEC.
That means actual **scratchpad destination** support is also needed when output
is implemented; do not mask it to main RAM or complete an empty output transfer.
Next unused run is214; check its directory and processes before use. Example:

```powershell
python scripts/run_native.py --input-complete --run 214 --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --pad-control --iop-runtime-millions 512
```

Once the guest reaches the no-card input phase, use `scripts/native_pad.py` for
Left and Cross, and verify the matching SIO2 press/release reports. Direct-input
automation used in213 triggered on real log marker `[diagnostic] frame=1200 `,
with700ms holds separated by1.2s. This is a reproduction recipe for this checkpoint,
not a universal semantic readiness predicate. Use direct GS captures to inspect
frames; do not resume Computer Use.
Do not call macroblock transport a completed MPEG decoder or substitute a movie
skip. Keep unsupported behavior closed and add state/PCM/pixel/output-FIFO proof
for each implemented layer. Native/reference runs must remain sequential.
No commit/push was made during this continuation; preserve the other agent's dirty
INTC/write-watch/signed-branch work and unrelated `.vscode/`.
See [research workflow](RESEARCH_WORKFLOW.md) for direct tool usage.

## Resumed checkpoint: INTC acknowledgement and the next device blocker

This section supersedes the historical resume boundary below. Work resumed from
published `ef1f5fe`; Computer Use was subsequently reauthorized. No new commit or
push has been made. Original inputs, the pinned upstream checkout and generated
guest code remain unchanged.

- The original CPU write-watch was ineffective: the forced PCH loaded the
  absolute upstream macro header before the include-directory shadow. The
  build-local overlay now replaces that exact PCH entry. Actual macro execution
  and native201 verified the hook, rather than merely a successful compilation.
- A bounded last-128 changed-store history now retains PC, RA, SP, operation,
  destination and watched-slot before/after values. Invalid transfers and normal
  executor exit dump the retained history. Scheduler scalar stores and SIF0
  payloads overlapping the slot are also covered.
- Native204 reproduced the bad return. Its **last CPU store** to `0x11FE70`
  saved the correct `0x301D24`; the eventual dump contained flag-like `1` there
  and a tick-like value at `0x11FE78`. Earlier ordinary stack saves of `1` were
  not evidence of corruption of the final live frame.
- Definite device bug: `INTC_STAT` (`0x1000F000`) fell through to ordinary MMIO
  storage. Guest VSync code at `0x31FF80` acknowledges `4`, then polls that bit
  or the flag registered with `SetVSyncFlag`. Ordinary storage makes the
  acknowledgement manufacture a VBlank, allowing an early return while the
  scheduler still holds pointers to the function's stack locals.
- `ee_intc_status.h`, the existing single-owner EE-facing bridge and build-local
  memory/scheduler overlays now implement write-one-to-clear acknowledgement
  and latch real scheduler causes before handler-mask filtering. Both 32-bit
  and 64-bit status reads use the same pending state. This is not a complete
  raw INTC_MASK or cycle-accurate interrupt-controller implementation.
- A private protocol smoke executed the unchanged translated VSync, syscall,
  DI and EI functions with real EE context/macros and a bounded device harness:
  old semantics returned before VBlank and a later flag write corrupted reused
  saved RA to `1`; new semantics yielded until the event, returned tick `1`, and
  preserved reused RA `0x301D24`. This is causal protocol evidence, **not** a
  full-runtime or hardware timing proof. Log: `work/ee-vsync-protocol-001/result.log`.
- Native207 delivered actual Left/Cross reports (`FF7F`/`BFFF`) and reached an
  IPU-related wait at `0x3076A0`; no invalid-transfer record occurred within the
  300-second bound. It timed out: this is not a successful menu run.
- Native208 repeated the final build **without write-watch**, with actual
  Left/Cross guest reports. It also timed out after300s in the IPU search path,
  last sampled PC `0x3072B8`, RA `0x3076B4`, SP `0x11FD80`; no invalid transfer.
  Binary SHA-256:
  `fee5dbd67a544608944161eb69b252200d204f54626973fc93ed8505e1e5c882`.
  The observed native window was dark with a brown strip, not a menu.
- Verification passed: final native Release build, 47 Python tests, doctor,
  all five documented C++ smoke builds/CTest suites, strict IOP CTest and the
  public-tree gate. Private validation logs: `work/ee-intc-validation-001/`.
  Temporary smoke/input helper sources were removed; private proof binaries,
  hashes, logs and captures remain ignored. No commit or push.

The next device gap is IPU: command writes currently remain passive stored
values, and reads mirror them rather than perform bitstream decoding. Native207
shows `r3=0x40000008` in the start-code search loop. Do not force the prefix,
skip the movie, alter the return address, or claim menu/audio/gameplay acceptance.

INTC semantics reference:
[PCSX2 v2.8.2 register writes](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/HwWrite.cpp).

## Historical resume boundary at publication

This supersedes the earlier same-day handoff for the current working tree.
Implementation began at `5a17b6b`; this handoff accompanies the next checkpoint.
The user subsequently authorized publishing this result. Preserve the
working tree, including unrelated `.vscode/`. Do not reset or stage everything.
The user stopped Computer Use with physical Escape, then requested that the
continuation documentation be saved. No further native run was started afterward.
An already running native build finished with compilation errors. The subsequent
publication preparation corrected the Windows macro collision; see validation.

Read `AGENTS.md`, this file, the final section of ignored `CURRENT_STATE.md`, and
the latest section of `docs/IOP_BOOTSTRAP.md`. `docs/ROADMAP.md` is absent.
Keep inputs read-only and new captures, dumps, logs and generated guest code
private. Do not continue the unrelated Drakengard 3 GPU snapshot/cache task.
The pinned `tools/ps2recomp` checkout was left unchanged; runtime adaptations
belong to the original build-local overlays under `native/runner/`.

If using subagents, the user's explicit model restriction is GPT-5.6 Luna only.
Independently verify their reasoning and code, particularly frame arithmetic and
SPU semantics. Earlier drafts contained mistakes corrected before validation.

## Implemented in the working tree

- `native/iop_probe/spu_adpcm.h`: signed low-nibble-first 28-sample ADPCM blocks,
  five predictors, saturating PCM, history across blocks, transactional invalid
  headers. Negative residual scaling avoids signed left-shift undefined behavior.
- `spu_gaussian.h`: original analytical coefficient generator. All 1024 numeric
  coefficients matched the pinned reference; no copied implementation/table.
- `spu_idle_voices.h`: block latch, 32-sample FIFO, pitch/Gaussian/ADSR path,
  PMON and direct volume routing, NAX/LSA/loop/ENDX state. Existing fetched data
  stays latched across RAM writes; a new block fetch reads current SPU RAM.
  NAX writes invalidate the latch; this is not a claim of full hardware accuracy.
- `spu_adpcm_tests.h`, `spu_wet_tests.h` and existing self-tests: deterministic
  PCM, predictor history/clipping, interpolation fingerprint, loop/state, RAM
  mutation, zero-pitch FIFO and routed wet-input checks.
- `spu_reverb_setup.h`: active reverb still requires zero actual wet input and
  verified zero workspace. FX-off nonzero input taints unmodeled history;
  re-enabling cannot silently assume zero history. Potential nonzero core0 dry
  output into core1 external wet routing fails closed. Noise, unsupported
  predictors/flags and volume sweeps remain explicit refusals.
- `host_pad_input.cpp`, new `host_arrow_input.cpp`, `quiescent_sio2.h` and virtual
  pad tests: real short host taps survive until an actual guest pad poll, retain
  real pressure values, and are canceled on focus loss without disconnecting.
  A separate Windows arrow-state fallback fixes actual arrow events absent from
  the raylib queue. No guest RAM or ready/connected flags were patched.
- `execution_budget.h`, bootstrap and launcher: explicit finite diagnostic
  runtime presets `--iop-runtime-millions 256|512|1024`, default unchanged.
  Select once before reboot; no renewal from polling or repeated BOOTEND.
- Build-local dispatch/scheduler diagnostics: new `ee_transfer_diagnostic.h`
  reports invalid nonzero PCs after execution, entry/current registers and
  stack, and saves the first private 32 MiB RAM dump. PC zero is legitimate.
- New `ee_write_watch.h` and `cmake/write_watch.cmake`: optional CPU RAM-store
  trace through a build-private shadow of the pinned macro header. Original
  stores remain intact. Bounds/alignment, RAM aliases, optional arm value,
  at most 128 changed overlapping writes. This does **not** observe DMA or host
  writes. Not yet exercised by a real run; build status must be checked.
- `scripts/run_native.py`: isolated diagnostic environment and metadata for
  `--ee-write-watch ADDRESS` and `--ee-write-watch-arm-value VALUE`.

Decoder success and PCM logs do not prove audible host audio. A full nonzero
reverb or host audio sink has not been accepted.

## Actual native evidence

All evidence below is local under ignored `work/`; do not publish captures/dumps.
Runs verified original input fingerprints. No reference emulator was run in
parallel with native execution.

| Run | Actual result |
| --- | --- |
| 191–194 | Input diagnosis; no reliable guest Left/Cross pair. Not menu evidence. |
| 195 | Actual SIO2 Left `FF7F`, Cross `BFFF`, nonzero core1 voice23 PCM. Left no-card dialog; final GS image visibly Square Enix. Finite budget, exit6, no timeout. |
| 196 | Repeated input/PCM path; final GS image visibly Cavia. Exit6, no timeout. |
| 197 | Explicit 512M preset; actual window Square Enix, Cavia, Project Dragonsphere. Then invalid EE target1, exit5, no timeout, before budget deadline. Final GS capture black. |
| 198 | Reproduced invalid return with entry/current diagnostics and full private RAM dump. Exit5, no timeout. No main menu. |

Use `work/native-run-NNN/runtime.log`, `result.json`, final GS `.ppm`/`.png`,
and `work/native-pad-NNN/input.json` for exact evidence. Final images for 195–197
were visually inspected. Do not claim a final-image review for 198.

Native198 executable SHA-256:
`81a4d753ab4273c79c8e700f0e15151157b5d53e88d7156be36773b9504a62fa`.
Its key diagnostic is `runtime.log:48393`; dump:
`work/native-run-198/ee-invalid-transfer-ram.bin`.

## Historical EE failure frame map (native197/198)

The scheduler resumes internal continuation **0x301B5C** in generated fn301A00.
Entry SP is **0x11FD20**, entry RA is 0x301B5C. The function returns with
PC=RA=**1**, SP=**0x11FE80**. The saved RA slot **0x11FE70 contains1**.
This is not a valid scheduler sentinel: normal invocation/new-thread RA is zero.
Do not substitute a return address or suppress the error.

Crucial: 0x301B5C is an internal continuation, so its switch/goto bypasses the
prologue. Entry SP already represents the allocated frame. **Do not subtract
0x160 again.** Read these generated files privately, without modifying them:

- `generated/lift-007/fn_00301A00_0x301a00.cpp`: prologue subtracts0x160;
  instruction0x301A30 saves RA at SP+0x150 = **0x11FE70**. Load at0x301CDC
  restores it; JR at0x301D04 returns and releases the frame.
- `fn_00301D10_0x301d10.cpp`: wrapper frame at0x11FE80 retains plausible
  caller0x286990. Its JAL0x301D1C sets expected saved RA **0x301D24**.
- `fn_002865AC_0x2865ac.cpp`: callback frame0x11FCF0 saves link0x301B5C
  at **0x11FD10**, which is legitimate and is NOT the corrupted parent slot.
- `fn_00301FE8_0x301fe8.cpp`, parser helpers0x309560/580/618/6B0/708,
  and `fn_003157C4_0x3157c4.cpp` (memcpy, including continuation0x315850)
  occur in the surrounding execution trace.

The writer of1 is unproven. CPU-store watch is the next experiment. A missed CPU
write may require examining SIF/DMA/host writes, not blaming guest code.
IRQ callback stacks are separately reserved by `invocationStackTop()`; overlap
is only a hypothesis, not an established cause.

## Historical validation at publication

- Native Release build008 passed and was actually exercised in native198.
- Strict IOP build/test008 passed: `work/adpcm-iop-build-008.log` and
  `work/adpcm-iop-test-008.log`.
- Bootstrap build002 passed; pressure/focus/short-tap regressions passed earlier
  in this session. Log: `work/adpcm-bootstrap-build-002.log`.
- Python **47 tests passed**: `work/adpcm-python-tests-009.log`.
- Doctor and private Gaussian full-reference comparison passed earlier.
- Public-tree gate and diff whitespace checks passed before this handoff.
  A later publication still needs manual
  review of all intended tracked AND untracked source plus the staged gate.
- Native009 failed because its shadow macro header could not include the watch
  header. Corrected to an absolute source include in CMake. Native010 then FAILED
  compilation in `ee_write_watch.h`, including line45, C2062/C2059. Inspect
  `work/adpcm-native-build-010.log` for all errors. A Windows min/max macro
  collision at `std::max`/`std::min` is a hypothesis to check, not a validated fix.
  Publication preparation protected both calls with parenthesized `std::max` and
  `std::min` names to prevent Windows macro expansion. Native Release build011
  **passed**: `work/adpcm-native-build-011.log`. Python47, strict IOP CTest and
  bootstrap CTest passed again during publication preparation. No real
  write-watch run has occurred yet; native199 remains the next experiment.

## Next experiment after the resumed checkpoint

1. Read the newest ignored `CURRENT_STATE.md` entry and inspect completed
   native208 evidence. Do not reuse a run directory or launch native/reference
   processes concurrently.
2. Build the actual runner with
   `cmake --build build/native --config Release --parallel 2`.
   The strict device regression is `strict_bare_iop_calls` in `build/iop-probe`.
3. The next unused run after208 is209. For a bounded reproduction:

```powershell
python scripts/run_native.py --input-complete --run 209 --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --iop-runtime-millions 512
```

4. Use real foreground input for Left, then Cross; stop automation on physical
   Escape. Require corresponding guest SIO2 reports, not only host taps.
5. Investigate actual IPU command/FIFO/DMA traffic around guest `0x3076A0`,
   `fn_003072B8`, `fn_003073D8` and `fn_00307658`. Establish the supported
   command boundary and a real bitstream operation before extending the device.
   Passive command mirrors do not establish successful IPU execution.
6. Preserve private evidence; update this handoff and `CURRENT_STATE.md`.
   No menu, audible output, gameplay, save or recovery acceptance is established.
   Publication requires the source-only gate and review of exactly staged files.

Actual runner: `build/native/ps2xRuntime/Release/ps2EntryRunner.exe`.
`build/smoke/Release/recomp_smoke.exe` is only a synthetic/minimal smoke.
