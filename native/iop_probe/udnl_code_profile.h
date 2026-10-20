#pragma once
#include "read_only_rom.h"
#include "verified_ram_code.h"
#include "core/iop_memory.h"
#include "services/iop_module_loader.h"

// Called only after the runner's exact BIOS gate. Original ROM-directory/ELF
// review identifies this UDNL image and .text. The temporary loader computes
// expected relocations in its own memory, never the running guest's memory.
inline VerifiedRamCode makeUdnlCodeProfile(const ReadOnlyRom& rom, uint32_t codeBegin, uint32_t codeEnd) {
    const auto image = rom.image(0xadf70, 0x1ef5);
    ps2x::iop::detail::IopMemory reference;
    const auto loaded = ps2x::iop::detail::IopModuleLoader::load(image, reference, 0x100030);
    if (!loaded || !loaded.relocationsComplete || loaded.base != 0x100030 ||
        loaded.entry != 0x10019c || loaded.size != 0x1900)
        throw std::runtime_error("Reviewed UDNL relocation/layout contract changed");
    std::vector<uint8_t> text(0x17b0);
    if (!reference.readRam(loaded.base, text.data(), text.size()))
        throw std::runtime_error("Cannot read independently relocated UDNL text");
    VerifiedRamCode code(loaded.base, loaded.entry, text, codeBegin, codeEnd);
    // Original UDNL switches its cache-control procedures into KSEG1 itself.
    // The alias can execute only after full normal-entry verification.
    code.enableUncachedAlias();
    return code;
}
