# Native source layout

The runtime adapter is in `native/runner/`; the pinned upstream checkout is kept
unchanged. `native/runner/overlay.cmake` wires the adapter and generated guest code into
the upstream targets.

| Directory | Purpose |
| --- | --- |
| `native/runner/fpu/` | Reviewed scalar EE floating-point result helpers |
| `native/runner/capture/` | Raw GS frame capture, PPM serialization and capture limits |
| `native/runner/cmake/` | Build-local overlays and fail-closed patches of pinned sources |
| `native/runner/` | EE devices, host input, RPC bridges and runner lifecycle |
| `native/iop_probe/` | Strict IOP execution, devices and synthetic device fixtures |
| `native/bootstrap_smoke/` | Small deterministic behavioral checks |
| `native/*_smoke/` | Separate focused executable harnesses |

Older harnesses and diagnostic headers still have consumers. Keep them until
their references and replacement coverage have been reviewed; age alone does
not make a file obsolete. Game inputs, generated lifts, dumps and research
artifacts stay in their existing ignored directories.

The macro overlay uses `native/runner/cmake/pinned_patch.cmake` to reject changed upstream
anchors. `native/runner/cmake/write_watch.cmake` defines the FPU substitutions and generates the five
write-width hooks, then replaces the runner's PCH macro entry as before.

For runtime status and the current blocker, start at
[`CONTINUE.md`](CONTINUE.md). Layout cleanup does not establish new
runtime or gameplay support.
