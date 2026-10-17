#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

// Data-only aperture. The caller verifies the file hash before constructing this
// owned copy. Instruction fetch still passes the independent owned-RAM guard.
class ReadOnlyRom {
public:
    explicit ReadOnlyRom(std::vector<uint8_t> bytes) : m_bytes(std::move(bytes)) {
        if (m_bytes.size() != 0x400000) throw std::runtime_error("Boot ROM must contain exactly 4 MiB");
    }
    bool allows(uint32_t address, uint32_t opcode) const {
        // Only physical, KSEG0 and KSEG1 aliases. KSEG3 includes BIU control,
        // which must never become a ROM alias through the physical-address mask.
        const uint32_t segment = address >> 29;
        if (segment != 0 && segment != 4 && segment != 5) return false;
        const uint32_t physical = address & 0x1fffffffu;
        if (physical < 0x1fc00000 || physical >= 0x20000000) return false;
        return opcode >= 0x20 && opcode <= 0x26;
    }
    template<class Memory>
    std::pair<uint32_t, uint32_t> stage(Memory& memory, uint32_t address, uint32_t opcode) const {
        if (!allows(address, opcode)) throw std::runtime_error("Unsupported boot ROM data access");
        const uint32_t aligned = address & ~3u;
        const uint32_t offset = (aligned & 0x1fffffffu) - 0x1fc00000;
        const uint32_t value = uint32_t(m_bytes[offset]) | uint32_t(m_bytes[offset + 1]) << 8 |
            uint32_t(m_bytes[offset + 2]) << 16 | uint32_t(m_bytes[offset + 3]) << 24;
        const uint32_t previous = memory.read32(aligned);
        memory.write32(aligned, value);
        return {aligned, previous};
    }
private:
    const std::vector<uint8_t> m_bytes;
};
