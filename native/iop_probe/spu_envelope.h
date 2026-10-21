#pragma once
#include <algorithm>
#include <cstdint>

// Functional SPU ADSR steps, reviewed against PCSX2 v2.8.2 ADSR.cpp.
struct SpuEnvelope {
    unsigned phase{}, counter{};
    int value{};
    void attack() { phase=1; counter=0; value=0; }
    void release() { if (phase) { phase=4; counter=0; } }
    void tick(uint16_t first,uint16_t second) {
        if (!phase) { value=0; return; }
        unsigned shift; int step, target=0; bool decreasing=false, exponential=false;
        switch (phase) {
        case 1: shift=(first>>10)&31; step=7-((first>>8)&3); target=32767; exponential=first&0x8000; break;
        case 2: shift=(first>>4)&15; step=-8; target=((first&15)+1)<<11; decreasing=exponential=true; break;
        case 3: shift=(second>>8)&31; step=7-((second>>6)&3); decreasing=second&0x4000;
                if (decreasing) step=~step; exponential=second&0x8000; break;
        default: shift=second&31; step=-8; decreasing=true; exponential=second&32; break;
        }
        unsigned increment=0x8000u>>std::max(0,int(shift)-11);
        int delta=step*(1<<std::max(0,11-int(shift)));
        if (exponential) {
            if (decreasing) delta=int16_t((delta*value)>>15);
            else if (value>0x6000) increment>>=2;
        }
        counter+=std::max(1u,increment);
        if (counter>=0x8000) { counter=0; value=std::clamp(value+delta,0,32767); }
        if (phase==3) { if (!value) phase=0; }
        else if (decreasing ? value<=target : value>=target) {
            if (++phase>4) { phase=0; value=0; }
        }
    }
};
