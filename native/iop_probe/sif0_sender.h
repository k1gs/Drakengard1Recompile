#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <span>
#include <stdexcept>
#include <utility>

// Bounded functional SIF0 source chain. The sink must accept each complete FIFO
// quantum. An unavailable receiver fails closed; there is no queued/fake reply.
class Sif0Sender {
    std::function<void(std::span<const uint8_t>)> sink;
public:
    void bind(std::function<void(std::span<const uint8_t>)> target) {
        if (sink || !target) throw std::runtime_error("SIF0 sink requires one fresh binding");
        sink = std::move(target);
    }
    template<class Memory> void start(Memory& memory) {
        if (!sink || memory.read32(0x1f801528) != 0x01000701 ||
            memory.read32(0x1f801524) != 0x20 || memory.read32(0x1f801578) != 1 ||
            !(memory.read32(0x1f801570) & 0x800))
            throw std::runtime_error("SIF0 requires an enabled, reviewed source chain and an actual sink");
        const auto range = [](uint32_t address, uint64_t size) {
            if ((address & 3) || address >= Memory::RamSize || size > Memory::RamSize - address)
                throw std::runtime_error("SIF0 source/tag exceeds actual word-aligned IOP RAM");
        };
        uint64_t bytes = 0;
        for (unsigned tags = 0; tags < 128; ++tags) {
            const uint32_t tadr = memory.read32(0x1f80152c);
            range(tadr, 24); // Includes the next tag's two real history words.
            const uint32_t tag = memory.read32(tadr), count = memory.read32(tadr + 4);
            const uint32_t source = tag & 0xffffffu;
            if ((tag & 0x3f000000u) || (count & ~0xfffffu))
                throw std::runtime_error("Unreviewed SIF0 source tag flags/count mode");
            range(source, uint64_t(count) * 4);
            std::array<uint8_t, 16> previous{};
            const auto put = [](auto& data, unsigned at, uint32_t value) {
                for (unsigned n = 0; n < 4; ++n) data[at + n] = uint8_t(value >> (n * 8));
            };
            for (unsigned n = 0; n < 4; ++n) put(previous, n * 4, memory.read32(tadr + 8 + n * 4));
            sink(previous);
            memory.write32(0x1f80152c, tadr + 16);
            memory.write32(0x1f801520, source);
            for (uint32_t sent = 0; sent < count;) {
                // Missing tail positions repeat the preceding complete quantum.
                // They are not source bytes and must not advance IOP MADR.
                auto quantum = previous;
                const uint32_t words = count - sent < 4 ? count - sent : 4;
                for (uint32_t n = 0; n < words; ++n)
                    put(quantum, n * 4, memory.read32(source + (sent + n) * 4));
                sink(quantum);
                sent += words; bytes += words * 4;
                memory.write32(0x1f801520, source + sent * 4);
                previous = quantum;
            }
            if (tag & 0xc0000000u) {
                memory.write32(0x1f801528, memory.read32(0x1f801528) & ~0x01000000u);
                auto icr = memory.read32(0x1f801574) | 0x04000000u;
                if (icr & 0x00800000u) icr |= 0x80000000u;
                memory.write32(0x1f801574, icr);
                memory.setInterruptStatus(memory.interruptStatus() | 8u);
                std::cerr << "[strict-sif0-complete] source_bytes=" << bytes << " tags=" << tags + 1
                          << " madr=0x" << std::hex << memory.read32(0x1f801520)
                          << " icr2=0x" << icr << std::dec << '\n';
                return;
            }
        }
        throw std::runtime_error("SIF0 source tag budget exhausted; no completion invented");
    }
};
