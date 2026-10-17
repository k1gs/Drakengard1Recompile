# Native SIF command wait

This records a verified blocker, not an implemented protocol bridge.

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

## Next experiment

Implement a bounded packet-level channel using the actual copied IOP request bytes. Preserve
EE/IOP address separation. Deliver replies to the negotiated EE packet buffer through the
existing SIF0 guest interrupt path, so guest handlers perform their own state updates.
Bind only to existing backend routes and execute calls only when genuinely handled.
Keep unknown commands, unsupported callbacks, invalid sizes and unavailable services explicit.
Do not set the observed guest ready flag directly or fabricate successful RPC/service results.

Synthetic tests should cover malformed headers, initialization order, negotiated addresses,
reply bytes, missing service binding, call payload/results, no-interrupt completion and reset.
Then repeat a bounded native run and record its first blocker before claiming progress.
