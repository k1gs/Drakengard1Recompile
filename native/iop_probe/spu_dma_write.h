#pragma once
#include "spu_ram_write_port.h"
#include "spu_quiescent_control.h"
#include <algorithm>
#include <array>
#include <bit>

// One owner for plain IOP->SPU writes and normal split-stereo PCM16 ADMA.
// ADMA refill copies real 256-halfword channel blocks into the shared input
// ring; MADR/completion follow the consumer sample clock, not instruction polls.
class SpuDmaWrite {
    struct Transfer {
        uint32_t source{},remaining{},end{},delay{},burst{},bcr{};
        bool active{},automatic{};
    };
    struct Input {
        uint32_t writePosition{0x100},pendingBytes{};
        uint64_t nextSample{},refills{},nonzeroFrames{};
        std::array<int32_t,2> pcm{};
    };
    std::array<Transfer,2> transfers{};
    std::array<Input,2> inputs{};
    SpuRamWritePort& ram;
    SpuQuiescentControl& control;
    uint64_t bytes{},completed{};
    bool sampleClockBound{};
    static uint32_t channelBase(unsigned core) { return core?0x1f801500u:0x1f8010c0u; }
    template<class Memory> uint32_t length(const Memory& memory,unsigned core,uint32_t value) const {
        const auto base=channelBase(core);
        const auto source=memory.read32(base),bcr=memory.read32(base+4);
        const uint64_t words=uint64_t(bcr>>16)*(bcr&0xffffu)*2;
        if (value!=0x01000201 || words<2 || words>0x100000 || (source&3) ||
            source>=Memory::RamSize || words*2>Memory::RamSize-source)
            throw std::runtime_error("Unsupported SPU DMA source/size/direction");
        if (control.autoDmaEnabled(core)) {
            if (!sampleClockBound) throw std::runtime_error("SPU ADMA requires a bound input sample clock");
            if (words<256 || (words&255) || !control.autoDmaReady(core))
                throw std::runtime_error("Unsupported SPU ADMA block size/core state");
        } else if (!control.dmaWriteReady(core))
            throw std::runtime_error("Unsupported plain SPU DMA core state");
        return uint32_t(words);
    }
    template<class Memory> void copyBurst(Memory& memory,unsigned core) {
        auto& transfer=transfers[core];
        transfer.burst=std::min(256u,transfer.remaining);
        for (unsigned n=0;n<transfer.burst;++n) ram.transferWord(core,memory.read16(transfer.source+n*2));
        transfer.source+=transfer.burst*2; transfer.remaining-=transfer.burst;
        transfer.delay=transfer.burst*24; bytes+=transfer.burst*2;
    }
    template<class Memory> void refill(Memory& memory,unsigned core) {
        auto& input=inputs[core]; auto& transfer=transfers[core];
        if (input.writePosition==0xffff || transfer.remaining<256) return;
        unsigned blocks=std::min(transfer.remaining/256,(input.writePosition&0x80)?1u:2u);
        control.observeAutoDmaBuffer(core,false);
        const auto first=transfer.source;
        const unsigned count=blocks;
        while (blocks--) {
            const uint32_t destination=0x2000u+core*0x400u+(input.writePosition&0x100u)+
                ((input.writePosition&0x80u)?0x200u:0u);
            for (unsigned n=0;n<256;++n) ram.writeWord(destination+n,memory.read16(transfer.source+n*2));
            transfer.source+=512; transfer.remaining-=256; input.pendingBytes+=512;
            input.writePosition+=0x80; bytes+=512;
        }
        if (!(input.writePosition&0x80)) input.writePosition=0xffff;
        const auto number=++input.refills;
        if (number<=8 || (number&(number-1))==0)
            std::cerr << "[spu-adma-refill] core=" << core << " source=0x" << std::hex << first
                      << " end=0x" << transfer.source << " write=0x" << input.writePosition
                      << std::dec << " halfwords=" << count*256 << " remaining=" << transfer.remaining
                      << " pending_bytes=" << input.pendingBytes << " count=" << number << '\n';
    }
    template<class Memory> void finish(Memory& memory,unsigned core) {
        auto& transfer=transfers[core]; const auto base=channelBase(core);
        memory.write32(base,transfer.end); memory.write32(base+8,0x00000201);
        if (transfer.automatic) control.finishAutoDma(core);
        else control.finishDmaWrite(core);
        ram.setDmaBusy(core,false); transfer.active=false;
        const uint32_t icrAddress=core?0x1f801574:0x1f8010f4;
        auto icr=memory.read32(icrAddress); const unsigned bit=core?0:4;
        if (icr&(1u<<(16+bit))) {
            icr|=1u<<(24+bit);
            if (icr&0x800000) icr|=0x80000000;
            memory.write32(icrAddress,icr); memory.setInterruptStatus(memory.interruptStatus()|8);
        }
        ++completed;
        std::cerr << (transfer.automatic?"[spu-adma-complete]":"[spu-dma-write-complete]")
                  << " core=" << core << " bytes=" << bytes << " transfers=" << completed
                  << " sample=" << inputs[core].nextSample << '\n';
    }
    template<class Memory> void validateActive(const Memory& memory,unsigned core) const {
        const auto& transfer=transfers[core]; const auto base=channelBase(core);
        if (memory.read32(base+8)!=0x01000201) {
            std::cerr << "[spu-dma-active-refusal] core=" << core
                      << " automatic=" << transfer.automatic << " chcr=0x" << std::hex
                      << memory.read32(base+8) << " madr=0x" << memory.read32(base)
                      << " bcr=0x" << memory.read32(base+4) << " source=0x" << transfer.source
                      << " end=0x" << transfer.end << std::dec
                      << " remaining_halfwords=" << transfer.remaining
                      << " pending_bytes=" << inputs[core].pendingBytes
                      << " sample=" << inputs[core].nextSample << '\n';
            throw std::runtime_error("Active SPU DMA cancellation/reconfiguration unsupported");
        }
        if (transfer.automatic && (memory.read32(base+4)!=transfer.bcr ||
            memory.read32(base)!=transfer.source-inputs[core].pendingBytes || !control.autoDmaEnabled(core)))
            throw std::runtime_error("Active SPU ADMA source/size/configuration changed");
    }
public:
    SpuDmaWrite(SpuRamWritePort& memory,SpuQuiescentControl& state):ram(memory),control(state) {}
    void bindSampleClock() {
        if (sampleClockBound) throw std::runtime_error("SPU input sample clock already bound");
        sampleClockBound=true;
    }
    static bool channel(uint32_t address) { return address==0x1f8010c8 || address==0x1f801508; }
    const std::array<int32_t,2>& inputPcm(unsigned core) const { return inputs.at(core).pcm; }
    uint64_t nonzeroInputFrames(unsigned core) const { return inputs.at(core).nonzeroFrames; }
    template<class Memory> void validateStart(const Memory& memory,uint32_t address,uint32_t value) const {
        const unsigned core=address==0x1f801508;
        if (transfers[core].active || (memory.read32(address)&0x01000000))
            throw std::runtime_error("Active SPU DMA rearm unsupported");
        length(memory,core,value);
    }
    template<class Memory> void pump(Memory& memory) {
        for (unsigned core=0;core<2;++core) {
            auto& transfer=transfers[core]; const auto base=channelBase(core);
            const auto chcr=memory.read32(base+8);
            if (!transfer.active) {
                if (!(chcr&0x01000000)) continue;
                if (memory.read32(0x1f801578)!=1 || !(memory.read32(core?0x1f801570:0x1f8010f0)&(core?8u:0x80000u))) continue;
                const auto words=length(memory,core,chcr);
                transfer={memory.read32(base),words,memory.read32(base)+words*2,0,0,memory.read32(base+4),
                          true,control.autoDmaEnabled(core)};
                if (transfer.automatic) {
                    if (inputs[core].pendingBytes) throw std::runtime_error("SPU ADMA restarted before prior MADR completion");
                    control.beginAutoDma(core); ram.setDmaBusy(core,true); refill(memory,core);
                } else {
                    control.beginDmaWrite(core); ram.setDmaBusy(core,true); copyBurst(memory,core);
                }
                continue;
            }
            validateActive(memory,core);
            if (transfer.automatic) continue; // Only consumer sample ticks advance ADMA.
            if (--transfer.delay) continue;
            memory.write32(base,transfer.source);
            if (transfer.remaining) { copyBurst(memory,core); continue; }
            finish(memory,core);
        }
    }
    // Called exactly once per core per existing SPU sample tick. The sample is
    // read before refilling the opposite half, matching the producer/consumer edge.
    template<class Memory> std::array<int32_t,2> readInput(Memory& memory,unsigned core,uint64_t sample) {
        if (!sampleClockBound) throw std::runtime_error("SPU input read without bound sample clock");
        auto& input=inputs.at(core); auto& transfer=transfers.at(core);
        if (sample!=input.nextSample) throw std::runtime_error("SPU ADMA sample cursor skipped/repeated");
        ++input.nextSample;
        if (transfer.active) validateActive(memory,core);
        const uint32_t position=uint32_t(sample&0x1ffu),base=0x2000u+core*0x400u;
        input.pcm={std::bit_cast<int16_t>(ram.peek(base+position)),
                   std::bit_cast<int16_t>(ram.peek(base+0x200+position))};
        if (input.pcm[0] || input.pcm[1]) {
            const auto count=++input.nonzeroFrames;
            if (count<=8 || (count&(count-1))==0)
                std::cerr << "[spu-adma-pcm] core=" << core << " sample=" << sample
                          << " left=" << input.pcm[0] << " right=" << input.pcm[1] << " count=" << count << '\n';
        }
        if (transfer.active && transfer.automatic && input.pendingBytes) {
            const uint32_t amount=std::min(input.pendingBytes,0x180u);
            input.pendingBytes-=amount;
            memory.write32(channelBase(core),memory.read32(channelBase(core))+amount);
            if (!input.pendingBytes && !transfer.remaining) finish(memory,core);
        }
        if (!(position&0x7fu)) {
            if (position==0) input.writePosition=0x100;
            else if (position==0x100) input.writePosition=0;
            if (transfer.active && transfer.automatic && transfer.remaining>=256) refill(memory,core);
            else control.observeAutoDmaBuffer(core,true);
        }
        return input.pcm;
    }
};
