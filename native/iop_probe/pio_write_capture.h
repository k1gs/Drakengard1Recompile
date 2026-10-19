#pragma once
#include <cstdint>
#include <iostream>
#include <stdexcept>

// Restricted PCSX2 v2.8.2 page-2 backing-store profile. This records real writes;
// it makes no claim about physical POST/config semantics and provides no reads,
// ready flags, IRQs, device reset or external debug-board implementation.
class PioWriteCapture {
    bool enabled{};
    uint64_t count{};
    uint32_t last{}, lastWidth{};
public:
    void enable() {
        if (enabled) throw std::runtime_error("PIO capture cannot be enabled twice");
        enabled = true;
    }
    template<class Memory> bool allows(const Memory& memory, uint32_t address, uint32_t opcode, uint32_t value) const {
        return enabled && address == 0x1f802070 &&
            memory.read32(0x1f801004) == 0x1f802000 && memory.read32(0x1f80101c) == 0x000d2077 &&
            ((opcode == 0x2b && value == 0xfc) || opcode == 0x28);
    }
    template<class Memory> void afterWrite(const Memory& memory, uint32_t opcode, uint32_t value, uint32_t pc) {
        if (!allows(memory, 0x1f802070, opcode, value))
            throw std::runtime_error("PIO capture received an unreviewed write");
        const uint32_t width = opcode == 0x28 ? 1 : 4;
        const uint32_t stored = width == 1 ? memory.read8(0x1f802070) : memory.read32(0x1f802070);
        const uint32_t actual = width == 1 ? value & 0xffu : value;
        if (stored != actual) throw std::runtime_error("PIO capture requires the actual completed CPU store");
        last = actual; lastWidth = width; ++count;
        if (count <= 32 || count % 256 == 0)
            std::cerr << "[iop-pio-write] pc=0x" << std::hex << pc << " value=0x" << actual
                      << std::dec << " width=" << width << " count=" << count << '\n';
    }
    uint64_t writes() const { return count; }
    uint32_t value() const { return last; }
    uint32_t width() const { return lastWidth; }
};
