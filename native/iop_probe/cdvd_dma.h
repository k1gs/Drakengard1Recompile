#pragma once
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

// Functional DMA3 receiver for complete 2048-byte user sectors. Arming is
// separate from receiving; STR survives an empty drive or disabled DMA gates.
class CdvdDma {
    static constexpr uint32_t Madr = 0x1f8010b0, Bcr = Madr + 4, Chcr = Madr + 8;
    uint64_t bytes{}, completions{};
    template<class Memory> static void configuration(const Memory& memory, uint32_t control) {
        const auto address = memory.read32(Madr), count = memory.read32(Bcr);
        const uint64_t length = uint64_t(count >> 16) * (count & 0xffffu) * 4;
        if (control != 0x41000200 || (count & 0xffffu) != 32 || !(count >> 16) ||
            (count >> 16) % 16 || (address & 3) || address >= Memory::RamSize ||
            length > Memory::RamSize - address)
            throw std::runtime_error("Unreviewed CDVD DMA3 configuration or RAM range");
    }
public:
    uint64_t transferredBytes() const { return bytes; }
    uint64_t completedTransfers() const { return completions; }
    template<class Memory> static void validateStart(const Memory& memory, uint32_t control) {
        if (memory.read32(Chcr) & 0x01000000)
            throw std::runtime_error("CDVD DMA3 active rearm unsupported");
        configuration(memory, control);
    }
    template<class Memory, class Drive> void pump(Memory& memory, Drive& drive) {
        const auto control = memory.read32(Chcr);
        if (!(control & 0x01000000)) return;
        configuration(memory, control);
        const auto enable = memory.read32(0x1f801578);
        if (enable > 1) throw std::runtime_error("Unreviewed IOP DMA global enable");
        if (!enable || !(memory.read32(0x1f8010f0) & 0x8000) || !drive.pendingBytes()) return;
        std::array<uint8_t, 2048> sector;
        if (drive.pendingBytes() < sector.size())
            throw std::runtime_error("CDVD DMA3 needs a complete actual user sector");
        const auto address = memory.read32(Madr), count = memory.read32(Bcr);
        if (drive.drainUserSectors(sector) != sector.size() ||
            !memory.writeRam(address, sector.data(), sector.size()))
            throw std::runtime_error("CDVD DMA3 actual sector transfer failed");
        bytes += sector.size();
        const auto next = count - 0x00100000u; // 2048 / (32 words * 4 bytes).
        memory.write32(Madr, address + uint32_t(sector.size()));
        memory.write32(Bcr, next);
        if (next >> 16) return;
        memory.write32(Chcr, control & ~0x01000000u);
        // Pinned DMA3 completion differs from the unmaskable SIF channels:
        // ICR channel-3 enable controls flag latching and INTC DMA delivery.
        auto icr = memory.read32(0x1f8010f4);
        if (icr & 0x00080000u) {
            icr |= 0x08000000u;
            if (icr & 0x00800000u) icr |= 0x80000000u;
            memory.write32(0x1f8010f4, icr);
            memory.setInterruptStatus(memory.interruptStatus() | 8u);
        }
        ++completions;
        std::cerr << "[strict-cdvd-dma-complete] bytes=" << bytes << " transfers=" << completions
                  << " madr=0x" << std::hex << memory.read32(Madr) << " icr=0x" << icr << std::dec << '\n';
    }
};
