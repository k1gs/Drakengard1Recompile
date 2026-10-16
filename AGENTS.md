# Project rules

- Scope: Drakengard 1, PS2, Europe. No game input is available at bootstrap.
- Read CURRENT_STATE.md and docs/ROADMAP.md before continuing. Preserve unrelated work.
- Never search for, inspect, or analyze incoming game files before the user confirms they are complete.
- Keep original input read-only. Inputs, BIOS, extracted files, analysis exports, generated guest code,
  saves, traces and screenshots belong in ignored private/, work/, tools/, or generated/.
- Public Git contains original source, synthetic tests, configuration templates and reviewed documentation only.
  Run scripts/check_public_tree.py before each publication; review staged content as well.
- Do not invent addresses, ELF hashes, symbols, timing, supported APIs or runtime success.
- Fail closed at unsupported behavior. Never make a stub return success just to advance boot.
- Gate game-specific bindings on a verified ELF SHA-256; the disc serial alone is insufficient.
- Record evidence, tool revisions, first blocker and next experiment in CURRENT_STATE.md.
- Loader output and synthetic tests do not establish a working port: require an actual rendered menu,
  then real gameplay, audio, saves and recovery checks.
- The user has authorized creating and maintaining this public repository: focused validated commits
  and normal pushes are in scope. Do not force-push or include unrelated changes.
