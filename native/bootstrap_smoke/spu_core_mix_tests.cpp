#include "../iop_probe/spu_core_mix.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        using Mix=SpuCoreMix;
        const Mix::Stereo dry{30000,-30000},wet{12000,-12000},input{4000,-4000},external{2000,-2000};
        // Each MMIX bit selects exactly one source, bus and side.
        for(unsigned bit=0;bit<12;++bit) {
            const auto r=Mix::route(dry,wet,input,external,uint16_t(1u<<bit));
            const unsigned side=(bit&1)?0:1;
            const bool isWet=(bit/2)%2==0;
            const int32_t expected=bit<4?external[side]:bit<8?input[side]:isWet?wet[side]:dry[side];
            const auto& selected=isWet?r.wet:r.dry;
            const auto& other=isWet?r.dry:r.wet;
            if(selected[side]!=expected || selected[side^1] || other!=Mix::Stereo{})
                throw std::runtime_error("MMIX source/side gate mismatch");
        }
        const auto r=Mix::route({100000,-100000},{100000,-100000},{-32768,32767},{0,0},0xff0);
        // Voice sums saturate BEFORE input addition; wet sum saturates again.
        if(r.dry!=Mix::Stereo{-1,-1} || r.wet!=Mix::Stereo{-1,-1})
            throw std::runtime_error("Mixer saturated sources in the wrong order");
        if(Mix::gain(-1,16384)!=-1 || Mix::gain(-32768,-32768)!=32768)
            throw std::runtime_error("Signed fixed-point gain lost inversion/rounding");
        struct Memory { uint16_t read16(uint32_t) { return 0; } void write16(uint32_t,uint16_t) {} } memory;
        SpuVolumeRegisters volumes;volumes.enable();
        const auto store=[&](uint32_t at,uint16_t value) {
            auto a=volumes.stage(memory,at,at,0x29,value);volumes.complete(memory,a,false);
        };
        store(0x1f900760,0x2000);store(0x1f900762,0x2000);
        store(0x1f900764,0x4000);store(0x1f900766,0x4000);
        if(Mix::finish({30000,-30000},{20000,-20000},volumes,0)!=Mix::Stereo{16383,-16384})
            throw std::runtime_error("FX gain / pre-master saturation / master ordering differs");
        std::cout<<"PASS MMIX twelve gates, staged clipping, signed gains and master/FX ordering\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
