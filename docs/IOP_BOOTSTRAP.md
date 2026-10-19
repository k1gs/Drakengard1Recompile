# IOP reboot research

The EE runner stops at an actual SIF reboot request, mode 0, with the argument
`rom0:UDNL cdrom0:\MODULES\IOPRP280.IMG;1`. Resetting host state and declaring BOOTEND would
skip module/kernel initialization. The new probes therefore run separately, with no EE acknowledgement.

## Reproducible local tools

`scripts/romdir.py` inspects a completed ROMFS image at an explicit ROMDIR offset. It validates
section order, RESET extent, bounded directory termination, 16-byte file placement, EXTINFO ranges,
identifiers, actual file hashes and basic MIPS ELF headers. Repeated `-` padding extents are
inventoried but not extracted as files. Inputs are read-only; output must be a fresh directory
inside ignored `work/`. It does not locate BIOS files or scan the computer for inputs.

```powershell
python scripts/romdir.py --input-complete --image work/extracted-001/MODULES/IOPRP280.IMG --output work/ioprp-001 --extract
cmake -S native/iop_probe -B build/iop-probe
cmake --build build/iop-probe --config Release --parallel 4
ctest --test-dir build/iop-probe -C Release --output-on-failure
```

Use new output IDs when repeating extraction. The optional C++ targets require a clean local
PS2Recomp checkout at `eb8688b2fd191129f103a39543002c005b33409c`; they are not the game-free CI targets.
They link GPLv3 upstream components locally. No upstream source or resulting binaries are published.

- `iop_image_probe --input-complete MODULE [MODULE ...]` loads images into isolated IOP RAM,
  validates relocation completion/entry placement and inventories decoded import dependencies.
  It never executes an instruction or establishes a boot order.
- `iop_sysmem_probe --input-complete SYSMEM VERIFIED_START` makes one bare SYSMEM entry call
  using the public UDNL ABI: placement=start+0x30, A0=RAM size, GP from the loaded image.
- `scripts/prepare_iop_boot.py --input-complete --base-rom LOCAL_ROM --base-romdir-offset OFFSET
  --update LOCAL_IOPRP --output work/FRESH_ID` reads actual IOPBTCONF and selects modules by
  EXTINFO type-2 version. Equal versions prefer the later image. Unknown directives/arguments,
  missing modules, malformed metadata and oversized inputs are refused.
- `iop_bootstrap_probe --input-complete work/FRESH_ID/boot-plan.bin` executes SYSMEM followed
  by LOADCORE in isolated 2-MiB RAM with the actual selected module images and ResetData layout.
  It does not notify EE, register HLE services or declare reboot completion.

The bootstrap loader uses a private build-tree copy of the pinned loader with 16-byte placement
instead of its normal 256-byte arena alignment. This represents the documented start+0x30 ABI;
all relocation operations are retained. It does not alter the upstream checkout or EE runner.
The caller supplies an isolated host stack and protected RAM source pool; physical UDNL addresses
and hardware timing are not claimed. This is a controlled bootstrap experiment, not a console reset.

## Observed evidence

IOPRP280 contains 19 records, including 16 MIPS ELF images. All 16 passed the separate image/relocation
probe. Base ROM plus update selected 29 modules from the actual private boot configuration.
Raw BIOS configuration text, firmware, modules, boot bundles and diagnostic exports stay ignored.

The synthetic return-42 ELF returned 42 in three instructions. The actual selected SYSMEM entry
at 0x890, loaded at 0x830, returned 0x1800 after 703 instructions. LOADCORE was then placed at
0x1830 and called with the prepared ResetData. After 14,485 instructions it attempted a read
at 0xBF801450, IOP PC 0xA000344C. The strict RAM-only guard stopped before the read.

The reference names this register HW_ICFG. A subsequent local PINE experiment captured six
PCSX2 2.8.2 states at 2.235/3/5/7/10/13.016 wall seconds. Each snapshot stored HW_ICFG=1,
with serial SLES-52322 and CRC 79585776 confirmed through PINE; patches were disabled.
The emulator's cold-reset zero was therefore not used as an initialized state.

`scripts/reference_state.py --input-complete --state LOCAL.p2s --expected-sha256 REVIEWED_HASH
--output work/FRESH_ID` verifies the reviewed snapshot hash, exact version/layout, component
uniqueness and bounded sizes before exporting minimal register state. The optional bootstrap
argument `--hardware-state work/FRESH_ID/hardware-state.bin` permits only reviewed word access
to HW_ICFG and DMA-global gate 0x1F801578. Gate values other than 0/1 remain unsupported.
The gate's value was 1 in every snapshot; primary dmacman source implements get/set and writes
1 on startup and 0 on shutdown. No channel DMA, IRQ, timer state or completion is restored.

