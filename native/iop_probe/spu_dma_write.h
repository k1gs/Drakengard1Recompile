#pragma once
#include "spu_ram_write_port.h"
#include "spu_quiescent_control.h"
#include <algorithm>

// Functional plain IOP->SPU DMA, bounded to RAM sources, ADMA/IRQ disabled.
// The pinned reference copies bursts of 256 halfwords and charges 24 modeled
// IOP cycles per halfword. Only actual copied data can lead to completion.
class SpuDmaWrite {
    struct Transfer { uint32_t source{}, remaining{}, end{}, delay{}, burst{}; bool active{}; };
    std::array<Transfer,2> transfers{};
    SpuRamWritePort& ram;
    SpuQuiescentControl& control;
    uint64_t bytes{}, completed{};
    template<class Memory> uint32_t length(const Memory& memory,unsigned core,uint32_t value) const {
        const auto base=core?0x1f801500:0x1f8010c0;
        const auto source=memory.read32(base), bcr=memory.read32(base+4);
        const uint64_t words=uint64_t(bcr>>16)*(bcr&0xffffu)*2;
        if (value!=0x01000201 || words<2 || words>0x100000 || (source&3) ||
            source>=Memory::RamSize || words*2>Memory::RamSize-source || !control.dmaWriteReady(core))
            throw std::runtime_error("Unsupported plain SPU DMA source/size/direction/core state");
        return uint32_t(words);
    }
    template<class Memory> void copyBurst(Memory& memory,unsigned core) {
        auto& transfer=transfers[core];
        transfer.burst=std::min(256u,transfer.remaining);
        for (unsigned n=0;n<transfer.burst;++n) ram.transferWord(core,memory.read16(transfer.source+n*2));
        transfer.source+=transfer.burst*2; transfer.remaining-=transfer.burst;
        transfer.delay=transfer.burst*24; bytes+=transfer.burst*2;
    }
public:
    SpuDmaWrite(SpuRamWritePort& memory,SpuQuiescentControl& state):ram(memory),control(state) {}
    static bool channel(uint32_t address) { return address==0x1f8010c8 || address==0x1f801508; }
    template<class Memory> void validateStart(const Memory& memory,uint32_t address,uint32_t value) const {
        const unsigned core=address==0x1f801508;
        if (transfers[core].active || (memory.read32(address)&0x01000000))
            throw std::runtime_error("Active SPU DMA rearm unsupported");
        length(memory,core,value);
    }
    template<class Memory> void pump(Memory& memory) {
        for (unsigned core=0;core<2;++core) {
            auto& transfer=transfers[core]; const uint32_t base=core?0x1f801500:0x1f8010c0;
            const auto chcr=memory.read32(base+8);
            if (!transfer.active) {
                if (!(chcr&0x01000000)) continue;
                if (memory.read32(0x1f801578)!=1 || !(memory.read32(core?0x1f801570:0x1f8010f0)&(core?8u:0x80000u))) continue;
                const auto words=length(memory,core,chcr);
                transfer={memory.read32(base),words,memory.read32(base)+words*2,0,0,true};
                control.beginDmaWrite(core); ram.setDmaBusy(core,true); copyBurst(memory,core);
                continue;
            }
            if (chcr!=0x01000201) throw std::runtime_error("Active SPU DMA cancellation/reconfiguration unsupported");
            if (--transfer.delay) continue;
            memory.write32(base,transfer.source);
            if (transfer.remaining) { copyBurst(memory,core); continue; }
            memory.write32(base,transfer.end); memory.write32(base+8,chcr&~0x01000000u);
            control.finishDmaWrite(core); ram.setDmaBusy(core,false); transfer.active=false;
            const uint32_t icrAddress=core?0x1f801574:0x1f8010f4;
            auto icr=memory.read32(icrAddress); const unsigned bit=core?0:4;
            if (icr&(1u<<(16+bit))) {
                icr|=1u<<(24+bit);
                if (icr&0x800000) icr|=0x80000000;
                memory.write32(icrAddress,icr); memory.setInterruptStatus(memory.interruptStatus()|8);
            }
            ++completed;
            std::cerr << "[spu-dma-write-complete] core=" << core << " bytes=" << bytes << " transfers=" << completed << '\n';
        }
    }
};
