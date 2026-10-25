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
DRAKENGARD_FRAME_LIMIT now accepts1..320 (default32). Historical257/258 used
interval300/cap64 for ~5s captures through300s and saved53/54 captures respectively,
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

## Atomic host-pad reads and actual application edges

Native268 exposed a host Windows sharing failure on a valid command. The bounded
reader shares READ/WRITE/DELETE with an atomic pathname replacement, reads one
open version, rejects over128 bytes and preserves OS failures. No neutral/cache/
retry fallback. Synthetic rename-ownership/boundary/error tests and actual269/271/
272 exercise the rebuilt e6d326c... runner; bootstrap16/Python57 PASS.

Native270 startup raced command-file creation: wait for pad-control.txt before
controllers; retain correct phase log names and menu-after-intro0 ownership.
271/272 prove briefing Cross writes actual game held346880=10 and fresh pressed
346884=10, then release. Start sets50. Serial ACK and converted application edge
are distinct from a particular UI consumer accepting Continue.272 confirms page
removal followed by black/flat125,118,101/black captures, not rendered gameplay.
A late scene snapshot is required to investigate that boundary; neither early
EE_RAM_OUTPUT nor269's briefing snapshot represents the post-Continue state.

## Order scene snapshots by retired device work

A GS observation number is a host UI counter, not an instruction timestamp. A
snapshot triggered by a new black frame can precede CDVD activity whose log rows
share its preceding UI marker. Record native-log byte offsets before and after
collection, then corroborate ordering with the actual monotonic IOP retired count.
Source-chain completion means FIFO acceptance; separately prove EE destination
writes and the guest request's completion status before diagnosing a lost DMA.

Native273 is a concrete counterexample: its verified late snapshot retains the
old destination bytes and request status1, but sampled IOP1182076928 precedes the
matching DVD DMA start1182375149. This does not establish a transfer defect.
The main EE thread is also past frame waiting in that snapshot, not permanently
stuck at its frequently observed polling PC. Validate a real GuestThread using
its id, entry and stack bounds; a context-shaped host stack copy is not another
scheduled thread. Keep declaration-derived layout checks pinned to the toolchain.

Request objects can be reused: match the job id together with status writes. A
prior job's1->2->3 cycle does not prove completion of the scene's next request.
Independent MDMP directory/payload validation is distinct from collector exit;
retain a nonzero exit truthfully even if the full snapshot is structurally valid.

## Require the actual Continue consumer to be ready

A readable briefing page can still be in its fade-in state. Original chain
13fcf4 -> UI task346e38 ->128b08 ->2c6514 uses base4fbb80 (signed ADDIU from
500000 by-4480). State4fbd30=2 checks341804, the remaining counter at48a434,
and returns without button processing until it reaches zero. State3 enters the
actual input consumer. Snapshot269 after a Cross ACK still had state2 and21
remaining steps; state4/enable0 in273 is the later closing phase. Do not equate
page visibility, serial delivery or converted pad edges with this readiness.

A private observer can use the existing aligned EE write watch at4fbd30 and wait
for actual state3 before sending a fresh normal Cross. Keep the hold/retry bounds,
require a fresh same-state image for retries, and do not modify the state. Advance
the image floor after ACK so images saved while waiting cannot trigger a retry.
Fail on unknown/saturated watch history; validate that the watched slot is the
consumer's slot before interpreting its numeric value. No guest-ready patch is
needed or authorized by this diagnosis. Host time delays are not a substitute
for the game's actual transition state.

## Continue readiness and diagnostic PPM writes (2026-10-25)

Native278: fresh No6120 ->Left16 ->Yes6300 ->Cross17; SIO2 ACKs verified.
At briefing14880, actual UI state4fbd30 changed2->3 BEFORE one normal Cross23,
then original PC2c692c wrote3->4. Continue accepted; last16140 black, watchdog300s,
no target789read or late dump. No rendered world/gameplay/audio/save proof.
Private278 menu observer rejects unknown/fading images and confirms Yes before
Cross; finite Left retry branch was not needed live. Preserve phase ownership.