Bootstrap-002 passed HW_ICFG, then stopped at the DMA-global gate, PC 0xA0003470. Bootstrap-003
with both registers stopped at 0xFFFE0130, PC 0xA0003488, after 14,500 LOADCORE instructions.

## Functional cache and kernel execution

The optional `--coherent-cache` mode executes the guest flush routine with coherent instruction
fetch, a small BIU shadow and isolated cache-line zero stores that leave backing RAM intact.
Supported control values are 0, 0xC04 and 0xC4; other isolated accesses remain unsupported.
Initial shadow zero follows the pinned reference's BIU read fallback, not a measured physical
BIU/reset state. The model has no cache timing. A private build-tree CPU unit changes only the
isolated word-store side effect; the original checkout and EE runtime remain unchanged.

The first apparent instruction-budget stall was a finite word fill, not a fixed wait. The observed
loop increments the write pointer by four until its end address. A 2-MiB five-instruction-per-word
fill already needs ~2.62M instructions. After a synthetic 1.875-MiB fill/boundary regression, the
fixed cap became 8M; an intentional infinite loop still stops. Budget diagnostics record PC,
registers and a bounded code window only in local logs.

The quiescent interrupt model implements I_STAT acknowledgement, I_MASK storage and I_CTRL
read-and-disable. DMA interrupt registers implement write-one-to-clear flags and master IRQ;
priority/idle-channel storage does not imply a transfer. A channel start is explicitly refused
until an actual engine is implemented. No device events, completed DMA or successful IRQ delivery
are fabricated. Enabled pending IRQs enter the actual installed guest exception vector;
missing handlers remain fatal.

SYSCALL traps execute the nonzero exception vector that the real guest kernel installed at 0x80.
Only actual owned vector instructions are fetched; CPU EPC/Cause/Status and guest RFE perform the
return. No host syscall result is substituted. Unknown exceptions remain fatal. Synthetic checks
cover a real guest handler returning via EPC+4/RFE, missing vectors, cache/RAM preservation,
I_CTRL side effects, pending IRQ rejection, DMA W1C flags and unsupported channel starts.

Bootstrap-012 passed six guest kernel syscalls and stopped at a byte read of 0xBF402005,
IOP PC 0xAF18, after 2,402,274 instructions in the LOADCORE call. It performed 10,240 isolated
cache stores without corrupting backing RAM. The next experiment is the actual CDVD hardware
interface backed by verified media, including real DMA bytes/IRQ completion. This does not yet
establish that all selected modules initialized, that EE reboot completes, or that a menu renders.

## CPU identity correction and current blocker

The initial bare CPU left CP0.PRId zero. That incorrectly selected PS1/P variants in the real
kernel's conditional startup, including timrman export 1.01. After CDVD register access was added,
bootstrap-014 reached an unresolved timrman:18 import requiring 1.03. The export/import version
diagnostic exposed the identity error. Those earlier PS2 bootstrap conclusions are superseded;
the traces remain historical instruction-execution evidence.

The probe now starts with immutable IOP revision 0x1F, following pinned PCSX2 reset code.
A synthetic MTC0 write cannot alter it. With the correct kernel variants, bootstrap-017 stopped
at a halfword write of zero to timer-5 mode 0xBF8014A4, PC 0x860C, after 3,922,376 instructions
and 14,080 isolated cache stores. Timer counting/mode/target and actual IRQ events are next.
No constant timer result or host success stub has been added.

An optional `--disc LOCAL_ISO` verifies the entire Europe ISO SHA-256 before any bootstrap
instructions, then connects the separate register-level device described in [MEDIA.md](MEDIA.md).
`--mecha-profile LOCAL.mec` supplies exactly four bytes from a local reference model profile;
this is an explicit virtual device property, not a claim about a physical console revision.
Unknown commands remain fatal. These optional inputs and all derived snapshots stay private.

Strict synthetic C++ checks cover normal return/delay slot, exception, non-RAM access, unresolved
import, unowned instruction fetch and instruction-budget exhaustion. Python tests cover ROMFS
extents/versions, explicit table offsets, extraction isolation and boot-plan selection.
These checks do not prove full module initialization, IOP/EE synchronization, a game frame or menu.

