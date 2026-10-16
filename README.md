# Drakengard 1 Europe — recompilation research

Early preparation for a native PC port of **Drakengard (PlayStation 2, Europe)**.
**The European ELF has been lifted locally. There is no verified playable build.**

Observed disc boot serial: **SLES-52322**, SYSTEM.CNF version **1.00**, video mode **PAL**.
The exact supplied ISO/ELF hashes are recorded in [the intake summary](docs/INPUT_EUROPE.md).
These identify the local input; external database authenticity/revision matching is not yet verified.

This repository currently provides:

- A read-only, opt-in image fingerprint and extracted ELF inspection tool.
- Pinned PS2Recomp/Ghidra workflow, guarded local lift and diagnostic native runner glue.
- An observed kernel-patch translation experiment with exact dispatch and instruction checks.
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
- [Verified input metadata and initial ELF findings](docs/INPUT_EUROPE.md)
- [Pinned toolchain, lift results and build commands](docs/TOOLCHAIN.md)
- [Kernel patch evidence, local preparation and validation limits](docs/KERNEL_PATCH.md)
- [Roadmap and acceptance gates (Russian)](docs/ROADMAP.md)
- [Runtime architecture and decision criteria](docs/ARCHITECTURE.md)
- [Sources and candidate tools](docs/RESEARCH.md)
- [Source publication policy](docs/PUBLICATION.md)

Windows x64 is the initial intended host. Linux portability is a later goal; current
cross-platform CI covers preparation tooling only. PS2Recomp is pinned for local research;
no upstream runtime or generated game code is vendored. Native coverage remains under validation.

Only original project code and documentation are licensed under MIT. No game data,
BIOS, original executables or generated game code are distributed. Third-party code,
if integrated later, retains its own license and must be reviewed before distribution.
This is an independent, unofficial research project.
