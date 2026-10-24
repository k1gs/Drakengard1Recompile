#pragma once
#include "ipu_vlc.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace drakengard {
// H.262 tables B.12/B.13. Code bits are left aligned; no bits are consumed here.
inline IpuVlcResult decodeIpuDcSize(uint32_t bits,bool chroma) {
    constexpr unsigned lumaCodes[]{4,0,1,5,6,14,30,62,126,254,510,511};
    constexpr unsigned lumaLengths[]{3,2,2,3,3,4,5,6,7,8,9,9};
    for (unsigned size=0;size<12;++size) {
        const unsigned length=chroma?(size<3?2:size<11?size:10):lumaLengths[size];
        const unsigned code=chroma?(size<3?size:size==11?1023:(1u<<length)-2):lumaCodes[size];
        if ((bits>>(32-length))==code) return {static_cast<uint16_t>(size),length};
    }
    return {};
}
struct IpuAcCode { unsigned run{},level{},length{}; bool end{},escape{}; };

// Numeric prefix assignments from H.262 (02/2000), table B.15. These compact
// code/run/level records and arithmetic ranges are independent of emulator LUTs.
inline IpuAcCode decodeIpuIntraAc(uint32_t bits) {
    struct Code { unsigned bits,length,run,level; };
    constexpr Code shortCodes[]{
        {2,2,0,1},{2,3,1,1},{6,3,0,2},{5,5,2,1},{7,4,0,3},
        {7,5,3,1},{6,6,4,1},{6,5,1,2},{7,6,5,1},{6,7,6,1},
        {4,7,7,1},{28,5,0,4},{7,7,2,2},{5,7,8,1},{120,7,9,1},
        {29,5,0,5},{5,6,0,6},{121,7,1,3},{38,8,3,2},{122,7,10,1},
        {33,8,11,1},{37,8,12,1},{36,8,13,1},{4,6,0,7},{39,8,1,4},
        {252,8,2,3},{253,8,4,2},{4,9,5,2},{5,9,14,1},{7,9,15,1},
        {13,10,16,1},{123,7,0,8},{124,7,0,9},{35,8,0,10},{34,8,0,11},
        {32,8,1,5},{12,10,2,4},{28,12,3,3},{18,12,4,3},{30,12,6,2},
        {21,12,7,2},{17,12,8,2},{31,12,17,1},{26,12,18,1},{25,12,19,1},
        {23,12,20,1},{22,12,21,1},{250,8,0,12},{251,8,0,13},{254,8,0,14},
        {255,8,0,15},{22,13,1,6},{21,13,1,7},{20,13,2,5},{19,13,3,4},
        {18,13,5,3},{17,13,9,2},{16,13,10,2}
    };
    if ((bits>>28)==6) return {0,0,4,true,false};
    if ((bits>>26)==1) return {0,0,6,false,true};
    for (const auto& code:shortCodes)
        if ((bits>>(32-code.length))==code.bits)
            return {code.run,code.level,code.length,false,false};
    // The pinned IPU reference also accepts these ten redundant long forms
    // (shared table-zero tails). They are not additional B.15 run/level pairs.
    constexpr Code aliases[]{
        {16,12,0,11},{19,12,0,10},{20,12,2,4},{24,12,0,9},{27,12,1,5},{29,12,0,8},
        {23,13,0,15},{24,13,0,14},{25,13,0,13},{26,13,0,12}
    };
    for (const auto& code:aliases)
        if ((bits>>(32-code.length))==code.bits)
            return {code.run,code.level,code.length,false,false};
    const auto p13=bits>>19, p14=bits>>18, p15=bits>>17, p16=bits>>16;
    if (p13>=27 && p13<=31) return {53-p13,1,13};
    if (p14>=16 && p14<=31) return {0,47-p14,14};
    if (p15>=16 && p15<=24) return {0,56-p15,15};
    if (p15>=25 && p15<=31) return {1,39-p15,15};
    if (p16>=16 && p16<=19) return {1,34-p16,16};
    if (p16==20) return {6,3,16};
    if (p16>=21 && p16<=26) return {37-p16,2,16};
    if (p16>=27 && p16<=31) return {58-p16,1,16};
    return {};
}

inline unsigned ipuZigzag(unsigned index) {
    unsigned at=0;
    for (unsigned diagonal=0;diagonal<15;++diagonal) {
        const unsigned low=diagonal>7?diagonal-7:0,high=std::min(diagonal,7u);
        for (unsigned offset=0;offset<=high-low;++offset) {
            const unsigned row=(diagonal&1)?low+offset:high-offset;
            if (at++==index) return row*8+(diagonal-row);
        }
    }
    return 0;
}

inline unsigned ipuQuantizerScale(unsigned code,bool nonlinear) {
    if (!nonlinear) return code*2;
    if (code<9) return code;
    if (code<17) return 8+(code-8)*2;
    if (code<25) return 24+(code-16)*4;
    return 56+(code-24)*8;
}

// Original separable mathematical inverse DCT. Q20 cosine basis, 64-bit sums,
// one rounding at the end; no emulator butterfly/permutation implementation.
// This is a functional transform, not a claim of bit-exact PS2 IDCT rounding.
inline std::array<uint8_t,64> ipuInverseDct(const std::array<int16_t,64>& coefficients) {
    static const auto basis=[] {
        std::array<std::array<int32_t,8>,8> values{};
        for (unsigned x=0;x<8;++x) for (unsigned u=0;u<8;++u)
            values[x][u]=static_cast<int32_t>(std::llround(1048576.0*(u?1.0:std::sqrt(0.5))*
                std::cos((2*x+1)*u*std::numbers::pi/16.0)));
        return values;
    }();
    std::array<std::array<int64_t,8>,8> rows{};
    for (unsigned v=0;v<8;++v) for (unsigned x=0;x<8;++x)
        for (unsigned u=0;u<8;++u) rows[v][x]+=int64_t(coefficients[v*8+u])*basis[x][u];
    std::array<uint8_t,64> samples{};
    constexpr int64_t divisor=int64_t(1)<<42;
    for (unsigned y=0;y<8;++y) for (unsigned x=0;x<8;++x) {
        int64_t sum=0;
        for (unsigned v=0;v<8;++v) sum+=rows[v][x]*basis[y][v];
        const auto rounded=(sum+divisor/2)>=0?(sum+divisor/2)/divisor:
            -(-(sum+divisor/2)+divisor-1)/divisor;
        samples[y*8+x]=static_cast<uint8_t>(std::clamp<int64_t>(rounded,0,255));
    }
    return samples;
}
}
