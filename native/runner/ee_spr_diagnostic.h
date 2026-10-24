#pragma once
#include <array>
#include <cstdint>
#include <iostream>

// Register snapshots only: never read the device, insert map entries or clear STR.
template<class Registers>
inline void diagnoseEeSprDma(uint32_t address,uint32_t value,bool write,const Registers& registers) {
    const uint32_t base=address&0xffffff00u;
    if (base!=0x1000d000u && base!=0x1000d400u) return;
    if (!write && address!=base) return;
    const unsigned slot=(base==0x1000d400u?2u:0u)+(write?1u:0u);
    static std::array<uint64_t,4> counts{};
    const auto count=++counts[slot];
    if (count>16 && (count&(count-1))!=0) return;
    const auto get=[&](uint32_t key) {
        const auto found=registers.find(key);
        return found==registers.end()?0u:found->second;
    };
    std::cerr << "[ee-spr-" << (write?"write":"poll") << "] address=0x" << std::hex << address
              << " value=0x" << value << " chcr=0x" << get(base)
              << " madr=0x" << get(base+0x10) << " qwc=0x" << get(base+0x20)
              << " tadr=0x" << get(base+0x30) << " sadr=0x" << get(base+0x80)
              << " dctrl=0x" << get(0x1000e000) << " dpcr=0x" << get(0x1000e020)
              << " sqwc=0x" << get(0x1000e030) << " denabler=0x" << get(0x1000f520)
              << std::dec << " count=" << count << '\n';
}
