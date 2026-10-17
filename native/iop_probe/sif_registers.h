#pragma once
#include <cstdint>
#include <stdexcept>

// Isolated IOP view of a quiescent SIF bus, following the pinned functional
// register contract. No EE command, DMA completion or BOOTEND is synthesized.
class SifRegisters {
public:
    bool allows(uint32_t address, uint32_t opcode) const {
        if (opcode != 0x23 && opcode != 0x2b) return false;
        switch (address) {
        case 0x1d000000: case 0x1d000010: case 0x1d000020:
        case 0x1d000030: case 0x1d000040: case 0x1d000060: return true;
        default: return false;
        }
    }
    uint32_t read(uint32_t address) const {
        if (!allows(address, 0x23)) throw std::runtime_error("Unsupported SIF register read");
        switch (address & 0xff) {
        case 0: return m_eeAddress;
        case 0x10: return m_iopAddress;
        case 0x20: return m_eeFlags;
        case 0x30: return m_iopFlags;
        case 0x40: return m_control | 0xf0000002u;
        default: return 0; // Explicit unused F260 view in pinned reference.
        }
    }
    void write(uint32_t address, uint32_t value) {
        if (!allows(address, 0x2b)) throw std::runtime_error("Unsupported SIF register write");
        switch (address & 0xff) {
        case 0: break; // EE-owned address: IOP writes have no effect.
        case 0x10: m_iopAddress = value; break;
        case 0x20: m_eeFlags &= ~value; break;
        case 0x30: m_iopFlags |= value; break;
        case 0x40: {
            const auto mask = value & 0xf0u;
            if (value & 0xa0u) m_control = (m_control & ~0xf000u) | 0x2000u;
            if (m_control & mask) m_control &= ~mask;
            else m_control |= mask;
            break;
        }
        case 0x60: break;
        }
    }
private:
    uint32_t m_eeAddress{}, m_iopAddress{}, m_eeFlags{}, m_iopFlags{}, m_control{};
};
