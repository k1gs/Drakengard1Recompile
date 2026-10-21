#pragma once
#include "bare_call.h"

namespace iop_tests {
inline void spdifSetup() {
    using ps2x::iop::detail::IopMemory;
    IopMemory memory;
    SpdifSetup setup;
    const auto store = [&](uint32_t address, uint32_t value) {
        memory.write32(0x1000, 0x34020000 | value); // ori v0,zero,value
        memory.write32(0x1004, 0xa4820000); // sh v0,0(a0)
        memory.write32(0x1008, 0x03e00008); memory.write32(0x100c, 0);
        return bareCall(memory, 0x1000, 0, address, 0x1000, 0x1100, {.spdif = &setup});
    };
    const auto refusesStore = [&](uint32_t address, uint32_t value) {
        try { store(address, value); }
        catch (const std::runtime_error& error) {
            if (std::string(error.what()).find("hardware contract") != std::string::npos) return;
            throw;
        }
        throw std::runtime_error("Unsupported SPDIF store was executed");
    };
    refusesStore(0xbf9007c6, 0x900);
    if (setup.retiredStores()) throw std::runtime_error("Inactive SPDIF setup changed state");
    setup.enable();
    unsigned count = 0;
    for (uint32_t alias : {0u, 0x80000000u, 0xa0000000u}) {
        for (auto [address, value] : std::array<std::pair<uint32_t,uint32_t>,3>{{
            {0x1f9007c6,0x900}, {0x1f9007c8,0x200}, {0x1f9007ca,8}}}) {
            memory.write16(address - 2, 0xabcd); memory.write16(address + 2, 0x1234);
            const auto result = store(address | alias, value);
            if (!result.completed || result.instructions != 4 || memory.read16(address) != value ||
                memory.read16(address - 2) != 0xabcd || memory.read16(address + 2) != 0x1234 ||
                setup.retiredStores() != ++count || memory.interruptStatus())
                throw std::runtime_error("SPDIF setup lost CPU store/alias/width or invented an IRQ");
        }
    }
    const std::array<uint16_t,3> expected{0x900,0x200,8};
    if (setup.configuration() != expected) throw std::runtime_error("SPDIF setup did not retain actual configuration");
    memory.write16(0x1f9007c0,0x55aa);
    store(0xbf9007c0,0); ++count;
    if (memory.read16(0x1f9007c0) || setup.outputValue() || setup.retiredStores() != count || memory.interruptStatus())
        throw std::runtime_error("SPDIF output disable changed mode or invented an IRQ");
    store(0xbf9007c0,0x8000); ++count;
    if (setup.outputValue() != 0x8000 || memory.read16(0x1f9007c0) != 0x8000 ||
        setup.retiredStores() != count || memory.interruptStatus())
        throw std::runtime_error("SPDIF normal routing latch did not retain the actual store");
    store(0xbf9007c0,0xc032); ++count;
    if (!setup.pcmClone() || setup.outputValue()!=0xc032 || memory.interruptStatus())
        throw std::runtime_error("SPDIF PCM clone did not preserve its routing latch");
    for (auto [address,value] : std::array<std::pair<uint32_t,uint32_t>,6>{{
        {0xbf9007c0,0x100}, {0xbf9007c0,4}, {0xbf9007c2,4}, {0xbf90019a,0xc000}, {0xbf9007c6,0x901},
        {0xbf900760,0}}}) {
        memory.write16(address,0x55aa); refusesStore(address,value);
        if (memory.read16(address) != 0x55aa || setup.retiredStores() != count ||
            setup.configuration() != expected || setup.outputValue() != 0xc032)
            throw std::runtime_error("Rejected SPDIF operation changed state");
    }
    if (setup.read(0x1f9007c0)!=0xc032 || setup.read(0x1f9007c6)!=0x900)
        throw std::runtime_error("SPDIF latch read lost stored state");
    for (auto opcode : {0x23u,0x28u,0x2bu})
        if (setup.allows(0x1f9007c6,opcode,0x900))
            throw std::runtime_error("SPDIF setup accepted an unreviewed access width/direction");
}
}
