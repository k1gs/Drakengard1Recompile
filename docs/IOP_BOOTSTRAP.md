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

The reference names this register HW_ICFG. Its update-reboot state has not been confirmed.
Cold-reset zeroing of the emulator's hardware array does not establish the BIOS-initialized value;
the probe does not inject a guessed zero. Next: establish this specific register contract/state,
then repeat bootstrap and record its next actual blocker.

Strict synthetic C++ checks cover normal return/delay slot, exception, non-RAM access, unresolved
import, unowned instruction fetch and instruction-budget exhaustion. Python tests cover ROMFS
extents/versions, explicit table offsets, extraction isolation and boot-plan selection.
These checks do not prove full module initialization, IOP/EE synchronization, a game frame or menu.

Primary references pinned to PS2SDK `329e2bb43287e16af16c4dde1c614e2662c24468`:
[ROMFS records](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/fs/romdrv/include/romdrv.h),
[UDNL selection, ResetData and bootstrap sequence](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/udnl/src/udnl.c).
Hardware identifier: [PCSX2 2.8.2 IOP register definitions](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/IopHw.h).
