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
}
