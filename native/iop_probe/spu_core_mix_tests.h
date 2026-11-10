#pragma once
#include "bare_call.h"

namespace iop_tests {
inline void spuCoreOutputRouting() {
    using ps2x::iop::detail::IopMemory;
    IopMemory memory;SpuRamWritePort ram;ram.enable();
    SpuVolumeRegisters volumes;volumes.enable();
    SpuIdleVoices voices;voices.enable(0,&ram);voices.bindMixer(volumes,ram);
    std::vector<SpuCoreMix::Stereo> emitted;
    voices.bindSampleSink([&](SpuCoreMix::Stereo frame) {emitted.push_back(frame);});
    SpuReverbSetup effects;effects.enable(&ram);effects.enableProcessing();
    const auto store=[&](auto& device,uint32_t at,uint16_t value) {
        const auto a=device.stage(memory,at,at,0x29,value);device.complete(memory,a,false);
    };
    for(auto at:{0x1f900760u,0x1f900762u,0x1f900788u,0x1f90078au}) store(volumes,at,0x2000);
    store(volumes,0x1f900790,0x4000);store(volumes,0x1f900792,0x4000);
    store(voices,0x1f900198,0xc0); // core0 input -> dry
    store(voices,0x1f900598,0xc);  // core0 output -> core1 dry
    const auto input=[](unsigned c,uint64_t) {return c?SpuCoreMix::Stereo{}:SpuCoreMix::Stereo{20000,-20000};};
    voices.advanceTo(768,&effects,input);effects.advanceSilentInputs(1);
    if(voices.mixedPcm(0)!=SpuCoreMix::Stereo{10000,-10000} ||
       voices.mixedPcm(1)!=SpuCoreMix::Stereo{2500,-2500} ||
       ram.peek(0x800)!=uint16_t(10000) || ram.peek(0xa00)!=uint16_t(-10000))
        throw std::runtime_error("Core0 master / core1 external/master or output RAM differs");
    store(voices,0x1f900598,3); // the formerly unsupported external-wet path
    voices.advanceTo(768*2,&effects,input);effects.advanceSilentInputs(2);
    if(voices.wetInput(1)!=SpuCoreMix::Stereo{5000,-5000} || voices.mixedPcm(1)!=SpuCoreMix::Stereo{})
        throw std::runtime_error("External wet input was dropped or bypassed FX gain");
    store(volumes,0x1f900760,0);store(volumes,0x1f900762,0);
    voices.advanceTo(768*3,&effects,input);effects.advanceSilentInputs(3);
    if(voices.wetInput(1)!=SpuCoreMix::Stereo{} || voices.mixedPcm(0)!=SpuCoreMix::Stereo{})
        throw std::runtime_error("Zero master did not mute the inter-core source");
    // Actual nonzero ADPCM voice -> ADSR -> per-voice gain -> dry bus -> output.
    store(ram,0x1f9001a8,0);store(ram,0x1f9001aa,0x2800);store(ram,0x1f9001ac,0x0704);
    for(unsigned i=1;i<8;++i) store(ram,0x1f9001ac,0x1111);
    store(voices,0x1f900010,0x3fff);store(voices,0x1f900012,0x3fff);store(voices,0x1f900014,0x1000);
    store(voices,0x1f900198,0xc00);store(voices,0x1f900598,0xc);
    store(volumes,0x1f900760,0x3fff);store(volumes,0x1f900762,0x3fff);
    store(voices,0x1f9001a0,2);
    voices.advanceTo(768*35,&effects);effects.advanceSilentInputs(35);
    const auto voice=voices.voiceOutput(0,1);
    const auto dry=SpuCoreMix::gain(voice,32766);
    if(!voice || ram.peek(0x400+34)!=uint16_t(voice) || ram.peek(0x1000+34)!=uint16_t(dry) ||
       voices.mixedPcm(0)[0]!=SpuCoreMix::gain(dry,32766))
        throw std::runtime_error("Decoded voice did not reach dynamic capture RAM and master output");
    const auto pair=[&](unsigned field,uint32_t value) {
        store(effects,0x1f9002e0+field*4,uint16_t(value>>16));
        store(effects,0x1f9002e2+field*4,uint16_t(value));
    };
    pair(0,0xf8000);pair(23,0xfffff);pair(1,1);pair(2,1);
    // Comb reads precede this frame's writes: consume the prior half-rate
    // integrator sample rather than the still-empty current destination.
    pair(3,0x100);pair(4,0x200);pair(5,0xff);pair(6,0x1ff);
    pair(11,0x300);pair(12,0x400);pair(19,0x500);pair(20,0x600);pair(21,0x700);pair(22,0x800);
    store(effects,0x1f900774,0x4000);store(effects,0x1f900776,0x4000);
    store(effects,0x1f900784,0x4000);store(volumes,0x1f900764,0x7fff);
    store(voices,0x1f900198,0x200);store(voices,0x1f900598,2);
    effects.setEffects(0,true);
    bool fxReachedCore1=false;
    for(unsigned sample=36;sample<=547;++sample) {
        voices.advanceTo(uint64_t(768)*sample,&effects);
        if(voices.mixedPcm(0)[0]) {
            fxReachedCore1=true;
            if(voices.wetInput(1)[0]!=SpuCoreMix::gain(voices.mixedPcm(0)[0],16384))
                throw std::runtime_error("Nonzero actual FX output failed to reach core1 external wet input");
        }
    }
    effects.advanceSilentInputs(547);
    if(!fxReachedCore1) throw std::runtime_error("Core0 FX-to-core1 fixture remained silent");
    if(emitted.size()!=547 || emitted[0]!=SpuCoreMix::Stereo{2500,-2500} || emitted[1]!=SpuCoreMix::Stereo{})
        throw std::runtime_error("Sample sink lost per-tick output order or repeated the last sample");
    if(ram.peek(0x800+34)!=uint16_t(voices.mixedPcm(0)[0]) ||
       ram.peek(0xa00+34)!=uint16_t(voices.mixedPcm(0)[1]) ||
       ram.peek(0x400+34)!=uint16_t(voices.voiceOutput(0,1)))
        throw std::runtime_error("Dynamic output capture did not wrap after512 frames");
    bool missingFxRejected=false;
    try { voices.advanceTo(768*548); }
    catch(const std::runtime_error& e) {missingFxRejected=std::string(e.what()).find("FX owner")!=std::string::npos;}
    if(!missingFxRejected) throw std::runtime_error("Missing FX owner silently discarded routed wet input");
    if(emitted.size()!=547) throw std::runtime_error("A failed SPU tick emitted an invented completed frame");
    bool duplicate=false;try {voices.bindMixer(volumes,ram);} catch(const std::runtime_error&) {duplicate=true;}
    if(!duplicate) throw std::runtime_error("Mixer admitted a second binding after sampling");
}
inline void spuCoreDecodeBeforeOutputWrite() {
    using ps2x::iop::detail::IopMemory;
    IopMemory memory;SpuRamWritePort ram;ram.enable();SpuVolumeRegisters volumes;volumes.enable();
    SpuIdleVoices voices;voices.enable(0,&ram);voices.bindMixer(volumes,ram);
    const auto store=[&](auto& device,uint32_t at,uint16_t value) {
        const auto a=device.stage(memory,at,at,0x29,value);device.complete(memory,a,false);
    };
    ram.writeWord(0x800,0x0704);
    for(unsigned i=1;i<8;++i) ram.writeWord(0x800+i,0x1111);
    store(volumes,0x1f900760,0x2000);store(volumes,0x1f900762,0x2000);
    store(voices,0x1f900198,0xc0);store(voices,0x1f900598,0);
    store(voices,0x1f9005c0,0);store(voices,0x1f9005c2,0x800);store(voices,0x1f9005a0,1);
    // If core0 output is written before core1 decodes, 10000 (0x2710) replaces
    // this valid block header and decoding fails in the very first sample.
    voices.advanceTo(768,nullptr,[](unsigned c,uint64_t) {
        return c?SpuCoreMix::Stereo{}:SpuCoreMix::Stereo{20000,20000};
    });
    if(ram.peek(0x800)!=10000) throw std::runtime_error("Core0 output buffer was not committed");
}
}
