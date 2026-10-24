#pragma once
#include <cstdint>

namespace drakengard {
struct IpuVlcResult { uint16_t value{}; unsigned length{}; };

// MPEG macroblock_address_increment prefix code. Numeric code assignments are
// expressed as their compact prefix ranges, not an imported decoder lookup table.
inline IpuVlcResult decodeIpuMacroblockIncrement(uint32_t bits,bool mpeg1) {
    for (unsigned length=1;length<=11;++length) {
        const auto prefix=bits>>(32-length);
        if (length==1 && prefix==1) return {1,length};
        if (length==3 && prefix>=2 && prefix<=3) return {uint16_t(5-prefix),length};
        if (length==4 && prefix>=2 && prefix<=3) return {uint16_t(7-prefix),length};
        if (length==5 && prefix>=2 && prefix<=3) return {uint16_t(9-prefix),length};
        if (length==7 && prefix>=6 && prefix<=7) return {uint16_t(15-prefix),length};
        if (length==8 && prefix>=6 && prefix<=11) return {uint16_t(21-prefix),length};
        if (length==10 && prefix>=18 && prefix<=23) return {uint16_t(39-prefix),length};
        if (length==11) {
            if (prefix>=24 && prefix<=35) return {uint16_t(57-prefix),length};
            if (prefix==8) return {35,length}; // IPU escape token, not increment33.
            if (prefix==15 && mpeg1) return {34,length}; // MPEG1 stuffing token.
        }
    }
    return {}; // Malformed VLC: ECD and zero DATA, with no consumed code bits.
}

// H.262 Table B.3 P-picture macroblock_type. VDEC uses frame prediction:
// forward motion adds the IPU MC_FRAME mode (0x80), not extra stream bits.
inline IpuVlcResult decodeIpuPredictiveMacroblockType(uint32_t bits) {
    if (bits&0x80000000u) return {0x8a,1}; // forward, pattern
    if ((bits>>30)==1) return {2,2};       // pattern
    if ((bits>>29)==1) return {0x88,3};    // forward
    switch (bits>>27) {
    case 3: return {1,5};                 // intra
    case 2: return {0x9a,5};              // quant, forward, pattern
    case 1: return {18,5};                // quant, pattern
    default: break;
    }
    if ((bits>>26)==1) return {17,6};      // quant, intra
    return {};
}

struct IpuMotionResult { uint32_t data{}; unsigned length{}; };

// H.262 B.4 B-picture types, expressed as prefix families. In the pinned
// IPU frame-prediction path MC_FRAME is added even to intra B macroblocks.
// The caller packs consumed length into DATA[31:16] for B-MBT only.
inline IpuVlcResult decodeIpuBidirectionalMacroblockType(uint32_t bits) {
    for (unsigned length=2;length<=4;++length) {
        const unsigned prefix=bits>>(32-length);
        if (prefix==2 || prefix==3) {
            const unsigned motion=length==2?12u:length==3?4u:8u;
            return {uint16_t(0x80u|motion|((prefix&1u)?2u:0u)),length};
        }
    }
    switch (bits>>27) {
    case 2: return {0x9e,5}; // quant, forward, backward, pattern
    case 3: return {0x81,5}; // intra + IPU frame mode
    default: break;
    }
    switch (bits>>26) {
    case 1: return {0x91,6}; // quant, intra + IPU frame mode
    case 2: return {0x96,6}; // quant, backward, pattern
    case 3: return {0x9a,6}; // quant, forward, pattern
    default: return {};
    }
}

// H.262 motion_code magnitude prefixes; sign follows each nonzero code.
// DATA packing follows the pinned IPU reference, including 32-bit negative
// sign extension and a positive length field excluding the sign bit.
inline IpuMotionResult decodeIpuMotionCode(uint32_t bits) {
    if (bits&0x80000000u) return {0x00010000u,1};
    for (unsigned length=2;length<=10;++length) {
        const unsigned prefix=bits>>(32-length);
        unsigned magnitude{};
        if (length<=4 && prefix==1) magnitude=length-1;
        else if (length==6 && prefix==3) magnitude=4;
        else if (length==7 && prefix>=3 && prefix<=5) magnitude=10-prefix;
        else if (length==9 && prefix>=9 && prefix<=11) magnitude=19-prefix;
        else if (length==10 && prefix>=12 && prefix<=17) magnitude=28-prefix;
        if (magnitude) {
            const bool negative=((bits>>(31-length))&1u)!=0;
            return {(length<<16)|(negative?0u-magnitude:magnitude),length+1};
        }
    }
    return {}; // Invalid prefix: no code consumption, DATA=0/ECD.
}
}
