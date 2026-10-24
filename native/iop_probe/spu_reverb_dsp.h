#pragma once
#include "spu_ram_write_port.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

// Functional scalar SPU2 FX graph. Delay addresses are PS2 word-addressed raw
// pairs, not the scaled PS1 compatibility registers. Histories persist while
// writes are disabled. Hardware/SIMD rounding equivalence is not claimed.
class SpuReverbDsp {
    std::array<std::array<int16_t,64>,2> down{},up{};
    unsigned cursor{};
    uint64_t nonzeroStores{};
    static int32_t floor15(int64_t value) {
        return static_cast<int32_t>(value>=0?value/32768:-((-value+32767)/32768));
    }
    static int16_t clip(int64_t value) { return static_cast<int16_t>(std::clamp<int64_t>(value,-32768,32767)); }
    static int32_t gain(int16_t coefficient,int32_t sample) {
        const int64_t product=int64_t(coefficient)*sample;
        if (product<INT32_MIN || product>INT32_MAX)
            throw std::runtime_error("SPU FX multiply exceeds reviewed signed32 domain");
        return floor15(product);
    }
    static int32_t filter(const std::array<int16_t,64>& history,unsigned position,bool doubling) {
        // Numeric 39-tap halfband response. Express symmetric nonzero pairs;
        // center coefficient is handled separately, including upsample clipping.
        constexpr std::array<int16_t,10> wing{-1,2,-10,35,-103,266,-616,1332,-2960,10246};
        int64_t sum=int64_t(history[(position+19)&63])*(doubling?32767:16384);
        for (unsigned pair=0;pair<10;++pair) {
            const int coefficient=int(wing[pair])*(doubling?2:1);
            sum+=int64_t(coefficient)*(int(history[(position+2*pair)&63])+history[(position+38-2*pair)&63]);
        }
        return clip(floor15(sum));
    }
public:
    uint64_t nonzeroStoreCount() const { return nonzeroStores; }
    std::array<int32_t,2> tick(SpuRamWritePort& ram,const std::array<uint16_t,48>& offsets,
                             const std::array<uint16_t,10>& coefficients,bool writesEnabled,
                             uint64_t sampleCycle,const std::array<int32_t,2>& input) {
        const auto pair=[&](unsigned field) { return (uint32_t(offsets[field*2])<<16)|offsets[field*2+1]; };
        const uint32_t rawStart=pair(0),rawEnd=pair(23);
        if (rawStart>=rawEnd) return {};
        const uint32_t start=rawStart&0x3fffffu,end=(rawEnd&0x3fffffu)|0xffffu;
        if (end<start) throw std::runtime_error("SPU FX wrapped workspace unsupported");
        const uint32_t phase=uint32_t(sampleCycle)>>1;
        const auto address=[&](uint32_t displacement) {
            return ((uint32_t(phase+displacement)%(end-start+1))+start)&0xfffffu;
        };
        const auto read=[&](uint32_t displacement) { return int32_t(std::bit_cast<int16_t>(ram.peek(address(displacement)))); };
        const auto coefficient=[&](unsigned field) { return std::bit_cast<int16_t>(coefficients[field]); };
        for (unsigned side=0;side<2;++side) down[side][cursor]=clip(input[side]);
        const unsigned side=unsigned(sampleCycle&1u);
        const int32_t injected=gain(coefficient(8+side),filter(down[side],(cursor+64-39)&63,false));

        // Two reflection integrators and four parallel comb taps. Snapshot all
        // reads before committing any of the four stores, including alias cases.
        std::array<uint32_t,4> destinations{pair(3+side),pair(11+side),pair(19+side),pair(21+side)};
        std::array<int32_t,4> stores{};
        const std::array<uint32_t,2> reflectionSource{pair(9+side),pair(17+(side^1u))};
        for (unsigned branch=0;branch<2;++branch) {
            const auto previous=read(destinations[branch]-1u);
            const int32_t delta=injected+gain(coefficient(5),read(reflectionSource[branch]))-previous;
            stores[branch]=previous+gain(coefficient(0),delta);
        }
        int32_t signal=0;
        for (unsigned comb=0;comb<4;++comb) {
            const unsigned field=comb<2?5+comb*2:13+(comb-2)*2;
            signal+=gain(coefficient(1+comb),read(pair(field+side)));
        }
        for (unsigned section=0;section<2;++section) {
            const int32_t delayed=read(destinations[2+section]-pair(1+section));
            stores[2+section]=signal-gain(coefficient(6+section),delayed);
            signal=delayed+gain(coefficient(6+section),stores[2+section]);
        }
        if (writesEnabled) for (unsigned field=0;field<stores.size();++field) {
            const auto sample=clip(stores[field]);
            ram.writeWord(address(destinations[field]),std::bit_cast<uint16_t>(sample));
            nonzeroStores+=sample!=0;
        }
        up[side][cursor]=clip(signal); up[side^1u][cursor]=0;
        cursor=(cursor+1)&63;
        return {filter(up[0],(cursor+64-39)&63,true),filter(up[1],(cursor+64-39)&63,true)};
    }
};
