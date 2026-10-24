#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "spu_ram_write_port.h"

// FX register state and a checked zero-input/workspace invariant. Address pairs
// retain the reference high/low halves; coefficients keep signed bit patterns.
// General nonzero reverb processing remains unsupported.
class SpuReverbSetup {
    struct Core {
        std::array<uint16_t,48> offsets{};
        std::array<uint16_t,10> coefficients{};
    };
    std::array<Core,2> cores{};
    bool enabled{};
    const SpuRamWritePort* ram{};
    std::array<bool,2> active{};
    std::array<bool,2> unmodeledInputHistory{};
    uint64_t samples{};
    static int index(uint32_t address) {
        if (address&1) return -1;
        for (unsigned core=0;core<2;++core) {
            const auto base=0x1f9002e0+core*0x400;
            if (address>=base && address<base+0x60) return int(core*58+(address-base)/2);
            const auto coef=0x1f900774+core*0x28;
            if (address>=coef && address<coef+20) return int(core*58+48+(address-coef)/2);
        }
        return -1;
    }
public:
    struct Access { uint32_t address,physical; uint16_t value,previous; bool write; };
    SpuReverbSetup() {
        for (unsigned core=0;core<2;++core) {
            auto& fields=cores[core].offsets;
            fields[0]=core?15:14; fields[1]=0xfff8;
            fields[46]=core?15:14; fields[47]=0xffff;
        }
    }
    void enable(const SpuRamWritePort* zeroInputRam=nullptr) {
        if (enabled) throw std::runtime_error("SPU FX setup already enabled");
        enabled=true; ram=zeroInputRam;
    }
    bool supportsZeroInput(unsigned index) const {
        if (!ram) return false;
        if (unmodeledInputHistory.at(index)) return false;
        const auto& fields=cores.at(index).offsets;
        const uint32_t start=(uint32_t(fields[0])<<16)|fields[1];
        const uint32_t end=(uint32_t(fields[46])<<16)|0xffff;
        return start>=end || ram->zeroRange(start,end);
    }
    void setEffects(unsigned core,bool value) {
        if (value && !supportsZeroInput(core))
            throw std::runtime_error("SPU FX requires a zero workspace and verified zero inputs");
        active.at(core)=value;
    }
    // With verified zero input, delay RAM and filter history, every multiply/add and RAM
    // write of the reference reverb remains zero for any signed coefficients.
    // The address phase follows the sample counter, including while FX is off.
    // The voice owner must validate routed PCM with observeInput for each frame.
    void advanceSilentInputs(uint64_t sampleCount) {
        if (sampleCount==samples) return;
        if (sampleCount<samples) throw std::runtime_error("SPU FX sample clock moved backwards");
        for (unsigned c=0;c<2;++c) if (active[c] && !supportsZeroInput(c))
            throw std::runtime_error("SPU FX workspace left the zero-input subset");
        samples=sampleCount;
    }
    // Observe the actual PCM routed into one core's FX path.  The current
    // reverb model is deliberately zero-input-only, so active FX must reject
    // nonzero PCM instead of silently treating it as zero.
    void observeInput(unsigned core,int32_t left,int32_t right) {
        if (core>=active.size()) throw std::out_of_range("SPU FX core index out of range");
        if (!active[core]) {
            // FX-off output is zero, but a nonzero downsample history cannot
            // be declared empty if effects are subsequently enabled.
            if (left || right) unmodeledInputHistory[core]=true;
            return;
        }
        if (!supportsZeroInput(core))
            throw std::runtime_error("SPU FX zero-input workspace invalid: core="+std::to_string(core));
        if (left!=0 || right!=0) {
            throw std::runtime_error("SPU FX zero-input guard rejected routed PCM: core="+
                                     std::to_string(core)+" left="+std::to_string(left)+
                                     " right="+std::to_string(right));
        }
    }
    bool allows(uint32_t address,uint32_t opcode,uint32_t=0) const {
        return enabled && index(address)>=0 && (opcode==0x21 || opcode==0x25 || opcode==0x29);
    }
    uint16_t read(uint32_t address) const {
        if (!allows(address,0x25)) throw std::runtime_error("Unsupported SPU FX setup read");
        const unsigned at=index(address), reg=at%58; const auto& core=cores[at/58];
        return reg<48?core.offsets[reg]:core.coefficients[reg-48];
    }
    template<class Memory> Access stage(Memory& memory,uint32_t address,uint32_t physical,
                                       uint32_t opcode,uint32_t value) const {
        if (!allows(physical,opcode,value)) throw std::runtime_error("Unsupported SPU FX setup access");
        Access access{address,physical,uint16_t(value),memory.read16(address),opcode==0x29};
        if (!access.write) memory.write16(address,read(physical));
        return access;
    }
    template<class Memory> void complete(Memory& memory,const Access& access,bool exception) {
        memory.write16(access.address,access.previous);
        if (exception || !access.write) return;
        const unsigned at=index(access.physical), reg=at%58; auto& core=cores[at/58];
        if (reg<48) { if (reg!=47) core.offsets[reg]=access.value; } // EEA low word is reference write-null.
        else core.coefficients[reg-48]=access.value;
    }
};
