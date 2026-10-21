#pragma once
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include "spu_reverb_setup.h"

// Pinned functional core state. IRQ/ADMA remain closed; mode2 arming requires
// a bound plain DMA writer, and FX requires the checked zero-input domain.
// The reference finishes
// core initialization with STATX=0x80; a real ATTR store with DMA mode zero clears
// that flag when no transfer is busy. This is
// not a timed hardware reset model or a completion source for active transfers.
class SpuQuiescentControl {
    struct Core { uint16_t attributes{}, autoDma{}, status{0x80}; };
    std::array<Core,2> cores{};
    bool enabled{};
    uint32_t stores{};
    SpuReverbSetup* reverb{};
    bool dmaWriter{};
    static int index(uint32_t address) {
        if (address==0x1f90019a || address==0x1f9001b0 || address==0x1f900344) return 0;
        if (address==0x1f90059a || address==0x1f9005b0 || address==0x1f900744) return 1;
        return -1;
    }
public:
    struct Access { uint32_t address, physical; uint16_t value, previous; bool write; };
    void enable(SpuReverbSetup* effects=nullptr,bool writer=false) {
        if (enabled) throw std::runtime_error("Quiescent SPU control already enabled");
        enabled=true; reverb=effects; dmaWriter=writer;
    }
    bool allows(uint32_t address,uint32_t opcode,uint32_t value=0) const {
        if (!enabled || index(address)<0) return false;
        const auto reg=address&0x3ff;
        if (opcode==0x21 || opcode==0x25) return true;
        if (opcode!=0x29) return false;
        if (reg==0x1b0) return uint16_t(value)==0;
        // Bits 0/14/15 are retained raw; the pinned functional reference does not
        // apply mute/core-enable effects for them. IRQ/mode3 remain closed;
        // mode2 and FX require the separate reviewed device contracts below.
        const unsigned mode=(value>>4)&3;
        return reg==0x19a && !(uint16_t(value)&0x3f4e) && mode!=3 && (mode!=2 || dmaWriter) &&
            (!(cores[index(address)].status&0x400) || value==cores[index(address)].attributes) &&
            (!(value&0x80) || (reverb && reverb->supportsZeroInput(index(address))));
    }
    uint16_t read(uint32_t address) const {
        if (!allows(address,0x25)) throw std::runtime_error("Unsupported SPU control read");
        const auto& core=cores[index(address)];
        switch (address&0x3ff) {
        case 0x19a: return core.attributes;
        case 0x1b0: return core.autoDma;
        default: return core.status;
        }
    }
    uint32_t retiredStores() const { return stores; }
    bool dmaWriteReady(unsigned core) const {
        return dmaWriter && ((cores.at(core).attributes>>4)&3)==2 && !cores.at(core).autoDma && !(cores.at(core).status&0x400);
    }
    void beginDmaWrite(unsigned core) {
        if (!dmaWriteReady(core)) throw std::runtime_error("SPU DMA write requires idle mode2 without ADMA");
        cores.at(core).status=(cores.at(core).status&~0x80u)|0x400;
    }
    void finishDmaWrite(unsigned core) {
        auto& state=cores.at(core); state.status&=uint16_t(~0x400u);
        if ((state.attributes>>4)&3) state.status|=0x80;
    }
    template<class Memory> Access stage(Memory& memory,uint32_t address,uint32_t physical,
                                       uint32_t opcode,uint32_t value) const {
        if (!allows(physical,opcode,value)) throw std::runtime_error("Unsupported SPU control access");
        Access access{address,physical,uint16_t(value),memory.read16(address),opcode==0x29};
        if (!access.write) memory.write16(address,read(physical));
        return access;
    }
    template<class Memory> void complete(Memory& memory,const Access& access,bool exception) {
        memory.write16(access.address,access.previous);
        if (exception || !access.write) return;
        auto& core=cores[index(access.physical)];
        if ((access.physical&0x3ff)==0x1b0) core.autoDma=access.value;
        else {
            const auto oldMode=(core.attributes>>4)&3;
            core.attributes=access.value;
            if (reverb) reverb->setEffects(index(access.physical),access.value&0x80);
            const auto mode=(core.attributes>>4)&3;
            if (!mode && !(core.status&0x400)) core.status&=uint16_t(~0x80u);
            else if (!oldMode && mode) core.status|=0x80;
        }
        ++stores;
        if (stores<=16 || stores%256==0)
            std::cerr << "[spu-control-store] address=0x" << std::hex << access.physical << " value=0x" << access.value
                      << " status=0x" << core.status << std::dec << " count=" << stores << "; guest SH retired\n";
    }
};
