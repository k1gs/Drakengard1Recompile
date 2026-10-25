# Drakengard Europe continuation handoff

## Latest: GS production measured; common T8 palette path verified (2026-10-25)

Continued from4b9ee66. Added opt-in `--gs-metrics`, recording bounded private
production/state counters. Source confirms UploadFrame usually reuses its image;
UI frame count is not game FPS. Observe UI calls, positive scheduler VSyncTick
deltas/regressions, valid copied latches, initial image/RGB changes/invalid images,
completed DrawPrimitive count/wall time, LoadClutUnlocked calls and64 state bins.
At most64 reports, minimum5s intervals; window bins clear, cumulative counters
persist, overflow explicit. Alpha-only changes do not count as RGB changes.
The hooks do not create frames, clocks or events. Default disabled. See
`docs/GS_PRODUCTION_METRICS.md` for exact meanings and eight-word key layout.

Bootstrap14/14 and Python56 PASS (new model/launcher opt-in/environment-isolation
regressions). Actual Release/PCH build PASS. Native253/254 both300s/1024M/frame600
with --gs-metrics, watchdogtrue/exitnull, no new explicit scheduler/device refusal.
Ordinary startup1..10 then Start11/SIO2 press+release, no NewGame. Raw GS640x511
menus privately inspected. 253menu9600 pixel-identical to252menu8400;
254menu8400 pixel-identical to253menu9600, whole RGBMAE0/0/0, no resize/alignment.
No final-on-exit capture for watchdog. Strongest game boundary remains prologue.

Matched first24 fully settled5s windows perrun,120.2459551s vs120.2222523s:

| Measurement | Native253 baseline | Native254 fast path |
|---|---:|---:|
| UploadFrame UI calls/s |59.2203|58.8161|
| Observed guest VBlank ticks/s |6.72788|7.15342|
| Valid latched GS copies/s |6.72788|7.15342|
| Observed RGB changes/s |5.65508|5.28188|
| DrawPrimitive wall time / interval wall time |54.2192%|51.6040%|

Tick/readback rate is6.325% higher in this pair; NOT a claim of complete game FPS.
RGB changes can fall when readback/draw phases differ; they omit identical redraws
and may reflect partial/missed images. One instrumented run perbuild, with
clock/mutex/RGB-comparison overhead; draw wall time is not thread CPU time.
Every included window passed exact bin+overflow count/time conservation. Zero
state overflow, tick regressions or invalid images. No whole-game guarantee.

Two large T8/CT32/CSA0 linearFST blended texture states consumed58.1866% of baseline
raster wall time. Their mean draw times12.26359->10.41889ms (triangle-strip) and
22.42277->18.80587ms (sprite). Implemented a narrow LIVE-CLUT fast path:
T8+CT32+CSA0 computes clutIndex==index for CSM0 AND CSM1; TEXA leaves CT32 unchanged.
Read m_clut[index] and m_clut[index+256] on every sample, keeping current palette
contents. No palette cache/invalidation change, filter/timing changes or aliases.
All other states retain original LookupCLUT. Upstream/generated/input unchanged.

Private actual-backend A/B464 paired synthetic draws, every4MiB VRAM byte equal
after every draw. Includes416 prior format/wrap/filter/primitive cases plus48
CSM0/1,nearest/linear,FST/STQ,sprite/triangle draws with repeated real palette
writes/TEXFLUSH/LoadClut; reloads must change output.1,221,721 changed-byte witnesses.
Synthetic128 T8CT32CSA0linear256x128sprite batches,3 alternating pairs:
median0.254178s->0.210281s (17.270% less time for this workload, not game FPS).

Native253 executable SHA256:
2a33b2d8395e295ed6867c44c6f8253642bbd4aa64c5345865447fe5d377029c.
Native254 final executable SHA256:
9c9c22cd4ae7bdc7d38b38d8d60e29b2a3cd9e1ca0711343259b576c25b066b9.
Evidence: work/gs-production-253 and work/gs-clut-254 include full/matched reports,
comparison.json, private preserved baseline/raster drivers/build/results, input
logs and raw-frame comparisons; work/native-run-{253,254}/gs-metrics.jsonl and
result.json; work/spr-vu0-runN-menu.jsonl. No captures/register traces published.

Next255 (verify unused/no active process): profile remaining T8 texture/page-cache
reads and per-pixel blending with these counters, then select one bounded fast
path with exact raster regression and comparable native before/after. Avoid
palette caching without invalidation proof, quality/timer/IOP skipping and global
compiler-flag changes. No new missing EE target; page advance/gameplay/audio/saves/
recovery unverified. Preserve .vscode/Splash; splash remains deferred.

## Previous: packed GS bilinear filtering preserves exact pixels (2026-10-25)

Continued the low menu-FPS task frombb4ba5b. Prior251 identified scalar
lerpChannel81/1906 raw samples, with124/1906 in unassigned ucrtbase work.
Original gs_bilinear.h now interpolates all RGBA channels using SSE2, preserving
separate float sub/mul/add order, exact fractional rounding and saturating pack.
Do not use add0.5/truncate: it fails for the float just below0.5. Fractional
coordinates are finite[0,1]; scalar fallback is retained for nonSSE2. Keep precise
FP/no reassociation or FMA. Quality/filter weights, texture reads/wrapping/CLUT,
TEXA/blending/depth, guest clocks and compiler flags are unchanged.

The graphics overlay pins the entire old arithmetic/packing body SHA256:
bd835128f1eb8ffbf87d5094768ff304dde8fc22da0858aec8310216ad478d55.
Only that build-local body is replaced. Upstream and existing generated lifts
remain untouched. Public regression2,048,594 exact syntheticRGBA comparisons:
all byte pairs/1/16 fractions, ties and adjacent floats, randomRGBA/triangle
fractions and4 host rounding modes. Bootstrap13/13, Python55 and actual Release/
PCH runner PASS. Local tests are not a queried CI result.

Private A/B compiles the preserved old and current actual CPU backend with the
same /O2/archAVX2/fp:precise flags.416 paired synthetic draws cover13texture
formats,4wraps,nearest/linear,FST/STQ,sprite/triangle plus varied CLUT/TEXA,
alpha/blend/mask/depth. All4MiB VRAM bytes match after EVERYdraw;1,145,627 changed
framebuffer-byte observations prevent accepting no-op draws. The first fixture
was rejected for zero writes (default depth NEVER); only the fixture was corrected
to explicit ALWAYS and a separate Z base, then real comparisons passed.

Synthetic actual-backend128 T8linear256x128 sprites,3 alternating pairs:
median0.301296s ->0.254510s (15.53% less time for THISworkload). This is not an
18% whole-game/FPS claim. Native252 menu8400 GS640x511 privately inspected;
whole decodedRGB is pixel-identical to prior251 menu9600, RGBMAE0/0/0/bboxnull,
without resize/alignment. Ordinary startup1..10 then Start11 with actual SIO2
press/release; no NewGame.300s/1024M/frame600, watchdogtrue/exitnull, no new explicit
scheduler/device refusal. No final-on-exit capture from watchdog termination.

Native252 executable SHA256:
4e3992b7052434131f18b6e1e95366e58b9f05cf0f6a05153e4a271b44d3816d.
Exact MAP relink code/layout proof .text SHA256:
8fadacb8019669601a0a148108bb0c47a9efc4f1c16dc3aa2b35bd89efee8c27.
New30.0020s profile1903IPs/hotCPU99.73%onecore. No lerpChannel/ucrtbase IP sampled
in this interval; packed math is now INLINED in SampleTexture, so costs move across
symbol boundaries. Before DLL samples were not stack-attributed. One RIP interval
perbuild/nearest MAP/ICF/suspend limitations; no weighted ETW/stacks, unique guest
FPS or GPU utilization measured. Current major costs: SampleTexture, texture/page
reads, CLUT, WritePixel and strict IOP. No precise whole-game speedup inferred.

Evidence: work/gs-bilinear-252/{report.md,report.json,comparison.json,
menu-comparison.json,binary-proof.json,samples-b.json,modules.json,symbols.map},
baseline_backend.*, raster_compare.cpp/build/results, logs and menu-review.png;
work/native-run-252 and work/spr-vu0-run252-menu.jsonl. All private/ignored.
Next253 (verify unused/no active process): establish a trustworthy guest GS frame/
draw-production rate distinct from UI callbacks and collect bounded indexed-texture/
CLUT draw-state statistics at the same menu. Use those before selecting palette/
texture fast paths; any cache needs explicit CLUT/TEXA/state invalidation tests.
Keep software raster accuracy and device/timer work. Saved game boundary remains
first prologue page below, not gameplay/audio/saves/recovery. Splash is deferred.

## Previous: main-menu sparse dispatch overhead reduced (2026-10-25)

The current user priority is low main-menu FPS. Native249 identified a saturated
execution thread (99.63% of one core), with software GS texture/pixel work the
largest identified group (~46% of raw RIP samples). Combined kernel/firmware
lookup accounted for141/1910 samples (7.38%). Exact MAP inspection corrects the
old kernel-only interpretation: linker ICF merged KernelEntry and FirmwareEntry
lower_bound instances at the same address; RIP samples cannot separate callers.

Original sparse_dispatch.h now rejects addresses outside the actual sorted,
nonempty prepared table before exact binary search. Both kernel and firmware
lookup use it. No address aliases, instruction-verification cache, guest timing,
render-quality, generated code or upstream edits. Real system dispatch retains
copied-instruction checks. Synthetic sparse/unaligned/boundary/UINT_MAX/singleton
lookups and mutation refusal after successful dispatch PASS; Python55 PASS and
real Release/PCH runner build PASS. Runner Release /Od versus GS/runtime /O2 was
confirmed; compiler flags were deliberately unchanged for this comparison.

