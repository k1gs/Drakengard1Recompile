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
