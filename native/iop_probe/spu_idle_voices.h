#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include "spu_envelope.h"
#include "spu_ram_write_port.h"

// Voice setup plus a deliberately bounded zero-sample ADPCM loop decoder. Other
// sample data, noise and sweeps fail closed. This produces no host audio claim.
class SpuIdleVoices {
    struct Voice {
        std::array<uint16_t,6> parameters{0,0,0x3fff,0,0,0};
        std::array<uint32_t,3> addresses{0x2800,0x2800,0x2801};
        bool loopMode{};
        SpuEnvelope envelope;
        uint32_t pitchFraction{}, decoded{}, consumed{};
        bool started{};
    };
    struct Core {
        std::array<Voice,24> voices{};
        std::array<uint32_t,6> masks{0,0,0xffffff,0xffffff,0xffffff,0xffffff};
        uint32_t keyOn{}, keyOff{}, endx{0xffffff};
        uint16_t mmix{};
    };
    std::array<Core,2> cores{};
    uint64_t cycle{}, samples{}, released{};
    uint32_t fraction{}, stores{};
    bool enabled{};
    const SpuRamWritePort* ram{};
    bool silentBlock(uint32_t address) const {
        if (!ram || ram->peek(address&0xffff8)!=0x0700) return false;
        for (unsigned i=1;i<8;++i) if (ram->peek((address&0xffff8)+i)) return false;
        return true;
    }
    static bool mapped(uint32_t address) {
        if (address<0x1f900000 || address>=0x1f900800 || (address&1)) return false;
        const auto reg=address&0x3ff;
        return reg<0x19a || (reg>=0x1a0 && reg<=0x1a6) ||
            (reg>=0x1c0 && reg<0x2e0) || reg==0x340 || reg==0x342;
    }
    static uint32_t half(uint32_t original,uint32_t reg,uint16_t value) {
        const unsigned shift=(reg&2)*8;
        return (original&~(0xffffu<<shift))|(uint32_t(value)<<shift);
    }
    static uint16_t directGain(uint16_t raw) { return uint16_t(uint32_t(raw)*2); }
public:
    struct Access { uint32_t address,physical; uint16_t value,previous; bool write; };
    SpuIdleVoices() { cores[0].mmix=0xff0; cores[1].mmix=0xffc; }
    void enable(uint64_t modeledCycle,const SpuRamWritePort* memory=nullptr) {
        if (enabled) throw std::runtime_error("Stopped SPU voices already enabled");
        enabled=true; cycle=modeledCycle; ram=memory;
    }
    bool allows(uint32_t address,uint32_t opcode,uint32_t value=0) const {
        if (!enabled || !mapped(address)) return false;
        if (opcode==0x21 || opcode==0x25) return true;
        if (opcode!=0x29) return false;
        const auto reg=address&0x3ff;
        if (reg<0x180) {
            const auto field=(reg&15)/2;
            if (field<2) return !(value&0x8000);
            if (field==5) return uint16_t(value)==0;
        }
        if (reg==0x1a0 || reg==0x1a2) {
            const auto& core=cores[(address>>10)&1];
            const uint32_t mask=uint32_t(uint16_t(value))<<((reg&2)*8);
            if (mask&core.masks[1]) return false; // Noise needs a real generator.
            for (unsigned v=0;v<24;++v)
                if ((mask&(1u<<v)) && !silentBlock(core.voices[v].addresses[0])) return false;
        }
        return true;
    }
    uint16_t read(uint32_t address) const {
        if (!allows(address,0x25)) throw std::runtime_error("Unsupported stopped-voice read");
        const auto reg=address&0x3ff; const auto& core=cores[(address>>10)&1];
        if (reg<0x180) {
            const auto& voice=core.voices[reg/16]; const auto field=(reg&15)/2;
            return field==5 ? uint16_t(voice.envelope.value) : field<6 ? voice.parameters[field] : directGain(voice.parameters[field-6]);
        }
        if (reg<0x198) return uint16_t(core.masks[(reg-0x180)/4]>>((reg&2)*8));
        if (reg==0x198) return core.mmix;
        if (reg==0x1a0 || reg==0x1a2) return uint16_t(core.keyOn>>((reg&2)*8));
        if (reg==0x1a4 || reg==0x1a6) return uint16_t(core.keyOff>>((reg&2)*8));
        if (reg>=0x1c0 && reg<0x2e0) {
            const auto at=reg-0x1c0, field=(at%12)/2;
            return uint16_t(core.voices[at/12].addresses[field/2]>>((field&1)?0:16));
        }
        return uint16_t(core.endx>>((reg&2)*8));
    }
    uint32_t pendingKeyOff(unsigned core) const { return cores.at(core).keyOff; }
    uint64_t sampleCount() const { return samples; }
    uint64_t stoppedReleaseCount() const { return released; }
    uint32_t retiredStores() const { return stores; }
    void advanceTo(uint64_t modeledCycle) {
        if (!enabled) return;
        if (modeledCycle<cycle) throw std::runtime_error("SPU sample clock moved backwards");
        const uint64_t elapsed=modeledCycle-cycle+fraction;
        cycle=modeledCycle; fraction=uint32_t(elapsed%768);
        const uint64_t ticks=elapsed/768;
        if (!ticks) return;
        samples+=ticks;
        for (uint64_t tick=0;tick<ticks;++tick) for (auto& core:cores) {
            released+=std::popcount(core.keyOff&0xffffffu);
            core.endx&=~core.keyOn;
            for (unsigned n=0;n<24;++n) {
                auto& voice=core.voices[n]; const auto bit=1u<<n;
                if (core.keyOff&bit) voice.envelope.release();
                if (core.keyOn&bit) {
                    if ((core.masks[1]&bit) || !silentBlock(voice.addresses[0]))
                        throw std::runtime_error("SPU key-on sample changed before retirement boundary");
                    voice.envelope.attack(); voice.loopMode=false;
                    voice.addresses[2]=voice.addresses[0]|1;
                    voice.pitchFraction=voice.decoded=voice.consumed=0; voice.started=true;
                }
                if (!voice.started) continue;
                if ((core.masks[1]&bit) || !silentBlock(voice.addresses[2]))
                    throw std::runtime_error("SPU voice left the verified zero-sample loop subset");
                if (!voice.loopMode) voice.addresses[1]=voice.addresses[2]&0xffff8;
                if (int32_t(voice.decoded-voice.consumed)<=12) {
                    voice.decoded+=4;
                    voice.addresses[2]=(voice.addresses[2]+1)&0xfffff;
                    if (!(voice.addresses[2]&7)) {
                        core.endx|=bit;
                        voice.addresses[2]=(voice.addresses[1]+1)&0xfffff;
                    }
                }
                voice.envelope.tick(voice.parameters[3],voice.parameters[4]);
                voice.pitchFraction+=std::min(voice.parameters[2],uint16_t(0x3fff));
                voice.consumed+=voice.pitchFraction>>12; voice.pitchFraction&=0xfff;
            }
            core.keyOn=core.keyOff=0;
        }
    }
    template<class Memory> Access stage(Memory& memory,uint32_t address,uint32_t physical,
                                       uint32_t opcode,uint32_t value) const {
        if (!allows(physical,opcode,value)) throw std::runtime_error("Unsupported stopped-voice access");
        Access access{address,physical,uint16_t(value),memory.read16(address),opcode==0x29};
        if (!access.write) memory.write16(address,read(physical));
        return access;
    }
    template<class Memory> void complete(Memory& memory,const Access& access,bool exception) {
        memory.write16(access.address,access.previous);
        if (exception || !access.write) return;
        const auto reg=access.physical&0x3ff; auto& core=cores[(access.physical>>10)&1];
        if (reg<0x180) {
            auto& voice=core.voices[reg/16]; const auto field=(reg&15)/2;
            if (field<6) voice.parameters[field]=access.value; // Current-volume writes are ignored by reference.
            if (field==5) voice.envelope.value=access.value;
        } else if (reg<0x198) {
            auto& mask=core.masks[(reg-0x180)/4]; mask=half(mask,reg,access.value);
        } else if (reg==0x198) core.mmix=access.value;
        else if (reg==0x1a0 || reg==0x1a2) core.keyOn=half(core.keyOn,reg,access.value);
        else if (reg==0x1a4 || reg==0x1a6) core.keyOff=half(core.keyOff,reg,access.value);
        else if (reg>=0x1c0 && reg<0x2e0) {
            const auto at=reg-0x1c0, field=(at%12)/2; auto& voice=core.voices[at/12];
            auto& target=voice.addresses[field/2];
            target=(field&1) ? (target&0xf0000)|(access.value&0xfff8) : (uint32_t(access.value&15)<<16)|(target&0xfff8);
            if (field/2==1) voice.loopMode=true;
            if (field/2==2) target|=1;
        } else if (reg==0x340) core.endx&=0xff0000;
        else if (reg==0x342) core.endx&=0xffff;
        ++stores;
        if (stores<=16 || stores%256==0)
            std::cerr << "[spu-stopped-voice-store] address=0x" << std::hex << access.physical << " value=0x" << access.value
                      << std::dec << " count=" << stores << "; guest SH retired\n";
    }
};