Native250 tested kernel-only bounds:83/1906 lookup samples (4.35%). Native251
final combined fix:26/1906 (1.36%),30.0117s interval; hot thread99.34% of one core.
No binary-search RIP sample in the final interval is not proof of zero calls.
One sampled interval per build, nearest MAP symbols, no stacks/CPU-weighted ETW;
these percentages are not unique guest FPS or measured whole-game speedup.
Synthetic37-entry /Od benchmark99%miss/1%hit gave16.6475ns ->4.2896ns medians,
3 alternating pairs:3.88x for this lookup workload ONLY, not game FPS.

Native251 ordinary startup Left/Cross then title Start11 (actual SIO2 press and
release); main menu9600 at640x511 privately inspected, control ROI MAE0 against
baseline. No NewGame input in these performance runs. Native250/251 each300s,
1024M/frame600, watchdog timed_out=true/exit_code=null. No new explicit
scheduler/device refusal. 250's first startup helper raced command-file creation
and exited without input; a later helper succeeded. Boot times are not comparable.
251 waits for the command file first. All helpers and captures remain private.

Final executable SHA256:
7d329c1d8dad3e29385c590a051aa2b5b51d5e2aabaeaddd9d2ccf28c41bdd18.
Exact private MAP relink executable code/layout matched .text SHA256:
3823b62e5f092266c375964baa8d20b47115f4c68b49c38c5e042afc990cd754.
Evidence: work/menu-perf-249 baseline; work/menu-perf-250 partial/profile/benchmark;
work/menu-perf-251 report.md/json, comparison.json, icf-proof.json, samples-b.json,
modules.json, binary-proof.json, menu-review.png; work/native-run-250/251 and
work/spr-vu0-run{250,251}-menu.jsonl. No OS input, Computer Use or reference run.

Next252 (verify unused and no active process): measure the dominant software GS
sprite/texture/filter path at the same menu. Choose one correctness-preserving
fast path from actual draw state, compare synthetic pixel results and native menu
before/after. Do not migrate renderers, lower filtering quality, skip device/timer
work or change all compiler flags as an unmeasured shortcut. Global unique-frame
FPS still needs a separate trustworthy counter. The saved gameplay boundary is
still first textual prologue below, not movement/combat/audio/saves/recovery.
Splash/.vscode are preserved; no new game captures are approved for publication.

## Previous: all three recovered leaves pass; textual prologue reached

Entry7e548a0 fixed owner29322C. Native245 then exposed truncated owner297878 at
297BB0. Verified original ELF SHA065a94591d295bb936a8dca92e722c3bd72cd05848c7274b26d572b0f782e918:
real JR RA297E3C, SP+0x60 delay297E40, exclusive bound297E44. Existing bounded
RecoverLeafFunctionFlow restored1484 actual bytes, merging824/32/240-byte map
fragments and adding388 covered bytes. No aliases, padded returns, PC/RA changes,
stubs or manual edits to generated units. Input and upstream remain read-only.

Read-only export reapplied ALL THREE verified recoveries:297E44..2983E8,
29322C..2937D0,297878..297E44. Map0118579vs8581 rows; both previous recoveries
and all unrelated records are identical. Map SHA256:
fb9e51aa556ae25caf877fc83031d3634ad50ebe195c3ec4a8d02556bdbf40f9.
Fresh lift-0108578 recompiled,0stubs/skips/decode failures. All36 resume switch
cases across the three owners match registration. Runner now selects010;
old009/maps/lifts remain untouched. Existing indirect-JR warnings remain.

Fresh Ghidra read-only synthetic preparation/recovery/emulation PASS: immediate
exit, two busy iterations, pause/resume, once-only delay effects. This is not a
full native C++ scheduler test. Full actual Release/PCH build PASS. Toolchain
Ghidra12.1.4/EE Reloaded2.1.38/JDK21 and pinned PS2Recomp unchanged. Reproduction
of cumulative recovery is in docs/ANALYSIS_REGRESSIONS.md.

All native246/247/248 used the SAME executable SHA256:
263cce6f2ee5718f400a7a5e5f01756fd9064771255fa5a7c589ad0f17dca38d,
1024M/300s/frame600. All ended watchdog timed_out=true/exit_code=null, with active
IOP/SIF/callback processing and no new missing-target or explicit device refusal.
Native246 ordinary inputs11Start/12Cross/13Left/14Cross all had SIO2 press/release;
no parent intervention. GS title9600/menu10200/NORMAL10800/Yes11400. Restored
297878 tail executes VU0-data source11004860/QWC78→MADR11004FE0/SADR780, equal
source/destination FNVb386a16d4f6d3525 (completion32). Old297BB0 is passed.

**New rendered milestone:** native246 last periodic15600 is the first textual
story-prologue page at640x511. Privately converted and visually inspected. The
preceding black image was transitional, not proof of a hang. No final-on-exit
capture from a watchdog stop, no OS-window verification, no gameplay/audio/saves/
recovery acceptance. Callback loop27F118 calls1215F4 (EE Timer1 COUNT10000800)
and a vtable callback; repeated PC alone is not evidence of a timer defect.

Native247 also passed the three leaves; the old observer delayed its valid
NORMAL/Yes matches to13200/13800 because an earlier correct-looking dialog had
raw ROI errors12.111/6.400 against4/4 bounds. It still sent all four inputs with
ACK. Last15600 black; prologue helper expired without a match. That helper error
is not a guest/device fault and does not establish application input rejection.

Native248 changed ONLY private image-oracle/controller scripts. Stable-menu MAE
bound2 instead of8; dialog text/button bounds15/8 instead of4/4. Known No versus
Yes and other-state negatives were checked. Menus completed cleanly, with one
observer and SIO2 ACKs. A sequential prologue helper waited for menu observer to
finish, recognized actual first page14400 using an explicitly recorded normalized
grayscale ROI (85,175,540,310), MAE9.8005/bound15 vs native24615600. Autocontrast
is for matching only; captured evidence is unchanged. Checked negatives include
title74/menu33/dialog61/black54 MAE, reference positive0. This is a calibrated
checkpoint-specific oracle, not universal state recognition or rendering proof.

At20:33:06.040928UTC Cross15/700ms was accepted/released by actual SIO2 on the
prologue page. Images14400 and15000 differ (RGB MAE6.946/6.366/5.683), but visual
review shows the SAME first-page text; no semantic page advance established.
Native248 last15000 is prologue640x511, watchdog300s. No manual/competing input,
no ready flag, timer or movie skip patch. One acknowledged Cross does not prove
the application consumed it as page advance. Old crashes/grouping key unresolved.

**Current verification boundary:** first story-prologue page after New Game;
there is NO newly established missing scheduler target or unsupported-device
contract. Next249 (verify unused) should establish the expected prologue behavior:
use ordinary Start/Cross at a verified ready state, observe actual callback/input
selection and Timer1 COUNT/MODE plus software progress if the page does not
advance; compare an equivalent owned reference state sequentially if needed.
Do not assume timer failure, frozen rendering or a ready flag to force. Keep each
run300s maximum; game-native input can advance scenes, not PC/RAM/timer patches.
Target next real scene and eventually actual movement/combat before gameplay claim.

Private evidence: work/leaf297878-contract-246.json (includes246/247/248),
work/leaf297878-epilogue-246.txt, export/synthetic/native-build246 logs,
work/native-run-{246,247,248}/, work/leaf297878-runN-{launcher,startup,observer}.log,
work/spr-vu0-runN-menu.jsonl. Original menu script work/spr-vu0-menu-244.py is
unchanged. Updated private248 scripts: work/leaf297878-menu-248.py and
work/leaf297878-prologue-248.py; prologue script hardcodes248 and outputs, so
adapt a NEW copy and verify run/sequence/output before reuse. Its normalized
ROI metrics are in work/leaf297878-run248-prologue.jsonl. Do not overwrite artifacts.
All frames/exports/generated/game data stay private; Splash/.vscode untouched.

## Historical: second leaf recovered; native245 passes0x29353C

This supersedes the checkpoints below. Entry e1214d3 already contains the normal
VU0-data→SPR fix. The next owning leaf0x29322C was truncated at0x29353C after
an alternate-entry jump delay. Verified original SHA-gated ELF instructions have
JR RA at0x2937C8 and SP+0x60 in its delay at0x2937CC, exclusive bound0x2937D0.
Read-only Ghidra recovery covers1444 actual instruction bytes; no invented alias,
PC/RA patch, padding or manual generated-code change.

The original project does not save temporary recoveries. Reapplied BOTH previous
0x297E44..0x2983E8 and new0x29322C..0x2937D0 recovery before full export.
Map010 has8581 records vs8583: only784/32/240-byte fragments at29322C/2936AC/
2936E0 merged, +388 covered bytes. All unrelated rows and prior297E44 recovery
remain identical. Map SHA256:
87f79278554d403e3979ffe6be9911c25393c2fd3cef0e697e24e38b6d1a8690.
Fresh lift-009:8580 recompiled,0 stubs/skips/decode failures; existing unresolved
indirect-JR warnings remain. All12 emitted resume switch cases for29322C are
registered to that owner, dispatch occurs before its SP-0x60 prologue.
Old lift-008, input and upstream remain untouched; overlay now selects009.

