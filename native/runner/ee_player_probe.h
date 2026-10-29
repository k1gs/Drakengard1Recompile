#pragma once
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace drakengard {
// These diagnostic bindings are prepared only for the verified Europe ELF.
// Observe actual guest entries; internal resume labels bypass the entry hook.
inline void observeEePlayerProbe(const uint8_t* ram,R5900Context* ctx,uint32_t stage) {
    static const char* output=std::getenv("DRAKENGARD_EE_PLAYER_TRACE");
    if(!output || !*output)return;
    constexpr std::array<uint32_t,4> entries{0x1274c8,0x1776f4,0x179758,0x19f068};
    unsigned slot=0;while(slot<entries.size() && entries[slot]!=stage)++slot;
    if(slot==entries.size())throw std::runtime_error("Unknown player probe entry");
    if(slot && GPR_U32(ctx,4)!=0x3b05f0)return;
    static std::array<uint64_t,4> calls{};
    static std::array<unsigned,4> records{};
    static std::array<uint32_t,4> previousUi{0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu};
    const auto word=[&](uint32_t address) { uint32_t value;std::memcpy(&value,ram+address,4);return value; };
    ++calls[slot];
    const auto ui=word(0x4fbd30);
    const bool changed=ui!=previousUi[slot];previousUi[slot]=ui;
    // Reserve heartbeat records for the loaded scene instead of spending the
    // whole bound on startup's neutral controller polls.
    if(records[slot]>=16 || (slot==0 && !changed && (ui!=4 || calls[0]%256!=0)) ||
       (slot!=0 && !changed && calls[slot]>4 && calls[slot]%256!=0))return;
    ++records[slot];
    std::ofstream file(output,std::ios::app);
    file<<"{\"entry\":"<<stage<<",\"pc\":"<<ctx->pc<<",\"a0\":"<<GPR_U32(ctx,4)
        <<",\"ra\":"<<GPR_U32(ctx,31)<<",\"sp\":"<<GPR_U32(ctx,29)
        <<",\"ui\":"<<ui<<",\"calls_poll_update_draw_body\":[";
    for(unsigned i=0;i<calls.size();++i) { if(i)file<<',';file<<calls[i]; }
    file<<"],\"caim_vptr\":"<<word(0x3b0680)<<",\"state_6b4\":"<<word(0x3b0ca4)
        <<",\"flags_6f4\":"<<word(0x3b0ce4)<<",\"position_bits\":["
        <<word(0x3b0670)<<','<<word(0x3b0674)<<','<<word(0x3b0678)<<"]}\n";
    file.flush();if(!file)throw std::runtime_error("Cannot write bounded player entry observation");
}
}