Primary references pinned to PS2SDK `329e2bb43287e16af16c4dde1c614e2662c24468`:
[ROMFS records](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/fs/romdrv/include/romdrv.h),
[UDNL selection, ResetData and bootstrap sequence](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/udnl/src/udnl.c).
Hardware identifier: [PCSX2 2.8.2 IOP register definitions](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopHw.h).
Reference capture: [loopback PINE protocol](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/PINE.cpp),
[savestate component layout](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SaveState.cpp),
[register reads](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/ps2/Iop/IopHwRead.cpp),
[register writes](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/ps2/Iop/IopHwWrite.cpp),
and [DMA-global get/set](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/dmacman/src/dmacman.c).
Cache isolation and syscall contracts:
[LOADCORE flush routines](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/loadcore/src/loadcore.c),
[PCSX2 functional IOP memory path](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopMem.cpp).
CPU identity: [PCSX2 IOP reset](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/R3000A.cpp).
Variant selection: [PS2SDK timer manager startup](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/timrman/src/timrman.c).

## Deterministic timers and owned IRQ delivery

The isolated probe now models six internal counters, prescalers, count/mode/target,
target and overflow flags, and their causal IRQ bits. External pixel/HBlank clocks and
unsupported gating remain explicit blockers. Each decoded instruction advances one modeled
cycle. This is a functional scheduling policy, not a physical IOP/PAL timing measurement.

Bootstrap-018 produced timer-5 IRQ `0x10000`, with I_MASK `0x10801` and CP0.Status `0x401`.
Bootstrap-019 delivered it through the actual installed vector: EPC `0x33E0`, Cause `0x400`.
The guest handler acknowledges I_STAT and returns with RFE. CP0 masking, missing vectors,
branch-delay EPC/BD and deferral until pending loads commit have synthetic coverage.

After that handler, actual startup reached a BIOS data read at `0xBFC00000`, PC `0x1A094`,
after 4,096,612 instructions. ROM bytes are not substituted. The next experiment maps the
verified local BIOS read-only for data access; ROM instruction execution remains unsupported.
No EE BOOTEND, native frame or complete module initialization has been established.

Game-free timer tests are also in CI:

```powershell
cmake -S native/timer_smoke -B build/timer-smoke
cmake --build build/timer-smoke --config Release
ctest --test-dir build/timer-smoke -C Release --output-on-failure
```

Primary counter contracts: [PCSX2 2.8.2 implementation](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopCounters.cpp)
and [counter state](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopCounters.h).

## ROM data and the EE peer boundary

The probe accepts `--boot-rom LOCAL_BIOS --boot-rom-sha256 REVIEWED_HASH`. Both options are
required together. The full file hash and 4-MiB size are checked before bootstrap execution.
The data aperture exposes actual immutable bytes through physical/KSEG0/KSEG1 aliases;
ROM stores and instruction execution remain unsupported. KSEG3, including BIU control,
is excluded. Temporary core backing words are restored after each load to avoid alias pollution.

Bootstrap-020 passed ROM lookup and reached SIF F260 at `0xBD000060`. The quiescent IOP bus
now supports reviewed word address/flag/control registers. F220 is cleared by IOP writes;
F230 is set by them. Peer address writes cannot create an EE address. The pinned reference's
F240 functional control bits and unused F260 view are explicit device contracts, not peer
completion signals. Unknown registers/widths still fail.

Bootstrap-021 reached its fixed 8M bound polling F220 (PC `0x1AEE8`). This is an expected
missing-peer boundary: the isolated experiment has no executing EE. Active SIF flags are
never copied from a savestate. Native-run-021 still refuses the real reboot request; a read-only
MMIO diagnostic recorded no EE bus writes before that refusal. Its original upstream boot
profile is not a post-reset IOP model. Accepting the reboot requires persistent cooperative
IOP execution, a shared SIF bus and actual causal DMA/IRQ completion.

Game-free ROM bounds, alias, write-refusal and SIF direction/control tests are available at
`native/bootstrap_smoke` and run in CI. Optional CPU tests additionally exercise load delays,
restored backing words and refusal to execute ROM. None of this establishes a native menu.

Primary bus contracts: [PCSX2 2.8.2 IOP memory access](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopMem.cpp),
[EE writes](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/HwWrite.cpp).

## Persistent instruction slices

