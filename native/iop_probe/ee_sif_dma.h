#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>

// Empty SIF0 receiver plus a bounded SIF1 source-chain engine. Only accepted
// source data changes progress/completion; empty receiver reads never clear STR.
class EeSifDma {
    std::array<uint32_t, 8> registers{};
    uint32_t enableWord{};
    bool sif1Enabled{}, completion{};
    static unsigned slot(uint32_t address) {
        const auto base = address & ~0xffu, offset = address & 0xffu;
        if ((base != 0x1000c000 && base != 0x1000c400) || offset > 0x30 || (offset & 15))
            throw std::runtime_error("Unreviewed EE SIF DMA register");
        return (base == 0x1000c400 ? 4 : 0) + offset / 16;
    }
public:
    void seedEnableWord(uint32_t value) {
        if (sif1Enabled || completion) throw std::runtime_error("EE DMA passive seed applied after activation");
        for (auto word : registers) if (word) throw std::runtime_error("EE DMA passive seed requires reset channels");
        enableWord = value;
    }
    void enableSif1() { sif1Enabled = true; }
    bool takeCompletion() { return std::exchange(completion, false); }
    static bool contains(uint32_t address) {
        return address == 0x1000f520 || address == 0x1000f590 ||
               (address >= 0x1000c000 && address < 0x1000c100) ||
               (address >= 0x1000c400 && address < 0x1000c500);
    }
    uint32_t read(uint32_t address) const {
        if (address == 0x1000f520) return enableWord;
        return registers[slot(address)];
    }
    void write(uint32_t address, uint32_t value) {
        if (address == 0x1000f590) { enableWord = value; return; }
        const auto index = slot(address);
        if (index == 4 && value != 0 && (!sif1Enabled || (value & 0xffffu) != 0x184))
            throw std::runtime_error("EE SIF1 DMA start requires an actual transfer engine");
        if (index == 0 && value != 0 && value != 0x184)
            throw std::runtime_error("Unreviewed EE SIF0 receiver mode");
        registers[index] = value;
    }
    template<class Sink> void pumpSif1(std::span<const uint8_t> ram, Sink sink) {
        if (!(registers[4] & 0x100)) return;
        if (!sif1Enabled || (registers[4] & 0xffffu) != 0x184 || (enableWord & 0x10000u) || registers[6])
            throw std::runtime_error("Unreviewed EE SIF1 start/suspend/QWC state");
        const auto range = [&](uint32_t address, size_t size) {
            if ((address & 15) || address >= ram.size() || size > ram.size() - address)
                throw std::runtime_error("EE SIF1 chain/source exceeds actual aligned RAM");
            return ram.subspan(address, size);
        };
        const auto word = [](std::span<const uint8_t> data, size_t at) {
            return uint32_t(data[at]) | uint32_t(data[at + 1]) << 8 |
                uint32_t(data[at + 2]) << 16 | uint32_t(data[at + 3]) << 24;
        };
        for (unsigned tags = 0; tags < 128; ++tags) {
            const uint32_t tadr = registers[7];
            const auto tag = range(tadr, 16);
            const uint32_t header = word(tag, 0), source = word(tag, 4), id = (header >> 28) & 7u;
            if ((header & 0x0fff0000u) || (id != 0 && id != 2 && id != 3))
                throw std::runtime_error("Unreviewed EE SIF1 source tag");
            const uint32_t count = header & 0xffffu;
            const uint32_t madr = id == 2 ? tadr + 16 : source;
            const auto payload = range(madr, size_t(count) * 16);
            if (id == 2) range(source, 16);
            registers[4] = (registers[4] & 0xffffu) | (header & 0xffff0000u);
            registers[5] = madr; registers[6] = count;
            registers[7] = id == 2 ? source : tadr + 16;
            for (uint32_t quantum = 0; quantum < count; ++quantum) {
                sink(payload.subspan(size_t(quantum) * 16, 16));
                registers[5] += 16; --registers[6];
            }
            if (id == 0 || (header & 0x80000000u)) {
                registers[4] &= ~0x100u; completion = true;
                return;
            }
        }
        throw std::runtime_error("EE SIF1 tag budget exhausted; no completion invented");
    }
};
