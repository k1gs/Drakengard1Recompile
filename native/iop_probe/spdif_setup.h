#pragma once
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

// Initial functional SPDIF configuration only. The pinned reference stores
// MODE/MEDIA and normal/PCM-clone output latches; 0x7CA is a raw latch.
// PCM clone uses the same mixer path in the pinned reference. Bypass/streaming,
// IRQINFO, core control, FIFO and DMA remain unsupported.
class SpdifSetup {
    bool enabled{};
    std::array<uint16_t, 3> values{};
    uint16_t output{};
    uint32_t writes{};
public:
    void enable() {
        if (enabled) throw std::runtime_error("SPDIF setup already enabled");
        enabled = true;
    }
    bool allows(uint32_t address, uint32_t opcode, uint32_t value) const {
        if (!enabled) return false;
        if (opcode==0x21 || opcode==0x25)
            return address==0x1f9007c0 || address==0x1f9007c6 || address==0x1f9007c8 || address==0x1f9007ca;
        if (opcode != 0x29) return false;
        return (address == 0x1f9007c0 && value<=0xffff && !(value&0x104)) ||
            (address == 0x1f9007c6 && value == 0x900) ||
            (address == 0x1f9007c8 && value == 0x200) ||
            (address == 0x1f9007ca && value == 8);
    }
    void retire(uint32_t address, uint32_t value) {
        if (!allows(address, 0x29, value)) throw std::runtime_error("Unsupported SPDIF setup store");
        if (address == 0x1f9007c0) output = uint16_t(value);
        else values[(address - 0x1f9007c6) / 2] = uint16_t(value);
        ++writes;
        if (writes <= 16 || writes % 256 == 0)
            std::cerr << "[spdif-setup-store] address=0x" << std::hex << address << " value=0x" << value
                      << std::dec << " count=" << writes << "; original guest SH retired\n";
    }
    const auto& configuration() const { return values; }
    uint16_t read(uint32_t address) const {
        if (!allows(address,0x25,0)) throw std::runtime_error("Unsupported SPDIF latch read");
        return address==0x1f9007c0 ? output : values[(address-0x1f9007c6)/2];
    }
    uint32_t retiredStores() const { return writes; }
    uint16_t outputValue() const { return output; }
    bool pcmClone() const { return output&0x20; }
};
