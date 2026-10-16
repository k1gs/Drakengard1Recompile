# Drakengard 1 Europe — recompilation research

Early preparation for a native PC port of **Drakengard (PlayStation 2, Europe)**.
**No game image has been supplied. There is no recompiled game or playable build.**

Expected European disc serial: **SLES-52322**. The actual revision, ELF and hashes
remain unverified until read-only intake of the user's complete copy.

This repository currently provides:

- A read-only, opt-in image fingerprint and extracted ELF inspection tool.
- Synthetic ELF tests and Windows/Linux CI requiring no game or BIOS.
- A staged research roadmap, evidence requirements and publication checks.

## Start

Python 3.10+ and Git are sufficient for the preparation tools; no pip packages are needed.

```powershell
python scripts/doctor.py
python -m unittest discover -s tests -v
python scripts/check_public_tree.py
```

When the game is complete, follow [the intake procedure](docs/INTAKE.md).
The tool does not extract an ISO, execute guest code, or validate authenticity.

## Project notes

- [Current state and next action](CURRENT_STATE.md)
- [Roadmap and acceptance gates (Russian)](docs/ROADMAP.md)
- [Runtime architecture and decision criteria](docs/ARCHITECTURE.md)
- [Sources and candidate tools](docs/RESEARCH.md)
- [Source publication policy](docs/PUBLICATION.md)

Windows x64 is the initial intended host. Linux portability is a later goal; current
cross-platform CI covers preparation tooling only. PS2Recomp is a candidate, not an
integrated or validated dependency. No upstream runtime is vendored or pinned yet.

Only original project code and documentation are licensed under MIT. No game data,
BIOS, original executables or generated game code are distributed. Third-party code,
if integrated later, retains its own license and must be reviewed before distribution.
This is an independent, unofficial research project.
