#pragma once
#include <array>
#include <cstdint>
#include <iostream>

inline void diagnoseEeIpuWrite(uint32_t address,uint32_t value) {
    unsigned slot;
    if (address>=0x10002000u && address<0x10002040u) slot=(address-0x10002000u)/4;
    else if (address>=0x10007000u && address<0x10007020u) slot=16+(address-0x10007000u)/4;
    else if (address>=0x1000b000u && address<0x1000b080u) slot=24+(address-0x1000b000u)/4;
    else if (address>=0x1000b400u && address<0x1000b480u) slot=56+(address-0x1000b400u)/4;
    else return;
    static std::array<uint64_t,88> counts{}; // EE executor only.
    const auto count=++counts[slot];
    if (count<=16 || (count&(count-1))==0)
        std::cerr << "[ee-ipu-write] address=0x" << std::hex << address << " value=0x" << value
                  << std::dec << " count=" << count << '\n';
}
