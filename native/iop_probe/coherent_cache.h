#pragma once
#include "core/iop_memory.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>

// Explicit functional reference mode: instruction fetch reads coherent RAM.
// Cache invalidate stores target empty cache lines, never backing kernel RAM.
// This is not a physical cache or cycle/timing model.
class CoherentCache {
public:
    void initialize(ps2x::iop::detail::IopMemory& memory) {
        // Pinned PCSX2 IopMem read path returns zero for BIU and ignores its writes.
        // Keep a shadow for validating this probe's supported flush sequence.
        memory.write32(0xfffe0130u, 0);
        std::cerr << "[coherent-cache] functional mode; initial BIU shadow=0; no hardware timing claim\n";
    }
    bool allows(uint32_t address, uint32_t opcode, uint32_t value) const {
        return address == 0xfffe0130u && (opcode == 0x23 ||
            (opcode == 0x2b && (value == 0 || value == 0xc04 || value == 0xc4)));
    }
    void isolatedAccess(ps2x::iop::detail::IopMemory& memory, uint32_t address,
                        uint32_t opcode, uint32_t value) {
        const uint32_t control = memory.read32(0xfffe0130u);
        const uint32_t limit = control == 0xc04 ? 0x1000 : control == 0xc4 ? 0x400 : 0;
        if (opcode != 0x2b || value != 0 || address >= limit || address % 16)
            throw std::runtime_error("Unsupported isolated cache access; no RAM side effect allowed");
        ++isolatedStores;
    }
    uint64_t isolatedStores{};
};
