# Native SIF command wait

The initial command-init blocker is resolved by the original packet bridge described below.
The current native blocker is an unavailable CDVD RPC service, SID `0x80000592`.

Native-run-012 (40 seconds) and native-run-013 (20 seconds) repeatedly observed guest PC
`0x003223E0`, RA `0x003239D4`, SP `0x0011FCC0`, thread 1. The loop calls `0x00321AE0(0)`
to read SREG[0] at `0x006B0B40` and waits for a nonzero value. Both runs ended by supervisor
timeout. A host render loop counter does not establish an actual rendered game frame.

The read-only transfer trace in native-run-013 captured both BeforeCopy and AfterCopy:

| EE source | IOP destination | Bytes | Command | Option | Additional field |
| --- | --- | --- | --- | --- | --- |
| `0x006B0980` | `0` | `0x14` | `0x80000002` | `0` | EE receive buffer `0x006B08C0` |
| `0x006B0C00` | `0` | `0x10` | `0x80000002` | `1` | None |

The pinned runtime copies these bytes directionally into IOP RAM. Its IopRpcBridge transfer
callback does not implement native SIF command responses. Guest SDK routines execute their
own protocol instead of calling the runtime's higher-level HLE RPC entry points.

## Protocol reference

Reference PS2SDK commit: `329e2bb43287e16af16c4dde1c614e2662c24468`.
The common header defines the 16-byte command header and command IDs.
EE command setup supplies its receive address; RPC synchronization waits for SREG[0].
The IOP RPC implementation announces readiness using a SET_SREG command packet.
These are protocol observations from public source, not proof of every SCE SDK implementation:

- [Common SIF command definitions](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/common/include/sifcmd-common.h)
- [Common RPC packet definitions](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/common/include/sifrpc-common.h)
- [EE command implementation](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/ee/kernel/src/sifcmd.c)
- [EE RPC implementation](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/ee/kernel/src/sifrpc.c)
- [IOP command implementation](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/sifcmd/src/sifcmd.c)
- [IOP RPC implementation](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/system/sifcmd/src/sifrpc.c)

## Implemented packet channel

`sif_channel.cpp` parses actual copied IOP request bytes, validates sizes/addresses/order and
negotiates the EE receive address. It implements command initialization, RPC bind/call/other-data,
normal interrupt completion, no-interrupt packet completion and reset. Unknown commands fail explicitly.
Replies are written to the negotiated buffer; guest SIF0 code updates its own software registers.
An outgoing EE DMA no longer creates a fictitious incoming SIF0 interrupt: the private SIF build
unit raises that interrupt only after delivery of an actual reply. Native-run-014 passed RPC init.

Bindings use actual available IOP routes or our implemented FileIO backend. Server records and
bounded 64-KiB request buffers occupy separate, allocated IOP RAM. Unavailable services return a
null binding. Calls require the real copied payload to match its recorded EE source before using
that source through the upstream host ABI. Changed payloads, missing routes, unhandled results or
unsupported guest-function callbacks stop explicitly; no fallback copies fabricate a successful call.

FileIO SID `0x80000001` supports read-only open, close, read and signed-32-bit lseek through runtime
VFS. Host reads copy all returned bytes, including unaligned edges. The 16-byte edge-completion
header reports no pending fragments; no bytes beyond the actual read result are filled into the
requested file buffer. Other FileIO commands and write modes remain unsupported.

The observed Europe client first issues function `0xFF` with two callback buffer addresses, then
selects a newer transfer ABI only if reply word 1 equals 2 (guest code `0x00323B5C`..`0x00323B88`).
Our legacy backend rejects that extension with a negative first result word and no advertised
version; it does not register the supplied pointers. Native-run-016 accepted the refusal and
advanced to the next RPC service. This is a deliberate unsupported-extension response, not an
assertion that the backend implements the newer SCE protocol or all legacy operations.

FileIO wire reference: [common request layouts](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/common/include/fileio-common.h),
[IOP service and SID](https://github.com/ps2dev/ps2sdk/blob/329e2bb43287e16af16c4dde1c614e2662c24468/iop/fs/fileio/src/fileio.c).
Version-probe behavior comes from the exact verified game ELF; raw game code stays private.

## Checks and current next experiment

```powershell
cmake -S native/sif_smoke -B build/sif-smoke
cmake --build build/sif-smoke --config Release
ctest --test-dir build/sif-smoke -C Release --output-on-failure
```

Both original synthetic C++ tests passed locally. They cover malformed headers, order,
negotiated reply buffers, unavailable binding, real backend payload/results, no-interrupt
completion, reset, actual file bytes, unaligned reads, EOF, seek, close and explicit refusals.
Native FileIO binding/probe was observed; an actual game file/sector read was not yet observed.

The next service is CDVD SID `0x80000592`, currently unavailable. Connect verified media and
implement only established wire commands with actual operations. Then repeat a bounded native run.

## Original experiment plan (historical)

Implement a bounded packet-level channel using the actual copied IOP request bytes. Preserve
EE/IOP address separation. Deliver replies to the negotiated EE packet buffer through the
existing SIF0 guest interrupt path, so guest handlers perform their own state updates.
Bind only to existing backend routes and execute calls only when genuinely handled.
Keep unknown commands, unsupported callbacks, invalid sizes and unavailable services explicit.
Do not set the observed guest ready flag directly or fabricate successful RPC/service results.

Synthetic tests should cover malformed headers, initialization order, negotiated addresses,
reply bytes, missing service binding, call payload/results, no-interrupt completion and reset.
Then repeat a bounded native run and record its first blocker before claiming progress.
