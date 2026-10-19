#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <span>
#include <stdexcept>
#include <utility>

// Resumable functional source chain. False means FIFO full: preserve progress,
// STR and history until a later pump. Exceptions remain fatal device failures.
class Sif0Sender {
    std::function<bool(std::span<const uint8_t>)> sink;
    bool active{}, loaded{}, terminal{}, blocked{};
    uint32_t remaining{}, madr{}, tadr{}, tags{};
    uint64_t bytes{};
    std::array<uint8_t, 16> previous{};
    static void put(auto& data, unsigned at, uint32_t value) {
        for (unsigned n = 0; n < 4; ++n) data[at + n] = uint8_t(value >> (n * 8));
    }
    bool emit(std::span<const uint8_t> quantum) {
        if (!sink(quantum)) {
            if (!blocked) std::cerr << "[strict-sif0-paused] FIFO full; source progress retained\n";
            blocked = true; return false;
        }
        if (blocked) std::cerr << "[strict-sif0-resumed] FIFO accepted next quantum\n";
        blocked = false; return true;
    }
public:
    void bind(std::function<bool(std::span<const uint8_t>)> target) {
        if (sink || !target) throw std::runtime_error("SIF0 sink requires one fresh binding");
        sink = std::move(target);
    }
    template<class Memory> void start(Memory& memory) {
        if (active || !sink || memory.read32(0x1f801528) != 0x01000701 ||
            memory.read32(0x1f801524) != 0x20 || memory.read32(0x1f801578) != 1 ||
            !(memory.read32(0x1f801570) & 0x800))
            throw std::runtime_error("SIF0 requires an enabled, reviewed source chain and an actual sink");
        active = true; loaded = terminal = blocked = false;
        tags = 0; bytes = 0;
        tadr = memory.read32(0x1f80152c);
        madr = memory.read32(0x1f801520);
        pump(memory);
    }
    template<class Memory> void pump(Memory& memory) {
        if (!active) return;
        if (memory.read32(0x1f801528) != 0x01000701 || memory.read32(0x1f801524) != 0x20 ||
            memory.read32(0x1f801578) != 1 || !(memory.read32(0x1f801570) & 0x800) ||
            memory.read32(0x1f801520) != madr || memory.read32(0x1f80152c) != tadr)
            throw std::runtime_error("SIF0 active source configuration changed while paused");
        const auto range = [](uint32_t address, uint64_t size) {
            if ((address & 3) || address >= Memory::RamSize || size > Memory::RamSize - address)
                throw std::runtime_error("SIF0 source/tag exceeds actual word-aligned IOP RAM");
        };
        while (active) {
            if (!loaded) {
                if (tags == 128) throw std::runtime_error("SIF0 source tag budget exhausted; no completion invented");
                range(tadr, 24); // Includes the next tag's two real history words.
                const uint32_t tag = memory.read32(tadr), count = memory.read32(tadr + 4);
                const uint32_t source = tag & 0xffffffu;
                if ((tag & 0x3f000000u) || (count & ~0xfffffu))
                    throw std::runtime_error("Unreviewed SIF0 source tag flags/count mode");
                range(source, uint64_t(count) * 4);
                std::array<uint8_t, 16> header{};
                for (unsigned n = 0; n < 4; ++n) put(header, n * 4, memory.read32(tadr + 8 + n * 4));
                if (!emit(header)) return;
                previous = header; tadr += 16; madr = source; remaining = count;
                terminal = (tag & 0xc0000000u) != 0; loaded = true; ++tags;
                memory.write32(0x1f80152c, tadr);
                memory.write32(0x1f801520, madr);
            }
            while (remaining) {
                // Missing tail positions repeat the preceding complete quantum.
                // They are not source bytes and must not advance IOP MADR.
                auto quantum = previous;
                const uint32_t words = remaining < 4 ? remaining : 4;
                for (uint32_t n = 0; n < words; ++n)
                    put(quantum, n * 4, memory.read32(madr + n * 4));
                if (!emit(quantum)) return;
                remaining -= words; bytes += words * 4; madr += words * 4;
                memory.write32(0x1f801520, madr);
                previous = quantum;
            }
            loaded = false;
            if (terminal) {
                memory.write32(0x1f801528, memory.read32(0x1f801528) & ~0x01000000u);
                auto icr = memory.read32(0x1f801574) | 0x04000000u;
                if (icr & 0x00800000u) icr |= 0x80000000u;
                memory.write32(0x1f801574, icr);
                memory.setInterruptStatus(memory.interruptStatus() | 8u);
                active = false;
                std::cerr << "[strict-sif0-complete] source_bytes=" << bytes << " tags=" << tags
                          << " madr=0x" << std::hex << memory.read32(0x1f801520)
                          << " icr2=0x" << icr << std::dec << '\n';
                return;
            }
        }
    }
};
