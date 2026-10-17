# Read-only ISO and CDVD transport

The native runner accepts only the exact Europe ELF and, when supplied, the exact original ISO
SHA-256 recorded in INPUT_EUROPE.md. Both are checked before creating the runtime window.
The image is opened with a read-only stream. No BIOS or game files are bundled or published.

`IsoMedia` supports 2048-byte ISO user sectors, a primary CD001 volume descriptor and terminator,
both-endian extent/size checks, bounded recursive directories and actual file LSN/size lookup.
Malformed extents, duplicate paths, cycles, extended/interleaved/multi-extent records and unsupported
versions fail explicitly. This is a supported subset, not a general optical-disc implementation.

`CdvdRpc` implements these established service operations:

| SID | Operations |
| --- | --- |
| 0x80000592 | Init/exit against an already opened, indexed image; no advertised SCE version |
| 0x80000593 | DVD media type, last error, modeled status, DVD media mode |
| 0x80000595 | Actual 2048-byte sector reads, ready, bounded logical seek, standby/stop/pause |
| 0x80000597 | Legacy 292-byte file-search request; actual 32-byte file record |
| 0x8000059A / 0x8000059C | Ready for the synchronous virtual drive with no pending operation |

Reads transfer the actual sector bytes, including unaligned destinations, and publish the actual
byte count. Invalid ranges return the CDVD error state without padding the requested buffer.
EE data/metadata ranges are validated and may not overlap. Edge headers report zero pending
fragments because the host copy already includes those bytes. Incoming SIF completion still goes
through the guest command handler. RTC, TOC, raw sectors and unknown commands remain unsupported.
No physical seek latency, optical-disc timing or PAL frame rate is inferred from this model.

Public protocol references are pinned to PS2SDK `329e2bb43287e16af16c4dde1c614e2662c24468`:
[shared CDVD layouts](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/common/include/cdvd-common.h),
[EE client](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/ee/rpc/cdvd/src/libcdvd.c),
[IOP service](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/cdvd/cdvdfsv/src/cdvdfsv.c).
ISO directory structure reference: [ECMA-119, fourth edition](https://www.ecma-international.org/wp-content/uploads/ECMA-119_4th_edition_june_2019.pdf).
These establish layout contracts; they do not establish compatibility with every game command.

## Reproduce synthetic checks

```powershell
cmake -S native/iso_smoke -B build/iso-smoke
cmake --build build/iso-smoke --config Release
ctest --test-dir build/iso-smoke -C Release --output-on-failure
```

The original fixture has directories and known bytes in separate sectors. Checks cover actual
LSNs/data, malformed media, unaligned CDVD read/completion, invalid ranges, search and read-only
preservation. No game or BIOS input is used. CI builds this target on Windows and Linux.

The optional `iso_probe` requires `--input-complete` before opening a user-supplied image.
On the verified Europe disc it found 2,170,432 sectors, 17 files and the boot ELF at LSN 287
with size 4,873,284. An independent extent read matched the exact ELF SHA-256.
Native-run-020 passed CDVD init/ready and reached an explicitly unsupported IOP reboot.
An actual game-initiated sector/file read, game frame and menu remain unconfirmed.
