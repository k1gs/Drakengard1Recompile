#pragma once
#include "core/iop_memory.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>

// Explicit functional reference mode: instruction fetch reads coherent RAM.
// Cache invalidate stores target empty cache lines, never backing kernel RAM.
// This is not a physical cache or cycle/timing model.
class CoherentCache {
    bool initialized{}, rebootControls{};
public:
    void initialize(ps2x::iop::detail::IopMemory& memory) {
        // Pinned PCSX2 IopMem read path returns zero for BIU and ignores its writes.
        // Keep a shadow for validating this probe's supported flush sequence.
        memory.write32(0xfffe0130u, 0);
        initialized = true;
        std::cerr << "[coherent-cache] functional mode; initial BIU shadow=0; no hardware timing claim\n";
    }
    void enableRebootControls() {
        if (!initialized || rebootControls)
            throw std::runtime_error("Reboot controls require initialized coherent memory and single activation");
        rebootControls = true;
    }
    bool rebootWrite(uint32_t address, uint32_t opcode, uint32_t value) const {
        // PCSX2 v2.8.2 IopMemReset leaves WLUT[0x1ffe] unmapped;
        // iopMemWrite32 has no handler for it. This is a restricted functional
        // reference profile, not a physical mapping/cache-register model.
        return rebootControls && opcode == 0x2b &&
            ((address == 0xfffe0140 && value == 0x1f800400) ||
             (address == 0xfffe0144 && value == 0x1f800000));
    }
    struct ControlWrite { uint32_t address, value, previous, pc; };
    ControlWrite stageControl(ps2x::iop::detail::IopMemory& memory,
                              uint32_t address, uint32_t opcode, uint32_t value, uint32_t pc) const {
        if (!rebootWrite(address, opcode, value))
            throw std::runtime_error("Unsupported reference CPU control write");
        return {address, value, memory.read32(address), pc};
    }
    void completeControl(ps2x::iop::detail::IopMemory& memory, const ControlWrite& write, bool exception) {
        // The pinned core masks KSEG3 into a generic hardware map. Restore that
        // temporary CPU-store slot even on exception: it is not a guest latch.
        memory.write32(write.address, write.previous);
        if (exception) return;
        ++controlWrites;
        if (controlWrites <= 32 || controlWrites % 256 == 0)
            std::cerr << "[reference-cpu-control] pc=0x" << std::hex << write.pc
                      << " address=0x" << write.address << " value=0x" << write.value
                      << std::dec << " count=" << controlWrites << '\n';
    }
    bool allows(uint32_t address, uint32_t opcode, uint32_t value) const {
        return rebootWrite(address, opcode, value) ||
            (address == 0xfffe0130u && (opcode == 0x23 ||
            (opcode == 0x2b && (value == 0 || value == 0xc04 || value == 0xc4 ||
                               (rebootControls && value == 0x1edd8)))));
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
    uint64_t controlWrites{};
};
