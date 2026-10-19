#pragma once
#include "core/iop_memory.h"
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>

// Functional destination-chain receiver. Each FIFO quantum is actual EE data;
// partial payloads keep STR set. Completion needs a terminal tag and all declared
// bytes. No padding or host RPC reply is fabricated.
class Sif1Receiver {
    uint32_t remaining{}, destination{};
    bool terminal{};
    uint64_t bytes{}, completions{};
public:
    uint64_t transferredBytes() const { return bytes; }
    uint64_t completedChains() const { return completions; }
    void feed(ps2x::iop::detail::IopMemory& memory, std::span<const uint8_t> quantum) {
        using ps2x::iop::detail::IopMemory;
        if (quantum.size() != 16 || memory.read32(0x1f801538) != 0x41000300 ||
            memory.read32(0x1f801578) != 1 || !(memory.read32(0x1f801570) & 0x8000))
            throw std::runtime_error("SIF1 requires a ready, enabled chain receiver and a complete FIFO quantum");
        if (!remaining) {
            const auto word = [&](size_t at) {
                return uint32_t(quantum[at]) | uint32_t(quantum[at + 1]) << 8 |
                    uint32_t(quantum[at + 2]) << 16 | uint32_t(quantum[at + 3]) << 24;
            };
            const auto tag = word(0), count = word(4);
            const auto target = tag & 0xffffffu;
            if ((tag & 0x3f000000u) || (count & ~0xffffcu) || (target & 3) ||
                target >= IopMemory::RamSize || uint64_t(count) * 4 > IopMemory::RamSize - target)
                throw std::runtime_error("Unreviewed SIF1 destination tag or RAM range");
            destination = target; remaining = count;
            terminal = (tag & 0xc0000000u) != 0;
            memory.write32(0x1f801530, destination);
            std::cerr << "[strict-sif1-tag] destination=0x" << std::hex << destination
                      << " words=0x" << count << " flags=0x" << (tag & 0xc0000000u) << std::dec << '\n';
        } else {
            if (!memory.writeRam(destination, quantum.data(), quantum.size()))
                throw std::runtime_error("SIF1 actual payload write failed");
            destination += 16; remaining -= 4; bytes += 16;
            memory.write32(0x1f801530, destination);
        }
        if (!remaining && terminal) {
            // Pinned SIF1 completion: channel 10 STR clears and ICR2 channel 3
            // latches even when its mask is off. Master bit follows bit 23.
            memory.write32(0x1f801538, memory.read32(0x1f801538) & ~0x01000000u);
            auto icr = memory.read32(0x1f801574) | 0x08000000u;
            if (icr & 0x00800000u) icr |= 0x80000000u;
            memory.write32(0x1f801574, icr);
            memory.setInterruptStatus(memory.interruptStatus() | 8u);
            terminal = false; ++completions;
            std::cerr << "[strict-sif1-complete] bytes=" << bytes << " chains=" << completions
                      << " madr=0x" << std::hex << destination << " icr2=0x" << icr << std::dec << '\n';
        }
    }
};