Fresh synthetic Prepare→Recover→CheckLeafFlowFixture PASS: actual immediate
return, two busy iterations, pause/resume and once-only delay effects in the
pinned Ghidra emulator; read-only fixture changes discarded. This is not a full
native C++ scheduler test. Real full Release/PCH/MSBuild build PASS.
Ghidra12.1.4/EE Reloaded2.1.38/JDK21 and pinned PS2Recomp revision unchanged.
Reproduction with both recoveries is in docs/ANALYSIS_REGRESSIONS.md.

Native245 executable SHA256:
2b198344b7029aefb4e9a0e91e57777cfa1abc7da6127d88300ae5b46bcc5508.
1024M/300s/frame600, exit5/timed_out=false. Actual trace passes old missing
0x29353C, returns from owner29322C, continues through2991B4/29E21C, then invokes
another leaf297878. New exact refusal: **EE scheduler target0x297BB0,
RA0x2984B4**, SP0x11D9D0. Map owner297878 ends297BB0 (824B), with additional
fragments297D20..297D40 and297D54..297E44. Its generated branch297B98→297BAC
again enters another jump's delay instruction then falls through at map end.
This suggests the same analysis gap, but verify its real epilogue before recovery.

Input validation limit: observer matched title10200/menu10800/NORMAL12000 and
sent ordinary Start11/Cross12/Left13, all SIO2 press/releases seen. Observer was
interrupted after13 based on misread delayed output; manual extraStart14 was
accepted/released. Late Cross15 was written to host control file but no SIO2
press is observed before stop; do not call it delivered. Intermediate menu image
was stale, so no claim about which input caused which transition. This is valid
control-flow progress evidence, not a clean minimal-input baseline. Next run
should leave one observer in charge and inspect its actual JSONL/events/ACKs
before intervening. No OS input/Computer Use or guest RAM patches were used.
Final GS640x511 is flatRGB128 gray, privately converted/visually inspected.
No gameplay/host-audio/saves/recovery proof. Prior crash/grouping key unresolved.

**Next246 (verify unused):** establish original instructions/real epilogue for
owner0x297878, then bounded recovery/export/fresh lift and resume audit with the
existing behavioral regression; actual Release build and bounded native246.
When exporting from the read-only project, reapply BOTH already verified leaf
recoveries above as well as the new one, preserving all prior map coverage.
Do not choose a bound merely from neighboring records or add arbitrary aliases.
Retain old maps/lifts; fresh map011/lift010 names must first be checked unused.

Private evidence: work/leaf29322c-contract-245.json, epilogue/map-delta/resume-audit
files with the same245 suffix; work/leaf29322c-recovery-export-245.log,
work/leaf29322c-synthetic-245.log, work/leaf29322c-native-build-245.log,
work/native-run-245/, work/leaf29322c-run245-{launcher,startup,observer}.log,
work/spr-vu0-run245-menu.jsonl and native pad-control-events.jsonl. Observer source
work/spr-vu0-menu-244.py accepts a new run number; its existing245 output must not
be overwritten. All project/maps/frames/generated/input evidence stays private.

## Historical: resume fix independently verified; VU0-data toSPR passed in native244

Entry checkpoint cf3692d already fixed native240 target0x298154. This continuation
independently compared the actual lift-008 switch and dispatch registrations:
all12 resume labels belong to fn0x297E44, including the observed loop0x2981E4.
0x298154 is ordinary recovered fallthrough, not a missing alias. Old before/after
Ghidra logs contain the missing-epilogue failure and actual pause/resume/delay-slot
PASS. Entry executable matches native243 SHA2052fa9845d1996478187657580203c590b2caf29cd785495164e76432c010bf.
Private audit: work/leaf-resume-audit-244.json. Native241 is already occupied and
was not repeated; new run244 also passes the old boundary and exits that leaf.

Implemented normal DMA9 VU0-data→SPR in original ee_spr_vu0.h/CMake overlay.
The upstream normal path mapped only main RAM and left the measured VU source
pending. This adapter reads the SAME m_vu0Data used by CPU/VIF/VU execution and
writes actual m_scratchpad. Canonical4KiB VU0 source, exact normal0x101 options,
full bounds before writing, 16KiB SPR wrap, real MADR/QWC/SADR progression.
DMAE/suspend gate admission; deferred normal VU requests are serviced by existing
completion consumption, not CHCR reads. Cause9 is queued only after data copy.
Existing main-RAM normal/fromSPR and source-chain paths remain unchanged.
Aliases/source crossings, micro memory/VU1/interleave are unsupported; no new
buffer, zero-data substitution, fake STR/IRQ or PC/RA patches. Current VU0 runtime
is synchronous on the EE owner; asynchronous arbitration/hardware timing unproven.
Contract references: pinned PCSX2v2.8.2 SPR.cpp and Dmac.cpp in work/ipu-reference-001,
exact hashes and scope in work/spr-vu0-contract-244.json. No implementation copied.

Bootstrap12/12 PASS (work/spr-vu0-bootstrap-test-244.log); real Release/PCH build
PASS (work/spr-vu0-native-build-244.log). Initial test build missing<string> was
corrected; failed log retained. New tests cover source producer updates, exact
1536 bytes, guards/source preservation, SPR wrap, stopped/deferred starts, final
VU quadword, empty transfer and once-only completion edge; bad options/mirrors/
crossings refuse before modifying state/data. Previous Python55/strict IOP1/1
remain prior-checkpoint results; those unrelated checks were not rerun here.

Native244: SHA256 eb05a13886f851f91c04a8bf0606163d745201c414eabe8ddf5216b93d3f9666,
1024M/300s/frame600, exit5/timed_out=false. Private observer scanned available
captures, matched actual GS title9000/menu9600/NORMAL10200/Yes10800 and ordinary
Start/Cross/Left/Cross700ms had SIO2 press/releases. Matched GS states were
subsequently visually inspected. No Computer Use or OS input.

At least16 actual VU0→SPR completions logged, with varying QWC/payloads. First:
source110049E0/requestedQWC60 → MADR11004FE0, SADR600, STR cleared, D_STAT27F
(bit9 asserted), final QWC0 from completed adapter. Source/destination FNV1a64
both64A7DDCB9D750C25; this differs from all-zero1536-byte hash6AB05EF9AA8B9B25.
The log's qwc field is the original request count. Subsequent fromSPR transfers
also progressed. Trace includes caller0x2983E8 after restored leaf return.

**Current blocker:** EE scheduler missing target0x29353C, RA0x2984EC,
SP0x11D9D0, exit5/no timeout. The next owner0x29322C map ends exactly0x29353C;
generated conditional exit293524→293538 enters another jump's delay instruction
then falls through at the function end. Adjacent fragments2936AC..2936CC and
2936E0..2937D0 are visible in the map. This supports another incomplete-leaf
hypothesis, but a real epilogue/bounded recovery must be verified before export.
Do not register arbitrary aliases or infer hardware VU success from this trace.
Final GS640x511 is flat RGB128 gray, privately converted/visually reviewed.
No gameplay/host-audio/saves/recovery proof; old crashes/grouping key unresolved.

**Next experiment245 (verify unused):** inspect original instructions/Ghidra owner
0x29322C and reachable epilogue, recover only verified flow using the existing
read-only analysis regression workflow, export a fresh map/lift, compare all
resume labels/registrations and run behavioral checks/build/bounded native245.
Do not overwrite lift008 or original inputs/upstream. The prior synthetic
alternate-delay-slot-exit regression remains relevant; no unverified padded span.
Use the existing ordinary menu recipe; private observer work/spr-vu0-menu-244.py
can be reused with a NEW run number/output after reviewing its exact GS ROIs.
Private logs: work/spr-vu0-run244-{launcher,startup,observer}.log and menu.jsonl,
work/native-run-244/, work/spr-vu0-contract-244.json. Captures/assets remain private.

## Historical: truncated leaf recovered; measured VU0-data toSPR wait

This supersedes all checkpoints below. Entry HEAD231fa5e includes the approved
README main-menu illustration; use Git HEAD/origin/main for subsequent publication.
Preserve `.vscode/` and `native/Splash.png`. No Computer Use, OS input, ready/PC/RA
patches, game skips, input/upstream edits or false device completions.

Native240's0x298154 was not established as a missing resume alias. Actual owner
begins0x297E44; old map ended0x298154, after a conditional exit into the delay
instruction of another jump. Ghidra showed that slot's ordinary fallthrough but
no decoded instruction there. Existing `RecoverLeafFunctionFlow.java` recovered
the real bounded leaf to exclusive0x2983E8, including JR0x2983E0 and stack-restoring
delay0x2983E4. No padding span or invented instruction was inserted.

Used existing project drakengard-eu-001/SLES_523.22 with `-readOnly -noanalysis`;
temporary disassembly/body changes were discarded after export. New private
`work/functions-ghidra-009.csv` SHA256:
`b71cae632f228fc1e7a04b6214763f485ff74c437e88cfcc0398c29a5d9728a2`.
8585→8583 records: the old leaf plus reachable0x2982C4..0x2982E4 and
0x2982F8..0x2983E8 fragments become one1444-byte leaf. +388 newly covered bytes;
unrelated rows unchanged. Fresh lift-008 generated with verified ELF and pinned
PS2Recomp eb8688b2fd191129f103a39543002c005b33409c:8582 functions,0 stubs/skips/
decode failures. Original lift-007 remains untouched. Runner overlay selects008.
Old fragment destinations are now real intra-function control flow, not shims.

