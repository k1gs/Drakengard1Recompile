#pragma once
#include "bare_call.h"

namespace iop_tests {
inline void spuQuiescentControl() {
    using ps2x::iop::detail::IopMemory;
    IopMemory memory;
    QuiescentDma dma; dma.reset(memory);
    SpuQuiescentControl control;
    const auto access=[&](uint32_t address,uint32_t value,uint32_t opcode) {
        memory.write32(0x1000,0x3c020000|(value>>16)); memory.write32(0x1004,0x34420000|(value&0xffff));
        memory.write32(0x1008,(opcode<<26)|(4u<<21)|(2u<<16)); memory.write32(0x100c,0);
        memory.write32(0x1010,0x03e00008); memory.write32(0x1014,0);
        return bareCall(memory,0x1000,0,address,0x1000,0x1100,{.dma=&dma,.spuControl=&control}).value;
    };
    const auto refuses=[&](uint32_t address,uint32_t value,uint32_t opcode) {
        try { access(address,value,opcode); }
        catch (const std::runtime_error& error) {
            const std::string message=error.what();
            if (message.find("hardware contract")!=std::string::npos ||
                message.find("actual transfer engine")!=std::string::npos) return;
            throw;
        }
        throw std::runtime_error("Unsupported SPU control operation succeeded");
    };
    refuses(0xbf90019a,0,0x29); control.enable();
    for (uint32_t coreOffset : {0u,0x400u}) {
        const auto attr=0x1f90019a+coreOffset, adma=0x1f9001b0+coreOffset, status=0x1f900344+coreOffset;
        if (control.read(status)!=0x80 || control.read(attr) || control.read(adma))
            throw std::runtime_error("SPU reset must retain pinned STATX=0x80 before the real ATTR store");
        for (uint32_t alias : {0u,0x80000000u,0xa0000000u}) {
            memory.write16(attr,0xface); memory.write16(status,0xabcd);
            access(adma|alias,0,0x29);
            for (uint32_t value : {0u,0x8000u,0xc000u,0xc001u,0x4000u,0u}) {
                access(attr|alias,value,0x29);
                if (access(attr|alias,0,0x25)!=value || access(status|alias,0,0x25)!=0 ||
                    access(adma|alias,0,0x25)!=0 || memory.read16(attr)!=0xface ||
                    memory.read16(status)!=0xabcd || memory.interruptStatus())
                    throw std::runtime_error("SPU quiescent ATTR/ADMAS/STATX state or backing restoration failed");
            }
            access(attr|alias,0x8010,0x29);
            if (access(status|alias,0,0x25)!=0x80) throw std::runtime_error("SPU mode arm lost its request flag");
            access(attr|alias,0x8000,0x29);
            if (access(status|alias,0,0x25)) throw std::runtime_error("SPU mode stop retained an idle request flag");
            access(attr|alias,0,0x29);
            const auto afterArm=control.retiredStores();
            for (uint32_t value : {0x20u,0x40u,0x80u,0xc080u}) refuses(attr|alias,value,0x29);
            refuses(adma|alias,1,0x29); refuses(status|alias,0,0x29);
            refuses(attr|alias,0,0x2b); refuses(attr|alias,0,0x24);
            if (control.retiredStores()!=afterArm || control.read(attr) || control.read(status))
                throw std::runtime_error("Rejected active SPU operation changed quiescent state");
        }
    }
    const auto before=control.retiredStores();
    refuses(0xbf8010c8,0x01000201,0x2b); refuses(0xbf801508,0x01000201,0x2b);
    refuses(0xbf9001a0,1,0x29); refuses(0xbf9001ac,1,0x29);
    memory.write16(0x1f90019a,0x55aa);
    const auto staged=control.stage(memory,0xbf90019a,0x1f90019a,0x29,0x8000);
    memory.write16(0x1f90019a,0x8000); control.complete(memory,staged,true);
    if (memory.read16(0x1f90019a)!=0x55aa || control.read(0x1f90019a) || control.retiredStores()!=before ||
        memory.interruptStatus()) throw std::runtime_error("Faulted SPU control access committed or invented IRQ");
}
}
