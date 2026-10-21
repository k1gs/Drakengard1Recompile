#pragma once
#include "core/iop_memory.h"
#include <stdexcept>

// Interrupt latch/access model. Actual device/video event owners set pending
// bits; the CPU exception path owns masked delivery. No snapshot IRQ injection.
class QuietInterrupts {
public:
    // The EE video event is the shared source; do not synthesize a second clock.
    // Pinned PCSX2 IopCounters.cpp: psxVBlankStart IRQ0, psxVBlankEnd IRQ11.
    void videoEvent(ps2x::iop::detail::IopMemory& memory,bool start) {
        memory.setInterruptStatus(memory.interruptStatus() | (start ? 1u : 1u<<11));
    }
    void reset(ps2x::iop::detail::IopMemory& memory) {
        memory.setInterruptStatus(0); memory.setInterruptMask(0); memory.setInterruptControl(0);
    }
    bool allows(uint32_t physical, uint32_t opcode, uint32_t writeValue) const {
        if (opcode != 0x23 && opcode != 0x2b) return false;
        if (physical == 0x1f801074) return true;
        if (physical == 0x1f801070) return opcode == 0x23 || writeValue != 0xffffffffu;
        return physical == 0x1f801078 && (opcode == 0x23 || writeValue <= 1);
    }
    void afterRead(ps2x::iop::detail::IopMemory& memory, uint32_t physical) {
        if (physical == 0x1f801078) memory.setInterruptControl(0); // Read-and-disable, not a plain load.
    }
    bool pending(const ps2x::iop::detail::IopMemory& memory) const {
        return memory.interruptControl() && (memory.interruptStatus() & memory.interruptMask());
    }
    void checkDelivery(const ps2x::iop::detail::IopMemory& memory, bool cpuEnabled = true) const {
        if (cpuEnabled && pending(memory))
            throw std::runtime_error("Pending IOP interrupt requires actual exception delivery");
    }
};
