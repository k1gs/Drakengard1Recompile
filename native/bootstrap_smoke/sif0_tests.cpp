#include "sif0_sender.h"
#include "ee_sif_dma.h"
#include "sif0_fifo.h"
#include <map>
#include <vector>
#include <algorithm>

namespace {
void check(bool value) { if (!value) throw std::runtime_error("SIF0 contract check failed"); }
template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Unsupported SIF0 operation accepted");
}
struct Memory {
    static constexpr uint32_t RamSize = 0x2000;
    std::map<uint32_t, uint32_t> words;
    uint32_t irq{};
    uint32_t read32(uint32_t address) const {
        const auto it = words.find(address); return it == words.end() ? 0 : it->second;
    }
    void write32(uint32_t address, uint32_t value) { words[address] = value; }
    void setInterruptStatus(uint32_t value) { irq = value; }
    uint32_t interruptStatus() const { return irq; }
};
Memory source(unsigned count) {
    Memory memory;
    for (auto [address, value] : std::map<uint32_t, uint32_t>{
        {0x1f801528, 0x01000701}, {0x1f801524, 0x20}, {0x1f801578, 1},
        {0x1f801570, 0x800}, {0x1f80152c, 0x104}, {0x104, 0x80000400},
        {0x108, count}, {0x10c, 0x90000000u | ((count + 3) / 4)}, {0x110, 0x800},
        {0x114, 0x13579bdf}, {0x118, 0x2468ace0}}) memory.write32(address, value);
    for (unsigned n = 0; n < count; ++n) memory.write32(0x400 + n * 4, 0xabc00000u + n);
    return memory;
}
uint32_t word(const std::vector<uint8_t>& ram, unsigned at) {
    return uint32_t(ram[at]) | uint32_t(ram[at+1]) << 8 |
        uint32_t(ram[at+2]) << 16 | uint32_t(ram[at+3]) << 24;
}
}
void checkSif0() {
    {
        auto memory = source(257);
        std::vector<uint8_t> ram(0x2000, 0xcd);
        EeSifDma ee; ee.seedEnableWord(0x1201);
        Sif0Fifo fifo; Sif0Sender sender;
        sender.bind([&](auto quantum) { return fifo.push(quantum); });
        const auto drain = [&](uint32_t control) {
            return fifo.drain([&](auto quantum) {
                if (!ee.canReceiveSif0(control)) return false;
                ee.feedSif0(ram, quantum, control); return true;
            });
        };
        sender.start(memory);
        check(fifo.size() == 32 && memory.read32(0x1f801520) == 0x5f0 &&
              memory.read32(0x1f80152c) == 0x114 && !memory.irq);
        sender.pump(memory); // Full FIFO does not advance or complete a paused producer.
        check(memory.read32(0x1f801520) == 0x5f0 && !memory.irq && !drain(1));
        refuses([&] { sender.start(memory); });
        ee.write(0x1000c000, 0x184);
        check(!drain(0) && fifo.size() == 32 && !ee.takeSif0Completion());
        ee.write(0x1000f590, 0x10000); check(!drain(1) && fifo.size() == 32);
        ee.write(0x1000f590, 0x1201);
        check(drain(1) == 32 && fifo.size() == 0 && ee.read(0x1000c020) == 34);
        sender.pump(memory);
        check(fifo.size() == 32 && memory.read32(0x1f801520) == 0x7f0 && !memory.irq);
        check(drain(1) == 32 && ee.read(0x1000c020) == 2 && !ee.takeSif0Completion());
        sender.pump(memory);
        check(fifo.size() == 2 && memory.read32(0x1f801520) == 0x804 && memory.irq == 8 &&
              !(memory.read32(0x1f801528) & 0x01000000) && !ee.takeSif0Completion());
        check(drain(1) == 2 && ee.takeSif0Completion());
        for (unsigned n = 0; n < 257; ++n) check(word(ram, 0x800 + n * 4) == 0xabc00000u + n);
        for (unsigned n = 257; n < 260; ++n) check(word(ram, 0x800 + n * 4) == 0xabc00000u + n - 4);
        check(ram[0x7ff] == 0xcd && ram[0xc10] == 0xcd);
    }
    {
        // Back-to-back terminal replies remain queued until each actual rearm.
        auto memory = source(6); Sif0Fifo fifo; Sif0Sender sender;
        sender.bind([&](auto quantum) { return fifo.push(quantum); });
        sender.start(memory);
        memory = source(2); memory.write32(0x110, 0x900); sender.start(memory);
        check(fifo.size() == 5 && memory.irq == 8);
        EeSifDma ee; ee.seedEnableWord(0x1201);
        std::vector<uint8_t> ram(0x2000, 0xcd);
        const auto drain = [&] {
            return fifo.drain([&](auto quantum) {
                if (!ee.canReceiveSif0(1)) return false;
                ee.feedSif0(ram, quantum, 1); return true;
            });
        };
        ee.write(0x1000c000, 0x184); check(drain() == 3 && fifo.size() == 2);
        ee.write(0x1000c000, 0); ee.write(0x1000c020, 0);
        check(!drain() && fifo.size() == 2 && word(ram, 0x900) == 0xcdcdcdcd);
        ee.write(0x1000c000, 0x184); check(!drain()); // Preserve unconsumed completion.
        check(ee.takeSif0Completion() && drain() == 2 && ee.takeSif0Completion());
        check(word(ram, 0x800) == 0xabc00000 && word(ram, 0x900) == 0xabc00000 && fifo.size() == 0);
        std::array<uint8_t, 16> quantum{};
        check(fifo.push(quantum));
        refuses([&] { fifo.drain([](auto) -> bool { throw std::runtime_error("bad destination tag"); }); });
        check(fifo.size() == 1);
        refuses([&] { fifo.push(std::span<const uint8_t>(quantum).first(15)); });
        check(fifo.size() == 1);
    }
    {
        auto memory = source(6); Sif0Fifo fifo; std::array<uint8_t, 16> filler{};
        for (unsigned n = 0; n < 32; ++n) check(fifo.push(filler));
        Sif0Sender sender; sender.bind([&](auto quantum) { return fifo.push(quantum); });
        sender.start(memory); sender.pump(memory);
        check(memory.read32(0x1f80152c) == 0x104 && memory.read32(0x1f801520) == 0 && !memory.irq);
        memory.write32(0x1f80152c, 0x114);
        refuses([&] { sender.pump(memory); }); // Changed active source is not silently resumed.
        check(fifo.size() == 32 && !memory.irq);
        memory.write32(0x1f80152c, 0x104);
        check(fifo.drain([](auto) { return true; }) == 32);
        sender.pump(memory);
        check(fifo.size() == 3 && memory.irq == 8 && memory.read32(0x1f801520) == 0x418);
    }
    for (unsigned count : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 129u}) {
        auto memory = source(count);
        EeSifDma ee; ee.seedEnableWord(0x1201); ee.write(0x1000c000, 0x184);
        std::vector<uint8_t> ram(0x2000, 0xcd);
        Sif0Sender sender;
        unsigned quanta = 0;
        sender.bind([&](auto quantum) {
            check(!ee.takeSif0Completion()); // Completion only follows the last quantum.
            ee.feedSif0(ram, quantum, 1); ++quanta; return true;
        });
        sender.start(memory);
        const unsigned rounded = (count + 3) & ~3u;
        check(quanta == 1 + rounded / 4 && ee.takeSif0Completion() && !ee.takeSif0Completion());
        check(!(ee.read(0x1000c000) & 0x100) && ee.read(0x1000c020) == 0 &&
              ee.read(0x1000c010) == 0x800 + rounded * 4);
        for (unsigned n = 0; n < count; ++n) check(word(ram, 0x800 + n * 4) == 0xabc00000u + n);
        for (unsigned n = count; n < rounded; ++n) {
            const unsigned position = n & 3;
            const uint32_t expected = count >= 4 ? 0xabc00000u + (count & ~3u) - 4 + position :
                memory.read32(0x10c + position * 4);
            check(word(ram, 0x800 + n * 4) == expected);
        }
        check(ram[0x7ff] == 0xcd && ram[0x800 + rounded * 4] == 0xcd);
        check(memory.read32(0x1f801520) == 0x400 + count * 4 && memory.read32(0x1f80152c) == 0x114 &&
              !(memory.read32(0x1f801528) & 0x01000000) && memory.irq == 8 &&
              memory.read32(0x1f801574) == 0x04000000);
    }
    for (unsigned failure = 0; failure < 7; ++failure) {
        auto memory = source(6); unsigned calls = 0; Sif0Sender sender;
        if (failure != 0) sender.bind([&](auto) { ++calls; return true; });
        if (failure == 1) memory.write32(0x1f801578, 0);
        if (failure == 2) memory.write32(0x1f801570, 0);
        if (failure == 3) memory.write32(0x1f80152c, 0x1ff0);
        if (failure == 4) memory.write32(0x108, 0x100000);
        if (failure == 5) memory.write32(0x104, 0x81000400);
        if (failure == 6) memory.write32(0x104, 0x80001ff0);
        refuses([&] { sender.start(memory); });
        check(!calls && memory.irq == 0 && memory.read32(0x1f801528) == 0x01000701);
    }
    auto memory = source(6); Sif0Sender blocked;
    blocked.bind([](auto) -> bool { throw std::runtime_error("receiver unavailable"); });
    refuses([&] { blocked.start(memory); });
    check(!memory.irq && memory.read32(0x1f80152c) == 0x104);
    // Two IOP records, one continuing EE chain, then an END without tag IRQ.
    memory = source(4); memory.write32(0x104, 0x400);
    memory.write32(0x10c, 0x10000001);
    memory.write32(0x114, 0x40000500); memory.write32(0x118, 4);
    memory.write32(0x11c, 0x70000001); memory.write32(0x120, 0x900);
    for (unsigned n = 0; n < 4; ++n) memory.write32(0x500 + n * 4, n + 17);
    EeSifDma chain; chain.seedEnableWord(0x1201); chain.write(0x1000c000, 0x184);
    std::vector<uint8_t> chainRam(0x2000);
    Sif0Sender chainSender;
    chainSender.bind([&](auto quantum) { chain.feedSif0(chainRam, quantum, 1); return true; });
    chainSender.start(memory);
    check(chain.takeSif0Completion() && word(chainRam, 0x800) == 0xabc00000 &&
          word(chainRam, 0x900) == 17 && memory.read32(0x1f80152c) == 0x124);
    // A finite run of nonterminal empty tags must stop at the budget, without IRQ.
    memory = source(0);
    for (unsigned n = 0; n < 128; ++n) {
        memory.write32(0x104 + n * 16, 0x400); memory.write32(0x108 + n * 16, 0);
    }
    Sif0Sender bounded; bounded.bind([](auto) { return true; });
    refuses([&] { bounded.start(memory); });
    check(!memory.irq && (memory.read32(0x1f801528) & 0x01000000));
    memory.write32(0x1f80152c, 0x104); memory.write32(0x1f801520, 0);
    Sif0Fifo boundedFifo; Sif0Sender resumedBudget;
    resumedBudget.bind([&](auto quantum) { return boundedFifo.push(quantum); });
    resumedBudget.start(memory);
    for (unsigned n = 0; n < 2; ++n) {
        check(boundedFifo.drain([](auto) { return true; }) == 32);
        resumedBudget.pump(memory);
    }
    check(boundedFifo.drain([](auto) { return true; }) == 32);
    refuses([&] { resumedBudget.pump(memory); }); // The same tag cap survives all pauses.
    check(!memory.irq && boundedFifo.size() == 32);
    std::array<uint8_t, 16> tag{}; tag[0] = 2; tag[3] = 0x90; tag[5] = 8;
    std::array<uint8_t, 16> payload{}; payload.fill(0x5a);
    std::vector<uint8_t> ram(0x2000, 0xcd);
    EeSifDma ee; ee.seedEnableWord(0x1201); ee.write(0x1000c000, 0x184);
    refuses([&] { ee.feedSif0(ram, tag, 0); });
    ee.write(0x1000f590, 0x10000);
    refuses([&] { ee.feedSif0(ram, tag, 1); });
    ee.write(0x1000f590, 0x1201);
    ee.feedSif0(ram, tag, 1); ee.feedSif0(ram, payload, 1);
    check(ee.read(0x1000c020) == 1 && !ee.takeSif0Completion() && (ee.read(0x1000c000) & 0x100));
    refuses([&] { ee.write(0x1000c010, 0x1fff); });
    refuses([&] { ee.write(0x1000c000, 0); });
    refuses([&] { ee.feedSif0(ram, payload, 0); });
    check(ee.read(0x1000c020) == 1 && !ee.takeSif0Completion());
    ee.feedSif0(ram, payload, 1);
    ee.write(0x1000c000, 0x184);
    refuses([&] { ee.feedSif0(ram, tag, 1); }); // Cannot overwrite an unconsumed event.
    check(ee.takeSif0Completion());
    ee.write(0x1000c000, 0x184); tag[3] = 0; // CNTS/stall semantics unsupported.
    refuses([&] { ee.feedSif0(ram, tag, 1); });
    tag[3] = 0x90; tag[4] = 1;
    refuses([&] { ee.feedSif0(ram, tag, 1); });
    tag[4] = 0; tag[5] = 0x20;
    refuses([&] { ee.feedSif0(ram, tag, 1); });
    check(!ee.takeSif0Completion());
}
