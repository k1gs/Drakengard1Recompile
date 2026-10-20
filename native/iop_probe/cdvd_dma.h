#pragma once
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <span>

// Functional DMA3 receiver for user sectors or reviewed reference DVD packets. Arming is
// separate from receiving; STR survives an empty drive or disabled DMA gates.
class CdvdDma {
    static constexpr uint32_t Madr = 0x1f8010b0, Bcr = Madr + 4, Chcr = Madr + 8;
    uint64_t bytes{}, completions{};
    template<class Memory> static uint32_t configuration(const Memory& memory, uint32_t control) {
        const auto address = memory.read32(Madr), count = memory.read32(Bcr);
        const uint64_t length = uint64_t(count >> 16) * (count & 0xffffu) * 4;
        const auto words = count & 0xffffu;
        uint32_t formats = 0;
        if (words && 512 % words == 0 && (count >> 16) % (512 / words) == 0) formats |= 1;
        if (words && 516 % words == 0 && (count >> 16) % (516 / words) == 0) formats |= 2;
        if (control != 0x41000200 || !formats || !(count >> 16) ||
            (address & 3) || address >= Memory::RamSize ||
            length > Memory::RamSize - address)
            throw std::runtime_error("Unreviewed CDVD DMA3 configuration or RAM range");
        return formats;
    }
    template<class Memory> void complete(Memory& memory, uint32_t control) {
        memory.write32(Chcr, control & ~0x01000000u);
        // Pinned DMA3 completion differs from the unmaskable SIF channels.
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
        const auto formats = configuration(memory, control);
        const auto enable = memory.read32(0x1f801578);
        if (enable > 1) throw std::runtime_error("Unreviewed IOP DMA global enable");
        if (!enable || !(memory.read32(0x1f8010f0) & 0x8000) || !drive.pendingBytes()) return;
        std::array<uint8_t, 2064> buffer;
        if (drive.pendingToc()) {
            if (memory.read32(Bcr) != 0x00810004)
                throw std::runtime_error("Unsupported DVD TOC DMA3 configuration");
            const auto toc = std::span(buffer).first(2048);
            if (drive.drainToc(toc) != toc.size() || !memory.writeRam(memory.read32(Madr), toc.data(), toc.size()))
                throw std::runtime_error("DVD TOC DMA3 structure transfer failed");
            bytes += toc.size(); complete(memory, control);
            return; // Pinned TOC path leaves MADR/BCR intact; last 16 bytes are not written.
        }
        const auto sectorSize = drive.dmaSectorSize();
        if ((sectorSize != 2048 && sectorSize != 2064) ||
            !(formats & (sectorSize == 2048 ? 1u : 2u)) || drive.pendingBytes() < sectorSize)
            throw std::runtime_error("CDVD DMA3 needs a complete supported pending sector");
        const auto sector = std::span(buffer).first(sectorSize);
        const auto address = memory.read32(Madr), count = memory.read32(Bcr);
        if (drive.drainDmaSectors(sector) != sector.size() ||
            !memory.writeRam(address, sector.data(), sector.size()))
            throw std::runtime_error("CDVD DMA3 actual sector transfer failed");
        bytes += sector.size();
        const auto next = count - ((sectorSize / ((count & 0xffffu) * 4)) << 16);
        memory.write32(Madr, address + uint32_t(sector.size()));
        memory.write32(Bcr, next);
        if (next >> 16) return;
        complete(memory, control);
    }
};
