#pragma once
#include "quiescent_sio2.h"

// Plain DMA11 RAM->SIO2 and DMA12 SIO2->RAM. BCR block size frames serial
// packets; bounded functional completion charges match pinned IopDma.cpp.
class Sio2Dma {
    QuiescentSio2& sio;
    struct Transfer { uint32_t cursor{}, blockBytes{}, blocks{}, delay{}; bool active{}; };
    std::array<Transfer,2> transfers{};
    uint64_t totalBytes{}, completions{};
    template<class Memory> Transfer parameters(const Memory& memory,unsigned channel,uint32_t value) const {
        const uint32_t base=channel?0x1f801550:0x1f801540;
        const auto address=memory.read32(base), bcr=memory.read32(base+4);
        const auto block=(bcr&0xffffu)*4, blocks=bcr>>16;
        const uint64_t length=uint64_t(block)*blocks;
        if (value!=(channel?0x41000200u:0x01000201u) || !block || block>4096 || !blocks ||
            length>Memory::RamSize || (address&3) || address>=Memory::RamSize || length>Memory::RamSize-address)
            throw std::runtime_error("Unsupported SIO2 DMA direction/size/RAM range");
        return {address,block,blocks,0,true};
    }
    template<class Memory> bool copyBlock(Memory& memory,unsigned channel,Transfer& transfer) {
        if (channel) {
            if (sio.availableBytes()<transfer.blockBytes) return false;
            for (unsigned i=0;i<transfer.blockBytes;++i) memory.write8(transfer.cursor+i,sio.fifoRead());
        } else {
            std::vector<uint8_t> source(transfer.blockBytes);
            for (unsigned i=0;i<transfer.blockBytes;++i) source[i]=memory.read8(transfer.cursor+i);
            sio.dmaBlock(source);
        }
        transfer.cursor+=transfer.blockBytes; --transfer.blocks;
        transfer.delay=std::max(1u,channel?transfer.blockBytes/4:transfer.blockBytes/16);
        totalBytes+=transfer.blockBytes;
        return true;
    }
public:
    explicit Sio2Dma(QuiescentSio2& device):sio(device) {}
    static bool channel(uint32_t address) { return address==0x1f801548 || address==0x1f801558; }
    template<class Memory> void validateStart(const Memory& memory,uint32_t address,uint32_t value) const {
        const unsigned channel=address==0x1f801558;
        if (transfers[channel].active || (memory.read32(address)&0x01000000))
            throw std::runtime_error("SIO2 DMA active rearm unsupported");
        parameters(memory,channel,value);
    }
    template<class Memory> void pump(Memory& memory) {
        for (unsigned channel=0;channel<2;++channel) {
            const uint32_t base=channel?0x1f801550:0x1f801540;
            const auto chcr=memory.read32(base+8);
            auto& transfer=transfers[channel];
            if (!transfer.active) {
                if (!(chcr&0x01000000)) continue;
                if (memory.read32(0x1f801578)!=1 || !(memory.read32(0x1f801570)&(channel?0x800000u:0x80000u))) continue;
                transfer=parameters(memory,channel,chcr);
            }
            if (chcr!=(channel?0x41000200u:0x01000201u))
                throw std::runtime_error("SIO2 DMA active cancellation unsupported");
            if (!transfer.delay) { if (!copyBlock(memory,channel,transfer)) continue; }
            else if (--transfer.delay) continue;
            else {
                memory.write32(base,transfer.cursor);
                if (transfer.blocks) continue;
                memory.write32(base+8,chcr&~0x01000000u); transfer.active=false;
                auto icr=memory.read32(0x1f801574); const unsigned bit=channel?5:4;
                if (icr&(1u<<(16+bit))) {
                    icr|=1u<<(24+bit);
                    if (icr&0x800000) icr|=0x80000000;
                    memory.write32(0x1f801574,icr); memory.setInterruptStatus(memory.interruptStatus()|8u);
                }
                if (++completions<=32 || completions%256==0)
                    std::cerr << "[sio2-dma-complete] channel=" << channel+11 << " bytes=" << totalBytes
                              << " transfers=" << completions << '\n';
            }
        }
    }
};