New original `delay-slot-exit` fixture and `CheckLeafFlowFixture.java` reproduce
the missing epilogue before recovery and execute real synthetic instructions
afterward. Immediate exit and two busy iterations return43; loop-edge pause sees
a0=1/v1=1/v0=7 and continuation ends a0=-1/v1=3/v0=43. Delay effects must occur
once, including the alternate-entry slot and return delay. Exact exported range
0x1000..0x1024/36 bytes. Ghidra may exit0 despite script errors: negative log has
the expected exception; after log has actual PASS/export. This is a pinned local
Ghidra emulation regression, not a complete C++ scheduler test.
Toolchain Ghidra12.1.4/EE Reloaded2.1.38/JDK21.0.9. Reproduction is documented
in docs/ANALYSIS_REGRESSIONS.md. Python55/bootstrap11/11/strict IOP1/1 PASS.

Real full Release runner build passed MSBuild17.11.9. Initial241/242 binary:
`de6a0908b1ddf88b547ad8eda1caf6bee9861d8006788c814c326d92255951d0`.
Native241 timed out300s at title: startup helper succeeded, but a throwaway
observer incorrectly waited for capture600, which was not created before GS
was ready. It did not exercise post-Yes. Failed observer/logs retained; not a
device refusal or proof of the recovered tail.

Native242 fixed only the throwaway observer, skipping absent early captures.
Actual GS title9000/menu9600/NORMAL10200/highlighted Yes10800 were matched to
reviewed private GS regions and subsequently visually checked. Ordinary
Start/Cross/Left/Cross700ms each had actual SIO2 press/release. Yes at197.86s;
no further input. The old missing target passed: actual PC repeatedly0x2981E4
inside restored code, waiting on toSPR CHCR STR. It did not complete the leaf.

Added bounded nonmutating channel9/global-control fields to existing progress
observations, rebuilt real runner; native243 SHA256:
`2052fa9845d1996478187657580203c590b2caf29cd785495164e76432c010bf`.
Same1024M/300s/frame600 profile; helper0, Yes at196.515s, roughly100s remaining.
Measured at PC0x2981E4/RA0x2984FC: CHCR0x101, MADR0x110049E0, QWC0x60,
SADR0, D_CTRL0x1, D_ENABLER0x1201. Pinned runtime maps11004000..11005000 to
the existing4-KiB VU0 data RAM/getVU0Data. Do not substitute a new RAM buffer.

Native242/243 timed_out=true/exit=null (launcher124). IOP/SIF continued; STR and
the transfer registers remained pending through watchdog. Last periodic16800
is black640x511, privately visually checked; no final-on-exit capture. No new
explicit device refusal, VCALLMS execution, gameplay/host-audio/saves/recovery
proof. Scalar key0xCD277739 and old C0000409 remain unexplained/not fixed.

Next244 (verify unused/processes): research normal toSPR admission and transfer
for the measured VU0-data source, not another chain tag or missing entry. Reuse
actual shared VU0 data/SPR ownership; establish register progression and real
completion/DMAC cause9, implement confirmed behavior with boundaries/IRQ tests,
build and run bounded native. Never clear STR on read or fake transfer/IRQ.
Menu states above are observations, not universal frame-based readiness rules.
Startup helper unchanged; no first-capture600 assumption.

Private evidence: work/leaf-flow-contract-243.json; work/functions-ghidra-009.csv;
work/flow-boundary-{inspect,recovery,full-export}-241*.log; work/lift-008/;
work/delay-slot-fixture-{before,after}-241.log; work/analysis-delay-slot-exit-001/;
work/native-run-{241,242,243}/ and work/flow-runN-{launcher,startup}.log/menu.jsonl.
All captures/projects/generated/exports stay ignored. Public README image from240
is unchanged. Entry point CONTINUE.md; next prompt NEXT_MODEL_PROMPT.md.

## Historical: post-Yes scalar compare fixed; next missing EE scheduler target

This supersedes every checkpoint below. Entry HEAD was published58c9520; use Git
HEAD/origin/main for subsequent validated publication. Preserve `.vscode/` and
`native/Splash.png`. No Computer Use, OS input, guest flags/RA/PC patches, game
skips, input/upstream/generated edits or fabricated completion.

Native238 repeated237's actual menu with1024M/300s/frame interval600, unchanged
binary SHA256 `8ce4e1004eaaa91f3c9b577fcb69a655347d0cebbaaa1b09c557f1dad55f517b`.
Ordinary Start/Cross/Left/Cross passed verified GS states earlier. First Start
at fading titleGS8400 was delivered by SIO2 but did not open the menu; repeat
at fully-lit9600 worked. GS10800 main menu,12000 NORMAL dialog,12600 highlighted
Yes, final black640x511. Exit6/timed_out=false: Unsupported IPU register read.

Do NOT implement a successful read at the terminal IPU offset. Diagnostics239
showed this was downstream of an invalid consumer loop, not a new movie command.
Actual producer entry0x137D30: list0x11FE30, count2, filter0x3B05F0. Objects
0x3B05F0 and0x3B0F30 had grouping reference0 and key0x7FFFFFFF. The diagnostic's
`type` label denotes this grouping-reference field, not an established object type.
Actual consumer0x13764C received a0=0x70000000/a1=0; n-gap underflows and its
unsigned loop runs outside scratchpad. Later LWC1 at0x137698 eventually aliases
IPU physical0x1000200C, which correctly refuses. Native239 exit6/no timeout,
binary SHA256 `0acd0c70ce5c287b05775067ab66827c433f4076590741b267669f6d6a76c1ae`.

Dataflow: real grouping producer0x1373E8 loads the max sentinel from0x56F100,
uses C.LT at0x1374A4 to update nearest grouping reference/key. Upstream scalar
comparison macro used host IEEE `<`, for which0x7FFFFFFF is NaN/unordered.
Implemented original `native/runner/ee_fpu_compare.h`: exponent-zero operands
compare as zero; exponent255 clamps to signed host-finite max; bit-derived
ordering keys implement EQ/LT/LE independent of host NaN/DAZ/FTZ. FPR source
bits and generated FCR31 update semantics are unchanged. Existing shared macro
shadow/PCH redirects emitted FPU_C_EQ_S/OLT_S/OLE_S; no game-specific workaround.

Contract pinned to [PCSX2v2.8.2 FPU.cpp](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/FPU.cpp)
C_cond_S/fpuDouble and
[iFPU.cpp](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/x86/iFPU.cpp)
fpuFloat3/recC_EQ/LE/LT. Private refs `work/fpu-compare-reference-001/`:
FPU.cpp SHA256 `204dcb290f9f56e9112622ab313ef12d2192eda5b43d24efcf2dbc8ca3fa7344`;
iFPU.cpp SHA256 `e8cd2ca18455fffce292ab65f011552e9eddb0c4f7d9cecdac724e41502e1c10`.
This is a functional reference convention, not hardware FPU arithmetic/rounding
equivalence. No emulator implementation copied into public source.

Behavioral tests `native/bootstrap_smoke/fpu_compare_tests.cpp`:17 boundary
vectors for EQ/LT/LE and reverse ordering, zeros/denormals/first normal, signed
max/clamp limits, ordinary precision and synthetic nearest-candidate selection
with PS2 sentinel. Bootstrap11/11 PASS; strict IOP1/1 and Python55 PASS.
Real full Release runner/PCH rebuild passed MSBuild17.11.9; checked binary240 SHA256
`26fab6b8e49ae69029683a14acd99950ae9a99ca48038a01bfd44d4ea45746a4`.

**Native240 after-fix proof:** same actual two objects now have grouping reference
0x3B05F0, first observed key0; consumer count2, twice. It passes the old runaway
loop rather than patching its count. Second observed key0xCD277739 is unexplained;
do not claim correct geometry or full FPU arithmetic. All ordinary menu input
remains SIO2-observable. Final direct GS black640x511, privately visually checked.

**First remaining boundary:** `[guest-branch:missing-target] kind=DirectJump
op=EE scheduler source=target=0x298154`, RA0x2984FC, SP0x11D9D0, GP0x572C70.
Trace ends in game code; runtime stops with Game thread returned at that PC.
Native240 exit5/timed_out=false, not game success. No gameplay/host audio/saves/
recovery proof; old crash219/226 remains unresolved. No debugger was attached
in238..240. Existing unsupported IPU/device guards remain intact.

Next241 (verify unused/processes): map0x298154 to owning lifted function and its
resumable labels; compare emitted checkpoint, scheduler dispatch and registered
function table. Establish whether this is a missing resume entry before changing
the original generator/overlay. Add behavioral yield/resume coverage, build the
real runner and run1024M/300s. No arbitrary alias/stub, PC/RA fix or guest patch.
Menu recipe239: verified full title9000→Start, main menu10200→Cross, NORMAL11400
→Left, highlighted Yes12000→Cross, each700ms. In240 title only settled at9600;
observe actual states. Startup helper remains `native_start.py --timeout 300`.

Private evidence: `work/native-run-{238,239,240}/`, corresponding
`work/post-menu-runN-launcher.log`, `work/fpu-compare-contract-240.json`,
`work/native-run-240/final-review.png`. Validation commands/results are recorded
in `work/fpu-compare-validation-240.json`; these are local exercised checks, not CI.
Entry point docs/CONTINUE.md; next-model prompt docs/NEXT_MODEL_PROMPT.md.


## Historical: ADMA disarm/stop passed; rendered main menu and New Game dialog

