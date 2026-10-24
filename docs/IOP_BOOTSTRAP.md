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
That first synchronous implementation did not yet include a queued SIF FIFO; the later model below
adds bounded storage and resumable source progress. Neither model claims cycle accuracy.

Native-run-054 delivered the first 24 source bytes but exposed an unused completion-drain hook;
the run timed out. Run-055 moved publication to the actual DMAC completion consumer. It observes
SIF0 source completion, EE channel 5 publication, guest receiver rearm, guest-owned software ready
register `0x80000002 = 1`, and a third outgoing DMA ID `0x201` with 64 actual SIF1 bytes.
No host reply backend or direct ready write supplies this result.

The later request sleeps at EE PC `0x31FAA8`; run-055 reaches its 45-second timeout. Existing
cooperative IOP slices in that version run at SIF interactions, so an EE wait can starve the IOP.
The later scheduling experiment below addresses this gap. Timeout is not a successful boot.
There is still no verified native frame/menu,
gameplay, audio-content or save acceptance.

Synthetic coverage includes 0..8 and 129 source words, both padding histories, multiple tags,
CNT/END, partial payloads, invalid and truncated RAM, suspend/DMAE, receiver refusal, tag budget,
pending completion preservation, and an actual IOP instruction storing CHCR before delivery.

## IOP progress while EE has no ready thread

The local scheduler adapter advances one 4,096-instruction IOP slice immediately before an
actual no-ready-thread EE wait. This executes on the EE executor, outside the event mutex;
the UI thread does not access either CPU's RAM. Only FirmwareEntry enables this path.
Completed DMA events use the existing masked DMAC handler dispatch. A resulting guest invocation
is selected before sleeping; otherwise the original EE event/deadline wait remains in place.
The total eight-million-instruction IOP bound remains unchanged. This is functional scheduling,
not a measured EE/IOP clock ratio, and it does not replace a general scheduler/checkpoint model.

Native-run-058/059 pass the former semaphore wait: ten completed SIF1 chains carry 1,632 actual
bytes, and nine SIF0 source chains complete, including two-record replies of 72 and 68 source
bytes. The next IOP start occurs at PC `0x1B640`, instruction count 5,533,413, while EE SIF0
CHCR is `0x90000084` (STR clear), QWC is zero, DMA control is `1`, and enable is `0x1201`.
The synchronous sink refuses this next packet instead of discarding it or inventing readiness.
Both runs report fatal exit 6 without timeout; this is a reproducible device boundary.

The next experiment was an actual bounded SIF0 FIFO that retains emitted data across EE completion/rearm
and pauses a full producer without inventing an early completion. Preserve tag history and
partial source progress. Do not bypass the boundary by shortening instruction slices, force-arming
the EE channel, dropping packets, or supplying a host RPC reply. No rendered menu/gameplay claim.

## Bounded SIF0 FIFO and original receiver rearm

SIF0 now stores up to 128 words as 32 complete FIFO quanta. Accepted source bytes remain queued
when EE STR is clear, master DMA is disabled, reception is suspended, or a prior EE completion
has not yet been consumed. The consumer removes a quantum only after accepting it. Unsupported
control/stall modes and invalid tags still fail explicitly.

The IOP source retains its tag budget, remaining count, MADR/TADR and padding history across a
full FIFO. A full queue leaves STR set without IRQ; freeing capacity resumes actual source reads.
IOP completion follows the last accepted source quantum, independently of later EE consumption.
An active source configuration change is refused. Pumping does not execute additional CPU code
or change instruction budgets; it runs on the existing EE executor at IOP slices and DMA service.

Synthetic checks cover a 257-word source across three FIFO fills, ring wrap, DMAE/suspend,
back-to-back replies across EE rearm, source IRQ before EE completion, full-queue retries,
source changes while paused, retained invalid consumer data and unchanged tail padding.

The new strict HLE guards exposed an older SetDChain adapter. Firmware syscall `0x78` now invokes
the already translated, hash/table/byte-checked procedure at `0x80006348`. Its void host ABI leaves
the caller return register untouched. No new firmware image or unchecked address is introduced.
Strict HLE start/stop is rejected before creating or changing the unrelated packet backend.

Preliminary run-062, with the old rearm adapter, completed 16 SIF1 chains before its stop-mode
refusal. Run-063 deliberately exposed that HLE start. Final run-065 uses the original SetDChain:
15 SIF1 chains transfer 2,048 bytes, 17 SIF0 source chains and EE completions occur, and buffered
quanta survive the previous receiver-not-ready boundary. It then reaches stop syscall `0x6B`
at EE PC `0x31FD38`, RA `0x327D18`, and fails before HLE stop. Exit 6 without timeout.
Different packet counts reflect the original rearm procedure and its cooperative slice; they
are not a timing-equivalence claim. Full-FIFO pause/resume is synthetically verified, not yet
observed as a full queue in this native run. Next work is verified original SIF0 stop execution.
Native menu/frame, gameplay, audio-content and saves remain unverified.

## Original SIF0 stop procedure

The verified BIOS syscall table has `0x6B` at slot `0x8001502C`, pointing to `0x80006380`.
The private Ghidra review recovers the exact range `0x80006380..0x800063AC`: 44 instruction
bytes that clear EE SIF0 CHCR and QWC and return. `firmware-005` adds this range to the existing
private translation (11 functions, 26 entries, 2,432 instruction bytes; no stubs/decode failures).
The source BIOS, extracted KERNEL, translation image, table slots and per-dispatch instruction
bytes remain independently checked. Generated firmware is not distributed.

Stop routes through the same bounded original-code component call as the other SIF syscalls,
preserving its void caller ABI. It does not call the unrelated HLE packet backend or discard
queued SIF data. Synthetic checks cover stop/rearm with a queued reply and pending completion,
and reject a changed or truncated table slot for every supported firmware SIF entry.

