#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

// Word storage and an armed SIF0 chain receiver. No tag has arrived yet, so STR
// stays set, QWC does not imply a completed transfer, and no IRQ is synthesized.
class EeSifDma {
    std::array<uint32_t, 8> registers{};
    static unsigned slot(uint32_t address) {
        const auto base = address & ~0xffu, offset = address & 0xffu;
        if ((base != 0x1000c000 && base != 0x1000c400) || offset > 0x30 || (offset & 15))
            throw std::runtime_error("Unreviewed EE SIF DMA register");
        return (base == 0x1000c400 ? 4 : 0) + offset / 16;
    }
public:
    static bool contains(uint32_t address) {
        return (address >= 0x1000c000 && address < 0x1000c100) ||
               (address >= 0x1000c400 && address < 0x1000c500);
    }
    uint32_t read(uint32_t address) const { return registers[slot(address)]; }
    void write(uint32_t address, uint32_t value) {
        const auto index = slot(address);
        if (index == 4 && value != 0)
            throw std::runtime_error("EE SIF1 DMA start requires an actual transfer engine");
        if (index == 0 && value != 0 && value != 0x184)
            throw std::runtime_error("Unreviewed EE SIF0 receiver mode");
        registers[index] = value;
    }
};
