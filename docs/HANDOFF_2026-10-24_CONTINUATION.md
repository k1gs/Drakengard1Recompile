# Continuation handoff: nonzero ADPCM, startup logos, EE return failure

## Resume boundary

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

## First confirmed blocker and frame mapping

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

## Validation and pending build

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

## Exact next experiment

1. Confirm no native/reference process, preserve unrelated changes, check build010
   and rebuild if necessary. Windows builds required the normal approved external
   build path; sandbox-only MSBuild stalled. Do not kill unrelated build trees.
2. Verify `work/native-run-199` is absent. **199 is next unused**, not191 or198.
3. Run from repository root, after confirming build success:

```powershell
python scripts/run_native.py --input-complete --run 199 --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --iop-runtime-millions 512 --ee-write-watch 0x11fe70 --ee-write-watch-arm-value 0x301d24
```

4. Using real foreground host input, choose Yes with Left and confirm X/Cross.
   Observe the window between actions and preserve actual event times privately.
   Check both host input and `[sio2-pad-input-report]` logs; host taps alone do not
   prove the guest sampled them. Stop UI automation if the user presses Escape.
5. Inspect `[ee-write-watch]` after it arms on expected saved RA0x301D24. Identify
   the actual producer changing that slot before proposing a fix. A trace capped
   too early or no matching writes is an observation limit, not evidence of safety.
6. Fix only the proved first cause, add a meaningful synthetic regression, rebuild,
   then repeat a bounded actual run with a new directory. Continue toward an
   actual rendered menu. Logos, audio samples or dialog response are insufficient.
7. Update `CURRENT_STATE.md` and this handoff. Publish only within user authorization.

Actual runner: `build/native/ps2xRuntime/Release/ps2EntryRunner.exe`.
`build/smoke/Release/recomp_smoke.exe` is only a small synthetic/minimal smoke.
