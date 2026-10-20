<div align="center">

# Drakengard 1 · Europe

### Static recompilation research for PlayStation 2

A source-only project working toward a native PC port of **Drakengard**.

[![Checks](https://github.com/k1gs/Drakengard1Recompile/actions/workflows/ci.yml/badge.svg)](https://github.com/k1gs/Drakengard1Recompile/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/project_code-MIT-blue.svg)](LICENSE)
![Platform: PS2](https://img.shields.io/badge/platform-PlayStation_2-003791)
![Region: Europe PAL](https://img.shields.io/badge/region-Europe_%C2%B7_PAL-6750A4)
![Stage: Research](https://img.shields.io/badge/stage-experimental_research-D97706)

[Progress](#progress) · [Getting started](#getting-started) · [Documentation](#documentation) · [Roadmap](docs/ROADMAP.md)

</div>

> **Research in progress. No playable build is available.**
> The European executable has been lifted locally. Original guest code reaches
> IOP reboot and module loading; a native rendered menu and gameplay are still unverified.

## The project

The goal is to bring **Drakengard (PS2, Europe)** to PC through static recompilation,
with explicit runtime support for the hardware and services its original code uses.
Windows x64 is the initial host target. Linux portability is a later goal.

The observed input is **SLES-52322 · version 1.00 · PAL**. Exact ISO and ELF fingerprints
are recorded in [input metadata](docs/INPUT_EUROPE.md); the serial alone does not establish
revision or dump authenticity. Game-specific bindings require the verified ELF hash.

## Progress

| Area | Verified milestone | Remaining work |
| --- | --- | --- |
| Input & toolchain | Read-only intake, pinned tools and local European ELF lift | Broader translation and runtime coverage |
| EE ↔ IOP | Guest-owned SIF handshake, initial BOOTEND and actual packet replies | Complete the game-requested IOP reboot |
| Guest modules | Original IOPBOOT and verified UDNL execute; 14 imports checked against real guest linking | Complete the verified reboot into game IOP services |
| Disc services | Original guest code drives 21 CDVD DMA3 transfers from the verified ISO | Review the next uncached code transition |
| Graphics & gameplay | Acceptance criteria documented | First native frame, rendered menu, then real gameplay |
| Audio, saves & recovery | Validation matrix documented | Verify these in actual gameplay |

Passing CI validates preparation tools and synthetic service contracts.
It does **not** establish a working port. Detailed evidence and the current boundary
live in [IOP bootstrap notes](docs/IOP_BOOTSTRAP.md) and the [roadmap](docs/ROADMAP.md).

## Getting started

The public checks need no game or BIOS. Install **Python 3.10+** and **Git**:

```powershell
python scripts/doctor.py
python -m unittest discover -s tests -v
python scripts/check_public_tree.py
```

Standalone C++ service checks additionally require **CMake 3.21+** and a **C++20 compiler**.
[CI](.github/workflows/ci.yml) runs synthetic checks on Windows and Linux.

To work with your own completed local input, start with the [intake procedure](docs/INTAKE.md),
then follow the [pinned toolchain and local build guide](docs/TOOLCHAIN.md).
The fingerprint tool inspects inputs; it does not extract an ISO or establish authenticity.
The native runner is a diagnostic experiment and requires additional local preparation.

## Documentation

| Guide | What it covers |
| --- | --- |
| [Roadmap / План работ](docs/ROADMAP.md) | Milestones, next experiment and acceptance gates · Russian |
| [Architecture](docs/ARCHITECTURE.md) | Runtime structure and design decisions |
| [Input metadata](docs/INPUT_EUROPE.md) | Verified fingerprints and initial ELF findings |
| [Toolchain](docs/TOOLCHAIN.md) | Pinned revisions, lift results and build commands |
| [IOP bootstrap](docs/IOP_BOOTSTRAP.md) | Guest execution evidence, strict device models and blockers |
| [SIF protocol](docs/SIF_PROTOCOL.md) | Packet, RPC and DMA coverage |
| [Media](docs/MEDIA.md) | Read-only ISO/CDVD services and checks |
| [Kernel patch](docs/KERNEL_PATCH.md) | Translation evidence and validation limits |
| [Research](docs/RESEARCH.md) | Primary sources and candidate tools |
| [Publication policy](docs/PUBLICATION.md) | Source-only boundary and release checks |

## Contributing

Focused work on device contracts, CPU behavior, synthetic tests and documentation is welcome.
For runtime reports, include the tool revision, input fingerprint, first blocker and a reproducible
procedure. Keep game data, BIOS, generated guest code, saves and raw traces out of issues and patches.

Unsupported behavior stops explicitly. Changes should demonstrate the real operation and its
failure cases before extending coverage. See [project rules](AGENTS.md) and the
[publication policy](docs/PUBLICATION.md).

## Source-only & licensing

This repository distributes original project source, synthetic tests, configuration templates
and reviewed documentation. It includes **no game data, BIOS, original executables or generated
game code**. Supply your own local inputs; keep them in ignored directories.

Original project code and documentation are licensed under [MIT](LICENSE). No upstream runtime
or generated game code is vendored. Any future third-party integration needs its own license review.

*Independent, unofficial research project. Not affiliated with the game's rights holders.*