`BareCallState` retains CPU, pending loads, branch-delay state and a cumulative bounded
instruction count. A partial slice returns `completed=false`; only the actual return boundary
produces completion. Resume rejects a changed memory or call contract. Devices remain owned
by the caller and persist alongside the CPU. Each decoded instruction still advances one
modeled timer cycle, regardless of the slice boundary.

Bootstrap-022 runs 4,096-instruction slices and reproduces the unsliced trace/blocker exactly
apart from its scheduling-policy line. One-instruction synthetic tests retain an uncommitted
load and a pending branch; a separate flag-polling guest waits until an explicit synthetic EE
write between slices. The bus now exposes directional EE flag/address writes as well as the
IOP view. Unsupported EE IRQ/reset control strobes fail before modifying state.
The EE control read has its own forced bit 8, separate from stored bit 8 observed by IOP,
following [PCSX2 2.8.2 EE reads](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/HwRead.cpp).

These are reusable integration primitives. The main EE runner still refuses reboot because
its persistent IOP peer and DMA engine have not yet been connected. Never replay synthetic
peer writes into the real boot or let a constant upstream ready value complete that reset.

## Optional cooperative native experiment

The main runner can now attach a separate strict IOP core:

```powershell
python scripts/run_native.py --input-complete --iop-bootstrap --seconds 60 --run 27
```

This requires the existing private boot plan, reviewed passive seed, local reference BIOS/profile
and verified Europe ISO. Their paths are local launcher configuration; each fingerprint is recorded
in the ignored run manifest and independently verified by native code before guest execution.
Wrong bundle/ROM hashes were rejected before opening a window. Default launch still refuses reboot
without these inputs. All binaries, firmware, game files, generated code and logs remain private.

The actual reset request activates persistent SYSMEM/LOADCORE execution. After reset both MMIO and
kernel register APIs use the same directional bus; ids 1..4 are MAINADDR/SUBADDR/MSFLAG/SMFLAG.
Software sysregs retain explicit guest writes. Old constant ready replies cannot satisfy reset.
Only the reviewed mode/request is accepted. Repeated reboot, unknown registers and post-reset HLE
command transfers fail explicitly. Progress occurs in bounded slices at SIF API boundaries; no
physical cycle ratio, PAL timing or complete scheduler implementation is claimed.

Native-run-026 proves that EE and IOP both advance, but exposes the missing initial EE SIF state.
EE clears IOP status bits and polls BOOTEND. IOP waits for EE SIFINIT in F220, which has no observed
writer in the pre-entry profile. The unchanged total instruction cap stops this mutual wait.
This does not establish DMA, full module initialization, rendered menu or audio content.

The next experiment is the actual firmware EE initialization routine. A private hash-gated kernel
image matches the reviewed reference RAM at physical zero (unique first 4 KiB plus the relevant code
window). Ghidra candidate `0x80006240` writes F220=0x10000 and waits for IOP SIFINIT after setting up
DMA registers. It and its helpers have only been analyzed; they are not currently native bindings.
They need explicit translation/code verification and real cooperative device behavior, rather
than copying active reference flags into the bus.

Primary kernel-register ABI: [PS2SDK sifdma.h](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/ee/kernel/include/sifdma.h).
Reset sequencing: [PS2SDK iopcontrol.c](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/ee/kernel/src/iopcontrol.c).

## Executed EE firmware component (2026-10-18)

An additional isolated mode executes the reviewed EE kernel SIF initialization before game entry:

```powershell
python scripts/run_native.py --input-complete --iop-bootstrap --ee-firmware-init --seconds 60 --run 31
```

It requires the private verified firmware lift. `prepare_firmware.py --input-complete` prepares
build units only after checking the exact BIOS, KERNEL extent, translation image and selected code
coverage. CMake includes those units only when the local `generated/firmware-003` registration
exists. The runtime rereads KERNEL from the reviewed BIOS and checks translated instruction bytes
before every firmware entry. The temporary harness stack/return boundary are explicit diagnostics,
not a claim that the entire EE BIOS reset state has been reproduced. This mode stops before game entry.

The five translated procedures now cover 656 actual instruction bytes. An initial 80-byte function
body ended inside a long NOP run; actual PC `0x80005F50` exposed the truncation. Bounded leaf-flow
recovery finds its real return and preserves disjoint coverage. All five procedures recompiled
without stubs, skips, decode failures or warnings. Generated firmware remains private.