Published accumulated checkpoint0859793 to origin/main; CI40106657373 succeeded.
The user authorized further validated source commits/pushes and a final next-model
prompt. Preserve unrelated `.vscode/` and `native/Splash.png`; no inputs, generated
code, upstream files, dumps or images were published. Use Git HEAD for the latest
publication rather than historical hashes below.

Native234 measured guest PC0x87778 clearing DMA4 STR, CHCR01000201->00000201.
Pinned v2.8.2 IopHw.h/DmaExec, IopDma.cpp and ReadInput.cpp distinguish DMAC STR
from the SPU producer: STR-clear does not reset input counters; final DMAC IRQ
checks STR. Implemented automatic-only exact0x201 disarm, real refill/MADR progress
and suppressed IRQ on later drain. Other CHCR values and active rearm still refuse.

Native235 verified this and then refused actual guest SH0 to ADMAS0xBF9001B0,
PC0x8787C, step377226678. Its executable SHA256 was
`c3c3d13cbea1d524f8f55ceb0cb81b748c63ca191d8bb2cb2c05a82e44ad6ca6`;
exit6/no timeout. Added the distinct ADMAS0 stop only after DMAC disarm (or idle):
release producer/RAM-port ownership; discard producer remainder/pending accounting;
retain MADR/BCR/STATX/interrupt latches, actual SPU RAM and sample/write cursors.
No finish() call or invented completion. The reference's RAM-zero workaround is
explicitly marked inaccurate in its own source and is deliberately not applied.
This is a functional convention; no hardware timing equivalence claim.

Tests cover both cores with pending0/pending640 byte stops, whole ring retention
over inactive ticks, armed-stop refusal, faulted SH rollback, disarm-only drain
without IRQ and genuine new transfer completion after reenable/rearm.
Strict IOP1/1 PASS005; native Release build002 PASS. Earlier bootstrap10/10 and
Python55 remain unchanged/passing. A test-only mixed-auto-type compile failure002
was corrected; failed log retained. Private contracts:
`work/adma-stop-dmac-contract-235.json`, `work/adma-stop-producer-contract-236.json`.

Native236 SHA256 `8ce4e1004eaaa91f3c9b577fcb69a655347d0cebbaaa1b09c557f1dad55f517b`:
actual core0 ADMAS0 retired; `[spu-adma-stopped]` MADR0x18E100, remaining3072,
pending0, sample488529, without completion/IRQ/RAM clearing. Continued beyond it.
Direct GS7200 shows Project Dragonsphere logo; GS8400/9000 show **Drakengard /
PRESS START BUTTON** at640x511. Privately converted and visually inspected.
These are guest GS captures, not OS-window screenshots. No Computer Use.

At15:32:32.636027UTC host pad sequence11 pressed Start0xFFF7 for700ms;
SIO2 press/release both observed. Final236 GS shows the title zoom/transition,
not an opened menu. Exit6/no timeout because the configured512M IOP budget was
exhausted. This is a diagnostic resource bound, not a newly measured device fault.
Native237 repeats unchanged code with existing1024M option, still300s maximum,
frame interval600 and ordinary startup helper300. Its outcome follows below.

Native237 used the SAME236 executable,1024M IOP budget and300s watchdog.
Start sequence11 FFF7/700ms at15:36:09.816133UTC was accepted/released by SIO2.
GS10800 **renders the main menu: NEW GAME / LOAD GAME / OPTIONS**. Cross sequence12
at15:36:49.911480UTC opened the NORMAL difficulty Yes/No dialog, visible atGS13200.
Left13 at15:37:36.436445UTC and Cross14 at15:37:42.469194UTC confirmed Yes;
all press/releases are logged through actual SIO2. Images remain private.

Native237 ended timed_out=true/exit_code=null at300s. Last periodic GS16200 is
black640x511; IOP/SIF continued through728402427+ instructions. No explicit new
unsupported-device refusal was observed. There is no final-GS-on-exit capture.
This is bounded post-New-Game transition evidence, **not gameplay or a diagnosed
hang**. Menu/dialog are directly rendered GS evidence, not OS-window or host-audio
verification. Private summary: `work/adma-stop-menu-evidence-237.json`.

**Next experiment238 (verify unused first):** reproduce title/menu with unchanged
code and1024M/300s, use ordinary Start/Cross/Left/Cross earlier once their actual
GS states are verified, leaving more time after New Game. Observe the first
post-Yes producer/device/control-flow boundary; instrument that boundary only.
Do not assume BDEC, movie decode or loading is the cause of a black frame. If
needed compare an equivalent bounded owned reference state sequentially. Preserve
unsupported guards and follow the first actual failure. Gameplay, host audio,
saves/recovery and older crash219/226 remain unverified. No splash integration.


## Publication and continuation authorization (2026-10-24)

The user authorized publishing the validated accumulated movie-device checkpoint
and continuing with the measured ADMA stop. The source/tests/docs below are
reviewed for publication; ignored evidence, `.vscode/`, and `native/Splash.png`
remain excluded. Statements about no publication in historical entries describe
those earlier experiments. The current checkpoint is identified by Git HEAD.

## Historical checkpoint: B-picture MBT passed; diagnostic window wait repaired

This supersedes the ADMA/FX checkpoint below. All work remains local; published
HEAD is still2cf7b2c. Preserve unrelated WIP, `.vscode/` and `native/Splash.png`.

Implemented all11 B-picture macroblock-type prefixes in the original IPU adapter.
B DATA packs consumed length in bits16+ and modes including MC_FRAME; this also
applies to intra B MBT. Existing I/P DATA has no length field. Invalid000000
sets ECD without consuming a VLC. D-picture remains explicitly unsupported.
FB and partial TOP refill retire each field only once. Tests cover11 canonical
vectors, all64 lookahead prefixes, invalid/ECD/BP and starvation/retry/IRQ.
Pinned PCSX2v2.8.2 conventions are reference evidence, not PS2 hardware proof.
Private provenance/vectors: `work/bmbt-contract-232.json`.

Bootstrap10/10 and Python55 PASS; native Release build PASS. Logs are
`work/bmbt-bootstrap-{build,test}-001.log`, `work/bmbt-python-test-001.log` and
`work/bmbt-native-build-001.log`. Native232 SHA256:
`92c3e6f05c9c64eef88ef037d81d33dff836e9a23335226d6b11f21f8e86d08e`.
It passed at least512 B-MBT calls with DATA0x40088 and advancing BP/TOP.
Four actual ADMA completion logs (aggregate transfer counters102..105) now prove
full transport completion beyond prior refill-only231. Host audio is still unproven.

Native232 timed out300s without a final GS capture. Progress had stalled before
an instrumented ProcDump snapshot. Offline dump parsing plus a diagnostic relink
MAP, verified by identical `.text` SHA256, identified a host event wait. Stack
addresses are code candidates, not a formal unwind. Direct raylib CORE memory
showed flags0xA04, minimized=true, ALWAYS_RUN=false, eventWaiting=false; local
raylib PollInputEvents waits while minimized unless ALWAYS_RUN is set. Private
dump/MAP/parsers and evidence stay in `work/bmbt-hang-232/` and must not be published.
No claim that older C0000409 failures are fixed.

For file-controlled `--pad-control` diagnostics only, configure ALWAYS_RUN before
window initialization, verify the flag afterward, and log minimize transitions.
This fixes host event waiting without changing guest data, device completions,
input injection or upstream code. Raylib remains isolated in host_pad_input.cpp;
including it in the PCH runner caused a Win32 name collision, corrected before
accepting build003/004 PASS. Native233 completed exit6/no timeout, SHA256
`1a41c0cd5646b07d1de90a788d3c053fd03ae8bdc0a17cb8ff97d48b0b9f6c56`.
The ALWAYS_RUN flag passed the startup runtime check; no minimize transition was
observed, so minimized execution itself was not exercised in233. B-MBT>=512 and
four actual ADMA completions reproduced. The next explicit refusal is **Active SPU
DMA cancellation/reconfiguration unsupported**. Final GS640x508 is all RGB0,
converted and visually reviewed privately. No Computer Use was used.

Native234 adds nonmutating diagnostics of the stopping CHCR store (PC, previous,
requested value) and active transport snapshot (core, mode, CHCR/MADR/BCR, source,
end, remaining, pending, sample). It retains the refusal; do not invent a cancel
completion or clear internal busy before establishing the exact observed contract.
Release build005 PASS. Private runs/logs: `work/native-run-233/234`,
`work/bmbt-run{233,234}-{launch,startup}.log`. Follow the234 outcome below.

Native234 reproduced exit6/no timeout, SHA256
`e1fbe572a39e9ef50ccfb9ddab5e3a983dab824b4b0400a495466e1589f3c4f0`.
Exact stop: IOP PC0x87778 writes CHCR0x00000201 over0x01000201 at0x1F8010C8,
step375375262. Core0 ADMA active, MADR/source0x18DD00, BCR0x00C00010,
end0x18F900, remaining3584 halfwords, pending0 bytes, sample481783. The guest
really clears STR; this is not another B-picture/video refusal. Four ADMA
completions reproduced before it. Final GS is byte-identical to reviewed233.
Private exact evidence: `work/bmbt-next-adma-stop-contract-234.json`.

**Next experiment (235, verify unused first):** establish active ADMA STR-clear
semantics against the pinned DMA/input owner and, if needed, a bounded owned
reference capture: already copied ring contents, MADR/BCR, remaining producer,
busy/STATX, consumer cursor, suppressed or pending IRQ and subsequent rearm.
Implement only the observed stop contract with synthetic stop/rearm assertions,
then build and run bounded native235. Do not call finish() on cancellation or
claim hardware timing from the emulator. No movie/menu/gameplay/audio proof.


