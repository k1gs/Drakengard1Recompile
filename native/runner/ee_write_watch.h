#pragma once

#include "ps2_runtime.h"
#include "ee_gif_write_probe.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace drakengard {
// CPU stores and invalid-transfer diagnosis run on the serialized EE executor.
struct EeWatchedWrite {
    uint64_t low{}, high{}, before{}, after{};
    uint32_t pc{}, ra{}, sp{}, address{}, width{};
    const char* op{};
};
struct EeWriteHistory {
    std::array<EeWatchedWrite,128> writes{};
    uint64_t count{};
    bool dumped{};
};
inline EeWriteHistory& eeWriteHistory() {
    static EeWriteHistory history;
    return history;
}
inline void printEeWatchedWrite(const char* tag,const EeWatchedWrite& write) {
    std::cerr << tag << " op=" << write.op << " pc=0x" << std::hex << write.pc
              << " ra=0x" << write.ra << " sp=0x" << write.sp << " address=0x" << write.address
              << " width=" << std::dec << write.width << " lo=0x" << std::hex << write.low
              << " hi=0x" << write.high << " before=0x" << write.before
              << " after=0x" << write.after << std::dec << '\n';
}
inline void dumpEeWatchedWrites() {
    auto& history=eeWriteHistory();
    if (history.dumped || !std::getenv("DRAKENGARD_EE_WRITE_WATCH")) return;
    history.dumped=true;
    const uint64_t first=history.count>history.writes.size() ? history.count-history.writes.size() : 0;
    std::cerr << "[ee-write-watch-history] total=" << history.count
              << " retained=" << history.count-first << '\n';
    for (uint64_t i=first;i<history.count;++i)
        printEeWatchedWrite("[ee-write-watch-tail]",history.writes[i%history.writes.size()]);
}
// The launcher selects the diagnostic environment before starting the process.
inline const uint32_t eeWriteWatchSlot=[] {
    const char* text=std::getenv("DRAKENGARD_EE_WRITE_WATCH");
    if (!text || !*text) return 0xffffffffu;
    errno=0; char* end{}; const auto value=std::strtoul(text,&end,0);
    if (errno || end==text || *end || value>32u*1024u*1024u-8u || (value&7u)) return 0xffffffffu;
    return static_cast<uint32_t>(value);
}();
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
inline void recordEeWatchedWrite(uint8_t* rdram,uint32_t address,uint32_t width,
                                 uint64_t low,uint64_t high,const char* op,
                                 const R5900Context* context,uint32_t watched) {
    if (!rdram || !width) return;
    const uint64_t writeEnd=uint64_t(address)+width;
    uint8_t oldBytes[16]{}; uint8_t newBytes[16]{};
    if (width>sizeof(oldBytes) || address+width>32u*1024u*1024u) return;
    std::memcpy(oldBytes,rdram+address,width);
    std::memcpy(newBytes,&low,sizeof(low));
    if (width>8) std::memcpy(newBytes+8,&high,sizeof(high));
    static const uint64_t armValue=[] {
        const char* text=std::getenv("DRAKENGARD_EE_WRITE_WATCH_ARM_VALUE");
        if (!text || !*text) return UINT64_MAX;
        errno=0; char* end{}; const auto value=std::strtoull(text,&end,0);
        return errno || end==text || *end || value>UINT32_MAX ? UINT64_MAX : value;
    }();
    static bool armed=armValue==UINT64_MAX;
    if (!armed && address<=watched && writeEnd>=uint64_t(watched)+4u) {
        uint32_t incoming{};
        std::memcpy(&incoming,newBytes+(watched-address),4);
        armed=incoming==armValue;
    }
    if (!armed) return;
    const uint32_t overlapBegin=(std::max)(address,watched);
    const uint32_t overlapEnd=(std::min<uint64_t>)(writeEnd,uint64_t(watched)+8u);
    if (std::memcmp(oldBytes+(overlapBegin-address),newBytes+(overlapBegin-address),overlapEnd-overlapBegin)==0) return;
    auto& history=eeWriteHistory();
    auto& write=history.writes[history.count%history.writes.size()];
    write={low,high,0,0,context?context->pc:0,context?getRegU32(context,31):0,
           context?getRegU32(context,29):0,address,width,op};
    std::memcpy(&write.before,rdram+watched,sizeof(write.before));
    write.after=write.before;
    std::memcpy(reinterpret_cast<uint8_t*>(&write.after)+(overlapBegin-watched),
                newBytes+(overlapBegin-address),overlapEnd-overlapBegin);
    if (history.count++<history.writes.size()) printEeWatchedWrite("[ee-write-watch]",write);
}
// Keep the disabled/non-overlap path small enough to inline at each RAM store.
#if defined(_MSC_VER)
__forceinline
#else
__attribute__((always_inline)) inline
#endif
void traceEeWatchedWrite(uint8_t* rdram,uint32_t address,uint32_t width,
                                uint64_t low,uint64_t high,const char* op,
                                const R5900Context* context) {
    const uint32_t watched=eeWriteWatchSlot;
    address &= 32u*1024u*1024u-1u; // Same physical RAM aliasing as FAST_WRITE.
    auto& gifProbe=eeGifWriteProbe();
    if (gifProbe.matches(address,width))
        gifProbe.observe(address,width,low,high,context?context->pc:0,
            context?getRegU32(context,31):0,context?getRegU32(context,29):0);
    if (watched==0xffffffffu || uint64_t(address)+width<=watched || uint64_t(watched)+8u<=address) return;
    recordEeWatchedWrite(rdram,address,width,low,high,op,context,watched);
}
}
