# Project rules

- Scope: Drakengard 1, PS2, Europe. CURRENT_STATE.md is a local, ignored progress journal.
- For "continue", "resume", or "продолжи", follow docs/CONTINUE.md. Read the latest
  CURRENT_STATE.md entries when available and the top current handoff section before acting.
  Read docs/ROADMAP.md if present; otherwise use the maintained handoff. Preserve unrelated work.
- Never search for, inspect, or analyze incoming game files before the user confirms they are complete.
- Keep original input read-only. Inputs, BIOS, extracted files, analysis exports, generated guest code,
  saves, traces and screenshots belong in ignored game/, private/, work/, tools/, or generated/.
  Exception authorized by the user: the two reviewed README screenshots in docs/images/,
  pinned by exact SHA-256 in scripts/check_public_tree.py. Other captures remain private.
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