## Historical checkpoint: real ADMA input and nonzero FX passed; next B-picture MBT

This supersedes the ADMAS-only checkpoint below. HEAD remains2cf7b2c; all changes
are local, including the preceding agent's P/motion/toSPR/CSC/ADMA work. Preserve
`.vscode/` and the untracked user asset `native/Splash.png`. No Computer Use,
upstream/generated/input changes, commit or push in this continuation.

The journal had missed completed runs228/229.228 measured core0 ADMA:
ADMAS1, ATTR0xC080, SPDIF0xC032 (normal/PCM-clone), MMIX0xFF0, input gains32512,
source6144 halfwords with5741 nonzero, FNV1a64 `1e1c9506188f40a3`.
The existing unjournaled `SpuDmaWrite` draft implements split-stereo PCM16 ring
refill in the shared SPU RAM, sample-paced MADR, and one-shot completion through
the existing DMA owner. Its transport/CPU/sample-clock tests rebuilt successfully.
Independent reference audit found no demonstrated transfer defect. Active stop,
cancel/reconfiguration and alternative stream formats remain explicit refusals.
Run229 actually copied two refills and delivered PCM(-2,-2), then hit the next
consumer: the zero-input-only FX guard. It was not still blocked on ADMA admission.

New native230 reproduced this without a debugger, exit6/no timeout. A nonmutating
snapshot established active core0 FX workspace0xFC810..0xFFFFF and all coefficients:
`6f60,4fa8,bce0,4510,bef0,a680,5680,52c0,8000,8000`. Thus zero substitution is invalid.
Executable SHA230 `bc19f06b26bf9dff62f543df6d512c32f7947f740a34644488faae03b4fb0997`.
Private contract/provenance: `work/adma-resume-fx-contract-230.json`.

Added original `spu_reverb_dsp.h`: paired symmetric39-tap FIR, persistent input/
output histories, alternating L/R reflections, four comb taps, two APF stages,
clipped writes to the same `SpuRamWritePort`. PS2 raw high/low address pairs are
word offsets; the reference's separate PS1 multiply-by4 path is not used.
All delay reads precede stores, including aliases. FX-off advances filters and
reads delay RAM while suppressing stores. Signed Q15 floor products explicitly
refuse signed32 overflow; no PS2 hardware or SIMD saturation equivalence claim.

The real bootstrap opts into this DSP at initialization. Standalone zero-domain
probes retain their original guards. Each core receives every routed sample from
the existing IOP/SPU clock; missing frames fail closed. Actual FX PCM and nonzero
delay-store counts are bounded diagnostics. Full dry/master/external output and
host audio sink remain incomplete; nonzero unsupported cross-core wet paths refuse.

Pinned PCSX2v2.8.2 reference remains ignored in `work/spu-adma-reference-001/`:
Reverb.cpp SHA `8dbb9d294ea58660f5d69c7bbddb947540f0acdabf7dd92bf74fe494242296ee`;
ReverbResample.cpp SHA `a10c04edc1f590fe3da1d8d4574d8e89a06b5bcb5fbd356c17886b124d56e872`.
No emulator implementation copied into public source. The original portable
implementation was independently audited against scalar phase/address equations.
FIR sum is32766, not32768: settled16000 ->15999 ->7999 ->3999 at half input/IIR
gain. Synthetic fixtures were corrected before accepting results; early failures
were fixture issues, not hidden. Final strict IOP1/1 passed with these goldens,
seeded comb/APF500/250, negative rounding, alias/wrap and real CPU ADMA→volume→
MMIX→FX RAM mutations plus DMA completion. Logs `work/adma-resume-fx-strict-*-007.log`.
Bootstrap10/10 and Python55 passed; final native Release build003 passed.

**Native231 complete:** SHA
`683e2c65ee47c02ba07279371864a447f67abbcf80b3f960f28868d067fd3757`,
exit6, timed_out=false. No debugger attached. Three real512-halfword refills,
actual nonzero routed PCM, at least320 nonzero delay-RAM stores. This passes the
229/230 guard with real processing. The full6144-halfword stream had not completed
before the next video blocker; do not claim native full-stream completion/audio.
First next refusal is **VDEC0x34000000, CTRL56770312 decimal=0x03623F08,
PCT3/B-picture**. Implement/verify observed B-MBT modes from pinned reference;
then follow the next actual backward-motion/non-intra contract rather than guessing.
Next unused232, verify directory/processes before reuse. Both230/231 used300s,
512M, frame interval600 and `native_start.py --timeout300` with confirmed SIO2.
Final GS231640x508 is all black and visually reviewed. No nonblack movie/menu/
gameplay/audio/save/recovery claim; earlier219/226 crash is not proven fixed.

Evidence: `work/native-run-230/`,231/; `work/adma-resume-runN-{launch,startup}.log`;
`work/adma-resume-fx-contract-230.json`; strict/native build/test logs above.
Future UI task requested by user: show `native/Splash.png` immediately in the
host window instead of the reported initial pink background. It is queued,
not implemented. Keep that host presentation separate from guest frame evidence;
review asset/public-tree handling before any publication.

## Historical checkpoint: ADMAS configuration retired, next actual ADMA transfer

This section supersedes all checkpoints below. Local changes over HEAD2cf7b2c,
no commit/push. Earlier source/docs and `.vscode/` preserved; no Computer Use,
upstream/generated/input edits, guest skips/ready flags or success stubs.

Native224's0xBF9001B0 is **SPU2 core0 ADMAS** (physical0x1F9001B0), core1
register0x1F9005B0. Existing owner is `SpuQuiescentControl`, reached through
`IopDataBus` prepare/retire staging; no second register mirror was introduced.
Idle halfword writes0..3 now retire and read back; other bits/PS1 switch remain
closed. Own-core ADMA enable is bit0 for core0 and bit1 for core1 (numeric
masks1 and2). The other bit does not disable that core's plain DMA readiness.
Enabling configuration alone does not transfer samples, alter STATX or create
IRQ. Changes during busy plain DMA are refused, while an identical idle-control
value remains harmless. Faulted guest stores restore backing without committing.
Plain DMA explicitly refuses active ADMA streaming before retiring the CHCR
start/copying data; it is never used as a fictitious ADMA transport.

Contract pinned against
[PCSX2 v2.8.2 regs.h](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SPU2/regs.h),
[ADMAS write handler](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SPU2/spu2sys.cpp)
and [DMA selection](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SPU2/Dma.cpp).
Private reference directory `work/spu-adma-reference-001/`; spu2sys.cpp SHA256
`f6b3a5d337e9df0e251c4f0da90770e9d7521fc8c3ef364106c0ac8b26b8292f`;
Dma.cpp SHA256 `322cd94a54e292a883ce378e147631759e19b0ca6011cf3b065d7420cf9472dc`.
No reference implementation copied or upstream changed. Private contract:
`work/spu-admas-contract-225.json`.

Behavioral regressions in `spu_control_tests.h`, invoked by strict `selftest.cpp`,
execute guest SH/LH/LHU for both cores/physical-KSEG aliases, halfword truncation,
core masks, unchanged backing/STATX/IRQ, exception rollback and ADMA-vs-plain DMA
refusal before any transfer/IRQ. The existing real plain-DMA test also checks the
busy ADMAS guard. Strict IOP CTest1/1 passed after final diagnostics, bootstrap10/10,
Python55 and public-tree PASS. Strict build still emits C4834 from unchanged
selftest code at151/168; these warnings were not suppressed. Two real Release
runner builds passed (MSBuild17.11.9). Final executable SHA256:
`3c5ddd729e714ca0e1eac99e6e3c6896616978254fcde300240447209edf8ec1`.
First build/run225 SHA256:
`8cbfd0a5b85626b9c75487d3ac1a288af1fe004d894ce326825822a30295e2a1`.
The second-build addition is bounded `[spu-admas-store]` retirement observations.

Run225: timed_out=true300s, exitnull, no GS captures; helper exit2 with no startup
input because it never reached the frame checkpoint. Log shows early EE27F4C4
and fixed IOP budget diagnostics. It does NOT exercise ADMAS=1.
Run226: Windows exit0xC0000409, no timeout/final GS, helper exit2 after three
Left/Cross attempts; only ADMAS0 initialization observed, not the new path.
This repeats the unresolved219 crash. Do not describe it as fixed.
Run227: attached reused ProcDump64 `-e -ma -w ps2EntryRunner.exe`, logs under
`work/spu-admas-crash-227/`. It observed handled C++/dispatcher exceptions, exit6,
no crash/dump. Debugger attachment changes the experiment; no timing/race fix
is inferred from the absence of a crash.

**Actual proof in227:** `[spu-admas-store] core=0 value=0x1 attr=0xC080
status=0x0 count=3; guest SH retired`. Then the first real unsupported request:
IOP PC0x88130, DMA4 CHCR register0x1F8010C8, attempted value0x01000201,
MADR0x0018C900, BCR0x00C00010, steps371590635. Size is12288 bytes/6144 halfwords.
The engine refuses it explicitly: `Unsupported SPU auto-DMA streaming transfer`.
The CHCR start was not retired and no ADMA completion was fabricated.
Run227 exit6, timed_out=false, helper succeeded/stopped at actual IPU. All runs
225..227 use300s/512M/frame interval600/ordinary pad/helper timeout300.
Final direct GS227640x508 all black, visually reviewed; no host audio, nonblack
movie/menu/gameplay/saves/recovery proof.

