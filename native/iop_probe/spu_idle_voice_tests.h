#pragma once
#include "bare_call.h"

namespace iop_tests {
inline void stoppedSpuVoices() {
    using ps2x::iop::detail::IopMemory;
    IopMemory memory; SpuIdleVoices voices; voices.enable(0);
    const auto access=[&](uint32_t address,uint32_t value,uint32_t opcode) {
        memory.write32(0x1000,0x3c020000|(value>>16)); memory.write32(0x1004,0x34420000|(value&0xffff));
        memory.write32(0x1008,(opcode<<26)|(4u<<21)|(2u<<16)); memory.write32(0x100c,0);
        memory.write32(0x1010,0x03e00008); memory.write32(0x1014,0);
        return bareCall(memory,0x1000,0,address,0x1000,0x1100,{.voices=&voices}).value;
    };
    for (unsigned core=0;core<2;++core) for (unsigned v=0;v<24;++v) {
        const uint32_t base=0xbf900000+core*0x400+v*16;
        if (access(base+4,0,0x25)!=0x3fff) throw std::runtime_error("Stopped voice reset pitch differs");
        access(base,0x4000,0x29);
        if (access(base,0,0x25)!=0x4000 || access(base+12,0,0x21)!=0xffff8000)
            throw std::runtime_error("Stopped voice raw/current signed volume differs");
        access(base+6,0x1234,0x29); access(base+8,0x5678,0x29);
        if (access(base+6,0,0x25)!=0x1234 || access(base+8,0,0x25)!=0x5678 || access(base+10,0,0x25))
            throw std::runtime_error("Stopped ADSR setup changed an envelope");
        const uint32_t addr=0xbf9001c0+core*0x400+v*12;
        access(addr,0xffff,0x29); access(addr+2,0xffff,0x29);
        access(addr+8,0xffff,0x29); access(addr+10,0xffff,0x29);
        if (access(addr,0,0x25)!=15 || access(addr+2,0,0x25)!=0xfff8 || access(addr+10,0,0x25)!=0xfff9)
            throw std::runtime_error("Stopped voice address masking/alignment failed");
    }
    access(0xbf9001a4,0xffff,0x29); access(0xbf9001a6,0xffff,0x29);
    voices.advanceTo(767);
    if (voices.pendingKeyOff(0)!=0xffffffff || voices.stoppedReleaseCount())
        throw std::runtime_error("KOFF was consumed before its sample boundary");
    voices.advanceTo(768);
    if (voices.pendingKeyOff(0) || voices.stoppedReleaseCount()!=24 || voices.sampleCount()!=1)
        throw std::runtime_error("KOFF did not release exactly 24 stopped voices at its boundary");
    access(0xbf9005a4,1,0x29); voices.advanceTo(1535);
    if (!voices.pendingKeyOff(1)) throw std::runtime_error("Core1 KOFF lost its deferred mask");
    voices.advanceTo(1536);
    if (voices.pendingKeyOff(1) || voices.stoppedReleaseCount()!=25 || memory.interruptStatus())
        throw std::runtime_error("Stopped voice update invented an IRQ or lost core1 release");
    access(0xbf900340,0xffff,0x29); access(0xbf900342,0xffff,0x29);
    if (access(0xbf900340,0,0x25) || access(0xbf900342,0,0x25)) throw std::runtime_error("ENDX halfword clear failed");
    for (auto [address,value] : std::array<std::pair<uint32_t,uint32_t>,3>{{
        {0xbf9001a0,1},{0xbf900000,0x8000},{0xbf90000a,1}}}) {
        bool refused=false;
        try { access(address,value,0x29); } catch (const std::runtime_error&) { refused=true; }
        if (!refused) throw std::runtime_error("Active voice operation escaped the stopped subset");
    }
}
inline void silentSpuVoices() {
    using ps2x::iop::detail::IopMemory;
    const auto store=[](auto& device,IopMemory& memory,uint32_t address,uint16_t value) {
        auto access=device.stage(memory,address,address,0x29,value);
        device.complete(memory,access,false);
    };
    const auto ramBlock=[&](SpuRamWritePort& ram,IopMemory& memory,unsigned core,
                            uint32_t base,uint16_t header) {
        const auto off=core*0x400u;
        store(ram,memory,0x1f9001a8+off,uint16_t(base>>16));
        store(ram,memory,0x1f9001aa+off,uint16_t(base));
        store(ram,memory,0x1f9001ac+off,header);
        for (unsigned n=1;n<8;++n) store(ram,memory,0x1f9001ac+off,0);
    };
    const auto voiceAddress=[](unsigned core,unsigned voice,unsigned field) {
        return 0x1f900000u+core*0x400u+0x1c0u+voice*12u+field*2u;
    };
    const auto coreRegister=[](unsigned core,unsigned reg) {
        return 0x1f900000u+core*0x400u+reg;
    };

    // A real core-1/voice-23 stream starts with header 0000.  It advances
    // through the following block rather than being treated as a fabricated
    // one-block loop, and an ordinary boundary does not set ENDX.
    {
        IopMemory memory; SpuRamWritePort ram; ram.enable();
        SpuIdleVoices voices; voices.enable(0,&ram);
        ramBlock(ram,memory,1,0x2808,0);
        const auto pitch=0x1f900000u+0x400u+23u*16u+4u;
        store(voices,memory,voiceAddress(1,23,1),0x2808);
        store(voices,memory,pitch,0x1000);
        const auto prekeyNax=voices.read(voiceAddress(1,23,5));
        store(voices,memory,coreRegister(1,0x1a2),0x0080);
        if (!voices.read(coreRegister(1,0x1a2)) || voices.read(voiceAddress(1,23,5))!=prekeyNax)
            throw std::runtime_error("Core-1 voice-23 zero stream retired key-on early");
        voices.advanceTo(768);
        if (voices.read(coreRegister(1,0x1a2)) || voices.read(voiceAddress(1,23,5))!=0x280a ||
            (voices.read(coreRegister(1,0x342))&0x80))
            throw std::runtime_error("Header-0000 zero stream did not advance normally");
        voices.advanceTo(768*12);
        if (voices.read(voiceAddress(1,23,5))!=0x280f ||
            (voices.read(coreRegister(1,0x342))&0x80))
            throw std::runtime_error("Ordinary zero stream advanced its block incorrectly");
        voices.advanceTo(768*13);
        if (voices.read(voiceAddress(1,23,5))!=0x2811 ||
            (voices.read(coreRegister(1,0x342))&0x80))
            throw std::runtime_error("Ordinary zero block fabricated a loop or ENDX");
        voices.advanceTo(768*17);
        if (voices.read(voiceAddress(1,23,5))!=0x2812)
            throw std::runtime_error("Ordinary zero stream stopped advancing");
    }

    // LOOP/START records LSA, LOOP/END|REPEAT returns to it, and an explicit
    // LSA write after KON wins over the automatic loop-start address.
    {
        IopMemory memory; SpuRamWritePort ram; ram.enable();
        SpuIdleVoices voices; voices.enable(0,&ram);
        ramBlock(ram,memory,0,0x2800,0x0400);
        ramBlock(ram,memory,0,0x2808,0x0300);
        ramBlock(ram,memory,0,0x2900,0);
        const auto pitch=0x1f900000u+23u*16u+4u;
        store(voices,memory,pitch,0x1000);
        store(voices,memory,0x1f9001a2,0x0080);
        voices.advanceTo(768);
        if (voices.read(voiceAddress(0,23,3))!=0x2800)
            throw std::runtime_error("Zero stream did not record automatic loop start");
        store(voices,memory,voiceAddress(0,23,3),0x2900);
        voices.advanceTo(768*40);
        if (voices.read(0x1f900342)&0x80)
            throw std::runtime_error("Zero stream loop ended too early");
        voices.advanceTo(768*41);
        if (!(voices.read(0x1f900342)&0x80) ||
            voices.read(voiceAddress(0,23,5))!=0x2901 ||
            voices.read(voiceAddress(0,23,3))!=0x2900)
            throw std::runtime_error("Zero stream loop/repeat or manual LSA override failed");
        voices.advanceTo(768*45);
        if (voices.read(voiceAddress(0,23,5))!=0x2902)
            throw std::runtime_error("Repeated zero stream stopped traversing");
    }

    // LOOP/END without LOOP/REPEAT sets ENDX and stops ADSR at the same
    // boundary; it must not be silently converted into a looping fixture.
    {
        IopMemory memory; SpuRamWritePort ram; ram.enable();
        SpuIdleVoices voices; voices.enable(0,&ram);
        ramBlock(ram,memory,0,0x2800,0x0400);
        ramBlock(ram,memory,0,0x2808,0x0100);
        store(voices,memory,0x1f900004,0x1000);
        store(voices,memory,0x1f9001a0,1);
        voices.advanceTo(768);
        if (!voices.read(0x1f90000a))
            throw std::runtime_error("Non-repeating zero endpoint stopped ADSR early");
        voices.advanceTo(768*40);
        if ((voices.read(0x1f900340)&1) || voices.read(0x1f90000a)==0)
            throw std::runtime_error("Non-repeating zero endpoint stopped before its boundary");
        voices.advanceTo(768*41);
        if (!(voices.read(0x1f900340)&1) || voices.read(0x1f90000a))
            throw std::runtime_error("Non-repeating zero endpoint did not stop ADSR");
    }

    // Preserve the original all-flags zero block fixture as a real loop.
    {
        IopMemory memory; SpuRamWritePort ram; ram.enable();
        SpuIdleVoices voices; voices.enable(0,&ram);
        ramBlock(ram,memory,0,0x2800,0x0700);
        store(voices,memory,0x1f900004,0x1000);
        store(voices,memory,0x1f9001a0,1);
        voices.advanceTo(768*13);
        if (!(voices.read(0x1f900340)&1) || voices.read(0x1f9001ca)!=0x2801)
            throw std::runtime_error("Legacy zero loop fixture changed behavior");
        store(voices,memory,0x1f9001a4,1);
        voices.advanceTo(768*17);
        if (voices.read(0x1f90000a))
            throw std::runtime_error("Zero-loop key-off failed to release its envelope");
    }
    // The domain remains fail-closed for unsupported header mutations and noise.
    {
        IopMemory memory; SpuRamWritePort ram; ram.enable();
        SpuIdleVoices voices; voices.enable(0,&ram);
        ramBlock(ram,memory,0,0x2800,0);
        store(voices,memory,0x1f900004,0x1000);
        store(voices,memory,0x1f9001a0,1);
        voices.advanceTo(768);
        store(ram,memory,0x1f9001a8,0);
        store(ram,memory,0x1f9001aa,0x2800);
        store(ram,memory,0x1f9001ac,0x0800);
        bool refused=false;
        try { voices.advanceTo(768*2); } catch (const std::runtime_error&) { refused=true; }
        if (!refused) throw std::runtime_error("Reserved ADPCM flags in mutated RAM were not rejected");
    }
    {
        IopMemory memory; SpuRamWritePort ram; ram.enable();
        SpuIdleVoices voices; voices.enable(0,&ram);
        ramBlock(ram,memory,0,0x2800,0);
        ramBlock(ram,memory,0,0x2808,0);
        store(ram,memory,0x1f9001aa,0x2808);
        store(ram,memory,0x1f9001ac,0x0050);
        store(voices,memory,0x1f900004,0x1000);
        store(voices,memory,0x1f9001a0,1);
        voices.advanceTo(768*13);
        bool refused=false;
        try { voices.advanceTo(768*14); } catch (const std::runtime_error&) { refused=true; }
        if (!refused) throw std::runtime_error("Unsupported next-block predictor was not rejected");
    }
    {
        IopMemory memory; SpuRamWritePort ram; ram.enable();
        SpuIdleVoices voices; voices.enable(0,&ram);
        ramBlock(ram,memory,1,0x2800,0);
        bool refused=false;
        try {
            store(voices,memory,0x1f900586,0x0080);
            store(voices,memory,0x1f9005a2,0x0080);
        } catch (const std::runtime_error&) { refused=true; }
        if (!refused) throw std::runtime_error("Noise key-on escaped the zero stream domain");
    }

    SpuEnvelope envelope; envelope.attack(); envelope.tick(0,0);
    if (envelope.value!=14336 || envelope.phase!=1) throw std::runtime_error("ADSR attack step differs");
    envelope.release(); envelope.tick(0,0);
    if (envelope.value || envelope.phase) throw std::runtime_error("ADSR release floor differs");
}
inline void zeroInputSpuEffects() {
    using ps2x::iop::detail::IopMemory;
    IopMemory memory; SpuRamWritePort ram; ram.enable();
    SpuReverbSetup effects; effects.enable(&ram);
    SpuQuiescentControl control; control.enable(&effects);
    const auto store=[&](auto& device,uint32_t address,uint16_t value) {
        auto access=device.stage(memory,address,address,0x29,value);
        device.complete(memory,access,false);
    };
    if (!ram.zeroRange(0xe0000,0xeffff) || ram.zeroRange(0x2000,0x2fff))
        throw std::runtime_error("SPU RAM zero-domain accounting lost reset sentinels");
    for (unsigned c=0;c<2;++c) {
        for (unsigned i=0;i<10;++i) store(effects,0x1f900774+c*0x28+i*2,0x89ab+i);
        store(control,0x1f90019a+c*0x400,0xc080);
    }
    effects.advanceSilentInputs(1000);
    if (!ram.zeroRange(0xe0000,0xfffff) || control.read(0x1f90019a)!=0xc080)
        throw std::runtime_error("Zero-input reverb failed its invariant");
    store(ram,0x1f9001a8,14); store(ram,0x1f9001aa,0xfff8); store(ram,0x1f9001ac,123);
    bool refused=false;
    try { effects.advanceSilentInputs(1001); } catch (const std::runtime_error&) { refused=true; }
    if (!refused || control.allows(0x1f90019a,0x29,0xc080))
        throw std::runtime_error("Nonzero FX workspace escaped the zero-input domain");
    store(ram,0x1f9001aa,0xfff8); store(ram,0x1f9001ac,0);
    if (!ram.zeroRange(0xefff8,0xeffff) || !effects.supportsZeroInput(0))
        throw std::runtime_error("SPU RAM zero-domain counters did not recover after a real zero write");
}
}
