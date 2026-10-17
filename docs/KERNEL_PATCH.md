# Europe kernel patch bring-up

This is a diagnostic translation experiment, not a working port. The original boot ELF
remains read-only and is the only input passed to the runtime loader.

## Evidence

For ELF SHA-256 `065a94591d295bb936a8dca92e722c3bd72cd05848c7274b26d572b0f782e918`:

- Native-run-004 stopped while installing syscall `0x5B`, handler `0x80076000`, PC `0x003286E8`.
- Startup code at `0x00328748` copies `0x740` bytes from `0x0051BFA0` to `0x80076000`,
  and `0x28` bytes from `0x0051C6E0` to `0x00082000` using the installed copy handler.
- Code at `0x0032A560` copies `0x330` bytes from `0x0051C770` to `0x80075000`.
- Native-run-005 found the first 64 handler bytes identical to RAM at `0x0051BFA0`.
  This confirms the prefix; it does not independently establish every copied byte.
- Native-run-007 confirmed a matching 64-byte prefix for the second patch at `0x0051C770`.
- The current private ELF wrapper places all three copies at their execution addresses.
  Its SHA-256 is `11f5a7f8dfa209f87716e0ce6c70bfc24fa7fc5076b113e911666084298d302e`.
- Explicit seeds include the observed `0x800766C0` handler and table entries
  `0x80075038`, `0x800750C8`, `0x80075108`. Ghidra recovered 16 functions and 2 440 code bytes.
  PS2Recomp translated 16 functions, 21 resumable entries, zero warnings/errors/stubs/skips/decode failures.
- Native-run-009 passed both patch installations and execution, reaching a missing application
  callback at `0x0027C630` from `0x0027FA9C`. This is progress beyond kernel bring-up,
  not proof that every privileged instruction or callback has correct semantics.

## Local preparation

Use the pinned tools and Java/XDG environment from [TOOLCHAIN.md](TOOLCHAIN.md).
Commands below require fresh outputs; preserve previous experiment directories.

```powershell
python scripts/kernel_image.py --input-complete
& tools/ghidra_12.1.4_PUBLIC/support/analyzeHeadless.bat `
  work/ghidra-projects kernel-002 -import work/kernel-image-002/kernel.elf `
  -processor r5900:LE:32:default -scriptPath scripts/ghidra `
  -postScript SeedAddressFunctions.java 0x80075000 0x800766C0 0x80075038 0x800750C8 0x80075108 `
  -postScript ExportRecompMap.java "$PWD/work/kernel-image-002/functions-002.csv"
& build/ps2recomp-tools/ps2xRecomp/Release/ps2_recomp.exe work/kernel-image-002/config.toml
```

The main native overlay now requires this kernel lift (`generated/kernel-004`) and application lift-007.
`prepare_kernel.py` checks the pinned image fingerprint, validates the map and writes only
private build units. Generated headers are renamed to prevent collisions with the main lift.
No kernel bytes or generated implementations are committed to this repository.

## Dispatch contract

The upstream dense table covers the original application code. Expanding it across the
unused interval to a high kernel virtual address would waste gigabytes. The private build
overlay adds a separate sparse lookup with 37 exact kernel entries, including resumable PCs.
It translates the real copied instructions; it does not alias kernel PCs to unrelated code.

Before each dispatched kernel entry and each direct generated function call, the original host wrapper
compares translated code ranges of that patch block against physical EE RAM. The blocks are copied
at different times, so a later block is checked when it is executed. Missing copies and instruction mutations throw a blocker.
Mutable data tables are excluded from this comparison. This deliberately leaves unrecognized
self-modifying code unsupported. Kernel semantics, privileged operations and BIOS dependencies
still require runtime validation; a successful translation report does not prove them.

## Synthetic verification

```powershell
cmake -S native/kernel_smoke -B build/kernel-smoke
cmake --build build/kernel-smoke --config Release
ctest --test-dir build/kernel-smoke -C Release --output-on-failure
```

The fixture uses original invented bytes and a minimal C++ interface. It verifies exact-address
lookup, rejection before copy, execution after a matching copy, and rejection after a one-byte
mutation, independent patch blocks, unknown regions and null RAM. It does not test the upstream ABI or real kernel behavior. Python tests separately
cover translation-image layout, address preservation, overlap/alignment rejection and opt-in/hash gates.
