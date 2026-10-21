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
    IopMemory memory; SpuRamWritePort ram; ram.enable();
    SpuIdleVoices voices; voices.enable(0,&ram);
    const auto store=[&](auto& device,uint32_t address,uint16_t value) {
        auto access=device.stage(memory,address,address,0x29,value);
        device.complete(memory,access,false);
    };
    store(ram,0x1f9001a8,0); store(ram,0x1f9001aa,0x2800);
    for (unsigned i=0;i<8;++i) store(ram,0x1f9001ac,i?0:0x0700);
    store(voices,0x1f900004,0x1000);
    store(voices,0x1f9001a0,1);
    if (voices.read(0x1f9001a0)!=1 || voices.read(0x1f90000a))
        throw std::runtime_error("KON must remain pending until a sample boundary");
    voices.advanceTo(768);
    if (voices.read(0x1f9001a0) || voices.read(0x1f90000a)!=14336 ||
        voices.read(0x1f9001ca)!=0x2802 || (voices.read(0x1f900340)&1))
        throw std::runtime_error("Silent KON did not attack/decode/clear ENDX");
    voices.advanceTo(768*32);
    if (!(voices.read(0x1f900340)&1)) throw std::runtime_error("Silent loop did not mark its end");
    store(voices,0x1f9001a4,1); voices.advanceTo(768*36);
    if (voices.read(0x1f90000a)) throw std::runtime_error("Silent KOFF envelope failed to release");
    store(ram,0x1f9001aa,0x2801); store(ram,0x1f9001ac,1);
    bool refused=false;
    try { voices.advanceTo(768*37); } catch (const std::runtime_error&) { refused=true; }
    if (!refused || voices.allows(0x1f9001a0,0x29,1))
        throw std::runtime_error("Changed audible ADPCM escaped the silent subset");
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