Native-run-030 executes the actual firmware write F220=0x10000. IOP then advances beyond its prior
mutual wait to a halfword BCR write at `0xBF801534`, PC `0x1B140`, 4,207,994 instructions.
An armed EE SIF0 chain receiver retains STR while no data arrives; no empty DMA completion/IRQ is
invented. The next implementation is the actual IOP BCR access and SIF1 receive path.

Strict device errors now use a distinct fatal type. Upstream memory wrappers normally turn standard
exceptions into guest address errors/zero reads; the private build rethrows strict device failures.
The original failing reproduction timed out on repeated errors; the corrected run stops at its first
unsupported instruction, exit 6 without timeout. This is still component initialization, with no
confirmed game image, complete boot or game audio.

## Completed SIFINIT component and post-initialization startup

Native-run-032 completed the actual two-sided SIF initialization: original EE code writes
MSFLAG/SIFINIT, the IOP arms its SIF1 chain receiver and writes its own SIFINIT, then the original
EE routine returns. The exact reviewed empty receiver configuration retains STR and waits for
actual tags. No payload bytes, DMA completion or IRQ are fabricated. BCR halfword reads/writes
preserve the opposite half. Unsupported modes and active rearm remain explicit failures.

The experiment now continues real IOP startup after that return, within the same cumulative
instruction budget, until guest-owned BOOTEND or the next unsupported operation. The ordinary
entry/reboot path remains separate; a full cold boot followed by game entry is not established.

The CDVD empty SDATAOUT read follows the pinned reference: zero data with the FIFO still empty,
no consumption and no completion. Firmware mode also explicitly selects a read-only host-backed
RTC, using the documented CDVD GMT+9 calendar and validated BCD fields. It does not modify the OS
clock or emulate persistent console NVM. Other profiles without a configured clock still refuse
that command. See [MEDIA.md](MEDIA.md) for the device contracts and limits.

Native-run-035 passes that RTC read and stops at CDVD SCMD `0x1B`, no parameters, PC `0x27A08`.
At that checkpoint neither BOOTEND nor a native game frame was verified. A changed private BIOS copy, even supplied with
its own matching intake hash, was rejected by the specific firmware binding before guest execution.

Primary receive behavior: [PS2SDK SIF manager](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/sifman/src/sifman.c),
[PCSX2 SIF1 FIFO/transfer logic](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/Sif1.cpp).

## Guest BOOTEND and experimental ELF entry (2026-10-19)

Native-run-041 completed the component experiment: after quiescent CDVD `0x1B` cancellation,
actual guest instructions set BOOTEND and the observed SMFLAG becomes `0x50000`. No ready flag
or boot-end bit is installed by the host. This remains a selected module boot plan and a reviewed
EE firmware call with harness context, rather than execution of the entire BIOS reset path.

The launcher keeps isolated initialization and experimental game entry separate:

```powershell
python scripts/run_native.py --input-complete --iop-bootstrap --ee-firmware-init --seconds 60 --run N
python scripts/run_native.py --input-complete --iop-bootstrap --ee-firmware-entry --seconds 60 --run N
```

Use a fresh numeric run ID. Both modes require the same locally prepared, verified inputs.
Entry checks actual BOOTEND before preserving the strict IOP session and running the loaded ELF.
The diagnostic runtime still supplies EE kernel/scheduler state; this is not full BIOS boot acceptance.

Native-run-042 reached the game's first SIF initialization packet: command `0x80000002`, 20 bytes,
EE source `0x6B0980`, actual IOP receive destination `0x1E340`, EE reply-buffer field `0x6B08C0`.
The transfer is rejected before copying into upstream HLE memory. Strict active IOP also refuses
GetOtherData transfers from that unrelated memory. Next work is actual SIF1 tags/payload delivery,
controller state/IRQ and execution of the real guest receive handler, followed by SIF0 replies.

The upstream game-thread catch used to print a fatal device error and return to the GUI loop.
The local adapter now preserves that exception across thread completion, joins and cleans up,
then reports it through the runner's fatal exit path. Native-run-043 verifies exit 6 without timeout,
exactly one BeforeCopy event for that packet, no AfterCopy event, and GUI cleanup before the fatal
report. A rendered menu, frame and gameplay remain unverified.

## Original EE DMA routines and first strict SIF1 payload

The exact BIOS syscall table at `0x80014E80` identifies SifDmaStat (`0x76`, entry `0x80006958`),
SifSetDma (`0x77`, entry `0x800067A0`) and SetDChain (`0x78`, entry `0x80006348`). Preparation
checks these source slots, the unchanged firmware image hash and ten exact function ranges.
The private `firmware-004` lift covers 2,388 instruction bytes, with no stubs or decode failures.
Instruction guards remain active for every firmware dispatch.