Diagnostic P6 serialization now batches4096pixels with the same header/RGB bytes
and alpha discard. Three capture paths share it, preserving existing frame
selection/no-overwrite behavior. No guest/device/timer/render math changes.
Synthetic17/17 including binary-equivalence at chunk boundaries PASS, Release
build PASS. /Od synthetic10image batch median0.0967087s legacy/0.0240027s new,
981135bytes equal; this is file-write overhead, NOT game FPS measurement.
Optional cap1..320/default32, interval60..3600; actual config logged.
Current runner SHA0d24cede033b00c545772545212b273d56994514e03b794834285d6c6db397f1.
Latest native outcomes and next experiment are kept in top HANDOFF/CURRENT_STATE.

Native279 repeated UI3->4 acceptance; last16560 black,watchdog300s,no targetread.
Native280 collected a complete owned-process snapshot immediately after original
PC2c692c UI3->4 (701889699bytes/794Memory64,collector1,observer0,offsets4032187->
4045688). Actual UI4/enable1,fader current20,target255,remaining55/60: still closing
briefing. Old completed slot status3/job19decimal/LSN867318 is NOT target789.
IOP964931584, no targetread. PC2fdd08 is an original VIF1DMA CHCR launch store,
not proof of a stuck DMA. Final16380 is darkened briefing; no world/gameplay.
Next experiment: measure actual closing-fader update cadence and host execution
cost within300s, preserving original timing, fixed budgets and deferred menu-FPS
scope. Do not repeat an unreachable post-read trigger or invent a DMA failure.

## Latest: closing fade completes; target delivery under observation (2026-10-25)

Runtime unchanged from83e1557; actual EXE SHA
0d24cede033b00c545772545212b273d56994514e03b794834285d6c6db397f1.
Native281 used the same300s/2048M/interval60/cap320/metricsOFF/watch4fbd30;
all4input phases0,watchdogtrue/exitnull. No new explicit native/device refusal.
A read-only owned-process observer verified unique EE RAM with the original ELF
hash and3code probes; no WriteProcessMemory, debugger or thread suspension.
Actual closing60->0:61observations,all60net decrements1,9.297wall seconds,
10.140625process CPU seconds,median0.156s between decrements. This proves
progress, not an exact trace of the341698a1 register. Target first1485718 and
final1485920 CDVD reads were reached; no gameplay/world proof.

Critical source-analysis correction: original UI switch table566090 maps state4
to2c6e80, not2c6930. It waits remaining48a434, calls2b02ac and clears503008 via
2c6eac, while leaving state4 intact. Do not infer a stuck fade from UI4 alone.
Earlier83e1557 notes attributing UI0x12 to state4 were incorrect and are corrected.

A separate read-only main GuestThread PC sample found3809samples/59.422s:
27f4c4=1272,2fdd08=469. The dispatch target is set BEFORE scheduler safe-point/
device work, so these are not host stack timings or proof those functions spin.
Two initial PC locator attempts failed closed (guard page / narrow region cap);
a fresh valid locator scanned74,469,376bytes in0.110s and verified one actual
id1/entry120008/stack100000,size20000/GP572c70/context1648. All helpers stay ignored.

Native282 completed300s watchdog/exitnull:274captures,last16560 black;
startup/intro/menu0,progression1(deadline295s). First observer1 on early unmatched
IOP; separate fresh read-only observer2 exited0, but NO targetread occurred.
Payload attempts0,delivery_verifiedfalse,NO RAM/payload saved. Final actual UI4/
enable0/fader255,255,0; old completedjob19/status3/LSN867382,dest4b1c00,131072B,
IOP991711232. This independently confirms that UI4 persists after fade completion.
No new native/device refusal; target CDVD->EE delivery remains unverified.

