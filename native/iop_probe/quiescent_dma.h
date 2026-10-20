#pragma once
#include "core/iop_memory.h"
#include "cdvd_dma.h"
#include <cstdint>
#include <stdexcept>

// Register/acknowledgement model. Explicitly enabled SIF and CDVD channels may
// arm and wait; their separate engines own actual transfers and completion.
class QuiescentDma {
    bool m_sif1Receiver{};
    bool m_sif0Sender{};
    bool m_cdvdReceiver{};
public:
    void enableCdvdReceiver() { m_cdvdReceiver = true; }
    void enableSif1Receiver() { m_sif1Receiver = true; }
    void enableSif0Sender() { m_sif0Sender = true; }
    static bool interruptRegister(uint32_t address) {
        return address == 0x1f8010f4 || address == 0x1f801574;
    }
    static bool priorityRegister(uint32_t address) {
        return address == 0x1f8010f0 || address == 0x1f801570 || address == 0x1f8015f0;
    }
    static bool channelRegister(uint32_t address) {
        const uint32_t offset = address & 15;
        if (address >= 0x1f801080 && address < 0x1f8010f0)
            return offset == 0 || offset == 4 || offset == 8 || address == 0x1f8010cc;
        if (address >= 0x1f801500 && address < 0x1f801560)
            return offset == 0 || offset == 4 || offset == 8 || address == 0x1f80152c;
        return address == 0x1f801560 || address == 0x1f801564 || address == 0x1f801568;
    }
    void reset(ps2x::iop::detail::IopMemory& memory) {
        for (uint32_t address = 0x1f801080; address <= 0x1f8015f0; address += 4)
            if (interruptRegister(address) || priorityRegister(address) || channelRegister(address))
                memory.write32(address, 0);
    }
    bool allows(uint32_t address, uint32_t opcode, uint32_t value,
                const ps2x::iop::detail::IopMemory* memory = nullptr) const {
        const auto offset = address & 15;
        if ((opcode == 0x21 || opcode == 0x25 || opcode == 0x29) &&
            (offset == 4 || offset == 6) && channelRegister(address & ~3u)) return true;
        if (opcode != 0x23 && opcode != 0x2b) return false;
        if (interruptRegister(address) || priorityRegister(address)) return true;
        if (!channelRegister(address)) return false;
        if (opcode == 0x2b && address == 0x1f8010b8 && (value & 0x01000000) && m_cdvdReceiver) {
            if (!memory) throw std::runtime_error("CDVD DMA3 requires backing memory");
            CdvdDma::validateStart(*memory, value);
            return true;
        }
        if (opcode == 0x2b && address == 0x1f801528 && value == 0x01000701 && m_sif0Sender) {
            if (!memory || memory->read32(0x1f801524) != 0x20 ||
                (memory->read32(address) & 0x01000000))
                throw std::runtime_error("Unreviewed SIF0 source configuration or active rearm");
            return true;
        }
        if (opcode == 0x2b && address == 0x1f801538 && value == 0x41000300 && m_sif1Receiver) {
            if (!memory || memory->read32(0x1f801534) != 0x20 ||
                (memory->read32(address) & 0x01000000))
                throw std::runtime_error("Unreviewed SIF1 receiver configuration or active rearm");
            return true; // The core stores STR; it remains set until stop or an actual future transfer.
        }
        if (opcode == 0x2b && (address & 15) == 8 && (value & 0x01000000))
            throw std::runtime_error("IOP DMA channel start requires an actual transfer engine");
        return true;
    }
    void afterWrite(ps2x::iop::detail::IopMemory& memory, uint32_t address, uint32_t previous, uint32_t value) {
        if (!interruptRegister(address)) return;
        uint32_t next = ((previous & 0xff000000u) | (value & 0x00ffffffu)) & ~(value & 0x7f000000u);
        const bool force = (next & 0x8000u) != 0;
        const bool enabled = (next & 0x800000u) != 0;
        const bool flagged = ((next >> 24) & (next >> 16) & 0x7fu) != 0;
        next = (next & 0x7fffffffu) | ((force || (enabled && flagged)) ? 0x80000000u : 0);
        memory.write32(address, next);
        const bool sifPending = address == 0x1f801574 && (next & 0x0c000000u);
        if ((next & 0x80000000u) || sifPending) memory.setInterruptStatus(memory.interruptStatus() | 8u);
    }
};