Next228 experiment: establish real ADMA input layout, refill/read cursors,
sample-clock pacing, MADR progression and completion/IRQ from pinned reference,
then implement/tests/build/bounded native. Reuse shared `SpuRamWritePort`2-MiB RAM,
`SpuDmaWrite`/`QuiescentDma` ownership and `SpuIdleVoices`/IOP timers' sample clock;
do not add another disconnected RAM or fake a plain-DMA copy with immediate IRQ.
Pinned `Dma.cpp` StartADMAWrite/AutoDMAReadBuffer, `ReadInput.cpp`, `spu2sys.cpp`
provide the producer/consumption/completion facts. Do not infer a particular PCM
format/mixer path from ADMAS alone; establish actual guest flags and input data.

Evidence: `work/native-run-225/`,226/,227/ and corresponding
`work/ipu-vdec-runN-launcher.log`; `work/spu-admas-contract-225.json`,
`work/spu-admas-validation-227.json`, `work/spu-admas-crash-227/procdump-utf8.log`,
private final GS227/review PNG. Next unused228, verify directory/processes.
Entry point: docs/CONTINUE.md; startup helper unchanged, no OS input.

## Historical checkpoint: RGB32 CSC passed, next IOP halfword MMIO

This section supersedes older checkpoints below. Local continuation over HEAD
`2cf7b2c`, no commit/push. All earlier source/docs edits and `.vscode/` preserved.
No Computer Use, upstream/generated/input edits, guest flags/RA patches or skips.

Native223's CSC0x700003E8 means MBC1000, OFM0/RGB32, DTE0. Implemented original
`native/runner/ipu_csc.h` and the CSC state machine in `ee_ipu.h`.
Each input macroblock is384 bytes (Y16x16/Cb8x8/Cr8x8 byte planes); output is
256 row-major RGBA8 pixels/1024 bytes. Chroma is replicated over2x2 pixels.
Integer coefficients149,-104,-50,204,258; clamp Y below16 first, separate signed
floor shifts by6, final add1/shift1/clipping. SETTH0 strict all-RGB comparison
zeros RGBA; otherwise SETTH1 strict comparison sets alpha0x40; default alpha0x80.
No generic floating-point BT.601 approximation, canned pixels or dummy output.
RGB16 is an explicit refusal; DTE has no effect on the implemented RGB32 path.
Bit-exact hardware conversion has not been independently measured on a PS2.

CSC buffers one input block and stages RGB into the existing eight-QW output
FIFO, retaining progress on input starvation/output pressure. Macroblock storage
is shared with BDEC, expanded to1024 bytes. Conversion is performed once per
block, not on each retry. Input staging uses current BP with no implicit reset
or FB. Completion IRQ occurs when the final QW is enqueued (may leave8QW in
FIFO), without trailing TOP reads; unlike BDEC it need not wait for full drain.
Reset abandons partial input/output state. MBC0 consumes/produces nothing.

Contract checked against pinned
[PCSX2 v2.8.2 IPU worker](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IPU/IPU_MultiISA.cpp),
[command fields](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IPU/IPU.h)
and [integer arithmetic facts](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IPU/yuv2rgb.cpp).
Original code uses a chroma-group2x2 traversal, not copied decoder implementation.
Private `work/ipu-reference-001/yuv2rgb.cpp` SHA256
`0486fa40bff7256705418788ea6eebf27dd8d69db7896fa7b5cead0ee965bb11`.
Research record: `work/ipu-csc-contract-224.json`.

`ipu_csc_tests.cpp` verifies independent color/rounding/clipping goldens,
SETTH equality/precedence/9-bit values, planar/chroma/luma pixel placement,
nonzero BP and trailing FDEC bits, reset of partial input and staged output,
completion before final FIFO drain, multi-block DMA, partial output DMA and input
backpressure, guards/unchanged source bytes, zero count and RGB16 refusal.
Bootstrap CTest10/10, Python55 and public-tree gate PASS; real Release runner
build passed (MSBuild17.11.9). Executable SHA256:
`c4e4dc125cff2a63fa5146d5a81bfc96c5092afe7ae4da3531727cbd33c6fbf8`.
Bounded diagnostics now log CSC starts and larger from-IPU RAM transfers/hashes,
without reading CMD or consuming stream bits.

Native224 passed CSC: actual1000-block command, output QWC0xFA00/1024000 bytes
into RAM0x01C4BB40..0x01D45B40; CTRL0x02623F00, BP0/TOP0 at completion.
Output FNV1a64 `0x21F997E470CAA325` independently matches1000 all-black RGBA
blocks with alpha128. This proves this initial black frame/transport, not colored
native movie correctness. Final direct GS640x508 is black, visually reviewed.
No nonblack movie/menu/audio/gameplay/saves/recovery claim.
Run exit6, timed_out=false;300s/512M/frame interval600. Startup helper succeeded
at real IPU with ordinary pad and timeout300; no OS input.

**First next blocker:** IOP halfword store, not another observed IPU command.
`[bootstrap-mmio-blocker] steps=368698821 pc=0x880B8 instruction=0xA6020000
address=0xBF9001B0 rt_value=1 ra=0x880BC a0=0 a1=0xBF9001B0`.
Physical register0x1F9001B0; establish its identity and side effects before editing.
Next225: locate existing IOP MMIO owner/mapping, check pinned register contract,
implement the actual16-bit behavior with regressions, rebuild real runner and
bounded run. No register-mirror success fallback simply to pass the access.
Earlier native219's0xC0000409 remains unresolved; no recurrence is not a fix.

Evidence: `work/native-run-224/{runtime.log,result.json,pad-control-events.jsonl}`,
`work/ipu-vdec-run224-launcher.log`, `work/ipu-csc-contract-224.json`,
`work/ipu-csc-validation-224.json`, private final/review GS images.
Next unused225; verify directory/processes. Resume via docs/CONTINUE.md.

## Historical checkpoint: real toSPR chains, next CSC command

This section supersedes all older checkpoints below. Local changes over published
HEAD `2cf7b2c`; no commit/push. Preserved earlier source/docs work and `.vscode/`.
No Computer Use, upstream/generated/input edits, guest patches or success stubs.

Native222 measured the actual old wait: toSPR channel9 CHCR0x105, source-chain,
QWC0, TADR0x006ACE80, SADR0x70000000, D_CTRL1, DENABLER0, SQWC0.
STR remained set over a billion polls; exit6 at fixed IOP budget, no timeout.
Upstream `tryProcessScratchpadDma` handles only normal mode and returned false
for this real source-chain start. Measurement executable SHA256:
`0d6611dfc0854c0708b654e93cc5564f22c15756414801293aa73b5b4294a974`.
No interleave or normal-mode defect was inferred from the wait.

Implemented original `native/runner/ee_spr_chain.h` and `cmake/spr.cmake`.
RAM-only REFE/CNT/NEXT/REF/END: actual tag traversal/payload copying, upper
CHCR tag bits, correct terminal MADR/TADR, SADR16-KiB wrap, full-tag TTE,
TIE+IRQ termination after payload. QWC0 fetches a tag, not fictitious64K output.
Disabled DMA/suspend preserves pending STR; the existing completion-consumer
services it after enable. Cause9 is queued by existing `completeDmacChannel`
only after actual transfer completion, never by reading CHCR.
REFS/CALL/RET, VU/SPR sources, nonzero-QWC restart and chains beyond65536 tags
are explicit refusals. Normal SPR retains pinned upstream semantics; other
scratchpad modes/directions fail closed, rather than silently leaving STR.
`ee_spr_diagnostic.h` adds bounded, nonmutating register snapshots.

Contract checked against pinned
[PCSX2 v2.8.2 SPR.cpp](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SPR.cpp)
and [Hw.cpp](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/Hw.cpp).
Private original downloads under `work/ipu-reference-001/`:
SPR.cpp SHA256 `722c702202ecaa62aaa8a1181429696821a9fe7e8e17d0d9d31abf5f03c421d3`;
Hw.cpp SHA256 `215c0707e81519d8d8f590d73abeec5cedaf9b5ea615fe69cc75bed6bf69e918`.
No upstream implementation was copied into public source or edited in place.

Behavioral `spr_chain_tests.cpp` verifies exact payload bytes, excluded tag bytes,
unchanged source RAM/guards, pointer updates, one-shot completion, CNT/NEXT/END,
full-tag TTE+IRQ stop, IRQ without TIE, multiple scratch wrap revolutions,
disabled/resumed transfer, zero-payload chains, invalid addresses/no partial
payload, unsupported tags/restarts and cyclic-chain refusal. CTest9/9, Python55
and public-tree gate PASS. Real Release runner built with MSBuild17.11.9.
Verification executable SHA256:
`19a155c03416c70540965a01559bd2008d1fa93692e01582e44a741341987288`.

Native223 passed the old wait: at least512 logged genuine transfers, each two
tags/96QW, alternating scratch start0 and0x1B00. First completion:
MADR0x01AE5480, TADR0x006ACEA0, SADR0x600, CHCR0x5, QWC0,
whole-scratch FNV1a64=0xE8454DE7F1872325. The guest then issued more motion
commands and progressed to **IPU CSC command0x700003E8/opcode7**. Exit6,
timed_out=false;300s/512M bound. Both222/223 startup helpers successfully stopped
at real IPU, using ordinary pad and `--timeout 300`; frame interval600.
Final direct GS223640x508 is all black and visually reviewed. No nonblack
movie/menu/gameplay/audio/saves/recovery claim.

