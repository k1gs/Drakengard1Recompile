#pragma once
#include "bare_call.h"

namespace iop_tests {
inline void spuSetupTransfers() {
    using ps2x::iop::detail::IopMemory;
    IopMemory memory; SpuRamWritePort ram; SpuReverbSetup reverb; ram.enable(); reverb.enable();
    const auto access=[&](uint32_t address,uint32_t value,uint32_t opcode) {
        memory.write32(0x1000,0x3c020000|(value>>16)); memory.write32(0x1004,0x34420000|(value&0xffff));
        memory.write32(0x1008,(opcode<<26)|(4u<<21)|(2u<<16)); memory.write32(0x100c,0);
        memory.write32(0x1010,0x03e00008); memory.write32(0x1014,0);
        return bareCall(memory,0x1000,0,address,0x1000,0x1100,{.reverb=&reverb,.spuRam=&ram}).value;
    };
    if (ram.peek(0x2800)!=0x707 || ram.peek(0xe870)!=0x707 || ram.peek(0))
        throw std::runtime_error("SPU RAM reference reset pattern differs");
    for (unsigned core=0;core<2;++core) {
        const auto base=0xbf900000+core*0x400;
        access(base+0x1a8,15,0x29); access(base+0x1aa,0xffff,0x29);
        access(base+0x1ac,0xabcd+core,0x29); access(base+0x1ac,0x1234+core,0x29);
        if (ram.peek(0xfffff)!=0xabcd+core || ram.peek(0)!=0x1234+core ||
            access(base+0x1a8,0,0x25)!=0 || access(base+0x1aa,0,0x25)!=1 || memory.interruptStatus())
            throw std::runtime_error("Actual SPU port data/wrap/shared memory/TSA failed");
        for (unsigned n=0;n<48;++n) {
            const auto address=base+0x2e0+n*2;
            const auto previous=access(address,0,0x25);
            access(address,0x1357+n,0x29);
            if (access(address,0,0x25)!=(n==47?previous:0x1357+n))
                throw std::runtime_error("Inactive reverb pair storage or EEA write-null failed");
        }
        for (unsigned n=0;n<10;++n) {
            const auto address=0xbf900774+core*0x28+n*2;
            access(address,0xfffe,0x29);
            if (access(address,0,0x21)!=0xfffffffe) throw std::runtime_error("Reverb coefficient signed read failed");
        }
    }
    const auto before=ram.writtenWords();
    const auto effect=ram.stage(memory,0xbf9001ac,0x1f9001ac,0x29,0xdead);
    ram.complete(memory,effect,true);
    if (ram.writtenWords()!=before || ram.peek(1)) throw std::runtime_error("Faulted SPU port committed data");
    if (ram.allows(0x1f9001ac,0x25) || ram.allows(0x1f9001ac,0x2b) || reverb.allows(0x1f90019a,0x29,0x80))
        throw std::runtime_error("SPU preparation accepted an unreviewed FIFO read/width/FX activation");
}
}
