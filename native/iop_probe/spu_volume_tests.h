#pragma once
#include "bare_call.h"

namespace iop_tests {
inline void spuVolumeRegisters() {
    using ps2x::iop::detail::IopMemory;
    IopMemory memory;
    SpuVolumeRegisters volumes;
    const auto access = [&](uint32_t address, uint32_t value, uint32_t opcode) {
        memory.write32(0x1000,0x3c020000 | (value>>16));
        memory.write32(0x1004,0x34420000 | (value&0xffff));
        memory.write32(0x1008,(opcode<<26) | (4u<<21) | (2u<<16));
        memory.write32(0x100c,0); memory.write32(0x1010,0x03e00008); memory.write32(0x1014,0);
        const auto result = bareCall(memory,0x1000,0,address,0x1000,0x1100,{.volumes=&volumes});
        if (!result.completed || result.instructions != 6) throw std::runtime_error("SPU volume CPU call failed");
        return result.value;
    };
    const auto refuses = [&](uint32_t address,uint32_t value,uint32_t opcode) {
        try { access(address,value,opcode); }
        catch (const std::runtime_error& error) {
            if (std::string(error.what()).find("hardware contract") != std::string::npos) return;
            throw;
        }
        throw std::runtime_error("Unsupported SPU volume operation succeeded");
    };
    refuses(0xbf900760,0,0x29);
    volumes.enable();
    for (unsigned core=0;core<2;++core) {
        const uint32_t base = core ? 0x1f900788 : 0x1f900760;
        if (volumes.read(base) || volumes.read(base+16) || volumes.gain(core,4)!=32767)
            throw std::runtime_error("SPU volume reset profile differs from pinned reference");
        for (uint32_t alias : {0u,0x80000000u,0xa0000000u}) {
            for (unsigned reg=0;reg<8;++reg) {
                for (auto [raw,gain] : std::array<std::pair<uint16_t,int32_t>,5>{{
                    {0,0},{1,reg<2?2:1},{0x3fff,reg<2?32766:16383},
                    {0x4000,reg<2?-32768:16384},{0x7fff,reg<2?-2:32767}}}) {
                    const auto physical=base+reg*2, address=physical|alias;
                    memory.write16(physical-2,0xabcd); memory.write16(physical,0xface); memory.write16(physical+2,0x1234);
                    access(address,0x12340000u|raw,0x29); // SH must use only the low halfword.
                    if (volumes.gain(core,reg)!=gain || access(address,0,0x25)!=raw ||
                        memory.read16(physical)!=0xface || memory.read16(physical-2)!=0xabcd ||
                        memory.read16(physical+2)!=0x1234 || memory.interruptStatus())
                        throw std::runtime_error("SPU volume raw/gain/alias/halfword contract failed");
                    if (reg<2 && (access((base+16+reg*2)|alias,0,0x25)!=uint16_t(gain) ||
                        access((base+16+reg*2)|alias,0,0x21)!=uint32_t(gain)))
                        throw std::runtime_error("SPU current master volume lost derivation or signed load");
                }
                if (reg>=2) {
                    access((base+reg*2)|alias,0x8000,0x29);
                    if (volumes.gain(core,reg)!=-32768) throw std::runtime_error("SPU signed input gain minimum failed");
                    access((base+reg*2)|alias,0xffff,0x29);
                    if (volumes.gain(core,reg)!=-1) throw std::runtime_error("SPU signed input gain negative one failed");
                }
            }
        }
    }
    const auto before=volumes.retiredStores();
    const auto raw=volumes.read(0x1f900760);
    refuses(0xbf900760,0x8000,0x29); // sweep mode needs audio-time progression.
    refuses(0xbf900770,0,0x29); // current-volume registers are not writable in this subset.
    refuses(0xbf90019a,0,0x29);
    refuses(0xbf900760,0,0x2b);
    refuses(0xbf900760,0,0x24);
    if (volumes.retiredStores()!=before || volumes.read(0x1f900760)!=raw ||
        volumes.allows(0x1f900761,0x25)) throw std::runtime_error("Rejected SPU volume access changed state");
    memory.write16(0x1f900760,0x55aa);
    const auto staged=volumes.stage(memory,0xbf900760,0x1f900760,0x25,0);
    volumes.complete(memory,staged,true);
    if (memory.read16(0x1f900760)!=0x55aa || volumes.retiredStores()!=before)
        throw std::runtime_error("SPU volume read staging leaked on exception");
    const auto write=volumes.stage(memory,0xbf900760,0x1f900760,0x29,42);
    memory.write16(0x1f900760,42); volumes.complete(memory,write,true);
    if (memory.read16(0x1f900760)!=0x55aa || volumes.read(0x1f900760)!=raw || volumes.retiredStores()!=before)
        throw std::runtime_error("Faulted SPU volume store committed state");
}
}
