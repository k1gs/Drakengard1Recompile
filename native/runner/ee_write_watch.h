#pragma once

#include "ps2_runtime.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace drakengard {
inline void traceEeWatchedWrite(uint8_t* rdram,uint32_t address,uint32_t width,
                                uint64_t low,uint64_t high,const char* op,
                                const R5900Context* context) {
    static const uint32_t watched=[] {
        const char* text=std::getenv("DRAKENGARD_EE_WRITE_WATCH");
        if (!text || !*text) return 0xffffffffu;
        errno=0; char* end{}; const auto value=std::strtoul(text,&end,0);
        if (errno || end==text || *end || value>32u*1024u*1024u-8u || (value&7u)) return 0xffffffffu;
        return static_cast<uint32_t>(value);
    }();
    if (watched==0xffffffffu || !rdram || width==0) return;
    address &= 32u*1024u*1024u-1u; // Same physical RAM aliasing as FAST_WRITE.
    const uint64_t writeEnd=uint64_t(address)+width;
    if (writeEnd<=watched || uint64_t(watched)+8u<=address) return;
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
    static std::atomic<unsigned> count{};
    if (count.fetch_add(1,std::memory_order_relaxed)>=128) return;
    const uint32_t pc=context?context->pc:0;
    const uint32_t ra=context?getRegU32(context,31):0;
    const uint32_t sp=context?getRegU32(context,29):0;
    std::cerr << "[ee-write-watch] op=" << op << " pc=0x" << std::hex << pc
              << " ra=0x" << ra << " sp=0x" << sp << " address=0x" << address
              << " width=" << std::dec << width << " lo=0x" << std::hex << low
              << " hi=0x" << high << std::dec << '\n';
}
}
