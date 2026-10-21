#pragma once
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

// Shared 2-MiB SPU RAM, reviewed CPU halfword write port and the separate plain
// DMA writer's data sink. No read-FIFO or DMA completion comes from CPU writes.
class SpuRamWritePort {
    struct Core { uint32_t irqa{0x800}, tsa{}; };
    std::array<Core,2> cores{};
    std::vector<uint16_t> ram=std::vector<uint16_t>(0x100000);
    std::array<unsigned,1024> nonzero{};
    std::array<bool,2> dmaBusy{};
    uint64_t written{};
    bool enabled{};
    static bool mapped(uint32_t address) {
        if (address<0x1f900000 || address>=0x1f900800 || (address&1)) return false;
        const auto reg=address&0x3ff;
        return reg==0x19c || reg==0x19e || reg==0x1a8 || reg==0x1aa || reg==0x1ac;
    }
public:
    struct Access { uint32_t address,physical; uint16_t value,previous; bool write; };
    SpuRamWritePort() {
        for (auto base:{0x2800u,0xe870u}) for (unsigned n=0;n<8;++n) { ram[base+n]=0x0707; ++nonzero[(base+n)/1024]; }
    }
    void enable() { if (enabled) throw std::runtime_error("SPU RAM write port already enabled"); enabled=true; }
    bool allows(uint32_t address,uint32_t opcode,uint32_t=0) const {
        if (!enabled || !mapped(address)) return false;
        if (opcode==0x29) return !dmaBusy[(address>>10)&1] || (address&0x3ff)<0x1a8;
        return (address&0x3ff)!=0x1ac && (opcode==0x21 || opcode==0x25);
    }
    uint16_t read(uint32_t address) const {
        if (!allows(address,0x25)) throw std::runtime_error("Unsupported SPU transfer-register read");
        const auto reg=address&0x3ff; const auto& core=cores[(address>>10)&1];
        const auto value=(reg==0x19c || reg==0x19e)?core.irqa:core.tsa;
        return uint16_t(value>>((reg==0x19c || reg==0x1a8)?16:0));
    }
    uint16_t peek(uint32_t word) const { return ram.at(word); }
    bool zeroRange(uint32_t begin,uint32_t end) const {
        if (begin>end || end>=ram.size()) return false;
        while (begin<=end) {
            if (!(begin%1024) && end-begin>=1023) {
                if (nonzero[begin/1024]) return false;
                begin+=1024;
            } else if (ram[begin++]) return false;
        }
        return true;
    }
    uint64_t writtenWords() const { return written; }
    void setDmaBusy(unsigned core,bool value) { dmaBusy.at(core)=value; }
    void transferWord(unsigned core,uint16_t value) {
        auto& cursor=cores.at(core).tsa;
        const auto at=cursor&0xfffff;
        nonzero[at/1024]-=ram[at]!=0; ram[at]=value; nonzero[at/1024]+=value!=0;
        cursor=(cursor+1)&0xfffff;
    }
    template<class Memory> Access stage(Memory& memory,uint32_t address,uint32_t physical,
                                       uint32_t opcode,uint32_t value) const {
        if (!allows(physical,opcode,value)) throw std::runtime_error("Unsupported SPU RAM port access");
        Access access{address,physical,uint16_t(value),memory.read16(address),opcode==0x29};
        if (!access.write) memory.write16(address,read(physical));
        return access;
    }
    template<class Memory> void complete(Memory& memory,const Access& access,bool exception) {
        memory.write16(access.address,access.previous);
        if (exception || !access.write) return;
        const auto reg=access.physical&0x3ff; auto& core=cores[(access.physical>>10)&1];
        if (reg==0x1ac) {
            const auto at=core.tsa&0xfffff;
            nonzero[at/1024]-=ram[at]!=0;
            ram[at]=access.value; nonzero[at/1024]+=access.value!=0;
            core.tsa=(core.tsa+1)&0xfffff;
            ++written;
            if (written<=8 || written%256==0)
                std::cerr << "[spu-ram-port] words=" << written << " next=0x" << std::hex << core.tsa
                          << std::dec << "; actual guest halfword stored\n";
        } else {
            auto& target=(reg==0x19c || reg==0x19e)?core.irqa:core.tsa;
            if (reg==0x19c || reg==0x1a8) target=(target&0xffff)|(uint32_t(access.value)<<16);
            else target=(target&0xffff0000)|access.value;
        }
    }
};