Only FirmwareEntry routes the DMA syscalls through an original-code call harness. It preserves the
EE caller context, supplies a documented harness stack/return boundary, and takes the DMA ID/status
from the guest return register. This is not a complete EE exception-entry implementation.

The bounded EE SIF1 source engine accepts reviewed REFE, REF and NEXT tags in actual aligned RAM.
It passes real quadwords to the IOP destination-chain receiver. Partial data retains STR; unknown
tags, unavailable receivers, bad ranges, suspend and cyclic chains fail explicitly. No alignment
padding is invented: the original firmware itself copies the source's complete quadwords.
Consumed terminal tags clear STR and cause real controller completion. The existing EE DMAC event
queue receives channel 6 completion; IOP channel 10 latches ICR2 flag 3 and requests its actual IRQ.
SIF tag IRQs cannot be suppressed merely by clearing the per-channel mask. The guest rearms the
receiver with its actual previous MADR, following the reviewed SIF manager contract.

An initial experiment found the component harness's missing EE DMA master configuration. The
firmware profile now takes only D_CTRL `1` and ENABLER/ENABLEW `0x1201` from the exact previously
reviewed PCSX2 v2.8.2 snapshot. Its full hash, version, component sizes and allowed field values are
checked during preparation. No busy channels, tags, IRQ flags, timing or replies are imported.
DMAE is still checked before an actual source transfer. This passive profile is an explicit
component-harness precondition; full BIOS hardware initialization remains unverified.

Native-run-049 observes DMAE enabled, original DMA ID `1`, a terminal IOP tag for `0x1E340`, and
32 actual payload bytes. The IOP raises its own IRQ, executes the guest handler, rearms SIF1 and
attempts SIF0 transmission at PC `0x1B640`, CHCR `0x01000701`, after 5,388,836 instructions.
That run stopped before the reverse direction, which the later experiment below implements.

Primary hardware behavior: [source chains](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/Hw.cpp),
[SIF1 FIFO/receiver](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/Sif1.cpp),
[IOP DMA interrupt semantics](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopHw.cpp),
and [pinned PS2SDK receiver rearm](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/sifman/src/sifman.c).

## First actual SIF0 reply

The strict source engine now accepts the observed IOP SIF0 mode, validates global/channel
enables and walks at most 128 actual RAM tags. Each tag forwards the real quadword starting
at TADR+8, then advances TADR by 16. Word-aligned tags need not be quadword aligned.
Payload MADR advances only for real source words. A partial final quadword repeats the matching
tail positions of the preceding complete FIFO quadword, including tag history for short packets.
This is the reviewed [SIF0 transfer contract](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/Sif0.cpp)
and [FIFO padding behavior](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/Sif.h).

The EE receiver supports destination CNT/END tags in owned RAM. It checks live DMA control and
suspend state, retains STR until all declared quadwords arrive, and publishes channel 5 completion
through the runtime's existing DMAC consumer. IOP completion clears its own STR and latches the
channel 2 ICR2 flag and IRQ. Unsupported tags, ranges, modes and unavailable receivers fail closed.
This bounded synchronous model does not claim cycle accuracy or a general queued SIF FIFO.

Native-run-054 delivered the first 24 source bytes but exposed an unused completion-drain hook;
the run timed out. Run-055 moved publication to the actual DMAC completion consumer. It observes
SIF0 source completion, EE channel 5 publication, guest receiver rearm, guest-owned software ready
register `0x80000002 = 1`, and a third outgoing DMA ID `0x201` with 64 actual SIF1 bytes.
No host reply backend or direct ready write supplies this result.

The later request sleeps at EE PC `0x31FAA8`; run-055 reaches its 45-second timeout. Existing
cooperative IOP slices run at SIF interactions, so an EE wait can starve the IOP. The next experiment
must provide bounded scheduling during that wait, then distinguish a missing service from a
scheduling gap. Timeout is not a successful boot. There is still no verified native frame/menu,
gameplay, audio-content or save acceptance.

Synthetic coverage includes 0..8 and 129 source words, both padding histories, multiple tags,
CNT/END, partial payloads, invalid and truncated RAM, suspend/DMAE, receiver refusal, tag budget,
pending completion preservation, and an actual IOP instruction storing CHCR before delivery.
