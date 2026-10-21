#pragma once
#include "bare_call.h"

namespace iop_tests {
inline void plainSpuDmaWrite() {
    using ps2x::iop::detail::IopMemory;
    IopMemory memory; SpuRamWritePort ram; ram.enable();
    SpuQuiescentControl control; control.enable(nullptr,true);
    SpuDmaWrite writer(ram,control);
    QuiescentDma dma; dma.reset(memory); dma.enableSpuWriter(writer);
    const auto store=[&](auto& device,uint32_t address,uint16_t value) {
        const auto access=device.stage(memory,address,address,0x29,value);
        device.complete(memory,access,false);
    };
    for (unsigned core=0;core<2;++core) {
        const uint32_t regs=0x1f900000+core*0x400, base=core?0x1f801500:0x1f8010c0;
        const uint32_t icr=core?0x1f801574:0x1f8010f4;
        store(control,regs+0x19a,0x8020);
        store(ram,regs+0x1a8,15); store(ram,regs+0x1aa,0xfff0);
        for (unsigned n=0;n<512;++n) memory.write16(0x8000+n*2,uint16_t(0x1234+n+core));
        memory.write32(base,0x8000); memory.write32(base+4,0x00020080);
        if (!dma.allows(base+8,0x2b,0x01000201,&memory)) throw std::runtime_error("Reviewed SPU DMA start rejected");
        memory.write32(base+8,0x01000201); writer.pump(memory);
        if (ram.peek(0xffff0)==0x1234+core || control.read(regs+0x344)!=0x80)
            throw std::runtime_error("SPU DMA ignored disabled DMA gates");
        memory.write32(0x1f801578,1); memory.write32(core?0x1f801570:0x1f8010f0,core?8:0x80000);
        memory.write32(icr,0x800000|(core?0x10000:0x100000)); memory.setInterruptStatus(0);
        writer.pump(memory);
        if (ram.peek(0xffff0)!=0x1234+core || ram.peek(0)!=0x1244+core ||
            control.read(regs+0x344)!=0x400 || memory.interruptStatus() ||
            ram.allows(regs+0x1ac,0x29,0) || control.allows(regs+0x19a,0x29,0x8000))
            throw std::runtime_error("SPU first burst/busy/wrap/CPU access guard differs");
        bool rearm=false;
        try { writer.validateStart(memory,base+8,0x01000201); } catch (const std::runtime_error&) { rearm=true; }
        if (!rearm) throw std::runtime_error("Active SPU DMA accepted rearm");
        for (unsigned n=0;n<512*24-1;++n) writer.pump(memory);
        if (!(memory.read32(base+8)&0x01000000) || memory.interruptStatus())
            throw std::runtime_error("SPU DMA completed before its last burst elapsed");
        writer.pump(memory);
        if ((memory.read32(base+8)&0x01000000) || memory.read32(base)!=0x8400 ||
            memory.read32(base+4)!=0x00020080 || control.read(regs+0x344)!=0x80 ||
            !(memory.interruptStatus()&8) || !(memory.read32(icr)&(core?0x01000000:0x10000000)) ||
            ram.peek(0x1ef)!=0x1433+core)
            throw std::runtime_error("SPU DMA lost data/MADR/BCR/STATX/real completion IRQ");
        memory.write32(0x1f801578,0);
    }
}
}
