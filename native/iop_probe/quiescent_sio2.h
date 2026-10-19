#pragma once
#include <cstdint>
#include <stdexcept>

// Reviewed SIO2MAN reset control only. PCSX2 v2.8.2 SetCtrl retains 0x3BC;
// START_TRANSFER (bit 0) is clear, so no transaction or IRQ occurs. FIFO,
// command/status registers, pads, memory cards and DMA remain unsupported.
class QuiescentSio2 {
    bool enabled{}, configured{};
public:
    void enable() {
        if (enabled) throw std::runtime_error("SIO2 initialization profile cannot be enabled twice");
        enabled = true;
    }
    bool allows(uint32_t address, uint32_t opcode, uint32_t value) const {
        return enabled && address == 0x1f808268 &&
            ((opcode == 0x2b && value == 0x3bc) || (opcode == 0x23 && configured));
    }
    template<class Memory> void afterWrite(const Memory& memory) {
        if (!enabled || memory.read32(0x1f808268) != 0x3bc)
            throw std::runtime_error("SIO2 reset control requires its actual completed CPU store");
        configured = true;
    }
};
