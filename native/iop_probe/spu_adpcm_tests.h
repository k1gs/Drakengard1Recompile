#pragma once
#include "spu_adpcm.h"
#include "spu_gaussian.h"

namespace iop_tests {
inline void nonzeroSpuAdpcm() {
    const auto check=[](bool condition,const char* message) { if (!condition) throw std::runtime_error(message); };
    // Independent integer golden vectors: low nibble first, signed residual,
    // predictor rounding and saturated history (not a second decoder implementation).
    constexpr std::array<std::array<int16_t,8>,5> golden{{
        {1792,256,-2048,-512,1792,1792,-2048,-2048},
        {1792,1936,-233,-730,1108,2831,606,-1480},
        {1792,3476,2742,1591,2423,4853,4704,2461},
        {1792,3000,1006,-1550,-1446,910,588,-1930},
        {1792,3672,3272,2283,3076,5515,5581,3420}
    }};
    for (unsigned predictor=0;predictor<5;++predictor) {
        SpuAdpcmDecoder decoder;
        const auto first=decoder.decodeWord(uint16_t(predictor*16+4),0xe817);
        const auto second=decoder.decodeWord(uint16_t(predictor*16+4),0x8877);
        check(first && second,"ADPCM golden header rejected");
        for (unsigned i=0;i<4;++i) {
            check((*first)[i]==golden[predictor][i],"ADPCM predictor first-word golden differs");
            check((*second)[i]==golden[predictor][4+i],"ADPCM predictor history golden differs");
        }
    }
    SpuAdpcmDecoder decoder;
    std::array<uint16_t,8> block{}; block.fill(0x7777); block[0]=0x40;
    auto pcm=decoder.decode(block);
    check(pcm && (*pcm)[0]==28672 && (*pcm)[1]==32767 && (*pcm)[27]==32767,
          "ADPCM clipping did not feed saturated predictor history");
    check(decoder.history().prev1==32767 && decoder.history().prev2==32767,"ADPCM final history differs");
    const auto before=decoder.history(); block[0]=0x50;
    check(!decoder.decode(block) && decoder.history().prev1==before.prev1,"Invalid predictor changed history");
    block[0]=0x0800; check(!decoder.decode(block),"Reserved ADPCM flags accepted");
    decoder.reset(); block.fill(0xe817); block[0]=0x14;
    pcm=decoder.decode(block);
    check(pcm && (*pcm)[24]==-579 && (*pcm)[27]==-2684,"Cross-block history fixture A differs");
    block.fill(0x8877); block[0]=0x14; pcm=decoder.decode(block);
    check(pcm && (*pcm)[0]==-724 && (*pcm)[1]==1113 && (*pcm)[27]==-3807,
          "Cross-block predictor history reset or changed");
    decoder.reset(); const auto tiny=decoder.decodeWord(15,0x8877);
    check(tiny && *tiny==std::array<int16_t,4>{0,0,-1,-1},"ADPCM shifts above 12 lost arithmetic sign");

    const auto gaussian=spuGaussianTable();
    check(gaussian[0]==std::array<int16_t,4>{4807,22963,4871,-1} &&
          gaussian[128]==std::array<int16_t,4>{412,15855,15948,424} &&
          gaussian[255]==std::array<int16_t,4>{-1,4871,22963,4807},"SPU Gaussian phase coefficients differ");
    uint32_t tableHash=2166136261u;
    for (const auto& row:gaussian) for (const auto value:row)
        for (uint8_t byte:{uint8_t(value),uint8_t(uint16_t(value)>>8)}) tableHash=(tableHash^byte)*16777619u;
    check(tableHash==0x3fd6eb7du,"Generated Gaussian table differs from pinned numeric reference");

    using ps2x::iop::detail::IopMemory;
    IopMemory memory; SpuRamWritePort ram; ram.enable();
    SpuIdleVoices voices; voices.enable(0,&ram);
    const auto store=[&](auto& device,uint32_t address,uint16_t value) {
        auto access=device.stage(memory,address,address,0x29,value); device.complete(memory,access,false);
    };
    store(ram,0x1f9001a8,0); store(ram,0x1f9001aa,0x2800);
    store(ram,0x1f9001ac,0x0704);
    for (unsigned i=0;i<7;++i) store(ram,0x1f9001ac,0x1111);
    store(voices,0x1f900004,0x1000); store(voices,0x1f9001a0,1);
    voices.advanceTo(768);
    check(voices.pcm(0,0)==253 && voices.nonzeroSampleCount()==1,"Nonzero ADPCM never reached voice PCM");
    // An already latched block/FIFO stays stable. The next loop fetch sees
    // actual updated SPU RAM rather than a stale address-only PCM cache.
    store(ram,0x1f9001aa,0x2801);
    for (unsigned i=0;i<7;++i) store(ram,0x1f9001ac,0x2222);
    voices.advanceTo(768*2); check(voices.pcm(0,0)==253,"RAM write rewrote already fetched PCM");
    voices.advanceTo(768*29); check(voices.pcm(0,0)==508,"Next loop reused stale decoded PCM after RAM write");
    // At pitch zero, prefetch fills exactly 16 samples and then holds NAX.
    store(voices,0x1f900004,0); voices.advanceTo(768*34);
    const auto nax=voices.read(0x1f9001ca);
    voices.advanceTo(768*50);
    check(voices.read(0x1f9001ca)==nax,"Pitch-zero FIFO did not stop prefetching when full");

    SpuReverbSetup effects; effects.enable(&ram); effects.setEffects(0,true);
    effects.observeInput(0,0,0);
    bool refused=false;
    try { effects.observeInput(0,1,0); } catch (const std::runtime_error&) { refused=true; }
    check(refused,"Nonzero wet PCM escaped the zero-input reverb contract");
    effects.setEffects(0,false); effects.observeInput(0,1,1);
    refused=false;
    try { effects.setEffects(0,true); } catch (const std::runtime_error&) { refused=true; }
    check(refused,"FX re-enable silently discarded nonzero input history");
}
}
