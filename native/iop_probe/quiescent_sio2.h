#pragma once
#include <cstdint>
#include <array>
#include <stdexcept>

// Reviewed SIO2MAN reset control only. PCSX2 v2.8.2 SetCtrl retains 0x3BC;
// START_TRANSFER (bit 0) is clear, so no transaction or IRQ occurs. PortStat
// reset reads retain 0xF (PCSX2 SioTypes.h); this does not indicate a pad.
// Port controls retain raw setup words. Command queue, other status registers,
// FIFO, pads, memory cards and DMA remain unsupported.
class QuiescentSio2 {
    bool enabled{}, configured{};
    std::array<uint32_t,8> portControls{};
    static bool portControl(uint32_t address) {
        return address>=0x1f808240 && address<=0x1f80825c && !(address&3);
    }
public:
    void enable() {
        if (enabled) throw std::runtime_error("SIO2 initialization profile cannot be enabled twice");
        enabled = true;
    }
    bool allows(uint32_t address, uint32_t opcode, uint32_t value) const {
        if (!enabled) return false;
        if (configured && portControl(address) && (opcode==0x23 || opcode==0x2b)) return true;
        if (configured && address==0x1f808270 && opcode==0x23) return true;
        return address == 0x1f808268 &&
            ((opcode == 0x2b && value == 0x3bc) || (opcode == 0x23 && configured));
    }
    uint32_t read(uint32_t address) const {
        if (!allows(address,0x23,0)) throw std::runtime_error("Unsupported quiescent SIO2 read");
        if (portControl(address)) return portControls[(address-0x1f808240)/4];
        return address==0x1f808270 ? 0xfu : 0x3bcu;
    }
    template<class Memory> void afterWrite(const Memory& memory,uint32_t address=0x1f808268) {
        if (configured && portControl(address)) {
            portControls[(address-0x1f808240)/4]=memory.read32(address);
            return;
        }
        if (address!=0x1f808268) throw std::runtime_error("Unsupported quiescent SIO2 store");
        if (!enabled || memory.read32(0x1f808268) != 0x3bc)
            throw std::runtime_error("SIO2 reset control requires its actual completed CPU store");
        configured = true;
    }
};
