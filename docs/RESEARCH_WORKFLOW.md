# PS2 research and frame investigation

The local research skills are `ps2-recomp-research` and
`ps2-frame-investigation`, installed under the user's Codex skills directory.
Invoke them by name when continuing device research or diagnosing frames.
Their installation is local; a fresh checkout alone does not install them.

The user subsequently requested **no Computer Use** for this project's diagnostic
runs. Prefer the native controller command channel and direct GS frame evidence.
Do not conflate a raw GS image with independent inspection of the host window.

## Native control without UI automation

Launch the verified runner profile with `--virtual-pad --pad-control` (and its
required IOP/firmware/video options). The launcher creates a fresh neutral
`pad-control.txt` inside that run's private evidence directory. Example after
checking that the run exists and has explicitly opted in:

```powershell
python scripts/native_pad.py --run 212 --press left --hold-ms 700
python scripts/native_pad.py --run 212 --press cross --hold-ms 700
```

Use the actual current run number, not the historical example blindly. Wait for
the first command's release/report before the next action. `--release` publishes
neutral input immediately. Holds are bounded to ten seconds; rereading a command
does not renew it. A fresh launcher run cannot inherit an old control path.

This is an explicit digital virtual-controller source, not an OS keyboard event
or physical-pad pressure measurement. It feeds the normal host snapshot and
DualShock2/SIO2 protocol; guest RAM and ready flags are untouched. Verify both
`[host-pad-control]` and `[sio2-pad-input-report]` rather than trusting the writer's
exit status. Native212 confirmed actual Left/Cross reports through this channel.
Events and UTC times are logged privately as `pad-control-events.jsonl`.

The user authorized automating this startup warning. Run
`python scripts/native_start.py --run N` alongside an already active opt-in run.
It waits for the checkpoint's frame1800 diagnostic, issues bounded Left/Cross
with real SIO2 press/release checks, retries at most five times, and stops all
startup input at the first IPU command. Default overall ceiling is180s; ended
runs, missing opt-in, absent guest reports and timeout are explicit failures.
This is a checkpoint-specific controller recipe, not visual dialog recognition
or a renderer/guest-memory patch. Native216's direct GS frames verified the
warning at observation1800 and the Square Enix logo at3000. Frame1200 still
showed Accessing, so the earlier1200 recipe was too early in214/215.
Current continuation uses `--timeout 300` for this helper alongside a native
launcher still bounded to300s. Helper218 exhausted its default180s before IPU;
220/221 successfully observed IPU with the300s option. Helper exit and native
timeout/exit are separate evidence fields; record both.

For an explicit diagnostic run, `DRAKENGARD_FRAME_INTERVAL=600` enables direct
GS snapshots every600 host-frame observations, capped at32 files. Accepted
intervals are60..3600; the default is disabled. Files are named
`periodic-guest-frame-N.ppm` inside the existing private run directory. They
preserve native dimensions and pixels, refuse overwrites, and can distinguish
warning/loading/logo phases without Computer Use. Record this environment
option with the run; observation counts are not EE cycles or proof of gameplay.

Existing `first-guest-frame.ppm`, `review-guest-frame.ppm` and final
`last-guest-frame.ppm` are direct GS evidence. Preserve input dimensions when
converting for visual inspection. These are selected checkpoints, not a recording
of every displayed host frame; add targeted capture instrumentation when that
distinction matters.

The first skill follows producer/transfer/completion/consumer boundaries, checks
that diagnostic hooks execute, and separates synthetic tests from actual runtime
proof. The second correlates guest output, raw GS frames and the presented window,
with controlled comparison and GPU capture when it answers the disputed question.

## Current host graphics path

Source inspection on 2026-10-24 found the raylib/OpenGL presentation path in
`ps2xRuntime/src/lib/ps2_runtime.cpp`: a CPU-prepared frame is uploaded using
`UpdateTexture`, then drawn using `DrawTexturePro`. The local CMake configuration
links OpenGL. Recheck this after changing the renderer.