Next experiment224: establish CSC parameter/layout/color-matrix/clipping,
OFM/DTE flags, input/output QW counts, FIFO/DMA backpressure and real completion
against pinned IPU reference before implementation. Immediately before refusal,
to-IPU DMA was MADR0x01AE2C00/QWC0x5DC0. Do not substitute color bars/black output
or force DMA completion. Native223 did not establish non-intra BDEC support.
Native219's earlier0xC0000409 remains unresolved; no repetition in222/223 is
not proof of a fix. If it returns, preserve exception/dump evidence separately.

Evidence: `work/native-run-222/` and223/ (runtime/result/controller/GS);
`work/ipu-vdec-run222-launcher.log`,223 equivalent;
`work/spr-chain-contract-222.json`, `work/spr-chain-validation-223.json`;
`work/native-run-223/last-guest-frame-review.png`. Source/tests/docs only public;
all inputs, reference source, raw captures and evidence remain ignored.
Next unused224, verify directory and processes; entry point docs/CONTINUE.md.

## Historical checkpoint: motion VDEC exercised, toSPR STR wait

This section supersedes the checkpoints below. Local continuation over HEAD
`2cf7b2c`, no commit/push. Preserved earlier P-MBT/AGENTS/CONTINUE edits and
`.vscode/`; no Computer Use, input/generated/upstream edits, skip or success stubs.

Implemented table2 motion VDEC in `ipu_vlc.h`/`ee_ipu.h`, all33 codes0/±1..16.
The H.262 magnitude VLC is followed by one sign bit for nonzero values; residual
bits are left to the guest. DATA follows pinned
[PCSX2 v2.8.2 ipuVDEC](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IPU/IPU_MultiISA.cpp):
zero=0x10000; positive=(prefix length<<16)|magnitude (length excludes sign);
negative is32-bit sign-extended. Hardware packing for negative DATA is **not**
verified. Invalid prefixes produce ECD/DATA0 without code consumption rather
than copying the reference's permissive invalid-prefix motion1 fallback.
Numeric prefixes: [H.262](https://www.itu.int/rec/T-REC-H.262-200002-S/en);
private research record `work/ipu-motion-contract-219.json`. No reference decoder
implementation or optimized table layout copied. B-MBT/DMVector remain refused.

`ipu_vlc_tests.cpp` covers all33 codes with exact DATA/BP/TOP/IRQ, invalid prefix,
ECD clearing on valid zero, FB-once/empty input, partial TOP without re-decoding.
Bootstrap8/8, Python55, public-tree gate PASS. Real Release runner built twice
with MSBuild17.11.9. Final runner SHA256:
`acb7393bd461e1c17886da789cb74c9b4ac115b8cce45d1fcfbd0a47429d27ad`.
The first build/run219/220 SHA256:
`aa7bfa3c8de8e18fa61108f9d97e47dd4781078eada6e109ce91677269637f69`.
The only second-build change is bounded nonmutating `[ee-ipu-motion]` diagnostics
in `iop_bridge.cpp` (first16 then powers of two; CTRL/BP/TOP/DATA).

Run219: native Windows exit0xC0000409 before movie IPU commands, no final GS;
not motion verification. No Windows Application crash event was found. Cause
unresolved. For220 downloaded official Sysinternals ProcDump into ignored tools;
archive SHA256 `5fe68e81632f13188930affff20a9a9910170bae654360850d3108cead708a2d`.
Attached `-e -ma -w ps2EntryRunner.exe`, output under `work/ipu-motion-crash-220/`.
It saw handled guest-transfer/C++ exceptions and process exit6, no crash/dump.
Do not call219 fixed merely because220 did not repeat it.

Run220: exit6, no timeout; first refusal was fixed IOP instruction budget.
EE repeatedly at PC0x303E48, RA0x303E2C. Final direct GS640x508 all black,
visually reviewed; no nonblack movie/menu/gameplay/audio/saves/recovery claim.
Run221: targeted logs prove two actual `0x38000000` returning0x10000,
CTRL0x02623F08, BP0x2086B then0x2086C, TOP0x80819C00 then0x01033800.
This exercises zero-motion only; other magnitudes/signs have synthetic evidence.
Run221 `result.json`: timed_out=true at300s, exit_code=null, final capture=null.
Do not misreport it as a clean budget exit even though the log reaches budget
diagnostics. Helper220/221 succeeded at real IPU startup; helper219 exited2 on
ended native. All three used512M,300s, frame interval600 and ordinary host pad.

**Next observed blocker:** STR wait on **toSPR DMA channel9**.
Read-only generated evidence `generated/lift-007/fn_00303D40_0x303d40.cpp:287-334`:
PC303E2C loads v1=0x1000D400; PC303E48 reads CHCR and loops on bit8.
The preceding guest call is303F90. Do not clear STR on read or raise budgets.
Next experiment222: bounded CHCR/MADR/QWC/SADR and global DMAE/suspend snapshots,
actual source/destination and completion path; establish the real toSPR contract
before implementation. Source routing begins in `native/runner/cmake/memory.cmake`;
upstream runtime has `tryProcessScratchpadDma`, so do not assume it is absent.
Implement only a proven transport/completion defect, with byte-level scratch/main
RAM tests, then real runner/build/bounded run. Non-intra BDEC is not yet observed.

Evidence: `work/native-run-219/`,220/,221/ (runtime/result/controller/captures);
`work/ipu-vdec-run219-launcher.log` and220/221 equivalents;
`work/ipu-motion-validation-221.json`, `work/ipu-motion-crash-220/procdump-utf8.log`.
Next unused222, verify directory/processes first. Continue with docs/CONTINUE.md;
startup helper uses `--timeout 300` alongside the300s-bounded native launcher.

## Historical checkpoint: measured P-picture MBT, next motion-code table

This section supersedes all checkpoints below. HEAD remains published `2cf7b2c`;
the following changes are local, without commit/push. Existing AGENTS/CONTINUE
edits and `.vscode/` were preserved. No Computer Use, upstream/generated/input
edits, dummy output, movie skip or guest flags/RA patches.

Native217 measured the old `0x34000000` refusal: CTRL=`0x02623F08`, PCT=2.
It is **P-picture**, not B. Exit6, no timeout. Diagnostic runner SHA256:
`6831be8ef04d1ff61eb33822f60655a7aa23e20d362929d2cdca39e38ae08879`.
Evidence: `work/native-run-217/{runtime.log,result.json,pad-control-events.jsonl}`,
`work/ipu-vdec-measurement-217.json`, `work/ipu-vdec-run217-launcher.log`.

Implemented all seven H.262 Table B.3 P-MBT codewords in
`native/runner/ipu_vlc.h`, integrated via `ee_ipu.h`. DATA is the modes mask
without length; forward motion additionally sets MC_FRAME=0x80. No extra motion
bits are consumed: VDEC forces frame prediction. Numeric contracts checked against
[PCSX2 v2.8.2](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IPU/IPU_MultiISA.cpp)
(`ipuVDEC` and P branch of `GetMacroblockModes`) and
[H.262](https://www.itu.int/rec/T-REC-H.262-200002-S/en), not copied decoder code.
B-MBT and motion/DMVector still fail closed. Refusals now report value/CTRL/PCT,
including the next unsupported BDEC mode.

Behavioral regressions in `native/bootstrap_smoke/ipu_vlc_tests.cpp`: all seven
codes, exact DATA/BP/TOP, malformed zero prefix/ECD/no advance, empty input,
FB-once, partial TOP refill without re-decoding, and one-shot completion IRQ.
Initial partial fixture left only30 bits for the32-bit peek; corrected the
fixture to BP94+FB2, not runtime semantics. Final bootstrap CTest8/8 and Python55
passed; real Release runner build passed. Public-tree gate passed.

Native218 passed P-MBT and reached **VDEC0x38000000/table2 motion code**,
CTRL=`0x02623F08`, PCT=2; exit6, `timed_out=false`,300s/512M bound.
Final runner SHA256:
`741260ec066223687527254cf57c076e3481c15ca85bfc4054a1c7ac6559c885`.
Evidence: `work/native-run-218/{runtime.log,result.json,pad-control-events.jsonl}`,
`work/ipu-vdec-run218-launcher.log`. Both runs used opt-in
`DRAKENGARD_FRAME_INTERVAL=600`. Native218 final direct GS640x508 is all black,
also visually reviewed; no nonblack movie/menu/gameplay/audio/saves/recovery proof.
The startup helper217 observed IPU and exited successfully. Helper218 sent all
five bounded Left/Cross attempts but hit its180s limit (exit2); runner subsequently
reached IPU. Do not confuse helper timeout with runner timeout or claim helper218
success. Next use `python scripts/native_start.py --run 219 --timeout 300`
alongside the usual bounded launcher; no OS input.

Next experiment: establish table2 signed motion-code DATA, length and sign-bit
consumption (including negative values, zero, malformed prefixes, FB/starvation),
implement the actual contract, rebuild runner and bounded-run219. Pinned reference:
`work/ipu-reference-001/IPU_MultiISA.cpp:1635-1658`, `mpeg2_vlc.h` motion codes.
The observed motion command has FB0. Do not assume non-intra BDEC is next until
the runner establishes it. Verify219 is unused and no native/reference is active.

## Historical checkpoint: intra BDEC, real scratchpad transport, automatic startup

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

## Historical checkpoint: IPU transport/VLC and control without Computer Use

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
