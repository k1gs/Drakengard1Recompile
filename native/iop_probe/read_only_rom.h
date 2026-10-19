#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

// The caller verifies the file hash before constructing this owned copy. Data
// mapping alone never grants execution; the runner separately gates IOPBOOT code
// on the fixed reviewed BIOS hash before explicitly enabling its code window.
class ReadOnlyRom {
public:
    explicit ReadOnlyRom(std::vector<uint8_t> bytes) : m_bytes(std::move(bytes)) {
        if (m_bytes.size() != 0x400000) throw std::runtime_error("Boot ROM must contain exactly 4 MiB");
    }
    void enableIopBootCode() {
        if (m_iopBoot) throw std::runtime_error("IOPBOOT code cannot be enabled twice");
        m_iopBoot = true;
    }
    void enableAbsentRom1() {
        if (m_absentRom1) throw std::runtime_error("Absent ROM1 profile cannot be enabled twice");
        m_absentRom1 = true;
    }
    bool ownsCode(uint32_t address) const {
        // Reviewed flat IOPBOOT code, excluding its trailing string data.
        // Only its actual uncached execution address is in this profile.
        return m_iopBoot && !(address & 3u) && address >= 0xbfc4a000 && address < 0xbfc4b120;
    }
    bool allows(uint32_t address, uint32_t opcode) const {
        // Only physical, KSEG0 and KSEG1 aliases. KSEG3 includes BIU control,
        // which must never become a ROM alias through the physical-address mask.
        const uint32_t segment = address >> 29;
        if (segment != 0 && segment != 4 && segment != 5) return false;
        const uint32_t physical = address & 0x1fffffffu;
        const bool bootRom = physical >= 0x1fc00000 && physical < 0x20000000;
        const bool absentRom1 = m_absentRom1 && physical >= 0x1e000000 && physical < 0x1e400000;
        if (!bootRom && !absentRom1) return false;
        return opcode >= 0x20 && opcode <= 0x26;
    }
    template<class Memory>
    std::pair<uint32_t, uint32_t> stage(Memory& memory, uint32_t address, uint32_t opcode) const {
        if (!allows(address, opcode)) throw std::runtime_error("Unsupported boot ROM data access");
        const uint32_t aligned = address & ~3u;
        const uint32_t physical = aligned & 0x1fffffffu;
        uint32_t value = 0;
        if (physical >= 0x1fc00000) {
            const uint32_t offset = physical - 0x1fc00000;
            value = uint32_t(m_bytes[offset]) | uint32_t(m_bytes[offset + 1]) << 8 |
                uint32_t(m_bytes[offset + 2]) << 16 | uint32_t(m_bytes[offset + 3]) << 24;
        }
        // Explicit PCSX2 v2.8.2 absent-ROM1 profile: memReset zeroes this
        // read-only allocation and CopyBIOSToMemory leaves it unchanged when
        // both ROM1 companions are absent. This is not generic open-bus data.
        const uint32_t previous = memory.read32(aligned);
        memory.write32(aligned, value);
        return {aligned, previous};
    }
private:
    const std::vector<uint8_t> m_bytes;
    bool m_iopBoot{};
    bool m_absentRom1{};
};
