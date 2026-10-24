#pragma once
#include "spu_adma_tests.h"

namespace iop_tests {
inline void spuAutoDmaReverbRouting() {
    adma_test::CpuStream stream;
    stream.configure(0,1024); stream.source(0,1024);
    SpuReverbSetup effects; effects.enable(&stream.ram); effects.enableProcessing();
    const auto pair=[&](unsigned field,uint32_t value) {
        stream.store(effects,0x1f9002e0+field*4,uint16_t(value>>16));
        stream.store(effects,0x1f9002e2+field*4,uint16_t(value));
    };
    pair(0,0xf8000); pair(23,0xfffff); pair(3,0x100); pair(4,0x200);
    pair(11,0x300); pair(12,0x400);
    pair(19,0x500); pair(20,0x600); pair(21,0x700); pair(22,0x800);
    stream.store(effects,0x1f900774,0x4000); // IIR gain
    stream.store(effects,0x1f900784,0x4000); // input left gain
    stream.store(stream.voices,0x1f900198,0x20); // wet input left
    stream.store(stream.volumes,0x1f90076c,0x7fff);
    effects.setEffects(0,true);
    stream.timers.advance(767); stream.step(&effects); // real first input tick before DMA admission
    stream.start(0); stream.advanceTo(320,&effects);
    adma_test::check(stream.writer.nonzeroInputFrames(0)>0);
    adma_test::check(stream.memory.read32(0x1f8010c8)==0x201 && stream.memory.interruptStatus()==8);
    adma_test::check(stream.memory.read32(0x1f8010c0)==0x10800);
    adma_test::check(!stream.ram.zeroRange(0xf8100,0xf81ff));
    adma_test::check(stream.ram.zeroRange(0xf8200,0xf82ff)); // right input gain remains zero
    adma_test::refuses([&] { effects.advanceSilentInputs(stream.voices.sampleCount()+1); });
}
}
