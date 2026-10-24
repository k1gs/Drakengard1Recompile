#pragma once
#include "bare_call.h"
#include <bit>
#include <source_location>

namespace iop_tests {
namespace adma_test {
inline void check(bool value,const std::source_location where=std::source_location::current()) {
    if (!value) throw std::runtime_error("ADMA regression line "+std::to_string(where.line()));
}
template<class Action> void refuses(Action action) {
    bool rejected=false;
    try { action(); } catch (const std::runtime_error&) { rejected=true; }
    check(rejected);
}
struct Stream {
    ps2x::iop::detail::IopMemory memory;
    SpuRamWritePort ram;
    SpuQuiescentControl control;
    SpuDmaWrite writer{ram,control};
    Stream() { ram.enable(); control.enable(nullptr,true); writer.bindSampleClock(); }
    template<class Device> void store(Device& device,uint32_t reg,uint16_t value) {
        const auto access=device.stage(memory,reg,reg,0x29,value);
        memory.write16(reg,value); device.complete(memory,access,false);
    }
    void configure(unsigned core,unsigned halfwords,uint32_t source=0x10000,bool irq=true) {
        const uint32_t regs=0x1f900000u+core*0x400u,base=core?0x1f801500u:0x1f8010c0u;
        store(control,regs+0x19a,0xc000); store(control,regs+0x1b0,uint16_t(1u<<core));
        store(ram,regs+0x1a8,0); store(ram,regs+0x1aa,0x3456);
        memory.write32(base,source); memory.write32(base+4,((halfwords/32)<<16)|16u);
        memory.write32(0x1f801578,1);
        memory.write32(core?0x1f801570:0x1f8010f0,core?8:0x80000);
        memory.write32(core?0x1f801574:0x1f8010f4,irq?(0x800000u|(1u<<(core?16:20))):0);
        memory.setInterruptStatus(0);
    }
    static uint16_t value(unsigned index,unsigned core) {
        const unsigned block=index/512,offset=index%256;
        if (index==0) return 0x8000;
        if (index==256) return 0x7fff;
        return uint16_t((index%512<256)?int(1000+core*100+block*17+offset):
                                                    -int(2000+core*100+block*19+offset));
    }
    void source(unsigned core,unsigned count,uint32_t address=0x10000) {
        for (unsigned i=0;i<count;++i) memory.write16(address+i*2,value(i,core));
    }
    void start(unsigned core) {
        const uint32_t reg=core?0x1f801508:0x1f8010c8;
        writer.validateStart(memory,reg,0x01000201); memory.write32(reg,0x01000201); writer.pump(memory);
    }
};
inline void transport() {
    for (unsigned core=0;core<2;++core) {
        Stream s; s.configure(core,6144); s.source(core,6144);
        const uint32_t base=core?0x1f801500:0x1f8010c0,ring=0x2000+core*0x400;
        // TimeUpdate has already consumed sample0 before the DMA store retires.
        check(s.writer.readInput(s.memory,core,0)==std::array<int32_t,2>{});
        check(s.control.read(0x1f9001b0+core*0x400)==uint16_t(0xfffcu|(1u<<core)));
        s.memory.write32(0x1f801578,0); s.start(core);
        check(s.ram.zeroRange(ring,ring+0x3ff) && s.memory.read32(base)==0x10000);
        s.memory.write32(0x1f801578,1); s.writer.pump(s.memory);
        check(s.control.read(0x1f9001b0+core*0x400)==uint16_t(1u<<core));
        for (unsigned i=0;i<256;++i) {
            check(s.ram.peek(ring+0x100+i)==Stream::value(i,core));
            check(s.ram.peek(ring+0x300+i)==Stream::value(256+i,core));
        }
        check(s.ram.zeroRange(ring,ring+0xff) && s.ram.zeroRange(ring+0x200,ring+0x2ff));
        check(s.control.read(0x1f900344+core*0x400)==0 && !s.memory.interruptStatus());
        for (unsigned i=0;i<1000;++i) s.writer.pump(s.memory);
        check(s.memory.read32(base)==0x10000 && (s.memory.read32(base+8)&0x01000000));
        check(!s.control.allows(0x1f9001b0+core*0x400,0x29,0));
        refuses([&] { s.writer.validateStart(s.memory,base+8,0x01000201); });
        for (uint64_t sample=1;sample<=2819;++sample) {
            const auto pcm=s.writer.readInput(s.memory,core,sample);
            if (sample<256) check(pcm==std::array<int32_t,2>{});
            else {
                const unsigned block=unsigned(sample/256)-1,at=unsigned(sample%256);
                check(pcm[0]==std::bit_cast<int16_t>(Stream::value(block*512+at,core)));
                check(pcm[1]==std::bit_cast<int16_t>(Stream::value(block*512+256+at,core)));
            }
            if (sample==1) check(s.memory.read32(base)==0x10180);
            if (sample==2) check(s.memory.read32(base)==0x10300);
            if (sample==3) check(s.memory.read32(base)==0x10400);
            if (sample<2819) check((s.memory.read32(base+8)&0x01000000) && !s.memory.interruptStatus());
        }
        check(s.memory.read32(base)==0x13000 && s.memory.read32(base+8)==0x201);
        check(s.memory.interruptStatus()==8 && (s.memory.read32(core?0x1f801574:0x1f8010f4)&
                                               (core?0x01000000u:0x10000000u)));
        check(s.memory.read32(base+4)==0xc00010 && s.control.read(0x1f900344+core*0x400)==0);
        check(s.ram.read(0x1f9001aa+core*0x400)==0x3456 && s.ram.peek(ring-1)==0);
        for (unsigned i=0;i<6144;++i) check(s.memory.read16(0x10000+i*2)==Stream::value(i,core));
        // A following request cannot overwrite a full ring half before the consumer boundary.
        for (uint64_t sample=2820;sample<=2944;++sample) s.writer.readInput(s.memory,core,sample);
        check(s.control.read(0x1f9001b0+core*0x400)==uint16_t(0xfffcu|(1u<<core)));
        s.memory.setInterruptStatus(0); s.memory.write32(core?0x1f801574:0x1f8010f4,0);
        s.memory.write32(base,0x18000); s.memory.write32(base+4,0x00100010);
        s.source(core,512,0x18000); s.start(core);
        check(s.memory.read32(base)==0x18000 && (s.memory.read32(base+8)&0x01000000));
        for (uint64_t sample=2945;sample<=3075;++sample) {
            s.writer.readInput(s.memory,core,sample);
            if (sample<3075) check(s.memory.read32(base+8)&0x01000000);
            if (sample==3072) check(s.control.read(0x1f9001b0+core*0x400)==uint16_t(1u<<core));
        }
        check(s.memory.read32(base)==0x18400 && s.memory.read32(base+8)==0x201 && !s.memory.interruptStatus());
        s.memory.setInterruptStatus(0);
        for (uint64_t sample=3076;sample<3090;++sample) s.writer.readInput(s.memory,core,sample);
        check(!s.memory.interruptStatus() && s.memory.read32(base+8)==0x201);
    }
}
inline void dmacDisarmKeepsProducer() {
    for (unsigned core=0;core<2;++core) {
        Stream s; s.configure(core,1024); s.source(core,1024);
        const uint32_t base=core?0x1f801500:0x1f8010c0, ring=0x2000+core*0x400;
        const uint32_t icr=core?0x1f801574:0x1f8010f4;
        s.writer.readInput(s.memory,core,0); s.start(core);
        for (unsigned sample=1;sample<=4;++sample) s.writer.readInput(s.memory,core,sample);
        const auto first=s.ram.peek(ring+0x100);
        s.memory.write32(base+8,0x201); s.writer.pump(s.memory);
        check(s.memory.read32(base)==0x10400 && s.ram.peek(ring+0x100)==first);
        refuses([&] { s.writer.validateStart(s.memory,base+8,0x01000201); });
        for (unsigned sample=5;sample<=259;++sample) s.writer.readInput(s.memory,core,sample);
        check(s.memory.read32(base)==0x10800 && s.memory.read32(base+8)==0x201);
        check(s.memory.read32(icr)==(0x800000u|(1u<<(core?16:20))) && !s.memory.interruptStatus());
        check(s.ram.peek(ring)==Stream::value(512,core));
        for (unsigned sample=260;sample<270;++sample) s.writer.readInput(s.memory,core,sample);
        check(!s.memory.interruptStatus());
    }
    Stream plain; plain.configure(0,1024); plain.source(0,1024);
    plain.writer.readInput(plain.memory,0,0); plain.start(0);
    plain.memory.write32(0x1f8010c8,0x200);
    refuses([&] { plain.writer.pump(plain.memory); });
}
inline void partialStereoAndBounds() {
    Stream s; s.configure(0,256); s.source(0,512);
    s.writer.readInput(s.memory,0,0); s.start(0);
    check(s.ram.peek(0x2100)==0x8000 && s.ram.zeroRange(0x2300,0x23ff));
    s.writer.readInput(s.memory,0,1);
    check(s.memory.read32(0x1f8010c0)==0x10180 && (s.memory.read32(0x1f8010c8)&0x01000000));
    s.writer.readInput(s.memory,0,2);
    check(s.memory.read32(0x1f8010c0)==0x10200 && s.memory.read32(0x1f8010c8)==0x201);
    s.memory.setInterruptStatus(0);
    s.memory.write32(0x1f8010c0,0x10200); s.memory.write32(0x1f8010c4,0x00080010); s.start(0);
    check(s.ram.peek(0x2300)==0x7fff); // fills pending right channel of the same half
    s.writer.readInput(s.memory,0,3); s.writer.readInput(s.memory,0,4);
    check(s.memory.read32(0x1f8010c0)==0x10400 && s.memory.read32(0x1f8010c8)==0x201);
    refuses([&] { s.writer.readInput(s.memory,0,4); });
    for (auto source:{0x10001u,0x1fff00u,0x200000u}) {
        Stream bad; bad.configure(0,512,source);
        refuses([&] { bad.start(0); });
        check(bad.memory.read32(0x1f8010c8)==0 && !bad.memory.interruptStatus() && bad.ram.zeroRange(0x2000,0x23ff));
    }
    Stream shortBlock; shortBlock.configure(0,128);
    refuses([&] { shortBlock.start(0); });
    check(shortBlock.ram.zeroRange(0x2000,0x23ff) && !shortBlock.memory.interruptStatus());
}
inline void producerStopAndRestart() {
    for (unsigned core=0;core<2;++core) for (unsigned at:{1u,4u}) {
        Stream s; s.configure(core,1024); s.source(core,1024);
        const uint32_t base=core?0x1f801500:0x1f8010c0,regs=0x1f900000+core*0x400;
        const uint32_t ring=0x2000+core*0x400, icr=core?0x1f801574:0x1f8010f4;
        s.writer.readInput(s.memory,core,0); s.start(core);
        check(!s.control.allows(regs+0x1b0,0x29,0)); // Cannot kill an armed DMAC.
        for (unsigned n=1;n<=at;++n) s.writer.readInput(s.memory,core,n);
        std::array<uint16_t,1024> snapshot;
        for (unsigned n=0;n<snapshot.size();++n) snapshot[n]=s.ram.peek(ring+n);
        const uint32_t madr=s.memory.read32(base),status=s.control.read(regs+0x344),mask=s.memory.read32(icr);
        s.memory.write32(base+8,0x201); s.writer.pump(s.memory);
        check(s.control.allows(regs+0x1b0,0x29,0));
        const auto faulted=s.control.stage(s.memory,regs+0x1b0,regs+0x1b0,0x29,0);
        s.memory.write16(regs+0x1b0,0); s.control.complete(s.memory,faulted,true);
        check(s.control.autoDmaEnabled(core));
        refuses([&] { s.writer.validateStart(s.memory,base+8,0x01000201); });
        s.store(s.control,regs+0x1b0,0); s.writer.pump(s.memory);
        check(s.memory.read32(base)==madr && s.memory.read32(base+4)==0x00200010);
        check(s.control.read(regs+0x344)==status && s.memory.read32(icr)==mask && !s.memory.interruptStatus());
        // Producer ownership released; original TSA/CPU RAM port is accessible.
        check(s.ram.allows(regs+0x1ac,0x29,0));
        for (unsigned n=at+1;n<=270;++n) s.writer.readInput(s.memory,core,n);
        for (unsigned n=0;n<snapshot.size();++n) check(s.ram.peek(ring+n)==snapshot[n]);
        check(s.memory.read32(base)==madr && !s.memory.interruptStatus());
        s.configure(core,512,0x18000); s.source(core,512,0x18000); s.start(core);
        for (unsigned n=271;n<=280;++n) s.writer.readInput(s.memory,core,n);
        check(s.memory.read32(base)==0x18400 && s.memory.read32(base+8)==0x201);
        check(s.memory.interruptStatus()==8);
    }
}
struct CpuStream:Stream {
    QuiescentDma dma;
    SpuIdleVoices voices;
    SpuVolumeRegisters volumes;
    drakengard::IopTimers timers;
    ps2x::iop::detail::IopCpuState cpu;
    ps2x::iop::detail::IopCpuCore processor{memory};
    uint32_t steps{};
    CpuStream() {
        dma.enableSpuWriter(writer); voices.enable(0,&ram); volumes.enable();
        memory.write32(0x1004,0x1000ffff); memory.write32(0x1008,0); cpu.pc=0x1004;
    }
    void step(SpuReverbSetup* effects=nullptr) {
        IopDataBus bus(memory,{.dma=&dma,.timers=&timers,.volumes=&volumes,.spuControl=&control,
                              .voices=&voices,.reverb=effects,.spuRam=&ram});
        const auto instruction=memory.read32(cpu.pc);
        const auto action=bus.prepare(cpu,instruction,steps++);
        check(processor.executeInstruction(cpu) && !cpu.exception);
        bus.complete(cpu,action);
    }
    void advanceTo(uint64_t samples,SpuReverbSetup* effects=nullptr) {
        while (voices.sampleCount()<samples) step(effects);
    }
};
inline void retirementBoundaryAndRouting() {
    CpuStream s; s.configure(0,1024); s.source(0,1024);
    s.store(s.volumes,0x1f90076c,0x7fff); s.store(s.volumes,0x1f90076e,0x8000);
    s.timers.advance(767);
    // Real SW on the first sample boundary: TimeUpdate must precede initial refill.
    s.memory.write32(0x1000,0xac820000); s.cpu.pc=0x1000;
    s.cpu.gpr[4]=0xbf8010c8; s.cpu.gpr[2]=0x01000201; s.step();
    check(s.voices.sampleCount()==1 && s.ram.peek(0x2100)==0x8000 && s.memory.read32(0x1f8010c0)==0x10000);
    s.advanceTo(257);
    check(s.voices.inputPcm(0)[0]==-32767 && s.voices.inputPcm(0)[1]==-32767);
    check(s.writer.inputPcm(0)==std::array<int32_t,2>{-32768,32767});

    const auto route=[](uint16_t mmix,uint16_t left,uint16_t right,bool reject) {
        CpuStream r; r.configure(0,512); r.source(0,512);
        r.store(r.voices,0x1f900198,mmix);
        r.store(r.volumes,0x1f90076c,left); r.store(r.volumes,0x1f90076e,right);
        SpuReverbSetup effects; effects.enable(&r.ram); effects.setEffects(0,true);
        r.step(&effects); // advances CPU, but not a sample boundary
        r.start(0);
        bool rejected=false;
        try { r.advanceTo(257,&effects); } catch (const std::runtime_error&) { rejected=true; }
        check(rejected==reject && r.writer.nonzeroInputFrames(0)>0);
    };
    route(0x20,0x7fff,0,true); // wet input left, independent of sound/voice gate
    route(0x10,0,0x7fff,true); // wet input right
    route(0x80,0x7fff,0,false); // dry-only PCM must not feed reverb
    route(0x30,0,0,false); // real zero input gain, not substituted silent samples
}
}
inline void spuAutoDmaStreaming() {
    adma_test::transport(); adma_test::dmacDisarmKeepsProducer();
    adma_test::partialStereoAndBounds(); adma_test::producerStopAndRestart();
    adma_test::retirementBoundaryAndRouting();
}
}
