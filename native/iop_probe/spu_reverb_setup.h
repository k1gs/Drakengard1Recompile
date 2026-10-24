#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <iostream>
#include "spu_ram_write_port.h"
#include "spu_reverb_dsp.h"

// FX register state and a checked zero-input/workspace invariant. Address pairs
// retain the reference high/low halves; coefficients keep signed bit patterns.
// The runtime may explicitly bind the functional scalar DSP at initialization.
// Unbound standalone probes retain their checked zero-input contract.
class SpuReverbSetup {
    struct Core {
        std::array<uint16_t,48> offsets{};
        std::array<uint16_t,10> coefficients{};
        SpuReverbDsp dsp;
        uint64_t frames{},nonzero{};
        std::array<int32_t,2> output{};
    };
    std::array<Core,2> cores{};
    bool enabled{};
    SpuRamWritePort* ram{};
    bool processing{};
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
    void enable(SpuRamWritePort* zeroInputRam=nullptr) {
        if (enabled) throw std::runtime_error("SPU FX setup already enabled");
        enabled=true; ram=zeroInputRam;
    }
    void enableProcessing() {
        if (!enabled || !ram || processing || samples || cores[0].frames || cores[1].frames)
            throw std::runtime_error("SPU FX processing requires fresh bound RAM/sample clock");
        processing=true;
    }
    bool canEnableEffects(unsigned core) const { return processing || supportsZeroInput(core); }
    const std::array<int32_t,2>& output(unsigned core) const { return cores.at(core).output; }
    bool supportsZeroInput(unsigned index) const {
        if (!ram) return false;
        if (unmodeledInputHistory.at(index)) return false;
        const auto& fields=cores.at(index).offsets;
        const uint32_t start=(uint32_t(fields[0])<<16)|fields[1];
        const uint32_t end=(uint32_t(fields[46])<<16)|0xffff;
        return start>=end || ram->zeroRange(start,end);
    }
    void setEffects(unsigned core,bool value) {
        if (value && !canEnableEffects(core))
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
        if (processing) {
            for (const auto& core:cores) if (core.frames!=sampleCount)
                throw std::runtime_error("SPU FX missing routed sample input");
            samples=sampleCount; return;
        }
        for (unsigned c=0;c<2;++c) if (active[c] && !supportsZeroInput(c))
            throw std::runtime_error("SPU FX workspace left the zero-input subset");
        samples=sampleCount;
    }
    // Process each actual routed frame, or retain the zero-only admission guard
    // for standalone probes that did not bind the scalar DSP at initialization.
    void observeInput(unsigned core,int32_t left,int32_t right) {
        if (core>=active.size()) throw std::out_of_range("SPU FX core index out of range");
        if (processing) {
            auto& state=cores[core];
            state.output=state.dsp.tick(*ram,state.offsets,state.coefficients,active[core],state.frames+1,{left,right});
            ++state.frames;
            if (left || right || state.output[0] || state.output[1]) {
                const auto count=++state.nonzero;
                if (count<=8 || (count&(count-1))==0)
                    std::cerr << "[spu-fx-pcm] core=" << core << " sample=" << state.frames
                              << " input_l=" << left << " input_r=" << right
                              << " output_l=" << state.output[0] << " output_r=" << state.output[1]
                              << " writes=" << active[core]
                              << " delay_nonzero_stores=" << state.dsp.nonzeroStoreCount()
                              << " count=" << count << '\n';
            }
            return;
        }
        if (!active[core]) {
            // FX-off output is zero, but a nonzero downsample history cannot
            // be declared empty if effects are subsequently enabled.
            if (left || right) unmodeledInputHistory[core]=true;
            return;
        }
        if (!supportsZeroInput(core))
            throw std::runtime_error("SPU FX zero-input workspace invalid: core="+std::to_string(core));
        if (left!=0 || right!=0) {
            const auto& state=cores[core];
            const uint32_t start=(uint32_t(state.offsets[0])<<16)|state.offsets[1];
            const uint32_t end=(uint32_t(state.offsets[46])<<16)|0xffff;
            std::cerr << "[spu-fx-domain] core=" << core << " sample=" << samples
                      << " active=" << active[core] << " left=" << left << " right=" << right
                      << " start=0x" << std::hex << start << " end=0x" << end << " offsets=";
            for (auto value:state.offsets) std::cerr << value << ',';
            std::cerr << " coefficients=";
            for (auto value:state.coefficients) std::cerr << value << ',';
            std::cerr << std::dec << "; first unsupported nonzero FX input\n";
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
