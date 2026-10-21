#pragma once
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

// Functional SPU2 core-volume registers. Direct master volume is signed 15-bit
// doubled; effect/external/input gains are signed 16-bit. Sweeps require sample
// time and remain unsupported. This component never produces samples or IRQs.
class SpuVolumeRegisters {
    struct Core {
        std::array<uint16_t,8> raw{0,0,0,0,0x7fff,0x7fff,0x7fff,0x7fff};
        std::array<int32_t,8> gain{0,0,0,0,32767,32767,32767,32767};
    };
    std::array<Core,2> cores{};
    bool enabled{};
    uint32_t stores{};
    static int index(uint32_t address) {
        if (address & 1) return -1;
        if (address >= 0x1f900760 && address <= 0x1f900772) return int((address-0x1f900760)/2);
        if (address >= 0x1f900788 && address <= 0x1f90079a) return 10+int((address-0x1f900788)/2);
        return -1;
    }
    static int32_t signed16(uint16_t value) { return value & 0x8000 ? int32_t(value)-65536 : value; }
public:
    struct Access { uint32_t address, physical; uint16_t value, previous; bool write; };
    void enable() {
        if (enabled) throw std::runtime_error("SPU volume registers already enabled");
        enabled = true;
    }
    bool allows(uint32_t address, uint32_t opcode, uint32_t value = 0) const {
        const int at = index(address);
        if (!enabled || at < 0) return false;
        if (opcode == 0x21 || opcode == 0x25) return true;
        const unsigned reg = at % 10;
        return opcode == 0x29 && reg < 8 && (reg >= 2 || !(value & 0x8000));
    }
    uint16_t read(uint32_t address) const {
        if (!allows(address,0x25)) throw std::runtime_error("Unsupported SPU volume read");
        const unsigned at = index(address), reg = at % 10;
        const auto& core = cores[at/10];
        return reg < 8 ? core.raw[reg] : uint16_t(core.gain[reg-8]);
    }
    int32_t gain(unsigned core, unsigned reg) const { return cores.at(core).gain.at(reg); }
    uint32_t retiredStores() const { return stores; }
    template<class Memory> Access stage(Memory& memory, uint32_t address, uint32_t physical,
                                       uint32_t opcode, uint32_t value) const {
        if (!allows(physical,opcode,value)) throw std::runtime_error("Unsupported SPU volume access");
        Access access{address,physical,uint16_t(value),memory.read16(address),opcode==0x29};
        if (!access.write) memory.write16(address,read(physical));
        return access;
    }
    template<class Memory> void complete(Memory& memory, const Access& access, bool exception) {
        // Temporary backing slots never become the authoritative device state.
        memory.write16(access.address,access.previous);
        if (exception || !access.write) return;
        const unsigned at = index(access.physical), reg = at % 10;
        auto& core = cores[at/10];
        core.raw[reg] = access.value;
        core.gain[reg] = signed16(reg < 2 ? uint16_t(uint32_t(access.value)*2) : access.value);
        ++stores;
        if (stores <= 32 || stores % 256 == 0)
            std::cerr << "[spu-volume-store] address=0x" << std::hex << access.physical << " raw=0x" << access.value
                      << std::dec << " gain=" << core.gain[reg] << " count=" << stores << "; guest SH retired\n";
    }
};