The user explicitly approved prepared600s observation283 ("yes, do as you judge").
It is now running: sameEXE,original game clocks,fixed2048M without renewal,
frameinterval120/cap320,progression595s,observerchildbirth+599.5s. Public300s
launcher remains unchanged; the private extended copy is confined to this run.
Read final283 outcome before any next launch; never duplicate/overwrite it.
Observer compares EEb7db40/481792bytes twice against ISO LSN1485718*2048, then
saves one private NONATOMIC liveRAM copy with before/after counters only on match.
No guest flags/PC/RAM/timers/movie changes, fake completions or budget renewal.
Menu-FPS work remains postponed; preserve unrelated .vscode/Splash, inputs,
upstream/generated/maps and private evidence. Next expected284 (recheck).

## Latest: CVT.W.S sort fixed and native verified; missing owner tail2ad344 (2026-10-25)

User approved600s diagnostic execution and delegated routine choices. Keep finite
600s maximum for this validation profile and fixed2048M IOP without renewal;
public launcher remains300s, private verified extended launcher stays ignored.
Native283 on old0d24cede...EXE completed600s watchdog/exitnull, allinput phases0,
observer0. Exact481792B file789 from ISO1485718*2048 matched EEb7db40 TWICE.
SHA cbf01937e8b52bc842272646c53743ab4791cefb5b19e787dc0453396c46b5ba.
IOP999749747->999756219 during reads; job34/status2 then actualstatus3/object6.
Live RAM copy is NONATOMIC (IOP999768532->999932807), not a full-process snapshot.
Next actual readLSN1482591/6402419B reached final1485664/54sectors; node48fd10
was reused, so oldjob34/status3 must NOT be attributed to this later request.
Late scene handler143194 at3b4c20 advanced substate+5c2->4. No rendered world.

Confirmed next defect: original comb-sort24e4d8 repeatedly has gap2/no swaps,
51entries in scratchpad70000000. Constant56e8c0=1.2999999523162842. The emitted
CVT.W.S24e50c called hostnearbyintf, giving2 for2/1.3; gap never reaches1 and
original backedge24e584 never terminates. Source/runtime cause, no gap patch.
Pinned PCSX2v2.8.2 CVT_W truncates towardzero and saturates by sign; recompiler
usesCVTTSS2SI. Original ee_fpu_convert.h + shared macro/PCH overlay now implements
that RESULT contract, independent of host rounding; no broader FCR31 flag model.
Upstream/generated/gamecode unchanged.18/18 tests PASS:boundaries/all4rounding
modes, fail-before nearbyint2/1.3=2, bounded synthetic51entry comb-sort termination.
Actual fullPCH Release build PASS; newEXE SHA
81886f391519c3acb95180d215c43d93e20e91b7e72a4ae658f2d5543b6f3583.
Native284 VERIFIED advancement beyond the old sort: nativeexit5/timed_out=false,
all4input phases0, same600s/2048M profile. Firstobserver1 on transient nullCIMpath;
freshobserver2 waited for validpath, verified owned EE/IOP, exited0 and proved
payload delivery again (two attempts, second matched). LiveRAM hash
bcf501867cf345a8de911c553a84f994cad7f57914f2b2a3c035979d2a5cddfd.
Reviewed final640x511 GS shows "Loading...", not a rendered world/gameplay.
All native/observer processes stopped. Next285 unused (recheck).

FIRST REMAINING BLOCKER: scheduler target2ad344,RA1a22d4,SP11f8d0,GP572c70.
Owning exported function2aca3c has exclusive end2ad344, so the real instruction
SRLa1,a1,1 at2ad344 is missing. BEQ2ad334 targets2ad340 (VNOP, also the delay
slot of J2ad33c), then should execute2ad344 and BNEZ2ad348 back to2acc08.
The export instead splits2ad348 into a separate owner which branches back inside
2aca3c; registration omits2ad344. Recover the actual owning body/control-flow
and resume set in original analysis/generator/overlay, preserve all3prior
recoveries, use fresh map/lift and bounded native285. Do NOT add a success stub,
blind alias, guestPC change or a second prologue. Original tail disassembly and
before/after evidence are private chapter1-284; next cause is ownership/export,
not a new DMA defect. No gameplay claim.
No subagents/ComputerUse/OS input, gameclock/PC/RAM patches, fake completions or
budget renewal. Preserve .vscode/Splash/upstream/generated/maps/private evidence.
