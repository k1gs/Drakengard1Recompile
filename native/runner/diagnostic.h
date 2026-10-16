#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <iomanip>

// Local log evidence only. A matching prefix is a candidate source, never an alias.
inline void diagnoseMissingHandler(const uint8_t* ram, size_t ramSize, uint32_t handler) {
    constexpr size_t prefixSize = 64;
    const uint32_t physical = handler & 0x1FFFFFFFu;
    if (!ram || ramSize < prefixSize || physical > ramSize - prefixSize) return;
    const auto* prefix = ram + physical;
    const auto flags = std::cerr.flags();
    const auto fill = std::cerr.fill();
    std::cerr << "Handler prefix at physical 0x" << std::hex << physical << ':';
    bool nonzero = false;
    for (size_t i = 0; i < prefixSize; ++i) {
        nonzero |= prefix[i] != 0;
        std::cerr << ' ' << std::setw(2) << std::setfill('0') << unsigned(prefix[i]);
    }
    std::cerr << '\n';
    if (nonzero) {
        unsigned matches = 0;
        for (size_t offset = 0; offset <= ramSize - prefixSize; offset += 4) {
            if (offset == physical || std::memcmp(ram + offset, prefix, prefixSize) != 0) continue;
            std::cerr << "Handler prefix candidate source: 0x" << offset << '\n';
            if (++matches == 16) break;
        }
    }
    std::cerr.flags(flags);
    std::cerr.fill(fill);
}
