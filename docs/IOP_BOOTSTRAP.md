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
are fabricated. An actual pending IRQ requires a separate exception-delivery implementation.

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
