#pragma once
#include <cstdint>
#include <stdexcept>

// Directional SIF bus, initially quiescent, following pinned functional register
// contracts. Only explicit peer writes change flags; completion is not synthesized.
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
    uint32_t readEe(uint32_t address) const {
        if (address < 0x1000f200 || address > 0x1000f240 || (address & 15))
            throw std::runtime_error("Unsupported EE SIF register read");
        if (address == 0x1000f240) return m_control | 0xf0000102u;
        return read(0x1d000000u + address - 0x1000f200u);
    }
    void writeEe(uint32_t address, uint32_t value) {
        switch (address) {
        case 0x1000f200: m_eeAddress = value; break;
        case 0x1000f220: m_eeFlags |= value; break;
        case 0x1000f230: m_iopFlags &= ~value; break;
        case 0x1000f240:
            if (value & ~0x100u) throw std::runtime_error("EE SIF control strobe requires actual IRQ/reset delivery");
            m_control = (m_control & ~0x100u) | value;
            break;
        default: throw std::runtime_error("Unsupported EE SIF register write");
        }
    }
private:
    uint32_t m_eeAddress{}, m_iopAddress{}, m_eeFlags{}, m_iopFlags{}, m_control{};
};