[RenderDoc supports OpenGL](https://renderdoc.org/docs/introduction.html) and fits
inspection of the uploaded texture and presentation draw.
[PIX GPU captures record D3D12 work](https://devblogs.microsoft.com/pix/gpu-captures/);
switching renderers solely to use PIX would not help the current investigation.

A host capture does not recover the original PS2 draw calls from a CPU-generated
image. If the uploaded texture is already wrong, inspect the IPU/GS producer,
FIFO/DMA traffic, VRAM and display selection. If the texture is correct and the
window is wrong, inspect host channels, sampler, viewport, source rectangle and
draw state. The current IPU checkpoint is documented separately in the handoff.

## Prepared local tool

Portable RenderDoc 1.46 is in ignored
`tools/renderdoc-1.46/RenderDoc_1.46_64/`. CLI version and capture help were checked;
no system installation, global injection or renderer change was performed.
Its reported revision is `e4bd23b671d3d5a747ff5221dbe08a63eb6ca200`.
The official archive's locally measured SHA-256 is
`9ca4d09ecaba2cc791168660d6fc2a7e3d70fe67146e87ec05c5f8dea70772f7`.
This fingerprint is not an independently verified signature.

The frame skill includes a launch recipe using the real Python launcher and
RenderDoc child-process hooking, preserving firmware arguments and the bounded
watchdog. Tool availability has been verified; hooking, actual RDC capture and
replay of this runner remain unverified.

Capture only the intended owned diagnostic runner. Keep native and reference
emulator runs sequential, preserve baseline and instrumented runs separately,
and record the binary/tool versions, guest state and resource/event identities.
RDC files and texture exports may contain game assets and belong in ignored
`work/` or `private/`, alongside screenshots. They are not public source artifacts.

Pixel state oracles are checkpoint-specific. Record exact ROIs, reference frames,
thresholds and any matching-only normalization, and inspect the raw frame itself.
SIO2 press/release proves serial input delivery, not that a fading UI accepted an
action. Native247's valid dialog differed from the old RGB oracle before a later
match; keep helper deadline/recognition failures separate from guest device faults.
Use one controller at a time and review its JSONL/ACKs before manual intervention.


## Targeted menu performance measurements (2026-10-25)

Before changing device timing or rendering quality, separate execution and UI
threads.249/250/251 used target-only suspended RIP samples with independent thread
CPU times at verified ordinary main-menu states. Private MAP relinks used current
objects; compare executable code bytes and layout before attribution. Keep raw
samples, modules, binary proof, input ACK/ROI and timeout results private.

Nearest MAP attribution is not a stack or weighted ETW. Inspect same-address
aliases: ICF merged kernel and firmware lower_bound in249/250; the apparent
kernel-only hotspot was shared search code. One interval per build suggests
direction, not exact whole-game speedup. Host observations are not unique guest
frames. Synthetic lookup speedups apply only to their stated workload. Compare
one change and record compiler flags. New results/next experiment are in handoff.

The bounded sparse helper rejects only PCs outside actual sorted table extremes;
holes inside still need exact search. Real dispatch retains mutation checks.


## Exact GS arithmetic optimization

The packed bilinear helper keeps the original scalar floating-point operation
order and lround half-away rounding for finite fractional coordinates[0,1].
Adding0.5 before truncation misrounds the float immediately below0.5; compare the
fractional part instead. Compile without FP contraction/reassociation. Public
bootstrap regression includes all byte pairs, sprite fractions, arbitrary RGBA/
triangle fractions, tie-neighbor floats and four host rounding modes.

For the252 experiment a private old/new actual-backend A/B compared every4MiB
VRAM byte after416 varied synthetic draws. Require a nonzero-write witness: the
initial default-depth-NEVER fixture was correctly rejected as vacuous. Math tests,
real raster equivalence, synthetic timed workload and native menu are distinct
validation layers. Timed synthetic sprites do not establish game FPS. Inlining
moves costs into SampleTexture; disappearance of a symbol/DLL sample is not a
precise whole-game speedup. Record exact binary/MAP proof and equivalent raw frames.


## Production counters and live T8 CLUT specialization

Use opt-in `--gs-metrics` for bounded draw-state/wall-time inventory and separate
UI/scheduler/readback/RGB counts; see GS_PRODUCTION_METRICS.md.253/254 showed
~59 UI calls/s but6.73->7.15 observed scheduler ticks/latched copies persecond.
RGB changes fell despite faster progress: none of these alone proves complete
game FPS. Compare fully settled equivalent states and equal window counts;
validate bin+overflow conservation. Draw wall time is not CPU time.

The measured T8/CT32/CSA0 path can read current CLUT halfword banks directly:
both CSM modes compute clutIndex==index and TEXA leaves CT32 unchanged. It adds
no cache. Preserve all other LookupCLUT states and palette loads. Private full
raster regression includes repeated palette writes/TEXFLUSH/LoadClut and requires
visible output changes, then full VRAM equality. Timed synthetic batches and one
instrumented native pair support a local improvement, not a general FPS promise.


## Chapter continuation after a diagnostic ceiling

Do not treat a fixed instruction allowance as a device contract violation.255
reached ChapterI before the exact1024M ceiling; an explicit finite2048M preset
kept monotonic counters and closed exhaustion.256 passed that ceiling, but late
Cross near300s did not establish a scene transition. Optional
DRAKENGARD_FRAME_LIMIT1..64 (default32), paired with interval300, permits bounded
~5s captures through300s. Actual257/258 saved53/54 captures on the rebuilt binary,
so the capture option now has native evidence as well as parser/build checks.

Keep phase completion separate from input acknowledgement: SIO2 reports establish
serial delivery, not application consumption.257's Start was pressed/released
while the first prologue page remained visible. A controller must await actual
next-state evidence, with only bounded normal-input retries on fresh same-state
frames.258 then reached the mission briefing after ChapterI Start: intermediate
black/gray frames alone were not a diagnosed hang. Review rawGS; normalized ROI
change is only an observation trigger, never gameplay proof. No ready/PC/timer
patch or instruction-budget renewal is justified by these observations.

## Distinguish a host unwind abort from the original failure

Windows0xC0000409 alone does not identify a guest device or even the original host
exception. Controlled265 injected a malformed private HOST pad command only after
actual UI callback120: a live std::thread was destroyed during UI unwind, masking
the parser error. GameThreadJoinGuard now stops/joins before propagation;266
returns the original diagnostic with exit6 and normal window cleanup. A real-worker
regression plus the before/after native experiment cover the lifetime contract.
This is not evidence that all prior spontaneous aborts had the same trigger.

Briefing input remains an application-consumption investigation: serial reports
and gray frames are insufficient. An early one-shot EE_RAM_OUTPUT dump is not
state from a later mission page. Capture late private state read-only from the
owned process when required; a full dump includes game data and must never enter
Git. Preserve the300s wall limit and fixed IOP budget during capture.
