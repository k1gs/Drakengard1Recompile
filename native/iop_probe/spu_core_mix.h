#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include "spu_volume.h"

// Original functional normal/PCM-clone mixer. Contract reference:
// PCSX2 v2.8.2 SPU2/Mixer.cpp: per-voice-sum clipping, MMIX source gates,
// FX gain, pre-master saturation, core0 master -> core1 external gain.
// Input PCM is already input-volume-scaled by the IOP bus. No host audio sink.
struct SpuCoreMix {
    using Stereo=std::array<int32_t,2>;
    struct Routed { Stereo dry{},wet{}; };
    static int32_t clip(int64_t value) { return int32_t(std::clamp<int64_t>(value,-32768,32767)); }
    static int32_t gain(int32_t sample,int32_t volume) { return int32_t((int64_t(sample)*volume)>>15); }
    static Stereo scale(const Stereo& signal,const SpuVolumeRegisters& volumes,unsigned core,unsigned reg) {
        return {gain(signal[0],volumes.gain(core,reg)),gain(signal[1],volumes.gain(core,reg+1))};
    }
    static Routed route(Stereo voiceDry,Stereo voiceWet,Stereo input,Stereo external,uint16_t mask) {
        Routed result;
        for(unsigned side=0;side<2;++side) {
            const unsigned shift=side?0:1;
            result.dry[side]=((mask&(0x400u<<shift))?clip(voiceDry[side]):0)
                +((mask&(0x40u<<shift))?input[side]:0)+((mask&(4u<<shift))?external[side]:0);
            result.wet[side]=clip(((mask&(0x100u<<shift))?clip(voiceWet[side]):0)
                +((mask&(0x10u<<shift))?input[side]:0)+((mask&(1u<<shift))?external[side]:0));
        }
        return result;
    }
    static Stereo finish(Stereo dry,Stereo effects,const SpuVolumeRegisters& volumes,unsigned core) {
        const auto wet=scale(effects,volumes,core,2);
        Stereo sum{clip(int64_t(dry[0])+wet[0]),clip(int64_t(dry[1])+wet[1])};
        return scale(sum,volumes,core,0);
    }
};
