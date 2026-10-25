# Bounded GS production measurements

Use the normal verified native launcher with `--gs-metrics`. It writes only
`work/native-run-NNN/gs-metrics.jsonl`, inside the fresh ignored run directory.
The option adds observation overhead and is off by default. The launcher removes
inherited diagnostic output paths when the option is absent. Keep the existing
300-second run bound, input gates and one-controller rule.

The original adapters observe completed DrawPrimitive calls, actual
LoadClutUnlocked calls, UploadFrame scheduler-tick polls and successful latched
frame copies. They never generate a frame, IRQ or guest timing event. Previous
filtering and fail-closed device behavior are unchanged.

| Field | Meaning |
|---|---|
| elapsed_ns | Steady wall time since the metric store was initialized |
| ui_calls | UploadFrame calls, including reuse of the prior image |
| observed_vblank_ticks | Positive scheduler VSyncTick deltas between UI observations; the first observation establishes a baseline |
| tick_regressions | A decreasing tick establishes a new baseline instead of unsigned underflow |
| presentations | Valid copied latched RGBA images; includes identical redraws |
| first_images | First valid observed image, counted separately from changes |
| rgb_changes | Exact RGB or dimension changes between successive valid observed images; alpha-only changes are ignored |
| invalid_images | Malformed/empty images rejected before accessing pixels |
| draws / draw_wall_ns | Completed DrawPrimitive calls and their measured wall time, excluding diagnostic aggregation after the call |
| clut_loads | Entries to the real LoadClutUnlocked routine, not distinct palette contents |
| states | Per-window draw counts/wall time grouped by bounded register-state keys |
| overflow_draws / overflow_wall_ns | Work outside the 64 retained state bins in this window |

These are different observables, not interchangeable FPS counters. VBlank is
scheduler progress; copied frames may contain partial draws, and host observations
may miss images. RGB changes exclude identical game redraws. A static menu is not
zero game FPS merely because its pixels repeat. No completed application-frame
boundary has been established by these hooks.

Logs contain at most64 reports at intervals of at least5 seconds. Cumulative
counts remain monotonic; state bins and overflow counters clear after each
snapshot. For every window the sum of state draws/wall time plus overflow must
equal the differences of cumulative draws/wall time. Do not silently discard
overflow when ranking hot states. File failures are explicit diagnostic errors.

State keys are eight unsigned64-bit words, decoded as follows:

| Word | Bits |
|---|---|
| 0 | primitive type0..7, texture PSM8..15, CLUT CPSM16..23, linear24, FST25, TME26, TFX27..28, TCC29 |
| 1 | texture width low32, height high32 |
| 2 | TBP0 low32, TBW high32 |
| 3 | CBP low32, CSM32..39, CSA40..47, CLD48..55 |
| 4 | TEXA TA0 low8, TA1 next8, AEM bit16 |
| 5 | raw CLAMP register |
| 6 | TEXCLUT CBW low8, COU next8, COV bits16..31 |
| 7 | framebuffer PSM low8, FBP bits8..39, blend bit40 |

A repeated descriptor does not prove unchanged palette/texture contents. Any
future cache still requires invalidation for all relevant writes, palette loads
and state changes. This instrumentation itself does not cache rendered data.

Measure equivalent states. The private253/254 analysis selects only complete
5-second intervals whose preceding UI counter exceeds the acknowledged menu
capture index by120 observations. It verifies count/time conservation for every
included interval and divides aggregate deltas by aggregate wall time. Compare
raw frames without resampling; keep all traces, register states and captures
private. Draw wall time can include preemption and is not thread CPU time.
The top continuation handoff records current results and their limits.
