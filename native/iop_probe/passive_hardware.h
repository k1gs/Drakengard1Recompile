#pragma once
#include "core/iop_memory.h"
#include "ssbus_config.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <span>
#include <stdexcept>

// Reviewed storage: HW_ICFG, DMA-global gate, and fixed SSBUS configuration.
// Retaining these is not a bus-cycle timing model
// or permission to access the mapped device; its ports still need their own contract.
// Never import snapshot DMA/interrupt/device state as a running device model.
class PassiveHardware {
public:
    void seed(std::span<const uint8_t> bytes, ps2x::iop::detail::IopMemory& memory) {
        const auto word = [&](size_t at) {
            return uint32_t(bytes[at]) | uint32_t(bytes[at + 1]) << 8 |
                uint32_t(bytes[at + 2]) << 16 | uint32_t(bytes[at + 3]) << 24;
        };
        if (bytes.size() < 12 || std::memcmp(bytes.data(), "D1HW001\0", 8) ||
            word(8) < 1 || word(8) > 2 || bytes.size() != 12 + word(8) * 8)
            throw std::runtime_error("Unreviewed passive hardware seed layout/address");
        bool icfg = false, gate = false;
        for (size_t index = 0; index < word(8); ++index) {
            const auto address = word(12 + index * 8), value = word(16 + index * 8);
            if (address == 0x1f801450 && !icfg) icfg = true;
            else if (address == 0x1f801578 && !gate && value <= 1) gate = true;
            else throw std::runtime_error("Unreviewed/duplicate passive hardware register or bits");
        }
        for (size_t index = 0; index < word(8); ++index)
            memory.write32(word(12 + index * 8), word(16 + index * 8));
        m_icfg = icfg; m_gate = gate;
    }
    void load(const char* path, ps2x::iop::detail::IopMemory& memory) {
        std::ifstream input(path, std::ios::binary);
        std::array<uint8_t, 29> data{};
        if (!input) throw std::runtime_error("Cannot open passive hardware seed");
        input.read(reinterpret_cast<char*>(data.data()), data.size());
        const auto count = input.gcount();
        if ((count != 20 && count != 28) || !input.eof() || input.bad())
            throw std::runtime_error("Passive hardware seed must contain exactly one or two records");
        seed(std::span(data).first(static_cast<size_t>(count)), memory);
    }
    bool allows(uint32_t physical, uint32_t opcode, uint32_t writeValue) const {
        if (opcode != 0x23 && opcode != 0x2b) return false;
        if (physical == 0x1f801450) return m_icfg;
        if (m_icfg && ssbus::allows(physical, opcode, writeValue)) return true;
        return m_gate && physical == 0x1f801578 && (opcode != 0x2b || writeValue <= 1);
    }
private:
    bool m_icfg{};
    bool m_gate{};
};