Native-run-069 executes the actual stop, preserving the caller's `v0 = 0x572C70`, and then
delivers a 112-byte DMA payload containing the reboot request through original SIF1 tags. The guest IOP rearms reception
and begins the reboot's hardware initialization. No transport reset or ready flag is substituted.

That sequence writes the SSBUS ROM delay, common delay, PIO delay and fixed PIO base from the
verified BIOS hardware table. The functional model retains only the reviewed word-sized profiles:
ROM delay `0x0016244F`, common delay `0x1225`, PIO delay `0xFF`/`0xD2077`, PIO base `0x1F802000`.
Other values, widths, registers and device ports remain unsupported. These actual writes create
no IRQ or host timing assumption; snapshot state is not imported. Register identity and storage
behavior follow the pinned [IOP definitions](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopHw.h)
and [word write/read paths](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/ps2/Iop/IopHwWrite.cpp).
Synthetic CPU stores/readbacks exercise aliases, unchanged neighbors and rejected profiles.

Run-073 gets through these settings and stops at IOP PC `0x1909C`, instruction count 6,025,262,
on a store of `0xFC` to `0xBF802070`. The caller reads the original BIOS initialization table;
the raw table and instruction review remain private. The PIO base profile does not grant access
to that port. MAME labels the mapping an [IOP debug write](https://github.com/mamedev/mame/blob/master/src/mame/sony/ps2sony.cpp),
but its empty handler is insufficient evidence for a physical-device contract here. Keep this
port fail-closed pending further review. Exit 6 without timeout; 16 SIF1 chains carry 2,160 bytes.
An actual completed IOP reboot, new BOOTEND, native frame/menu, gameplay and saves are unverified.
Run-074 repeats the same port, value, PC and instruction count. Final isolated init (run-075)
still reaches actual guest BOOTEND with exit 5; ordinary IOP mode (run-076) keeps its fixed-budget
exit 6. Neither non-entry mode uses the new stop call or idle IOP scheduling; no run times out.

## Restricted PIO write capture and fixed SSBUS configuration

The next review establishes a narrower functional contract for `0x1F802070`, rather than its
uncertain physical meaning. In the pinned [PCSX2 IOP memory paths](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopMem.cpp),
page-2 byte/word writes retain their actual backing-store value without a device callback or IRQ.
The [hardware research](https://psx-spx.consoledev.net/expansionportpio/) leaves POST/configuration
semantics uncertain. The diagnostic profile therefore records writes only; it implements no
physical debug board, status reads, reset acknowledgement or ready flag.

`PioWriteCapture` is enabled only for explicit entry into the ELF after firmware initialization.
The actual PIO base/delay latches must already match the reviewed configuration. It accepts the
observed `SW 0xFC` and byte writes to this exact port. Halfwords, loads, other word values, adjacent
ports and other mappings fail closed. Recording occurs after the real CPU store, preserving byte
width and checking the stored value; failed instructions cannot produce a capture event.
Run-077 records the actual `0xFC` store, then reaches the SPU bus-delay setting.

The reviewed SSBUS settings now have their own `ssbus_config.h` component. It retains fixed base
addresses and the known transition/final delay profiles for ROM, PIO, DEV1, DEV5, SPU, SPU1 and DEV9.
The [register definitions](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopHw.h) and
[storage write path](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/ps2/Iop/IopHwWrite.cpp)
support configuration storage, not a cycle-accurate bus or a working audio/disc/network device.
Only word loads/stores under the existing passive profile are permitted. No new snapshot state
is seeded. Device ports, arbitrary remapping, unsupported widths and CPU-internal controls stay
separate and unsupported. Synthetic CPU tests cover both delay stages, cached/uncached aliases,
load delays, rejected writes leaving prior values intact, unchanged neighbors and no IRQ.
PIO tests additionally check disabled capture, byte truncation and preservation of other bytes.

Run-078 gets through the reviewed bus settings and stops at the next explicit contract:
IOP PC `0x1909C`, instruction count 6,025,406, `SW 0x1F800400` to CPU register `0xFFFE0140`.
The original table also contains `0xFFFE0144`; neither is opened as an SSBUS latch or a no-op.
The run exits 6 without timeout after 16 SIF1 chains carrying 2,160 bytes. The next experiment
must establish the CPU mapping/control contract and its interaction with coherent memory.
Completed reboot, repeated BOOTEND, native frame/menu, gameplay, audio-content and saves remain
unverified.
Run-079 repeats the same port, value, PC and instruction count on the same executable. Run-080
still returns from isolated initialization after actual guest BOOTEND (exit 5); run-081 retains
the ordinary fixed-budget boundary (exit 6). Neither non-entry mode records a PIO write. None
times out. A changed private BIOS copy, even with its matching supplied hash, is still rejected
by the firmware-specific binding before guest execution; the original remains unchanged.

## Original IOPBOOT execution after the reboot request

The CPU-control review uses the pinned [PCSX2 memory implementation](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopMem.cpp):
reset leaves write-table page `0x1FFE` unmapped and its word-write path has no handler for it.
The diagnostic coherent-memory profile now accepts only the two observed writes, with their
exact original KSEG3 addresses and values. It restores the core's temporary masked backing slot,
records retired writes and creates no guest-visible latch, status or IRQ. Reads, aliases, other
widths and values stay unsupported. This models the reference's functional write behavior;
the physical meaning of these controls remains unestablished.

The existing BIU shadow also accepts `0x1EDD8` only after explicit game entry. This value is
verified in the original RESET configuration and is written by the original LOADCORE routine.
The shadow is for validating known cache sequences, not an implementation of physical cache
timing or a claim that all BIU reads match PCSX2. Unknown values and isolated accesses outside
the existing reviewed flush sequences still fail.

Native-run-082 reaches the BIU configuration. Run-084 then identifies the actual jump from
guest reboot code to `0xBFC4A000`, previously stopped by the owned-RAM execution guard.
Private ROM-directory/hash checks identify IOPBOOT at ROM offset `0x4A000`, 4,448 bytes,
SHA-256 `572e8acd6db6c7854cba1418b3ecfc02f23c3752e1073baa3ccd728a986abc54`.
A read-only Ghidra import reviews the entry, control flow and code/data boundary.

ROM data access alone still grants no execution. Explicit game entry enables only aligned
instruction addresses `0xBFC4A000..0xBFC4B11C`; trailing string data and all other ROM addresses
remain non-executable. The runner separately requires the exact reviewed BIOS hash before
enabling this profile. The interpreter decodes the original instruction through a scoped
fetch slot that is restored on every exit, including bus errors. Nested ROM data reads restore
the instruction before the fetch scope restores its prior slot. No ROM bytes are copied to
owned RAM by the host, and the original ROM input remains read-only.

Synthetic CPU checks cover sliced ROM execution, pending loads/delay slots, a data load from
the instruction's own ROM word, restored backing slots, disabled execution, rejected aliases,
unaligned fetch, branches into trailing data and a rejected store to ROM. CPU-control checks
exercise rejected writes and exception restoration without IRQ or RAM changes. Portable
bootstrap checks independently cover the ROM execution window.

Run-085 executes original IOPBOOT and reaches initialization code loaded into RAM by the guest.
This code uses the fixed DEV1 base's KSEG1 alias and an additional delay profile; the model
retains the exact observed `0xBE000000` base and `0x0018344F` delay without granting device access.
Run-087 retires the first IOPBOOT instruction at step 6,032,020, then reaches a word read from
ROM1 at `0xBE000000`, IOP PC `0x15E94`, step 6,537,975. The read is refused; there is no imported
ROM1 image or fabricated zero/open-bus response. Exit 6 without timeout, still 16 SIF1 chains
carrying 2,160 bytes. The 8-million-instruction budget remains unchanged.
Next work is to establish the optional ROM1 mapping/content or absence contract against the
verified reference. Completed reboot, repeated BOOTEND and native frame/menu/gameplay remain
unverified.
Run-088 repeats the same ROM entry and ROM1 blocker, including instruction counts. Isolated
initialization (run-089) still reaches actual guest BOOTEND with exit 5; ordinary mode (run-090)
retains its fixed-budget exit 6. Neither non-entry mode enables the new control/ROM path. All
four runs use the same executable and verified inputs without timeout. The changed-BIOS
negative check still refuses execution before guest startup; the original file remains intact.

## Verified absent-ROM1 profile and quiescent SIO2 control

The pinned reference reports that its optional ROM1 module is absent. The original reference
log and a fresh isolated PCSX2 v2.8.2 run both report this. Two independent PINE reads of the
entire 4-MiB ROM1 allocation return identical zero-filled data, SHA-256
`bb9f8df61474d25e71fa00722318cd387396ca1736605e1248821cc0de3d3af8`.
The reference executable and original BIOS hashes remain verified; these observations establish
reference memory contents, not native rendering or physical open-bus behavior.

This agrees with [memory reset](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/Memory.cpp),
the [optional BIOS loader](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/ps2/BiosTools.cpp)
and the [IOP ROM1 mapping](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopMem.cpp).
The entry experiment now explicitly enables a zero-backed, read-only aperture at physical
`0x1E000000..0x1E3FFFFF`, with physical/KSEG0/KSEG1 data aliases. Stores, instruction fetches,
KSEG3 aliases and neighboring regions remain unsupported. ROM2 is not enabled. Temporary data
staging preserves the original core's load delays and restores its previous backing value.

The runner still requires the fixed reviewed BIOS hash. Before enabling entry devices it also
checks both companion names used by the reference: `<bios>.rom1` and the BIOS path with its
extension replaced by `.rom1`. Any entry, including an empty file, or an indeterminate filesystem
status rejects this absent-module profile. A provided ROM1 image requires separate review;
there is no fallback that silently ignores it or zeroes unrelated memory.
Native negative checks verify rejection for both names using separate private BIOS copies.

Run-091 passes the ROM1 probe and reaches SIO2 initialization at IOP PC `0x36F3C`, step 6,896,374:
the guest writes `0x3BC` to control register `0x1F808268`. The pinned
[SIO2 control definitions](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SIO/SioTypes.h)
identify this reset profile; the [control implementation](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SIO/Sio2.cpp)
retains it without starting a transfer or raising IRQ. A separate `QuiescentSio2` component
permits this exact word write and word readback after its actual CPU store. Unknown values,
transfer-start writes, other widths, FIFO, command/status registers, pads, cards and DMA remain
unsupported. Nothing imports a ready flag or pretends a transaction completed.

Synthetic tests cover ROM1 aliases/edges, disabled profiles, retained backing values, no ROM1
execution or stores, actual SIO2 CPU store/load delay and unchanged neighbors/IRQ. Portable
checks additionally reject SIO2 readback before configuration and invalid initialization.

Run-092 gets through SIO2 initialization and reaches the next owned-code guard at IOP PC
`0x10019C`, step 7,373,114, RA `0x14778`. This exceeds the original bootstrap's code boundary.
Original ROM UDNL metadata has entry offset `0x16C`; an allocation at `0x100000` with its `0x30`
module header would produce exactly this entry. This is a candidate identification: guest RAM
bytes and relocations have not yet been compared. Execution remains refused. The next experiment
must verify that loaded image and a bounded execution contract, rather than opening upper RAM.
Exit 6 without timeout; the original 8-million-instruction cap remains unchanged. Completed
reboot, repeated BOOTEND, native menu/frame, gameplay, audio-content and saves remain unverified.
Run-093 repeats the same entry boundary and instruction count on the final executable. Isolated
initialization (run-094) still reaches actual guest BOOTEND with exit 5; ordinary mode (run-095)
retains its fixed-budget exit 6. These non-entry modes do not enable ROM1/SIO2 entry devices.
All runs verify their inputs and finish without timeout. Separate negative checks still reject
a changed BIOS before guest execution and either ROM1 companion before enabling entry devices.

## Verified guest-loaded UDNL and original linked imports

The ROM image review confirms UDNL at offset `0xADF70`, size `0x1EF5`, SHA-256
`dd05d86f9e47e900904c3eb1a5ee6a7fa700a678effc267ec3cdd929a8200051`.
Its load segment is `0x1860` bytes and its `.text` spans `0x17B0` bytes. The existing pinned
bootstrap loader computes relocations in a separate temporary memory object at `0x100030`;
the expected entry is `0x10019C`. Its rounded allocation size is `0x1900`. The host never
loads, repairs or patches this reference into the running guest.

`VerifiedRamCode` enables the new window only at the actual entry, with the entire text in
owned guest RAM and every executable word matching its relocated reference. Initial run-096
correctly refused a changed import-table link field; treating mutable link metadata as an
instruction would have been the wrong contract. The original import headers and terminators
are now non-executable data, with identity/version/link mode validated separately.

The import binding review follows the pinned
[PS2SDK guest linker](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/loadcore/src/loadcore.c).
For each original stub, the validator requires a real MIPS jump, the unchanged ordinal delay
instruction and a matching guest export. The export must have the same library identity and
major version, be enabled, include this actual import table in its bounded caller chain, and
map the requested ordinal to that exact jump target. Targets stay within the existing owned
bootstrap-code range. Unregistered tables, unresolved returns, missing exports, invalid modes,
cycles beyond the finite bound and jumps into unreviewed upper RAM are refused.

Only the validator's private expected bytes are adjusted for proven guest linking. The actual
guest tables and jumps are never written by this mechanism. Each subsequent instruction in
the new module window is checked again before execution. Metadata, adjacent data, alternate
entry before validation, unknown aliases and later changed instructions remain forbidden.
No upper-RAM-wide permission, host import implementation or instruction-budget increase occurs.

Run-097 verifies the 6,064-byte text extent and all 14 linked imports across six libraries,
then the original UDNL reaches CDVD S-command `0x05`, at step 7,375,027. This is execution of
the original module through the existing CPU core, not a loader-output or host-return claim.

The virtual closed ISO drive now models the tray-detection latch operation: no parameters,
one response byte, and its sticky tray bit follows the current closed-tray state. It neither
opens a tray nor completes sector reads or acknowledges IRQs. This functional profile follows
the pinned [CDVD implementation](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/CDVD/CDVD.cpp).
Run-098 then reaches a HOWTO write at register `0x06`. Its observed `0x80` profile is retained
as a write latch; reads at the same offset still consume ERROR. Other HOWTO values fail closed.

Run-099 reaches a real request to arm CDVD DMA3: PC `0x1B020`, step 7,375,321,
CHCR `0x41000200`, MADR `0x1F7D0`, BCR `0x00100020`. The existing DMA guard rejects this
before the CPU writes STR, because this strict IOP path has no CDVD transfer engine yet.
No bytes, DMA completion or IRQ are invented. Exit 6 without timeout; 16 SIF1 chains still
carry 2,160 bytes. Next work is a reviewed DMA3 receive engine with actual sector bytes,
pending receiver state, register progress, gates and independent device/DMA completion.
Completed reboot, repeated BOOTEND, native frame/menu/gameplay, audio-content and saves remain
unverified.

Synthetic checks cover altered instructions before/after activation, unowned text, an invalid
entry, caller-chain cycles, wrong library versions, ordinals, missing exports, unowned or escaped
jump targets, and attempted execution of metadata. The actual CPU executes a synthetic module
outside the old window and rejects later modification without repairing it. CDVD checks cover
unconsumed replies, invalid tray parameters, pending sectors and preserved IRQ/error state.
Run-100 repeats the same verified UDNL and DMA3 boundary on the final executable. Isolated
initialization (run-101) still reaches actual guest BOOTEND with exit 5; ordinary mode (run-102)
retains its fixed-budget exit 6. Neither non-entry mode enables the new module window. All
four runs verify inputs and finish without timeout. Negative checks continue to reject a changed
BIOS before guest execution and either ROM1 companion before enabling entry devices.

## CDVD DMA3: actual user-sector reception

The entry experiment now enables a separate functional DMA3 receiver. Its reviewed profile
is CHCR `0x41000200`, 32 words per block, a nonzero whole-sector block count and a fully
bounded, aligned physical RAM destination. Unsupported modes, zero or partial-sector counts,
active rearming and escaped destinations fail before consuming sector data. Other channels
retain their existing start guards. Non-entry modes do not enable this receiver.

The contract follows pinned PCSX2 v2.8.2:
[DMA3 arming](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/CDVD/Ps1CD.cpp),
[sector copy and MADR/BCR progress](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/CDVD/CDVD.cpp),
[channel enable](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopHw.h) and
[DMA interrupt latching](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopHw.cpp).
This receiver also requires the strict IOP global DMA-enable profile to permit transfer.
DMA3 interrupt latching is channel-mask dependent; it does not inherit SIF's unmaskable IRQs.

An actual CPU store arms STR. With no payload or a disabled DMA gate, registers and payload
remain pending. With actual sectors, one complete 2,048-byte sector is copied per service
step; MADR advances and the BCR block count decreases by the amount actually transferred.
STR clears only when the DMA count is exhausted. A longer drive command can span multiple
DMA transfers, and drive-command completion remains separate from DMA completion. No sector
padding, fabricated read data or RPC response is introduced. Raw DVD frames remain unsupported.
This is a synchronous functional model, not a measured hardware timing implementation.

Run-103 first crosses the old boundary. Final-build runs 104 and 105 repeat 21 actual DMA3
completions, 278,528 bytes (136 user sectors), from the hash-verified ISO. Original code requests
LSNs 16, 257 and 262 before the subsequent reads; the last request is LSN 2818. The trace
records accepted reads and actual DMA completions separately. On this path ICR remains zero,
so no DMA3 interrupt is invented; the original guest proceeds by polling and drive IRQ handling.

The next refusal is at step 7,665,075: PC `0xA010148C`, physical `0x10148C`, RA `0x100A90`.
The physical address is within the UDNL text extent, but the current verified-code profile
does not admit that uncached alias. Next work is to review the original transition and memory
mapping, then retain ownership, instruction and metadata checks for any explicitly admitted
alias. This run does not establish a completed reboot, repeated BOOTEND, native frame/menu,
gameplay, audio-content or saves. The 8,000,000-instruction bound is unchanged.

Synthetic ISO tests compare every transferred byte against the source fixture, check canaries,
armed waiting, both DMA gates, partial progress across two sectors, multiple DMA completions
within one drive command, channel/master IRQ masks, preserved unrelated IRQ bits and no duplicate
completion. Invalid configurations are refused without consuming data. The real CPU smoke test
checks that a disabled profile refuses the STR store and the enabled profile retains the actual
store without inventing bytes, register progress or IRQ. All tests use original synthetic data.

Final regression runs preserve isolated initial BOOTEND (run-106, exit 5) and ordinary bounded
execution (run-107, exit 6). These modes do not enable DMA3 or the UDNL window. All final runs
verify their inputs and finish without timeout. Changed BIOS and both ROM1 companion cases
remain refused on the same final executable; original inputs remain unchanged.

## Verified UDNL uncached execution

The original, independently hash-checked ROM UDNL contains its own KSEG1 transition:
it saves CP0 Status, constructs its relocated procedure address, combines it with
`0xA0000000` and jumps there. The Status-zeroing instruction also executes as the real
jump delay slot. This identifies the observed target `0xA010148C`; the host does not supply
a jump, CPU context change or replacement procedure. The pinned
[PCSX2 IOP memory mapping](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopMem.cpp)
and the pinned CPU core both map this KSEG1 address to the same physical RAM.

`VerifiedRamCode` now has an explicit, single-use uncached-alias option. Only the reviewed
UDNL profile enables it, before activation. The actual physical entry must still pass the
full relocated-text and guest-import validation first. Later KSEG1 instruction fetches use
the same bounded physical reference and ownership checks; import headers and terminators
remain non-executable. KSEG0, other segments, adjacent data, unaligned addresses and changed
instructions remain refused. A policy cannot gain this option after becoming active.
The coherent functional cache model and all BIOS/ELF gates remain unchanged.

Runs 108 and 109 repeat the 21 DMA3 completions and then verify original code at the uncached
target. Execution proceeds to the existing 8,000,000-instruction limit, at LOADCORE PC `0x2B48`.
The selected original LOADCORE image matches the bounded fill-loop instructions and its
observed base `0x1830`. At the limit, the next destination is `0x3CC88`, the exclusive end is
`0x101D00`, and the stored pattern is `0x400D`. The loop increments by four and compares against
that end: 201,758 word stores remain. Its five-instruction body therefore needs approximately
1,008,790 more instructions, excluding the pending branch and final return overhead. This is
an instruction-count estimate, not hardware timing or evidence of a completed reboot.
The pinned [PS2SDK LOADCORE](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/loadcore/src/loadcore.c)
also documents the memory-fill pattern during initialization.

Next work is to observe guest phase boundaries and finite fill progress before changing budget
accounting. The cap is unchanged here; the guest fill is neither skipped nor replaced with a
host memory operation. No second BOOTEND, native frame/menu/gameplay, audio-content or saves
have been established.

Portable checks cover default refusal, alias-before-entry, duplicate/late option activation,
ownership, exact boundaries, other segments, instruction mutation without repair and aliased
import metadata. A real CPU synthetic test executes the jump, delay slot, aliased instructions
and return in seven instructions; its default profile refuses the same alias. Regression runs
110 and 111 preserve isolated initial BOOTEND (exit 5) and ordinary bounded execution (exit 6),
with no alias enabled in those modes. All four runs finish without timeout and verify their
inputs; changed BIOS and both ROM1 companion cases remain refused on the final executable.

## Local experiment: completed guest reboot and subsequent DVD loading

The old single budget included both initial IOP startup and the subsequent game-requested reboot.
The observed LOADCORE fill makes progress from `0x3CC88` at instruction 8,000,000 to `0x100188`
at 9,000,000 and then exits that loop. No host fill or CPU state replacement is needed.
`IopExecutionBudget` now permits three finite phases: initial startup, one explicitly enabled
reboot after the verified original IOPBOOT entry retires, and runtime after a second actual
guest BOOTEND rising store retires. Each has an 8,000,000-instruction allowance; the same
monotonic counter stays below 24,000,000. No idle poll, repeated BOOTEND or second reboot
can renew it. Non-entry modes retain the original budget.

The actual ROM entry is at instruction 6,032,020, giving a reboot deadline of 14,032,020.
The second BOOTEND store retires at 13,048,540, giving a runtime deadline of 21,048,540.
The observer counts successful CPU stores without altering SIF flags or inventing an IRQ.
This establishes the requested IOP reboot and new post-reboot traffic; it does not establish
a rendered menu or gameplay.

The DVD device now retains the full HOWTO byte, as the pinned functional controller does,
and derives SPEED from its accepted spindle setting and spinning state. DMA geometry is
checked against whole 512-word user sectors or 516-word reference DVD packets. The pending
device format resolves geometries that can represent both. Unsupported partial sectors,
destinations, formats and active rearms remain refused.

This new DVD subset follows the pinned
[CDVD command/transfer implementation](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/CDVD/CDVD.cpp)
and [ISO provider](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/CDVD/CDVDisoReader.cpp).
It requires equal primary-volume and file sector counts; the verified input has 2,170,432
of each. Command `0x08` constructs the reference 2,064-byte single-layer packet from the
actual 2,048 ISO bytes, a derived sector ID and the reference's zero IED/CPR_MAI/EDC fields.
These are explicitly functional reference packets, not physical raw-sector reconstruction,
physical CRC validation, encryption or authentication support.

Command `0x09`, parameter zero, has a distinct contract: copy a 2,048-byte reference DVD
structure with its last LSN derived from the actual image. It completes DMA3 without advancing
MADR/BCR, matching the pinned TOC path; the following 16 bytes remain untouched. It is not
counted as an ISO-sector read. Completion still needs the actual structure copy and DMA gates.

At the 2026-10-20 checkpoint, the clean native path completed 34 DMA transfers: 136 initial user sectors, another 61 user
sectors in reference DVD packets, and one TOC structure, totaling 406,480 DMA bytes. The next
stop is an actual address-error store at instruction 14,573,618: PC `0x11978`, bad address
`0x11`. A private bounded CPU history and RAM snapshot identify the caller as the MCMAN module
at text base `0x5A830`, entry `0x5A9A8`. Its original HI16, LO16 and 26-bit call relocations at
offsets `0x194`, `0x198` and `0x19C` are present in the exact supplied IRX, but their runtime
words still match the unrelocated source. The call at `0x5A9CC` therefore reaches `0x11954`
inside THREADMAN instead of the relocated local target. Why the guest loader reaches this
state was unresolved at that checkpoint; the next section records its cause. No pointer, import, relocated code or exception handler
has been supplied by the host to conceal the error.

Tests cover finite phase transitions and refusals, real CPU ROM retirement with a monotonic
counter, successful BOOTEND rising stores, all HOWTO bytes preserving pending data and IRQs,
DVD framing and block geometries, exact TOC bytes, untouched TOC tail, format mismatches and
single-volume refusal. Temporary address-specific instrumentation and snapshot writing are
removed from the source; evidence stays ignored. Native graphics/menu/gameplay, audio-content
and saves remain unverified.

## Local experiment: unaligned load merge and complete MCMAN intake (2026-10-21)

The pinned CPU merged each LWL/LWR instruction with the old architectural register, losing
the preceding partial load that was still pending. The MIPS I exception for adjacent unaligned
loads requires forwarding that pending merge value while retaining the final load delay; see
the IDT R30xx Family Software Reference Manual, section 2, pages 2-6/2-7
([manual](https://manuals.plus/m/921bbff6fec5c6fb953a0fefbc4c62e99cdd1d422ad8c6dd2d73892a23d8bcaa)).
The source overlay now changes only those two merge operands. Effective-address and ordinary
instruction operands keep their previous semantics. The pinned tools checkout remains clean;
configuration refuses an unexpected replacement count.

A synthetic regression failed before the fix and passes after it for all four byte alignments
and both instruction orders. It verifies byte-accurate merging, the pending first load and the
delay after the second load using the actual CPU core, with no game data in the fixture.

Controlled native file-call observations show the earlier end-of-file seek returning `0x76A5`
(30,373), followed by a read of that same truncated length. With the CPU merge corrected, both
return the full `0x176A5` (95,909). The original guest LOADCORE then reaches MCMAN's actual
relocation sections with 2,647 text, 23 rodata and 31 data relocation entries. The host does
not relocate the module, override its arguments or replace its calls.

The cleaned native path advances to 41 DMA transfers and 509,680 bytes: 136 initial user sectors,
111 user sectors in reference DVD packets and one 2,048-byte TOC. The previous MCMAN/THREADMAN
address error is passed. The next explicit stop is instruction 15,042,867, PC `0x868A4`, SW
`0xACC20000`, attempting value `0xBF900000` at `0xBF801404`, the SPU bus base configuration
register. Its supported values remain restricted; this observation does not establish SPU audio.
Temporary loader/file-call instrumentation is removed. Rendered graphics/menu, gameplay,
audio-content, saves and recovery remain unverified.

## Local experiment: original LIBSD bus and SPDIF setup (2026-10-21)

The original LIBSD writes `0xBF900000` and `0xBF900800` to the SPU/SPU1 SSBUS base latches,
then sets the SPU1 bus delay to `0x200B31E1`. These exact values are now accepted by the
existing configuration profile. They retain register storage without opening the mapped
device ports or simulating bus timing. The pinned
[IOP hardware write implementation](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/ps2/Iop/IopHwWrite.cpp)
routes these addresses to generic backing storage.

`SpdifSetup` is a separate, initially inactive component enabled only with the reviewed
entry-device profile. It accepts halfword stores of MODE=`0x0900`, MEDIA=`0x0200`, raw
latch `0x7CA`=`8`, and OUT=`0` or `0x8000`. The pinned
[SPU2 register handlers](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SPU2/spu2sys.cpp)
store these fields; both supported OUT values retain normal routing in `UpdateSpdifMode`.
This subset does not implement PCM clone, bypass, data streaming, IRQINFO, core control,
SPU memory transfers, synthesis or audible output. Reads and other widths/values are refused.
State and the bounded diagnostic counter update only after the original CPU SH retires.

Synthetic CPU tests cover the configuration stores through all three address aliases,
halfword canaries, retained values, inactive refusal and unchanged IRQ state. Writes to
output mode `0xC032`, IRQINFO, core control and even a zero master-volume store remain
refused before changing state. No generic SPU zero-write fallback is installed.

The native path executes five actual SPDIF stores and next stops at instruction 15,621,176:
PC `0x87238`, instruction `0xA6C00000`, attempting SH zero to `0xBF900760`, the core-0
master-volume-left register. The existing 41 DMA transfers, 247 user sectors, 509,680 DMA
bytes and two guest BOOTEND stores remain intact. The next work is a stateful core-volume
and control model, including its interaction with timing and transfers. No rendered menu,
gameplay, game audio, saves or recovery is established by this setup experiment.

## Local experiment: direct volume and quiescent SPU core control (2026-10-21)

`SpuVolumeRegisters` owns the master/effect/external/input gains for both cores, with separate
raw and current master-volume reads. Direct master values are signed 15-bit values doubled
into a signed 16-bit coefficient; the other gain registers retain signed 16-bit values.
Initial master/effect gains are zero, external/input gains are `0x7FFF`, matching the pinned
core initialization. The conversion follows
[ADSR.cpp](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SPU2/ADSR.cpp)'s volume setter and
the [register handlers](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SPU2/spu2sys.cpp).
Sweep-mode writes and writes to current-volume registers remain refused. No sample-time or
audio-output behavior is inferred from these direct gains.

`SpuQuiescentControl` accepts only ADMAS zero and ATTR zero/`0x8000`, together with halfword
reads of those latches and STATX. The reference finishes initialization with STATX=`0x80`;
a real ATTR store with DMA mode zero clears that flag when no transfer is busy. The guest
observes this state transition rather than a prefilled ready flag. This component has no
operation that starts a transfer, enables DMA/IRQ/reverb or starts a voice; attempts remain
explicit blockers. Existing IOP DMA gates also refuse SPU channel 4/7 starts. It does not
implement cancellation of an active transfer or a timed physical reset.

Both components stage temporary CPU halfword reads and restore backing slots after each
instruction. Device writes commit only after a successful guest SH; faulted operations retain
the previous state. Tests cover signed extrema, master raw/current differences, both cores,
all three aliases, halfword canaries, load delays, exception restoration and refusals. The
control tests verify initial STATX=`0x80` before the first ATTR store and its subsequent clear.

The native guest executes eight volume stores and six control stores, then passes its
quiescent-status polling. The next stop is instruction 15,623,182, PC `0x8733C`, SH
`0xA4430000`, writing `0xFFFF` to core-0 KOFF at `0xBF9001A4`. The next experiment needs the
pending KeyOff mask and voice-envelope behavior at sample boundaries; an unconditional
success/no-op for KOFF is not provided. The existing two guest BOOTEND stores and 41 real
DMA transfers (247 user sectors, 509,680 DMA bytes) remain intact. Native graphics/menu,
gameplay, game audio, saves and recovery remain unverified.

## Local experiment: SPU data transfers and graphics DMA progress (2026-10-22)

Earlier sections record historical diagnostic boundaries. The KOFF stop described above
has been passed locally. Voice setup now retains pending key masks, sample-clock envelope
state, addresses and ENDX. Key-on is limited to a verified zero-sample ADPCM loop in actual
SPU RAM; other samples, noise and sweeps fail closed. Reverb is limited to the invariant
of zero input, history and workspace, checked against the real shared RAM at sample time.
This is not general DSP or audible playback support.

The shared 2-MiB RAM receives actual CPU halfwords and plain DMA4/7 writes from IOP RAM.
DMA copies bounded bursts, preserves busy/STR until the last modeled interval, updates
MADR/TSA, retains BCR and raises a masked completion only after copying the data. The
functional 256-halfword burst and 24-cycle charge follow the pinned
[SPU2 DMA reference](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SPU2/Dma.cpp).
Instruction charging is approximate; physical timing is not established. ADMA, DMA reads,
active cancellation and SPU IRQ are unsupported. The strict memory overlay removes the
upstream HLE shortcut that cleared STR without copying samples. The pinned tool checkout
remains unchanged.

The native experiment completed 67 SPU DMA writes, copying 574,784 bytes. DVD DMA reached
226 transfers and 5,997,856 bytes. The next run permits longer bounded resource loading:
8M initial instructions, at most one 8M reboot phase, then a non-renewable 256M runtime
allowance. No device poll renews this budget. The launcher accepts at most 300 seconds.

Both CPUs run on the EE executor. Safe points advance bounded IOP work and consume real
DMA completions even when an EE thread busy-polls RAM. GS CSR/IMR observations route actual
unmasked events to the registered HLE INTC handler; they do not generate FINISH flags.

A separate graphics defect was reproduced: an empty source DMA chain fetched NEXT/END
tags but never queued completion because it had no payload buffer. Real zero-QWC chains
now retire their actual aligned RAM tags, update address/status registers and complete
the channel, following the pinned
[source-chain semantics](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/Hw.cpp) and
[GIF completion path](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/Gif.cpp).
CALL/RET and payload chains remain with the existing engine. Bounded cycles and invalid
tag ranges fail closed. CHCR reads retain STR instead of clearing it. This fix passed
the previously stuck graphics initialization and led to further resource loading.

The capture helper observes the latched guest GS presentation after actual draw commands.
An initial dark two-band clear was visually rejected. After resource loading, two native
runs produced recognizable game text: the DualShock 2 controller is not detected in port 1.
Both were visually inspected; a second capture 120 presentation callbacks later retains
the message. These initial images were dark and duplicated vertically; menu and gameplay are not proven.
Brightness/contrast filtering only selects captures and never changes rendered pixels.
Captures and executor-owned RAM snapshots stay in ignored work/.
Synthetic checks and successful resource transfers do not establish menu, gameplay,
audio, save or recovery success.

### Frame scanout and controller experiment (2026-10-22)

Actual video registers select CRT2 only and `SMODE2.INT=FFMD=1`. The CPU backend
incorrectly read the full display height from VRAM. The build-local overlay now reads
half-height (rounded up), then expands scanout rows, following the pinned
[PCRTC rectangle calculation](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/GS/GSState.cpp).
A new native capture was visually inspected: the warning appears once, centered, with
the expected vertical scale. The image remains dim; no brightness correction is claimed.

The first-frame profile does not deliver video events to the strict IOP and cannot
validate controller polling. The optional `--iop-video-events` launcher flag forwards
the existing EE VBlank start/end events after the real post-reboot BOOTEND. IOP pending
bits are IRQ0/IRQ11, respectively, as in the pinned
[IOP counter implementation](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopCounters.cpp).
Masking and CPU exception delivery remain guest-controlled. This is functional event
routing; the upstream fixed-period scheduler does not establish PAL-accurate timing.

The experiment reaches real SIO2 accesses previously absent: PortStat read, raw port
setup stores, then a command-queue write. PortStat's reset value is `0xF`; it does not
report a connected controller. Port control words are retained from actual CPU stores.
The pinned [SIO2 state](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SIO/Sio2.cpp) and
[register definitions](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SIO/SioTypes.h)
define this restricted quiescent state. No pad replies, FIFO data, DMA completions or
SIO2 IRQs are invented. The first unsupported operation is a command-descriptor store;
the earlier experimental run stopped before rendering. The checkpoint below supersedes
that limitation only when the virtual-pad option is explicitly enabled.

### Virtual DualShock2 checkpoint (2026-10-22)

`--virtual-pad` requires `--iop-video-events` and firmware entry. It enables a bounded
packet-level DualShock2 device, SIO2 FIFO/START/IRQ handling, and actual DMA11/12
RAM transfers. PADMAN negotiated digital, configuration and analog modes in a native
run. A later bounded run reached its instruction budget without an unsupported-device
stop; first and follow-up captures visibly show “Accessing…”. This is not a menu or
gameplay result. Physical controller input and reconnection remain unverified.

The host adapter samples keyboard or raylib gamepad 0. Enter is Start, Backspace is
Select, arrows are the D-pad, WASD/IJKL are the sticks, X/C/Z/V are Cross/Circle/Square/
Triangle, Q/E are L1/R1, 1/3 are L2/R2, and 2/4 are L3/R3. Losing focus releases inputs.
Motor requests are recorded without claiming physical rumble. Guest ready flags are
not patched. Unknown device commands and unsupported DMA modes fail closed.

No memory card is attached. Reviewed absent-card probes return negative serial replies;
card storage, formatting, saves and successful authentication are not implemented.
Color tracing records actual GS inputs without modifying pixels. Brightness and the
later frame composition still need a valid reference comparison. The final-frame
capture helper is build-checked but has not yet been exercised in a new native run.

Validation: Release native build, bootstrap CTest and 43 Python tests passed. Synthetic
tests cover pad negotiation, button/axis packets, disconnection, SIO2 PIO/IRQ, DMA data
and completion boundaries, and unsupported operations. These checks do not establish
interactive input, game audio, saving or recovery.

### Final-frame and zero-sample stream checkpoint (2026-10-24)

The final-frame helper was exercised in native186. Its actual GS capture shows the
no-memory-card Yes/No dialog, beyond the earlier “Accessing…” image. The run ended
at the finite IOP budget without a supervisor timeout. A separate isolated PCSX2
software-renderer reference with a DualShock2 and no cards reached the same dialog.
Its raw framebuffer dumps are not final presented/deinterlaced screen captures.

Real foreground host keyboard input (Left, then X/Cross) exposed a strict SPU
blocker: core 1 voice 23 key-on at SSA `0x2808`. The first ADPCM block was entirely
zero with an ordinary header, not the exact looping `0x0700` header previously
accepted by the bounded decoder.

The zero-sample model now follows ordinary blocks and actual start/end/repeat
flags, including guest-written LSA precedence, NAX progression, ENDX and ADSR stop.
Known predictors and zero sample words keep decoder history and PCM zero;
nonzero sample data, noise and sweeps remain explicit refusals.
Synthetic transition/error checks and the native Release build passed.

Native189/190 passed the old key-on refusal and then stopped on nonzero ADPCM data
at NAX `0x2811`, core 1 voice 23, pitch `0x0759`, active ADSR. General ADPCM voice
processing and its interaction with the zero-input-only reverb model are the next
device work; no sample was discarded or completion invented. Main menu, audible
output, gameplay, saves and recovery remain unverified. The user requested a local
checkpoint commit without a push. See [the handoff](HANDOFF_2026-10-24.md) for
commands, evidence boundaries and the next experiment.

### Nonzero ADPCM and real input reports (2026-10-24, local work)

The voice model now decodes signed low-nibble-first ADPCM into a block latch and
a 32-sample FIFO. It retains predictor history across blocks, saturates PCM,
advances fixed-point pitch, and applies four-tap Gaussian interpolation before
ADSR. Existing NAX, LSA, loop, ENDX and key-off checks remain in place. A write to
already fetched sample RAM does not rewrite the current block/FIFO; the next
block load reads the actual current RAM. Unsupported predictors, reserved flags,
noise and volume sweeps remain explicit refusals.

The interpolation coefficients are generated analytically and checked against all
1024 numeric coefficients in the pinned
[SPU interpolation reference](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SPU2/interpolate_table.h).
The decoder/state semantics were checked against the pinned
[SPU mixer](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/SPU2/Mixer.cpp).
No upstream implementation or coefficient table is vendored.

Active effects still require verified zero routed input and workspace. Nonzero
wet PCM is rejected. Nonzero input while FX is off marks unmodeled filter history,
so re-enabling FX cannot silently assume that history is empty. Potential nonzero
core-to-core external wet mixing also remains unsupported. Decoder tests and PCM
traces do not establish audible host output.

Native host taps are retained until one real SIO2 poll report samples them. Pressure
comes from the actual host snapshot; losing focus cancels pending taps without
disconnecting the virtual device. A Windows arrow-state fallback handles host arrow
events that did not reach raylib in the controlled experiment. There are no guest
RAM input patches or ready-flag substitutions.

Native195 delivered actual Left and Cross reports, decoded nonzero PCM on core 1
voice 23 and left the no-card dialog. Its final GS capture shows Square Enix.
Native196 repeated the changed path with earlier input and its final GS capture
shows Cavia. Both ended at the finite IOP instruction budget without a supervisor
timeout. These logos are not a verified main menu or gameplay result.

The launcher now offers `--iop-runtime-millions 256|512|1024`; the default remains
256. Larger diagnostic presets require firmware entry, are selected once before
guest reboot, and cannot be renewed by device polling or a second BOOTEND. Initial
boot and reboot allowances stay unchanged; the supervisor timeout is separate.
